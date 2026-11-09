#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "core/status.h"
#include "core/trash.h"

#ifndef _WIN32
#include <unistd.h>
#endif

#include "core/archive.h"
#include "core/fs.h"
#include "core/helpers.h"
#include "core/paths.h"
#include "core/settings.h"
#include "core/soundpack_manager.h"
#include "test_server.h"

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
  const std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

struct SoundFixture {
  explicit SoundFixture(const std::string& name)
      : dir(make_temp_dir(name)),
        settings(dir / "s.json", status),
        paths(dir, settings, status),
        trash(settings, paths, status),
        sounds(settings, paths, status, trash) {
    settings.load();
  }

  // Makes a zip like the real soundpack archives: a directory with a
  // soundpack.txt in it.
  std::tuple<fs::path, std::string> make_pack_zip(const std::string& dir_in_zip,
                                                  const std::string& marker) {
    write_file(dir / "zipsrc" / dir_in_zip / "soundpack.txt",
               "VIEW: Test Pack\nDESCRIPTION: " + marker + "\n");
    write_file(dir / "zipsrc" / dir_in_zip / "sfx/boom.ogg", marker);
    const std::string path = (marker + ".zip");
    const fs::path zip = dir / path;
    REQUIRE(archive_helper::zip(dir / "zipsrc", dir_in_zip, zip, status));
    fs::remove_all(dir / "zipsrc");
    return std::make_tuple(zip, path);
  }

  fs::path dir;
  Status status;
  Settings settings;
  Paths paths;
  Trash trash;
  SoundpackManager sounds;
};

TEST_CASE("soundpack table") {
  const std::vector<SoundpackSource>& packs = available_soundpacks();
  REQUIRE(packs.size() == 12);
  REQUIRE(packs[0].name == "CC-Sounds");
  for (const auto& pack : packs) {
    REQUIRE(!pack.url.empty());
    REQUIRE(!pack.filename.empty());
    REQUIRE(!pack.internal_path.empty());
  }
}

TEST_CASE("parse_sound_dir reads soundpack.txt") {
  const fs::path dir = make_temp_dir("sound_parse");
  write_file(dir / "a/soundpack.txt",
             "VIEW: Pack A\r\nDESCRIPTION: Loud and clear\r\nMODS: x\r\n");
  write_file(dir / "b/soundpack.txt", "DESCRIPTION: No name here\n");
  write_file(dir / "c/readme.txt", "not a soundpack");
  write_file(dir / "d/soundpack.txt.disabled", "VIEW: Pack D\n");

  std::vector<InstalledSoundpack> packs =
      SoundpackManager::parse_sound_dir(dir);
  REQUIRE(packs.size() == 3);
  REQUIRE(packs[0].name == "Pack A");
  REQUIRE(packs[0].description == "Loud and clear");
  REQUIRE(packs[0].location == dir / "a");
  REQUIRE(packs[0].enabled);
  REQUIRE(packs[1].name == "b");  // Falls back to the directory name.
  REQUIRE(packs[2].name == "Pack D");
  REQUIRE(!packs[2].enabled);

  REQUIRE(SoundpackManager::parse_sound_dir(dir / "missing").empty());

#ifndef _WIN32  // Windows has no permission bits for directories.
  // A directory that can't be searched is skipped, not an exception.
  write_file(dir / "e/soundpack.txt", "VIEW: Locked\n");
  fs::permissions(dir / "e", fs::perms::owner_read);
  std::vector<InstalledSoundpack> with_locked;
  CHECK_NOTHROW(with_locked = SoundpackManager::parse_sound_dir(
                    dir));  // CHECK, so the cleanup below still runs.
  fs::permissions(dir / "e", fs::perms::owner_all);
  if (geteuid() != 0) {  // root can read it anyway.
    REQUIRE(with_locked.size() == 3);
  }
#endif
}

TEST_CASE("get_installed marks stock packs") {
  SoundFixture f("sound_installed");
  // A registered install, so that the stock directory is inside it.
  write_file(f.dir / "dda/game0" / INFO_FILENAME, R"({"name": "Test Build"})");
  f.paths.installs_summary();
  write_file(f.paths.sound_user() / "mine/soundpack.txt", "VIEW: Mine\n");
  write_file(f.paths.sound_stock() / "stock/soundpack.txt", "VIEW: Stock\n");

  std::vector<InstalledSoundpack> user_only = f.sounds.get_installed(false);
  REQUIRE(user_only.size() == 1);
  REQUIRE(!user_only[0].is_stock);

  std::vector<InstalledSoundpack> all = f.sounds.get_installed(true);
  REQUIRE(all.size() == 2);
  REQUIRE(all[1].is_stock);
  REQUIRE(all[1].name == "Stock");
}

