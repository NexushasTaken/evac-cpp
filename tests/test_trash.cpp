#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/helpers.h"
#include "core/paths.h"
#include "core/settings.h"
#include "core/trash.h"

namespace fs = std::filesystem;

static std::string make_temp_dir(const std::string& name)
{
    fs::path p = fs::temp_directory_path() / ("catapult_test_" + name);
    fs::remove_all(p);
    fs::create_directories(p);
    return p.string();
}

static void write_file(const std::string& path, const std::string& content)
{
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream(path) << content;
}

static std::string read_file(const std::string& path)
{
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// A fixed time, plus some seconds, so tests don't depend on the clock.
static Trash::Clock::time_point at(int seconds)
{
    return Trash::Clock::from_time_t(1792332000) + std::chrono::seconds(seconds);
}

struct TrashFixture
{
    TrashFixture(const std::string& name)
        : dir(make_temp_dir(name)), settings(dir + "/s.json", status), paths(dir, settings, status),
          trash(settings, paths, status)
    {
        settings.load();
    }

    // A directory with files of the given total size.
    std::string make_thing(const std::string& rel, size_t bytes)
    {
        std::string path = dir + "/" + rel;
        write_file(path + "/data.bin", std::string(bytes, 'x'));
        return path;
    }

    std::string dir;
    Status status;
    Settings settings;
    Paths paths;
    Trash trash;
};

TEST_CASE("trash settings and directory")
{
    TrashFixture f("trash_settings");
    REQUIRE(f.settings.get_bool("use_trash"));
    REQUIRE(f.settings.get_int("trash_max_size_mb") == 5120);
    REQUIRE(!f.settings.get_bool("trash_auto_prune"));
    REQUIRE(f.paths.trash_dir() == f.dir + "/trash");

    // The trash is next to the game directories, but is not an install.
    write_file(f.dir + "/trash/x/" + INFO_FILENAME, "{\"name\": \"Not A Build\"}");
    write_file(f.dir + "/dda/game0/" + INFO_FILENAME, "{\"name\": \"Test Build\"}");
    Paths::InstallsSummary installs = f.paths.installs_summary();
    REQUIRE(installs.count("trash") == 0);
    REQUIRE(installs["dda"].size() == 1);
    REQUIRE(installs["dda"].count("Test Build") == 1);
    for (Paths::InstallsSummary::const_iterator g = installs.begin(); g != installs.end(); ++g)
        REQUIRE(g->second.count("Not A Build") == 0);
}

TEST_CASE("remove moves into the trash and list reads it back")
{
    TrashFixture f("trash_remove");
    std::string pack = f.make_thing("dda/userdata/sound/CC-Sounds", 1000);
    write_file(pack + "/sfx/boom.ogg", "12345");

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

    std::string item_dir = f.paths.trash_dir() + "/" + items[0].id + "/item";
    REQUIRE(read_file(item_dir + "/sfx/boom.ogg") == "12345");
}

TEST_CASE("make_id keeps the name safe for a directory")
{
    std::string id = Trash::make_id("backup", "a/b c:d", at(0));
    REQUIRE(id.find('/') == std::string::npos);
    REQUIRE(id.find(' ') == std::string::npos);
    REQUIRE(id.substr(id.size() - 7) == "a_b_c_d");
    REQUIRE(id.find("_backup_") != std::string::npos);

    std::string long_id = Trash::make_id("mod", std::string(100, 'n'), at(0));
    REQUIRE(long_id.size() < 80);
}

TEST_CASE("remove with use_trash off deletes for good")
{
    TrashFixture f("trash_off");
    f.settings.set_bool("use_trash", false);
    std::string thing = f.make_thing("dda/save_backups/old", 10);

    REQUIRE(f.trash.remove(thing, "backup", "old", at(0)));
    REQUIRE(!fs::exists(thing));
    REQUIRE(f.trash.list().empty());
    REQUIRE(!fs::exists(f.paths.trash_dir()));
}

TEST_CASE("removes at the same moment get their own entries")
{
    TrashFixture f("trash_same_time");
    std::string a = f.make_thing("one/Pack", 1);
    std::string b = f.make_thing("two/Pack", 2);

    REQUIRE(f.trash.remove(a, "soundpack", "Pack", at(0)));
    REQUIRE(f.trash.remove(b, "soundpack", "Pack", at(0)));
    std::vector<TrashItem> items = f.trash.list();
    REQUIRE(items.size() == 2);
    REQUIRE(items[0].id != items[1].id);
    REQUIRE(f.trash.total_size() == 3);
}

TEST_CASE("remove refuses what it can't or shouldn't move")
{
    TrashFixture f("trash_refuse");
    REQUIRE(!f.trash.remove(f.dir + "/missing", "mod", "missing", at(0)));

    std::string thing = f.make_thing("thing", 1);
    REQUIRE(f.trash.remove(thing, "mod", "thing", at(0)));
    std::string entry = f.paths.trash_dir() + "/" + f.trash.list()[0].id;

    // The trash itself, something inside it, and a directory that holds it.
    REQUIRE(!f.trash.remove(f.paths.trash_dir(), "mod", "trash", at(1)));
    REQUIRE(!f.trash.remove(entry, "mod", "entry", at(1)));
    REQUIRE(!f.trash.remove(f.dir, "mod", "everything", at(1)));
    REQUIRE(fs::exists(entry + "/item/data.bin"));
    REQUIRE(f.trash.list().size() == 1);
}

TEST_CASE("list is newest first and skips half-written entries")
{
    TrashFixture f("trash_list");
    REQUIRE(f.trash.remove(f.make_thing("a", 1), "mod", "a", at(10)));
    REQUIRE(f.trash.remove(f.make_thing("b", 1), "mod", "b", at(30)));
    REQUIRE(f.trash.remove(f.make_thing("c", 1), "mod", "c", at(20)));

    write_file(f.paths.trash_dir() + "/no-item/info.json", "{\"name\": \"x\"}");
    write_file(f.paths.trash_dir() + "/no-info/item/file", "x");

    std::vector<TrashItem> items = f.trash.list();
    REQUIRE(items.size() == 3);
    REQUIRE(items[0].name == "b");
    REQUIRE(items[1].name == "c");
    REQUIRE(items[2].name == "a");
}

TEST_CASE("delete_items and empty")
{
    TrashFixture f("trash_delete");
    REQUIRE(f.trash.remove(f.make_thing("a", 1), "mod", "a", at(0)));
    REQUIRE(f.trash.remove(f.make_thing("b", 1), "mod", "b", at(1)));
    std::vector<TrashItem> items = f.trash.list();

    std::vector<std::string> one;
    one.push_back(items[0].id);
    REQUIRE(f.trash.delete_items(one));
    REQUIRE(f.trash.list().size() == 1);
    REQUIRE(f.trash.list()[0].name == "a");

    // Ids that are not entries in the trash are refused, and nothing outside it is touched.
    std::string outside = f.make_thing("outside", 1);
    std::vector<std::string> bad;
    bad.push_back("");
    bad.push_back("..");
    bad.push_back("../outside");
    bad.push_back("no-such-id");
    REQUIRE(!f.trash.delete_items(bad));
    REQUIRE(fs::exists(outside));
    REQUIRE(fs::exists(f.paths.trash_dir()));
    REQUIRE(f.trash.list().size() == 1);

    write_file(f.paths.trash_dir() + "/no-info/item/file", "x");  // Junk goes too.
    REQUIRE(f.trash.empty());
    REQUIRE(Paths::list_dir(f.paths.trash_dir()).empty());
    REQUIRE(fs::exists(outside));
}

TEST_CASE("over_limit picks the oldest and keeps the newest")
{
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
