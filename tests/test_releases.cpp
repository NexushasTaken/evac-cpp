#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#include "core/game_launcher.h"
#include "core/helpers.h"
#include "core/paths.h"
#include "core/release_installer.h"
#include "core/release_manager.h"
#include "core/settings.h"

namespace fs = std::filesystem;

static std::string make_temp_dir(const std::string& name)
{
    fs::path p = fs::temp_directory_path() / ("evac_test_" + name);
    fs::remove_all(p);
    fs::create_directories(p);
    return p.string();
}

static void write_file(const std::string& path, const std::string& content)
{
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream(path) << content;
}

TEST_CASE("stable releases")
{
    // The tables grow with every stable release, so don't count them.
    std::vector<Release> dda = stable_releases("dda", "linux");
    REQUIRE(dda.size() >= 10);
    bool found_herbert = false;
    for (size_t i = 0; i < dda.size(); ++i)
    {
        REQUIRE(!dda[i].url.empty());
        REQUIRE(!dda[i].filename.empty());
        if (dda[i].name == "0.H Herbert")
            found_herbert = true;
    }
    REQUIRE(found_herbert);
    REQUIRE(stable_releases("bn", "win").size() >= 14);
    REQUIRE(stable_releases("eod", "linux").empty());
}

TEST_CASE("parse GitHub releases")
{
    const char* body =
        "[{\"name\": \"Experimental 2025-01-01\", \"assets\": ["
        "   {\"name\": \"cbn-linux-tiles-x64-2025.tar.gz\", \"browser_download_url\": \"https://x/a.tar.gz\"},"
        "   {\"name\": \"cbn-windows-tiles-x64-2025.zip\", \"browser_download_url\": \"https://x/a.zip\"}]},"
        " {\"name\": \"No matching asset\", \"assets\": [{\"name\": \"other\", \"browser_download_url\": \"https://x/o\"}]}]";

    std::vector<Release> result;
    std::string error;
    REQUIRE(ReleaseManager::parse_builds(body, "cbn-linux-tiles-x64", false, result, error));
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].name == "Experimental 2025-01-01");
    REQUIRE(result[0].url == "https://x/a.tar.gz");
    REQUIRE(result[0].filename == "cbn-linux-tiles-x64-2025.tar.gz");

    REQUIRE(ReleaseManager::parse_builds(body, "cbn-linux-tiles-x64", true, result, error));
    REQUIRE(result[0].name == "2025-01-01");
}

TEST_CASE("parse rate limit answer")
{
    std::vector<Release> result;
    std::string error;
    REQUIRE(!ReleaseManager::parse_builds("{\"message\": \"API rate limit exceeded\"}", "x", false, result, error));
    REQUIRE(error == "API rate limit exceeded");
    REQUIRE(!ReleaseManager::parse_builds("not json", "x", false, result, error));
}

TEST_CASE("fetch stable list")
{
    std::string dir = make_temp_dir("relmgr");
    Status status;
    Settings settings(dir + "/s.json", status);
    settings.load();
    ReleaseManager mgr(settings, status);

    REQUIRE(mgr.get("dda-stable").empty());
    REQUIRE(mgr.fetch("dda-stable"));
    REQUIRE(mgr.get("dda-stable").size() >= 10);
    REQUIRE(!mgr.fetch("garbage"));
}