TEST_CASE("disable and enable a soundpack") {
  SoundFixture f("sound_toggle");
  write_file(f.dir / "dda/game0" / INFO_FILENAME, R"({"name": "Test Build"})");
  f.paths.installs_summary();
  const fs::path mine = f.paths.sound_user() / "mine";
  write_file(mine / "soundpack.txt", "VIEW: Mine\n");
  write_file(f.paths.sound_stock() / "stock/soundpack.txt", "VIEW: Stock\n");

  REQUIRE(f.sounds.set_enabled(mine, false));
  REQUIRE(!fs::exists(mine / "soundpack.txt"));
  REQUIRE(fs::exists(mine / "soundpack.txt.disabled"));
  std::vector<InstalledSoundpack> installed = f.sounds.get_installed(false);
  REQUIRE(installed.size() == 1);
  REQUIRE(!installed[0].enabled);

  REQUIRE(
      f.sounds.set_enabled(mine, false));  // Already disabled: nothing to do.
  REQUIRE(fs::exists(mine / "soundpack.txt.disabled"));

  REQUIRE(f.sounds.set_enabled(mine, true));
  REQUIRE(fs::exists(mine / "soundpack.txt"));
  REQUIRE(!fs::exists(mine / "soundpack.txt.disabled"));
  REQUIRE(f.sounds.get_installed(false)[0].enabled);

  REQUIRE(!f.sounds.set_enabled(f.paths.sound_user() / "missing", false));
  REQUIRE(!f.sounds.set_enabled(f.paths.sound_stock() / "stock",
                                false));  // Stock packs are left alone.
  REQUIRE(fs::exists(f.paths.sound_stock() / "stock/soundpack.txt"));

  // Never overwrite a file that is already there.
  write_file(mine / "soundpack.txt.disabled", "VIEW: Old copy\n");
  REQUIRE(!f.sounds.set_enabled(mine, false));
  REQUIRE(fs::exists(mine / "soundpack.txt"));
  REQUIRE(read_file(mine / "soundpack.txt.disabled") == "VIEW: Old copy\n");
  fs::remove(mine / "soundpack.txt.disabled");

  // A dangling soundpack.txt symlink is not replaced on enable either.
  REQUIRE(f.sounds.set_enabled(mine, false));
  fs::create_symlink(f.dir / "nowhere", mine / "soundpack.txt");
  REQUIRE(!f.sounds.set_enabled(mine, true));
  REQUIRE(fs::is_symlink(mine / "soundpack.txt"));
  REQUIRE(fs::exists(mine / "soundpack.txt.disabled"));
  fs::remove(mine / "soundpack.txt");
  REQUIRE(f.sounds.set_enabled(mine, true));

  // A disabled pack can still be deleted.
  REQUIRE(f.sounds.set_enabled(mine, false));
  REQUIRE(f.sounds.delete_pack(mine));
  REQUIRE(!fs::exists(mine));
}

TEST_CASE("delete picks the pack by location, not by name") {
  SoundFixture f("sound_same_name");
  write_file(f.dir / "dda/game0" / INFO_FILENAME, R"({"name": "Test Build"})");
  f.paths.installs_summary();
  const fs::path pack = f.paths.sound_user() / "CC-Sounds";
  fs::path copy = f.paths.sound_user() / "CC-Sounds-old";
  write_file(pack / "soundpack.txt", "VIEW: CC-Sounds\n");
  write_file(copy / "soundpack.txt", "VIEW: CC-Sounds\n");
  write_file(f.paths.sound_stock() / "stock/soundpack.txt", "VIEW: Stock\n");

  // The second one in the list, which used to delete the first.
  REQUIRE(f.sounds.delete_pack(copy));
  REQUIRE(fs::exists(pack));
  REQUIRE(!fs::exists(copy));
  std::vector<TrashItem> trashed = f.trash.list();
  REQUIRE(trashed.size() == 1);
  REQUIRE(trashed[0].kind == "soundpack");
  REQUIRE(trashed[0].original_path == copy);

  // Only paths of the user's packs are accepted.
  REQUIRE(!f.sounds.delete_pack(f.paths.sound_stock() / "stock"));
  REQUIRE(!f.sounds.delete_pack(f.paths.sound_user()));
  REQUIRE(!f.sounds.delete_pack(
      pack / ""));  // Must match exactly (no trailing separator).
  REQUIRE(fs::exists(f.paths.sound_stock() / "stock"));
  REQUIRE(fs::exists(pack));
}

TEST_CASE("install from a local file, reinstall and delete") {
  SoundFixture f("sound_install");
  SoundpackSource pack;
  pack.name = "Test Pack";
  pack.internal_path = "Inner-1";

  auto [zip1, _] = f.make_pack_zip("Inner-1", "first");
  REQUIRE(f.sounds.install_pack(pack, zip1, false, true));
  REQUIRE(read_file(f.paths.sound_user() / "Test Pack/sfx/boom.ogg") ==
          "first");
  REQUIRE(fs::exists(zip1));  // keep_archive: the user's file stays.
  REQUIRE(!fs::exists(f.paths.tmp_dir() / "Test Pack"));

  // Reinstall replaces the old contents.
  auto [zip2, _] = f.make_pack_zip("Inner-1", "second");
  REQUIRE(f.sounds.install_pack(pack, zip2, true, false));
  REQUIRE(read_file(f.paths.sound_user() / "Test Pack/sfx/boom.ogg") ==
          "second");
  REQUIRE(!fs::exists(zip2));           // Not kept.
  REQUIRE(f.trash.list().size() == 1);  // The first copy.
  REQUIRE(read_file(f.paths.trash_dir() / f.trash.list()[0].id /
                    "item/sfx/boom.ogg") == "first");

  std::vector<InstalledSoundpack> installed = f.sounds.get_installed(false);
  REQUIRE(installed.size() == 1);
  REQUIRE(installed[0].name == "Test Pack");

  REQUIRE(!f.sounds.delete_pack(f.paths.sound_user() / "No Such Pack"));
  REQUIRE(f.sounds.delete_pack(installed[0].location));
  REQUIRE(!fs::exists(f.paths.sound_user() / "Test Pack"));
  REQUIRE(f.trash.list().size() == 2);
}

