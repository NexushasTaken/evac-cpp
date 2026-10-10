#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "core/helpers.h"
#include "core/paths.h"
#include "core/settings.h"

namespace fs = std::filesystem;

static std::string make_temp_dir(const std::string& name)
{
    fs::path p = fs::temp_directory_path() / ("catapult_test_" + name);
    fs::remove_all(p);
    fs::create_directories(p);
    return p.string();
}

TEST_CASE("settings creates a file with defaults")
{
    std::string dir = make_temp_dir("settings_create");
    Status status;
    Settings s(dir + "/catapult_settings.json", status);
    s.load();

    REQUIRE(fs::exists(dir + "/catapult_settings.json"));
    REQUIRE(s.get_string("game") == "dda");
    REQUIRE(s.get_int("num_releases_to_request") == 10);
    REQUIRE(s.get_bool("keep_open_after_starting_game"));
}

TEST_CASE("settings keeps unknown keys and fills in missing ones")
{
    std::string dir = make_temp_dir("settings_compat");
    std::ofstream(dir + "/catapult_settings.json") << "{\"game\": \"bn\", \"launcher_theme\": \"Gray.tres\", \"proxy_port\": 8080}";

    Status status;
    Settings s(dir + "/catapult_settings.json", status);
    s.load();

    REQUIRE(s.get_string("game") == "bn");
    REQUIRE(s.get_int("proxy_port") == 8080);
    REQUIRE(s.get_string("channel") == "stable");  // Missing in file -> default.

    s.set_string("channel", "experimental");
    REQUIRE(s.save());

    nlohmann::json saved;
    REQUIRE(load_json_file(dir + "/catapult_settings.json", saved, status));
    REQUIRE(saved["launcher_theme"] == "Gray.tres");
    REQUIRE(saved["channel"] == "experimental");
}

TEST_CASE("settings falls back to default on wrong type")
{
    std::string dir = make_temp_dir("settings_type");
    std::ofstream(dir + "/catapult_settings.json") << "{\"debug_mode\": \"yes\"}";

    Status status;
    Settings s(dir + "/catapult_settings.json", status);
    s.load();
    REQUIRE(s.get_bool("debug_mode") == false);
}

TEST_CASE("paths follow the original layout")
{
    std::string dir = make_temp_dir("paths");
    Status status;
    Settings s(dir + "/catapult_settings.json", status);
    s.load();
    Paths p(dir, s, status);

    REQUIRE(p.userdata() == dir + "/dda/userdata");
    REQUIRE(p.savegames() == dir + "/dda/userdata/save");
    REQUIRE(p.save_backups() == dir + "/dda/save_backups");
    REQUIRE(p.next_install_dir() == dir + "/dda/game0");

    fs::create_directories(dir + "/dda/game0");
    REQUIRE(p.next_install_dir() == dir + "/dda/game1");
}

TEST_CASE("installs summary finds installs and sets the active one")
{
    std::string dir = make_temp_dir("installs");
    Status status;
    Settings s(dir + "/catapult_settings.json", status);
    s.load();
    Paths p(dir, s, status);

    fs::create_directories(dir + "/dda/game0");
    create_info_file(dir + "/dda/game0", "0.H Herbert", status);
    fs::create_directories(dir + "/dda/junk");  // No info file.

    Paths::InstallsSummary sum = p.installs_summary();
    REQUIRE(sum.size() == 1);
    REQUIRE(sum["dda"]["0.H Herbert"] == dir + "/dda/game0");
    REQUIRE(s.get_string("active_install_dda") == "0.H Herbert");
    REQUIRE(p.game_dir() == dir + "/dda/game0");
}

TEST_CASE("xdg locations")
{
    Locations loc = xdg_locations("/home/u", "", "", "");
    REQUIRE(loc.config_dir == "/home/u/.config/catapult");
    REQUIRE(loc.games_dir == "/home/u/.local/share/catapult");
    REQUIRE(loc.cache_dir == "/home/u/.cache/catapult");

    loc = xdg_locations("/home/u", "/c", "/d", "/k");
    REQUIRE(loc.config_dir == "/c/catapult");
    REQUIRE(loc.games_dir == "/d/catapult");
    REQUIRE(loc.cache_dir == "/k/catapult");

    // Relative values must be ignored.
    loc = xdg_locations("/home/u", "rel", "also/rel", "");
    REQUIRE(loc.config_dir == "/home/u/.config/catapult");
    REQUIRE(loc.games_dir == "/home/u/.local/share/catapult");
}

TEST_CASE("portable locations keep everything in one directory")
{
    Locations loc = portable_locations("/x");
    REQUIRE(loc.config_dir == "/x");
    REQUIRE(loc.games_dir == "/x");
    REQUIRE(loc.cache_dir == "/x/cache");
}

TEST_CASE("settings are saved into a config directory that does not exist yet")
{
    std::string dir = make_temp_dir("settings_newdir");
    Status status;
    Settings s(dir + "/a/b/catapult_settings.json", status);
    s.load();
    REQUIRE(fs::exists(dir + "/a/b/catapult_settings.json"));
}
