#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "core/fs.h"
#include "core/helpers.h"
#include "core/paths.h"
#include "core/settings.h"
#include "core/trash.h"

namespace fs = std::filesystem;

static fs::path make_temp_dir(const std::string& name) {
  fs::path p = fs::temp_directory_path() / ("evac_test_" + name);
  fs::remove_all(p);
  fs::create_directories(p);
  return p;
}

static void write_file(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  std::ofstream(path) << content;
}

static std::string read_file(const fs::path& path) {
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

// path with suffix added to its last part, like the trash names a restored
// copy.
static fs::path restored(fs::path path, const std::string& suffix) {
  path += suffix;
  return path;
}

// A fixed time, plus some seconds, so tests don't depend on the clock.
static Trash::Clock::time_point at(int seconds) {
  return Trash::Clock::from_time_t(1792332000) + std::chrono::seconds(seconds);
}

struct TrashFixture {
  TrashFixture(const std::string& name)
      : dir(make_temp_dir(name)),
        settings(dir / "s.json", status),
        paths(dir, settings, status),
        trash(settings, paths, status) {
    settings.load();
  }

  // A directory with files of the given total size.
  fs::path make_thing(const std::string& rel, size_t bytes) {
    fs::path path = dir / rel;
    write_file(path / "data.bin", std::string(bytes, 'x'));
    return path;
  }

  fs::path dir;
  Status status;
  Settings settings;
  Paths paths;
  Trash trash;
};

TEST_CASE("trash settings and directory") {
  TrashFixture f("trash_settings");
  REQUIRE(f.settings.get_bool("use_trash"));
  REQUIRE(f.settings.get_int("trash_max_size_mb") == 5120);
  REQUIRE(!f.settings.get_bool("trash_auto_prune"));
  REQUIRE(f.paths.trash_dir() == f.dir / "trash");

  // The trash is next to the game directories, but is not an install.
  write_file(f.dir / "trash/x" / INFO_FILENAME, "{\"name\": \"Not A Build\"}");
  write_file(f.dir / "dda/game0" / INFO_FILENAME, "{\"name\": \"Test Build\"}");
  Paths::InstallsSummary installs = f.paths.installs_summary();
  REQUIRE(installs.count("trash") == 0);
  REQUIRE(installs["dda"].size() == 1);
  REQUIRE(installs["dda"].count("Test Build") == 1);
  for (Paths::InstallsSummary::const_iterator g = installs.begin();
       g != installs.end(); ++g)
    REQUIRE(g->second.count("Not A Build") == 0);
}

TEST_CASE("remove moves into the trash and list reads it back") {
  TrashFixture f("trash_remove");
  fs::path pack = f.make_thing("dda/userdata/sound/CC-Sounds", 1000);
  write_file(pack / "sfx/boom.ogg", "12345");

  REQUIRE(f.trash.remove(pack, "soundpack", "CC-Sounds", at(0)));
  REQUIRE(!fs::exists(pack));

  std::vector<TrashItem> items = f.trash.list();
  REQUIRE(items.size() == 1);
  REQUIRE(items[0].kind == "soundpack");
  REQUIRE(items[0].name == "CC-Sounds");
  REQUIRE(items[0].game == "dda");
  REQUIRE(items[0].original_path == pack);
  REQUIRE(items[0].size == 1005);
  REQUIRE(items[0].id == Trash::make_id("soundpack", "CC-Sounds", at(0)));
  REQUIRE(f.trash.total_size() == 1005);

  fs::path item_dir = f.paths.trash_dir() / items[0].id / "item";
  REQUIRE(read_file(item_dir / "sfx/boom.ogg") == "12345");
}

TEST_CASE("make_id keeps the name safe for a directory") {
  std::string id = Trash::make_id("backup", "a/b c:d", at(0));
  REQUIRE(id.find('/') == std::string::npos);
  REQUIRE(id.find(' ') == std::string::npos);
  REQUIRE(id.substr(id.size() - 7) == "a_b_c_d");
  REQUIRE(id.find("_backup_") != std::string::npos);

  std::string long_id = Trash::make_id("mod", std::string(100, 'n'), at(0));
  REQUIRE(long_id.size() < 80);
}

TEST_CASE("remove with use_trash off deletes for good") {
  TrashFixture f("trash_off");
  f.settings.set_bool("use_trash", false);
  fs::path thing = f.make_thing("dda/save_backups/old", 10);

  REQUIRE(f.trash.remove(thing, "backup", "old", at(0)));
  REQUIRE(!fs::exists(thing));
  REQUIRE(f.trash.list().empty());
  REQUIRE(!fs::exists(f.paths.trash_dir()));
}

TEST_CASE("removes at the same moment get their own entries") {
  TrashFixture f("trash_same_time");
  fs::path a = f.make_thing("one/Pack", 1);
  fs::path b = f.make_thing("two/Pack", 2);

  REQUIRE(f.trash.remove(a, "soundpack", "Pack", at(0)));
  REQUIRE(f.trash.remove(b, "soundpack", "Pack", at(0)));
  std::vector<TrashItem> items = f.trash.list();
  REQUIRE(items.size() == 2);
  REQUIRE(items[0].id != items[1].id);
  REQUIRE(f.trash.total_size() == 3);
}

TEST_CASE("remove refuses what it can't or shouldn't move") {
  TrashFixture f("trash_refuse");
  REQUIRE(!f.trash.remove(f.dir / "missing", "mod", "missing", at(0)));

  fs::path thing = f.make_thing("thing", 1);
  REQUIRE(f.trash.remove(thing, "mod", "thing", at(0)));
  fs::path entry = f.paths.trash_dir() / f.trash.list()[0].id;

  // The trash itself, something inside it, and a directory that holds it.
  REQUIRE(!f.trash.remove(f.paths.trash_dir(), "mod", "trash", at(1)));
  REQUIRE(!f.trash.remove(entry, "mod", "entry", at(1)));
  REQUIRE(!f.trash.remove(f.dir, "mod", "everything", at(1)));
  REQUIRE(fs::exists(entry / "item/data.bin"));
  REQUIRE(f.trash.list().size() == 1);
}

TEST_CASE("list is newest first and skips half-written entries") {
  TrashFixture f("trash_list");
  REQUIRE(f.trash.remove(f.make_thing("a", 1), "mod", "a", at(10)));
  REQUIRE(f.trash.remove(f.make_thing("b", 1), "mod", "b", at(30)));
  REQUIRE(f.trash.remove(f.make_thing("c", 1), "mod", "c", at(20)));

  write_file(f.paths.trash_dir() / "no-item/info.json", "{\"name\": \"x\"}");
  write_file(f.paths.trash_dir() / "no-info/item/file", "x");

  std::vector<TrashItem> items = f.trash.list();
  REQUIRE(items.size() == 3);
  REQUIRE(items[0].name == "b");
  REQUIRE(items[1].name == "c");
  REQUIRE(items[2].name == "a");
}

TEST_CASE("delete_items and empty") {
  TrashFixture f("trash_delete");
  REQUIRE(f.trash.remove(f.make_thing("a", 1), "mod", "a", at(0)));
  REQUIRE(f.trash.remove(f.make_thing("b", 1), "mod", "b", at(1)));
  std::vector<TrashItem> items = f.trash.list();

  std::vector<std::string> one;
  one.push_back(items[0].id);
  REQUIRE(f.trash.delete_items(one));
  REQUIRE(f.trash.list().size() == 1);
  REQUIRE(f.trash.list()[0].name == "a");

  // Ids that are not entries in the trash are refused, and nothing outside it
  // is touched.
  fs::path outside = f.make_thing("outside", 1);
  std::vector<std::string> bad;
  bad.push_back("");
  bad.push_back("..");
  bad.push_back("../outside");
  bad.push_back("no-such-id");
  REQUIRE(!f.trash.delete_items(bad));
  REQUIRE(fs::exists(outside));
  REQUIRE(fs::exists(f.paths.trash_dir()));
  REQUIRE(f.trash.list().size() == 1);

  write_file(f.paths.trash_dir() / "no-info/item/file", "x");  // Junk goes too.
  REQUIRE(f.trash.empty());
  REQUIRE(Paths::list_dir(f.paths.trash_dir()).empty());
  REQUIRE(fs::exists(outside));
}

TEST_CASE("over_limit picks the oldest and keeps the newest") {
  TrashFixture f("trash_limit");
  const size_t MB = 1024 * 1024;
  f.settings.set_int("trash_max_size_mb", 3);

  REQUIRE(f.trash.remove(f.make_thing("old", 2 * MB), "mod", "old", at(0)));
  REQUIRE(f.trash.remove(f.make_thing("mid", 1 * MB), "mod", "mid", at(1)));
  REQUIRE(f.trash.over_limit().empty());  // 3 MB is not over 3 MB.

  REQUIRE(f.trash.remove(f.make_thing("new", 1 * MB), "mod", "new", at(2)));
  std::vector<TrashItem> extra = f.trash.over_limit();
  REQUIRE(extra.size() == 1);
  REQUIRE(extra[0].name == "old");

  // A single item bigger than the limit is never offered for deletion.
  f.settings.set_int("trash_max_size_mb", 0);
  extra = f.trash.over_limit();
  REQUIRE(extra.size() == 2);
  REQUIRE(extra[0].name == "old");
  REQUIRE(extra[1].name == "mid");

  // over_limit only answers; nothing was deleted.
  REQUIRE(f.trash.list().size() == 3);
}

// The newest item in the trash.
static TrashItem newest(TrashFixture& f) {
  std::vector<TrashItem> items = f.trash.list();
  REQUIRE(!items.empty());
  return items[0];
}

TEST_CASE("restore puts things back, or next to a new copy") {
  TrashFixture f("trash_restore_plain");
  fs::path sound = f.dir / "dda/userdata/sound";
  fs::path pack = f.make_thing("dda/userdata/sound/CC-Sounds", 3);

  REQUIRE(f.trash.remove(pack, "soundpack", "CC-Sounds", at(0)));
  RestorePlan plan = f.trash.plan_restore(newest(f).id);
  REQUIRE(plan.kind == RestorePlan::ORIGINAL);
  REQUIRE(plan.dest == pack);
  REQUIRE(f.trash.restore(newest(f).id, false));
  REQUIRE(fs::exists(pack / "data.bin"));
  REQUIRE(f.trash.list().empty());
  REQUIRE(
      Paths::list_dir(f.paths.trash_dir()).empty());  // The entry is gone too.

  // Reinstalled in the meantime: the old one comes back next to it.
  REQUIRE(f.trash.remove(pack, "soundpack", "CC-Sounds", at(1)));
  write_file(pack / "new.txt", "new");
  plan = f.trash.plan_restore(newest(f).id);
  REQUIRE(plan.kind == RestorePlan::RENAMED);
  REQUIRE(plan.dest == restored(pack, " (restored)"));
  REQUIRE(f.trash.restore(newest(f).id, false));
  REQUIRE(read_file(pack / "new.txt") == "new");
  REQUIRE(fs::exists(restored(pack, " (restored)") / "data.bin"));

  // And again: the next free name.
  REQUIRE(f.trash.remove(f.make_thing("other/CC-Sounds", 1), "soundpack",
                         "CC-Sounds", at(2)));
  TrashItem item = newest(f);
  fs::path info_file = f.paths.trash_dir() / item.id / "info.json";
  nlohmann::json info;
  REQUIRE(load_json_file(info_file, info, f.status));
  info["original_path"] =
      to_utf8(pack);  // As if it had come from the same place.
  REQUIRE(save_json_file(info, info_file, f.status));
  REQUIRE(f.trash.plan_restore(item.id).dest ==
          restored(pack, " (restored 2)"));
  REQUIRE(f.trash.restore(item.id, false));
  REQUIRE(fs::exists(sound / "CC-Sounds (restored 2)/data.bin"));
}

TEST_CASE("restoring a mod whose ID is taken needs replace") {
  TrashFixture f("trash_restore_mod");
  fs::path mods = f.dir / "dda/userdata/mods";
  write_file(mods / "ModA/modinfo.json",
             "{\"type\": \"MOD_INFO\", \"id\": \"a\", \"name\": \"Old A\"}");
  REQUIRE(f.trash.remove(mods / "ModA", "mod", "Old A", at(0)));
  std::string old_id = newest(f).id;

  // Another folder with the same mod ID.
  write_file(mods / "ModA-v2/modinfo.json",
             "{\"type\": \"MOD_INFO\", \"id\": \"a\", \"name\": \"New A\"}");
  RestorePlan plan = f.trash.plan_restore(old_id);
  REQUIRE(plan.kind == RestorePlan::CONFLICT);
  REQUIRE(plan.conflict_path == mods / "ModA-v2");
  REQUIRE(plan.conflict_name == "New A");

  REQUIRE(!f.trash.restore(old_id, false));  // Nothing changes without replace.
  REQUIRE(fs::exists(mods / "ModA-v2"));
  REQUIRE(f.trash.list().size() == 1);

  REQUIRE(f.trash.restore(old_id, true));
  REQUIRE(fs::exists(mods / "ModA/modinfo.json"));
  REQUIRE(!fs::exists(mods / "ModA-v2"));
  std::vector<TrashItem> items = f.trash.list();
  REQUIRE(items.size() == 1);  // The one that was replaced.
  REQUIRE(items[0].name == "New A");
  REQUIRE(items[0].kind == "mod");

  // Another mod in a folder with the same name is not a conflict: it gets
  // renamed.
  REQUIRE(f.trash.remove(mods / "ModA", "mod", "Old A", at(5)));
  write_file(mods / "ModA/modinfo.json",
             "{\"type\": \"MOD_INFO\", \"id\": \"b\", \"name\": \"B\"}");
  // Not newest(f): "New A" went to the trash with the real clock, so which one
  // is newer depends on today's date.
  std::string again_id;
  for (const TrashItem& item : f.trash.list())
    if (item.name == "Old A") again_id = item.id;
  REQUIRE(!again_id.empty());
  plan = f.trash.plan_restore(again_id);
  REQUIRE(plan.kind == RestorePlan::RENAMED);
  REQUIRE(plan.dest == mods / "ModA (restored)");
}

TEST_CASE("a renamed restore can get a name the user picked") {
  TrashFixture f("trash_restore_named");
  fs::path sound = f.dir / "dda/userdata/sound";
  fs::path pack = f.make_thing("dda/userdata/sound/CC-Sounds", 3);
  REQUIRE(f.trash.remove(pack, "soundpack", "CC-Sounds", at(0)));
  std::string id = newest(f).id;
  write_file(pack / "new.txt", "new");  // Reinstalled: the old place is taken.
  write_file(sound / "Other/x", "x");

  REQUIRE(f.trash.check_restore_name(id, "") != "");
  REQUIRE(f.trash.check_restore_name(id, "..") != "");
  REQUIRE(f.trash.check_restore_name(id, "a/b") != "");
  REQUIRE(f.trash.check_restore_name(id, "CC-Sounds") != "");  // Taken.
  REQUIRE(f.trash.check_restore_name(id, "Other") != "");
  REQUIRE(!f.trash.restore(id, false, "Other"));
  REQUIRE(read_file(sound / "Other/x") == "x");
  REQUIRE(f.trash.list().size() == 1);

  REQUIRE(f.trash.check_restore_name(id, "CC-Sounds old") == "");
  REQUIRE(f.trash.restore(id, false, "CC-Sounds old"));
  REQUIRE(fs::exists(sound / "CC-Sounds old/data.bin"));
  REQUIRE(read_file(pack / "new.txt") == "new");
  REQUIRE(!fs::exists(restored(pack, " (restored)")));
  REQUIRE(f.trash.list().empty());

  // Backups follow the backup name rules.
  fs::path backup = f.make_thing("dda/userdata/evac/backups/Manual", 1);
  REQUIRE(f.trash.remove(backup, "backup", "Manual", at(1)));
  write_file(backup / "x.zip", "x");
  REQUIRE(f.trash.check_restore_name(newest(f).id, "a:b") != "");
  REQUIRE(f.trash.check_restore_name(newest(f).id, " Manual 2") != "");
  REQUIRE(f.trash.check_restore_name(newest(f).id, "Manual 2") == "");

  // Saves and installs keep their automatic names.
  REQUIRE(!Trash::can_choose_name("saves"));
  REQUIRE(!Trash::can_choose_name("install"));
  REQUIRE(Trash::can_choose_name("mod"));
}

TEST_CASE("restoring an install") {
  TrashFixture f("trash_restore_install");
  fs::path base = f.dir / "dda";
  write_file(base / "game0" / INFO_FILENAME, "{\"name\": \"Build 1\"}");
  REQUIRE(f.trash.remove(base / "game0", "install", "Build 1", at(0)));
  std::string id = newest(f).id;
  REQUIRE(f.trash.plan_restore(id).kind == RestorePlan::ORIGINAL);

  // Updated in the meantime: game0 holds a newer build, so it goes to game1.
  write_file(base / "game0" / INFO_FILENAME, "{\"name\": \"Build 2\"}");
  RestorePlan plan = f.trash.plan_restore(id);
  REQUIRE(plan.kind == RestorePlan::RENAMED);
  REQUIRE(plan.dest == base / "game1");
  REQUIRE(f.trash.restore(id, false));
  Paths::InstallsSummary installs = f.paths.installs_summary();
  REQUIRE(installs["dda"]["Build 1"] == base / "game1");
  REQUIRE(installs["dda"]["Build 2"] == base / "game0");

  // The same build installed again: replace or nothing.
  REQUIRE(f.trash.remove(base / "game1", "install", "Build 1", at(1)));
  id = newest(f).id;
  write_file(base / "game2" / INFO_FILENAME, "{\"name\": \"Build 1\"}");
  write_file(base / "game2/marker", "reinstalled");
  plan = f.trash.plan_restore(id);
  REQUIRE(plan.kind == RestorePlan::CONFLICT);
  REQUIRE(plan.conflict_path == base / "game2");
  REQUIRE(!f.trash.restore(id, false));

  REQUIRE(f.trash.restore(id, true));
  REQUIRE(fs::exists(base / "game1" / INFO_FILENAME));
  REQUIRE(!fs::exists(base / "game2"));
  REQUIRE(read_file(f.paths.trash_dir() / newest(f).id / "item/marker") ==
          "reinstalled");
}

TEST_CASE("restoring saves merges the worlds") {
  TrashFixture f("trash_restore_saves");
  fs::path saves = f.paths.savegames();
  write_file(saves / "Alpha/map.dat", "alpha old");
  write_file(saves / "Beta/map.dat", "beta old");
  REQUIRE(f.trash.remove(saves, "saves", "Saves before restoring x", at(0)));
  std::string id = newest(f).id;

  // No saves now: the whole directory goes back.
  REQUIRE(f.trash.plan_restore(id).kind == RestorePlan::ORIGINAL);

  // Played on: a new Alpha and a new Gamma.
  write_file(saves / "Alpha/map.dat", "alpha new");
  write_file(saves / "Gamma/map.dat", "gamma");
  RestorePlan plan = f.trash.plan_restore(id);
  REQUIRE(plan.kind == RestorePlan::RENAMED);
  REQUIRE(plan.dest == saves);

  REQUIRE(f.trash.restore(id, false));
  REQUIRE(read_file(saves / "Alpha/map.dat") == "alpha new");
  REQUIRE(read_file(saves / "Alpha (restored)/map.dat") == "alpha old");
  REQUIRE(read_file(saves / "Beta/map.dat") == "beta old");
  REQUIRE(read_file(saves / "Gamma/map.dat") == "gamma");
  REQUIRE(f.trash.list().empty());
}

TEST_CASE("what can't be restored stays in the trash") {
  TrashFixture f("trash_restore_impossible");
  REQUIRE(f.trash.plan_restore("no-such-id").kind == RestorePlan::IMPOSSIBLE);
  REQUIRE(!f.trash.restore("no-such-id", true));
  REQUIRE(f.trash.plan_restore("../x").kind == RestorePlan::IMPOSSIBLE);

  // The folder it was in is gone, e.g. the install it lived in was deleted.
  fs::path pack = f.make_thing("dda/game0/data/sound/Pack", 1);
  REQUIRE(f.trash.remove(pack, "soundpack", "Pack", at(0)));
  fs::remove_all(f.dir / "dda/game0");
  std::string id = newest(f).id;
  REQUIRE(f.trash.plan_restore(id).kind == RestorePlan::IMPOSSIBLE);
  REQUIRE(!f.trash.restore(id, true));
  REQUIRE(f.trash.list().size() == 1);
  REQUIRE(!fs::exists(f.dir / "dda/game0"));  // Not created again.

  // An info.json that points into the trash.
  fs::path info_file = f.paths.trash_dir() / id / "info.json";
  nlohmann::json info;
  REQUIRE(load_json_file(info_file, info, f.status));
  info["original_path"] = f.paths.trash_dir() / id / "item/x";
  REQUIRE(save_json_file(info, info_file, f.status));
  REQUIRE(f.trash.plan_restore(id).kind == RestorePlan::IMPOSSIBLE);
  info["original_path"] = "relative/path";
  REQUIRE(save_json_file(info, info_file, f.status));
  REQUIRE(f.trash.plan_restore(id).kind == RestorePlan::IMPOSSIBLE);
}

#ifndef _WIN32  // Windows has no permission bits for directories.
TEST_CASE("a failed restore keeps the item") {
  if (geteuid() == 0) SKIP("root can write anywhere");
  TrashFixture f("trash_restore_fail");
  fs::path sound = f.dir / "dda/userdata/sound";
  fs::path pack = f.make_thing("dda/userdata/sound/Pack", 1);
  REQUIRE(f.trash.remove(pack, "soundpack", "Pack", at(0)));
  std::string id = newest(f).id;

  fs::permissions(sound, fs::perms::owner_read | fs::perms::owner_exec);
  bool restored = f.trash.restore(id, false);
  fs::permissions(sound, fs::perms::owner_all);
  REQUIRE(!restored);
  REQUIRE(f.trash.list().size() == 1);
  REQUIRE(fs::exists(f.paths.trash_dir() / id / "item/data.bin"));
  REQUIRE(f.trash.restore(id, false));  // Works once it can.
}
#endif