TEST_CASE("a stock pack with the same name is not a user copy") {
  SoundFixture f("sound_stock_same_name");
  write_file(f.dir / "dda/game0" / INFO_FILENAME, R"({"name": "Test Build"})");
  f.paths.installs_summary();
  write_file(f.paths.sound_stock() / "CC-Sounds/soundpack.txt",
             "VIEW: CC-Sounds\n");
  SoundpackSource pack;
  pack.name = "CC-Sounds";
  pack.internal_path = "Inner-1";

  // Only the stock one: installing is not a reinstall, and nothing goes to the
  // trash.
  REQUIRE(!f.sounds.has_user_copy(pack));
  auto [zip1, _] = f.make_pack_zip("Inner-1", "first");
  REQUIRE(f.sounds.install_pack(pack, zip1, false, true));
  REQUIRE(f.sounds.has_user_copy(pack));
  REQUIRE(f.trash.list().empty());
  REQUIRE(fs::exists(f.paths.sound_stock() / "CC-Sounds/soundpack.txt"));

  // Installing over the user's copy without reinstall is refused, not merged.
  write_file(f.paths.sound_user() / "CC-Sounds/mine.txt", "keep");
  auto [zip2, _] = f.make_pack_zip("Inner-1", "second");
  REQUIRE(!f.sounds.install_pack(pack, zip2, false, true));
  REQUIRE(read_file(f.paths.sound_user() / "CC-Sounds/sfx/boom.ogg") ==
          "first");
  REQUIRE(read_file(f.paths.sound_user() / "CC-Sounds/mine.txt") == "keep");

  // A reinstall replaces it, and the old copy is in the trash.
  REQUIRE(f.sounds.install_pack(pack, zip2, true, true));
  REQUIRE(read_file(f.paths.sound_user() / "CC-Sounds/sfx/boom.ogg") ==
          "second");
  REQUIRE(f.trash.list().size() == 1);
  REQUIRE(read_file(f.paths.trash_dir() / f.trash.list()[0].id /
                    "item/mine.txt") == "keep");
}

TEST_CASE("install downloads the archive and uses the cache") {
  SoundFixture f("sound_download");
  auto [zip, path] = f.make_pack_zip("Inner-2", "downloaded");
  INFO(path);

  TestServer server(f.dir);
  server.start();

  SoundpackSource pack;
  pack.name = "Test Pack";
  pack.url = server.url("/" + path);
  pack.filename = "test-pack.zip";
  pack.internal_path = "Inner-2";

  REQUIRE(f.sounds.install_pack(pack, "", false, false));
  REQUIRE(read_file(f.paths.sound_user() / "Test Pack/sfx/boom.ogg") ==
          "downloaded");
  REQUIRE(!fs::exists(f.paths.cache_dir() /
                      "test-pack.zip"));  // keep_cache is off.

  f.settings.set_bool("keep_cache", true);
  REQUIRE(f.sounds.install_pack(pack, "", true, false));
  REQUIRE(fs::exists(f.paths.cache_dir() / "test-pack.zip"));
}

TEST_CASE("failed installs leave the old copy alone") {
  SoundFixture f("sound_fail");
  SoundpackSource pack;
  pack.name = "Test Pack";
  pack.url = "file://" + to_utf8(f.dir / "nope.zip");
  pack.filename = "nope.zip";
  pack.internal_path = "Inner-1";

  auto [zip, _] = f.make_pack_zip("Inner-1", "good");
  REQUIRE(f.sounds.install_pack(pack, zip, false, true));

  // Download fails.
  REQUIRE(!f.sounds.install_pack(pack, "", true, false));
  // The archive is not a zip.
  write_file(f.dir / "bad.zip", "garbage");
  REQUIRE(!f.sounds.install_pack(pack, f.dir / "bad.zip", true, true));
  // The archive does not have the expected directory.
  auto [wrong, _] = f.make_pack_zip("Other-Dir", "wrong");
  REQUIRE(!f.sounds.install_pack(pack, wrong, true, true));

  REQUIRE(read_file(f.paths.sound_user() / "Test Pack/sfx/boom.ogg") == "good");
  REQUIRE(f.trash.list().empty());  // Nothing was replaced.
}
