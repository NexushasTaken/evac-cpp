#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/fs.h"
#include "core/git.h"
#include "core/paths.h"
#include "core/settings.h"
#include "core/snapshot_manager.h"

namespace fs = std::filesystem;

static fs::path make_temp_dir(const std::string& name)
{
    fs::path p = fs::temp_directory_path() / ("evac_test_" + name);
    fs::remove_all(p);
    fs::create_directories(p);
    return p;
}

static void write_file(const fs::path& path, const std::string& content)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path) << content;
}

static std::string read_file(const fs::path& path)
{
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct SnapshotFixture
{
    SnapshotFixture(const std::string& name)
        : dir(make_temp_dir(name)), settings(dir / "s.json", status), paths(dir, settings, status),
          trash(settings, paths, status), snapshots(paths, status, trash)
    {
        settings.load();
    }

    fs::path world(const std::string& name) { return paths.savegames() / to_path(name); }

    WorldInfo find(const std::string& name)
    {
        std::vector<WorldInfo> worlds = snapshots.list_worlds();
        for (size_t i = 0; i < worlds.size(); ++i)
        {
            if (worlds[i].name == name)
                return worlds[i];
        }
        FAIL("world " << name << " not listed");
        return WorldInfo();
    }

    fs::path dir;
    Status status;
    Settings settings;
    Paths paths;
    Trash trash;
    SnapshotManager snapshots;
};

TEST_CASE("git version parsing and shell quoting")
{
    int major = 0;
    int minor = 0;
    REQUIRE(git::parse_version("git version 2.43.0\n", major, minor));
    REQUIRE((major == 2 && minor == 43));
    REQUIRE(git::parse_version("git version 2.39.3 (Apple Git-145)", major, minor));
    REQUIRE(!git::parse_version("hello", major, minor));

    REQUIRE(git::shell_quote("--git-dir=/a/b.git") == "--git-dir=/a/b.git");
    REQUIRE(git::shell_quote("--work-tree=/a/My World") == "'--work-tree=/a/My World'");
    REQUIRE(git::shell_quote("it's") == "'it'\\''s'");
}

TEST_CASE("default snapshot name")
{
    std::tm t = {};
    t.tm_year = 126;  // 2026
    t.tm_mon = 9;
    t.tm_mday = 19;
    t.tm_wday = 1;
    t.tm_hour = 21;
    t.tm_min = 4;
    REQUIRE(SnapshotManager::default_snapshot_name(t) == "Mon Oct 19 21:04 2026");
}

TEST_CASE("git runs without the user's git environment")
{
    REQUIRE(git::available());
#ifdef _WIN32
    _putenv_s("GIT_DIR", "/nonexistent");
    git::Result r = git::run({"config", "--get", "user.name"});
    _putenv_s("GIT_DIR", "");  // An empty value removes it.
#else
    setenv("GIT_DIR", "/nonexistent", 1);
    git::Result r = git::run({"config", "--get", "user.name"});
    unsetenv("GIT_DIR");
#endif
    REQUIRE(r.ok());
    REQUIRE(r.out == "evac\n");
}

TEST_CASE("take snapshots")
{
    SnapshotFixture f("snap_take");
    write_file(f.world("Alpha") / "master.gsav", "v1");
    write_file(f.world("Alpha") / "maps/0.0.0/1.map", "map v1");
    write_file(f.world("Beta") / "master.gsav", "beta");

    REQUIRE(f.find("Alpha").state == WorldInfo::NO_SNAPSHOTS);
    REQUIRE(f.snapshots.list_snapshots("Alpha").empty());

    REQUIRE(f.snapshots.take_snapshot("Alpha", "first night", "a long\ndescription") == SnapshotManager::DONE);
    REQUIRE(fs::is_directory(f.paths.snapshots() / "Alpha.git"));
    REQUIRE(!fs::exists(f.world("Alpha") / ".git"));  // Nothing in the game's folders.
    REQUIRE(f.find("Alpha").state == WorldInfo::UNCHANGED);
    REQUIRE(f.find("Beta").state == WorldInfo::NO_SNAPSHOTS);

    // Nothing changed: no empty commit.
    REQUIRE(f.snapshots.take_snapshot("Alpha", "again", "") == SnapshotManager::NOTHING_TO_DO);

    write_file(f.world("Alpha") / "master.gsav", "v2");
    REQUIRE(f.find("Alpha").state == WorldInfo::CHANGED);
    REQUIRE(f.snapshots.worlds_with_changes() == std::vector<std::string>{"Alpha"});  // Not Beta: no snapshots.
    REQUIRE(f.snapshots.take_snapshot("Alpha", "#2 second", "") == SnapshotManager::DONE);
    REQUIRE(f.snapshots.worlds_with_changes().empty());

    std::vector<Snapshot> list = f.snapshots.list_snapshots("Alpha");
    REQUIRE(list.size() == 2);
    REQUIRE(list[0].name == "#2 second");  // Newest first, '#' kept.
    REQUIRE(list[0].description.empty());
    REQUIRE(list[1].name == "first night");
    REQUIRE(list[1].description == "a long\ndescription");
    REQUIRE(list[0].hash.size() == 40);
    REQUIRE(list[0].time > 0);
    REQUIRE(f.snapshots.changed_files("Alpha", list[0].hash) == 1);
    REQUIRE(f.snapshots.changed_files("Alpha", list[1].hash) == 2);
    REQUIRE(f.snapshots.history_size("Alpha") > 0);

    REQUIRE(f.snapshots.take_snapshot("Alpha", "   ", "") == SnapshotManager::FAILED);
    REQUIRE(f.snapshots.take_snapshot("Nope", "x", "") == SnapshotManager::FAILED);
}

TEST_CASE("take a snapshot of all worlds")
{
    SnapshotFixture f("snap_all");
    write_file(f.world("Alpha") / "master.gsav", "a");
    write_file(f.world("Beta") / "master.gsav", "b");
    write_file(f.world("Gamma") / "master.gsav", "c");
    REQUIRE(f.snapshots.take_snapshot("Gamma", "earlier", "") == SnapshotManager::DONE);

    SnapshotManager::AllResult r = f.snapshots.take_snapshot_all("together", "all of them");
    REQUIRE(r.taken == 2);
    REQUIRE(r.unchanged == 1);
    REQUIRE(r.failed == 0);
    REQUIRE(f.snapshots.list_snapshots("Beta")[0].name == "together");
    REQUIRE(f.snapshots.list_snapshots("Beta")[0].description == "all of them");
    REQUIRE(f.snapshots.list_snapshots("Gamma").size() == 1);
}

TEST_CASE("restore a snapshot")
{
    SnapshotFixture f("snap_restore");
    fs::path w = f.world("Alpha");
    write_file(w / "master.gsav", "v1");
    write_file(w / "doomed.sav", "still here");
    REQUIRE(f.snapshots.take_snapshot("Alpha", "first night", "") == SnapshotManager::DONE);
    std::string first = f.snapshots.list_snapshots("Alpha")[0].hash;

    // Play on: change a file, the game deletes one, adds another.
    write_file(w / "master.gsav", "v2");
    fs::remove(w / "doomed.sav");
    write_file(w / "maps/new.map", "new");
    REQUIRE(f.snapshots.take_snapshot("Alpha", "second", "") == SnapshotManager::DONE);
    write_file(w / "master.gsav", "v3 unsaved");

    REQUIRE(f.snapshots.restore("Alpha", first) == SnapshotManager::DONE);
    REQUIRE(read_file(w / "master.gsav") == "v1");
    REQUIRE(read_file(w / "doomed.sav") == "still here");  // Deleted after X: back.
    REQUIRE(!fs::exists(w / "maps/new.map"));               // Added after X: gone.
    REQUIRE(f.find("Alpha").state == WorldInfo::UNCHANGED);

    // History is never rewritten: the unsaved state is in "Before restoring".
    std::vector<Snapshot> list = f.snapshots.list_snapshots("Alpha");
    REQUIRE(list.size() == 4);
    REQUIRE(list[0].name == "Restored \"first night\"");
    REQUIRE(list[1].name == "Before restoring \"first night\"");
    REQUIRE(list[2].name == "second");
    REQUIRE(f.snapshots.restore("Alpha", list[1].hash) == SnapshotManager::DONE);
    REQUIRE(read_file(w / "master.gsav") == "v3 unsaved");

    // The world already matches: no commits.
    std::size_t count = f.snapshots.list_snapshots("Alpha").size();
    REQUIRE(f.snapshots.restore("Alpha", list[1].hash) == SnapshotManager::NOTHING_TO_DO);
    REQUIRE(f.snapshots.list_snapshots("Alpha").size() == count);

    REQUIRE(f.snapshots.restore("Alpha", "0123456789abcdef") == SnapshotManager::FAILED);
    REQUIRE(f.snapshots.restore("Beta", first) == SnapshotManager::FAILED);
}

TEST_CASE("discard changes")
{
    SnapshotFixture f("snap_discard");
    fs::path w = f.world("Alpha");
    write_file(w / "master.gsav", "v1");
    write_file(w / "keep.sav", "keep");
    REQUIRE(f.snapshots.discard_changes("Alpha") == SnapshotManager::FAILED);  // No snapshot yet.
    REQUIRE(f.snapshots.take_snapshot("Alpha", "first", "") == SnapshotManager::DONE);
    REQUIRE(f.snapshots.discard_changes("Alpha") == SnapshotManager::NOTHING_TO_DO);

    write_file(w / "master.gsav", "v2");
    fs::remove(w / "keep.sav");
    write_file(w / "extra/new.sav", "new");
    REQUIRE(f.snapshots.discard_changes("Alpha") == SnapshotManager::DONE);
    REQUIRE(read_file(w / "master.gsav") == "v1");
    REQUIRE(read_file(w / "keep.sav") == "keep");
    REQUIRE(!fs::exists(w / "extra/new.sav"));
    REQUIRE(f.snapshots.list_snapshots("Alpha").size() == 1);  // No commits.
    REQUIRE(f.find("Alpha").state == WorldInfo::UNCHANGED);
}

TEST_CASE("world names with spaces and quotes")
{
    SnapshotFixture f("snap_names");
    std::string name = "My \"Big\" World's End";
    write_file(f.world(name) / "master.gsav", "v1");
    REQUIRE(f.snapshots.take_snapshot(name, "start", "") == SnapshotManager::DONE);
    std::string first = f.snapshots.list_snapshots(name)[0].hash;
    write_file(f.world(name) / "master.gsav", "v2");
    REQUIRE(f.snapshots.restore(name, first) == SnapshotManager::DONE);
    REQUIRE(read_file(f.world(name) / "master.gsav") == "v1");
    REQUIRE(f.snapshots.git_command(name).find("'--work-tree=") != std::string::npos);
}

TEST_CASE("worlds that no longer exist")
{
    SnapshotFixture f("snap_missing");
    write_file(f.world("Gone") / "master.gsav", "v1");
    write_file(f.world("Gone") / "maps/a.map", "map");
    REQUIRE(f.snapshots.take_snapshot("Gone", "only", "") == SnapshotManager::DONE);
    std::string only = f.snapshots.list_snapshots("Gone")[0].hash;
    fs::remove_all(f.world("Gone"));

    WorldInfo info = f.find("Gone");
    REQUIRE(info.state == WorldInfo::DELETED);
    REQUIRE(!info.kept);
    REQUIRE(f.snapshots.list_snapshots("Gone").size() == 1);  // Still viewable.
    REQUIRE(f.snapshots.take_snapshot("Gone", "x", "") == SnapshotManager::FAILED);

    REQUIRE(f.snapshots.mark_kept("Gone"));
    REQUIRE(f.find("Gone").kept);

    // Restore brings the folder back, without a "Before restoring" commit.
    REQUIRE(f.snapshots.restore("Gone", only) == SnapshotManager::DONE);
    REQUIRE(read_file(f.world("Gone") / "maps/a.map") == "map");
    REQUIRE(f.find("Gone").state == WorldInfo::UNCHANGED);
    REQUIRE(f.snapshots.list_snapshots("Gone").size() == 1);

    // Delete snapshots: the repository goes to the trash.
    fs::remove_all(f.world("Gone"));
    REQUIRE(f.snapshots.delete_snapshots("Gone"));
    REQUIRE(!fs::exists(f.snapshots.repo_path("Gone")));
    std::vector<TrashItem> trashed = f.trash.list();
    REQUIRE(trashed.size() == 1);
    REQUIRE(trashed[0].kind == "snapshots");
    REQUIRE(trashed[0].name == "Gone");
    REQUIRE(f.snapshots.list_worlds().empty());
}

TEST_CASE("export snapshots to a bundle")
{
    SnapshotFixture f("snap_export");
    fs::path bundle = f.dir / "out/Alpha.bundle";
    fs::create_directories(f.dir / "out");
    write_file(f.world("Alpha") / "master.gsav", "v1");
    REQUIRE(!f.snapshots.export_snapshots("Alpha", bundle));  // No snapshots yet.
    REQUIRE(!fs::exists(bundle));

    REQUIRE(f.snapshots.take_snapshot("Alpha", "one", "") == SnapshotManager::DONE);
    write_file(f.world("Alpha") / "master.gsav", "v2");
    REQUIRE(f.snapshots.take_snapshot("Alpha", "two", "") == SnapshotManager::DONE);
    REQUIRE(f.snapshots.export_snapshots("Alpha", bundle));
    REQUIRE(git::run({"bundle", "verify", "-q", to_utf8(bundle)}).ok());

    // The bundle holds the whole history.
    fs::path clone = f.dir / "clone.git";
    REQUIRE(git::run({"clone", "-q", "--bare", to_utf8(bundle), to_utf8(clone)}).ok());
    git::Result count = git::run({"--git-dir=" + to_utf8(clone), "rev-list", "--count", "HEAD"});
    REQUIRE(count.ok());
    REQUIRE(count.out == "2\n");

    // Exporting again replaces the file.
    write_file(f.world("Alpha") / "master.gsav", "v3");
    REQUIRE(f.snapshots.take_snapshot("Alpha", "three", "") == SnapshotManager::DONE);
    REQUIRE(f.snapshots.export_snapshots("Alpha", bundle));
    fs::remove_all(clone);
    REQUIRE(git::run({"clone", "-q", "--bare", to_utf8(bundle), to_utf8(clone)}).ok());
    REQUIRE(git::run({"--git-dir=" + to_utf8(clone), "rev-list", "--count", "HEAD"}).out == "3\n");

    // A world that no longer exists can still be exported.
    fs::remove_all(f.world("Alpha"));
    REQUIRE(f.snapshots.export_snapshots("Alpha", f.dir / "out/gone.bundle"));
}

TEST_CASE("import snapshots from a bundle")
{
    SnapshotFixture f("snap_import");
    fs::path bundle = f.dir / "Alpha.bundle";
    write_file(f.world("Alpha") / "master.gsav", "v1");
    write_file(f.world("Alpha") / "maps/a.map", "map");
    REQUIRE(f.snapshots.take_snapshot("Alpha", "one", "first") == SnapshotManager::DONE);
    write_file(f.world("Alpha") / "master.gsav", "v2");
    fs::remove(f.world("Alpha") / "maps/a.map");
    REQUIRE(f.snapshots.take_snapshot("Alpha", "two", "") == SnapshotManager::DONE);
    write_file(f.world("Alpha") / "master.gsav", "not in a snapshot");
    REQUIRE(f.snapshots.export_snapshots("Alpha", bundle));

    REQUIRE(SnapshotManager::bundle_world_name("/x/My World.bundle") == "My World");
    REQUIRE(SnapshotManager::bundle_world_name("/x/plain") == "plain");

    // The name is taken: never replaced.
    REQUIRE(f.snapshots.check_import_name("Alpha") != "");
    REQUIRE(f.snapshots.import_snapshots(bundle, "Alpha") == SnapshotManager::FAILED);
    REQUIRE(read_file(f.world("Alpha") / "master.gsav") == "not in a snapshot");
    REQUIRE(f.snapshots.list_snapshots("Alpha").size() == 2);

    // The game's rules for world names.
    REQUIRE(f.snapshots.check_import_name("") != "");
    REQUIRE(f.snapshots.check_import_name("save") != "");
    REQUIRE(f.snapshots.check_import_name("a/b") != "");
    REQUIRE(f.snapshots.check_import_name("a:b") != "");
    REQUIRE(f.snapshots.check_import_name("..") != "");
    REQUIRE(f.snapshots.check_import_name("tab\there") != "");
    REQUIRE(f.snapshots.check_import_name("Copy of 'Alpha' \xc3\xa9") == "");

    // Under a new name: the history, and the world as in the latest snapshot.
    REQUIRE(f.snapshots.import_snapshots(bundle, "Alpha 2") == SnapshotManager::DONE);
    REQUIRE(read_file(f.world("Alpha 2") / "master.gsav") == "v2");
    REQUIRE(!fs::exists(f.world("Alpha 2") / "maps/a.map"));
    std::vector<Snapshot> list = f.snapshots.list_snapshots("Alpha 2");
    REQUIRE(list.size() == 2);
    REQUIRE(list[0].name == "two");
    REQUIRE(list[1].description == "first");
    REQUIRE(f.find("Alpha 2").state == WorldInfo::UNCHANGED);
    REQUIRE(git::run({"--git-dir=" + to_utf8(f.snapshots.repo_path("Alpha 2")), "remote"}).out.empty());

    // The imported world works like any other.
    REQUIRE(f.snapshots.restore("Alpha 2", list[1].hash) == SnapshotManager::DONE);
    REQUIRE(read_file(f.world("Alpha 2") / "maps/a.map") == "map");

    // A file that is not a bundle leaves nothing behind.
    write_file(f.dir / "junk.bundle", "junk");
    REQUIRE(f.snapshots.import_snapshots(f.dir / "junk.bundle", "Junk") == SnapshotManager::FAILED);
    REQUIRE(!fs::exists(f.world("Junk")));
    REQUIRE(!fs::exists(f.snapshots.repo_path("Junk")));
    REQUIRE(f.snapshots.import_snapshots(f.dir / "missing.bundle", "Junk") == SnapshotManager::FAILED);
    REQUIRE(!fs::exists(f.snapshots.repo_path("Junk")));
}

TEST_CASE("start fresh")
{
    SnapshotFixture f("snap_fresh");
    f.settings.set_bool("use_trash", true);
    write_file(f.world("Alpha") / "master.gsav", "v1");
    REQUIRE(f.snapshots.take_snapshot("Alpha", "one", "") == SnapshotManager::DONE);
    write_file(f.world("Alpha") / "master.gsav", "v2");
    REQUIRE(f.snapshots.take_snapshot("Alpha", "two", "") == SnapshotManager::DONE);
    write_file(f.world("Alpha") / "master.gsav", "v3");  // Not in a snapshot yet.

    REQUIRE(f.snapshots.start_fresh("Alpha", "fresh", "after the lab") == SnapshotManager::DONE);
    std::vector<Snapshot> list = f.snapshots.list_snapshots("Alpha");
    REQUIRE(list.size() == 1);
    REQUIRE(list[0].name == "fresh");
    REQUIRE(list[0].description == "after the lab");
    REQUIRE(read_file(f.world("Alpha") / "master.gsav") == "v3");  // The world is not touched.
    REQUIRE(f.find("Alpha").state == WorldInfo::UNCHANGED);

    // The old history is in the trash. Restoring it replaces the new one,
    // which goes to the trash in turn: never renamed out of sight.
    std::vector<TrashItem> trashed = f.trash.list();
    REQUIRE(trashed.size() == 1);
    REQUIRE(trashed[0].kind == "snapshots");
    REQUIRE(f.trash.plan_restore(trashed[0].id).kind == RestorePlan::CONFLICT);
    REQUIRE(!f.trash.restore(trashed[0].id, false));
    REQUIRE(f.trash.restore(trashed[0].id, true));
    REQUIRE(f.snapshots.list_snapshots("Alpha").size() == 2);
    REQUIRE(f.trash.list().size() == 1);
    REQUIRE(fs::directory_iterator(f.paths.snapshots()) != fs::directory_iterator());
    for (const fs::directory_entry& e : fs::directory_iterator(f.paths.snapshots()))
        REQUIRE(e.path().filename() == "Alpha.git");

    // Nothing to start fresh from.
    REQUIRE(f.snapshots.start_fresh("Nope", "x", "") == SnapshotManager::FAILED);
    write_file(f.world("Beta") / "master.gsav", "b");
    REQUIRE(f.snapshots.start_fresh("Beta", "x", "") == SnapshotManager::FAILED);
    REQUIRE(f.snapshots.start_fresh("Alpha", " ", "") == SnapshotManager::FAILED);
    REQUIRE(f.snapshots.list_snapshots("Alpha").size() == 2);
}