TEST_CASE("install, update and remove a release from a local archive")
{
    std::string dir = make_temp_dir("install");
    Status status;
    Settings settings(dir + "/s.json", status);
    settings.load();
    Paths paths(dir, settings, status);
    Trash trash(settings, paths, status);
    ReleaseInstaller installer(settings, paths, status, trash);

    // A fake game archive with one top directory, like the real ones.
    write_file(dir + "/pack/cdda-1/cataclysm-launcher", "#!/bin/sh\n");
    write_file(dir + "/pack/cdda-1/data/mods/readme.txt", "mods");
    std::string tar = "tar -czf '" + dir + "/fake.tar.gz' -C '" + dir + "/pack' cdda-1";
    if (std::system(tar.c_str()) != 0)
        SKIP("tar is not available");

    Release release;
    release.name = "Test Build 1";
    release.url = "file://" + dir + "/fake.tar.gz";
    release.filename = "fake-1.tar.gz";

    REQUIRE(installer.install_release(release, ""));
    REQUIRE(fs::exists(dir + "/dda/game0/cataclysm-launcher"));
    REQUIRE(fs::exists(dir + "/dda/game0/" + INFO_FILENAME));
    REQUIRE(!fs::exists(dir + "/cache/fake-1.tar.gz"));  // keep_cache is off.

    Paths::InstallsSummary installs = paths.installs_summary();
    REQUIRE(installs["dda"]["Test Build 1"] == dir + "/dda/game0");
    REQUIRE(settings.get_string("active_install_dda") == "Test Build 1");

    // Userdata inside the game directory is reported before deleting.
    REQUIRE(installer.check_game_dir_for_userdata("Test Build 1").empty());
    write_file(dir + "/dda/game0/save/World/x", "x");
    REQUIRE(installer.check_game_dir_for_userdata("Test Build 1").size() == 1);

    // Update in place with a new name.
    release.name = "Test Build 2";
    release.filename = "fake-2.tar.gz";
    REQUIRE(installer.install_release(release, dir + "/dda/game0"));
    REQUIRE(settings.get_string("active_install_dda") == "Test Build 2");
    installs = paths.installs_summary();
    REQUIRE(installs["dda"].size() == 1);
    REQUIRE(installs["dda"].count("Test Build 2") == 1);
    std::vector<TrashItem> trashed = trash.list();
    REQUIRE(trashed.size() == 1);  // The old install, with the user data that was in it.
    REQUIRE(trashed[0].kind == "install");
    REQUIRE(trashed[0].name == "Test Build 1");
    REQUIRE(fs::exists(paths.trash_dir() + "/" + trashed[0].id + "/item/save/World/x"));

    // Removing.
    REQUIRE(!installer.remove_release_by_name("No Such Build"));
    REQUIRE(installer.remove_release_by_name("Test Build 2"));
    REQUIRE(!fs::exists(dir + "/dda/game0"));
    REQUIRE(trash.list().size() == 2);
    REQUIRE(trash.list()[0].name == "Test Build 2");  // Newest first.
    REQUIRE(paths.installs_summary()["dda"].empty());  // Trashed installs are not listed.
}

TEST_CASE("install fails cleanly when the download fails")
{
    std::string dir = make_temp_dir("install_fail");
    Status status;
    Settings settings(dir + "/s.json", status);
    settings.load();
    Paths paths(dir, settings, status);
    Trash trash(settings, paths, status);
    ReleaseInstaller installer(settings, paths, status, trash);

    Release release;
    release.name = "Nope";
    release.url = "file://" + dir + "/does-not-exist.tar.gz";
    release.filename = "nope.tar.gz";
    REQUIRE(!installer.install_release(release, ""));
    REQUIRE(!fs::exists(dir + "/dda/game0"));
}

TEST_CASE("game launcher starts the active install with the right arguments")
{
    std::string dir = make_temp_dir("launch");
    Status status;
    Settings settings(dir + "/s.json", status);
    settings.load();
    Paths paths(dir, settings, status);
    GameLauncher launcher(settings, paths, status);

    write_file(dir + "/dda/game0/" + INFO_FILENAME, "{\"name\": \"B\"}");
    std::string script = dir + "/dda/game0/cataclysm-launcher";
    write_file(script, "#!/bin/sh\necho \"$@\" > '" + dir + "/args.txt'\n");
    fs::permissions(script, fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write);
    paths.installs_summary();  // Sets the active install.

    REQUIRE(!launcher.has_last_world());
    write_file(paths.config() + "/lastworld.json", "{\"world_name\": \"My World\"}");
    REQUIRE(launcher.has_last_world());
    REQUIRE(launcher.last_world() == "My World");

    REQUIRE(launcher.start("My World"));
    for (int i = 0; i < 50 && !fs::exists(dir + "/args.txt"); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(fs::exists(dir + "/args.txt"));

    std::ifstream f(dir + "/args.txt");
    std::stringstream ss;
    ss << f.rdbuf();
    REQUIRE(ss.str() == "--userdir " + dir + "/dda/userdata/ --world My World\n");
}

TEST_CASE("game launcher reports a missing executable")
{
    std::string dir = make_temp_dir("launch_missing");
    Status status;
    Settings settings(dir + "/s.json", status);
    settings.load();
    Paths paths(dir, settings, status);
    GameLauncher launcher(settings, paths, status);
    REQUIRE(!launcher.start(""));
}
