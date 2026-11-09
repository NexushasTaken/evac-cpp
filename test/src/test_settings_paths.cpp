#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <nlohmann/json_fwd.hpp>
#include <string>

#include "core/helpers.h"
#include "core/paths.h"
#include "core/settings.h"
#include "core/status.h"

namespace fs = std::filesystem;

static fs::path make_temp_dir(const std::string& name) {
  fs::path p = fs::temp_directory_path() / ("evac_test_" + name);
  fs::remove_all(p);
  fs::create_directories(p);
  return p;
}

TEST_CASE("settings creates a file with defaults") {
  const fs::path dir = make_temp_dir("settings_create");
  Status status;
  Settings s(dir / "evac_settings.json", status);
  s.load();

  REQUIRE(fs::exists(dir / "evac_settings.json"));
  REQUIRE(s.get_string("game") == "dda");
  REQUIRE(s.get_int("num_releases_to_request") == 10);
  REQUIRE(s.get_bool("keep_open_after_starting_game"));
}

TEST_CASE("settings keeps unknown keys and fills in missing ones") {
  const fs::path dir = make_temp_dir("settings_compat");
  std::ofstream(dir / "evac_settings.json")
      << "{\"game\": \"bn\", \"launcher_theme\": \"Gray.tres\", "
         "\"proxy_port\": 8080}";

  Status status;
  Settings s(dir / "evac_settings.json", status);
  s.load();

  REQUIRE(s.get_string("game") == "bn");
  REQUIRE(s.get_int("proxy_port") == 8080);
  REQUIRE(s.get_string("channel") == "stable");  // Missing in file -> default.

  s.set_string("channel", "experimental");
  REQUIRE(s.save());

  nlohmann::json saved;
  REQUIRE(load_json_file(dir / "evac_settings.json", saved, status));
  REQUIRE(saved["launcher_theme"] == "Gray.tres");
  REQUIRE(saved["channel"] == "experimental");
}

TEST_CASE("settings falls back to default on wrong type") {
  const fs::path dir = make_temp_dir("settings_type");
  std::ofstream(dir / "evac_settings.json") << R"({"debug_mode": "yes"})";

  Status status;
  Settings s(dir / "evac_settings.json", status);
  s.load();
  REQUIRE(s.get_bool("debug_mode") == false);
}

TEST_CASE("paths follow the original layout") {
  const fs::path dir = make_temp_dir("paths");
  Status status;
  Settings s(dir / "evac_settings.json", status);
  s.load();
  Paths p(dir, s, status);

  REQUIRE(p.userdata() == dir / "dda/userdata");
  REQUIRE(p.savegames() == dir / "dda/userdata/save");
  REQUIRE(p.launcher_dir() == dir / "dda/userdata/evac");
  REQUIRE(p.save_backups() == dir / "dda/userdata/evac/backups");
  REQUIRE(p.snapshots() == dir / "dda/userdata/evac/snapshots");
  REQUIRE(p.next_install_dir() == dir / "dda/game0");

  fs::create_directories(dir / "dda/game0");
  REQUIRE(p.next_install_dir() == dir / "dda/game1");
}

TEST_CASE("installs summary finds installs and sets the active one") {
  const fs::path dir = make_temp_dir("installs");
  Status status;
  Settings s(dir / "evac_settings.json", status);
  s.load();
  Paths p(dir, s, status);

  fs::create_directories(dir / "dda/game0");
  create_info_file(dir / "dda/game0", "0.H Herbert", status);
  fs::create_directories(dir / "dda/junk");  // No info file.

  Paths::InstallsSummary sum = p.installs_summary();
  REQUIRE(sum.size() == 1);
  REQUIRE(sum["dda"]["0.H Herbert"] == dir / "dda/game0");
  REQUIRE(s.get_string("active_install_dda") == "0.H Herbert");
  REQUIRE(p.game_dir() == dir / "dda/game0");
}

TEST_CASE("xdg locations") {
  Locations loc = xdg_locations("/home/u", "", "", "");
  REQUIRE(loc.config_dir == "/home/u/.config/evac");
  REQUIRE(loc.games_dir == "/home/u/.local/share/evac");
  REQUIRE(loc.cache_dir == "/home/u/.cache/evac");

  loc = xdg_locations("/home/u", "/c", "/d", "/k");
  REQUIRE(loc.config_dir == "/c/evac");
  REQUIRE(loc.games_dir == "/d/evac");
  REQUIRE(loc.cache_dir == "/k/evac");

  // Relative values must be ignored.
  loc = xdg_locations("/home/u", "rel", "also/rel", "");
  REQUIRE(loc.config_dir == "/home/u/.config/evac");
  REQUIRE(loc.games_dir == "/home/u/.local/share/evac");
}

TEST_CASE("portable locations keep everything in one directory") {
  Locations loc = portable_locations("/x");
  REQUIRE(loc.config_dir == "/x");
  REQUIRE(loc.games_dir == "/x");
  REQUIRE(loc.cache_dir == "/x/cache");
}

TEST_CASE(
    "settings are saved into a config directory that does not exist yet") {
  const fs::path dir = make_temp_dir("settings_newdir");
  Status status;
  Settings s(dir / "a/b/evac_settings.json", status);
  s.load();
  REQUIRE(fs::exists(dir / "a/b/evac_settings.json"));
}

TEST_CASE("custom userdata folders") {
  const fs::path dir = make_temp_dir("userdata_folders");
  Status status;
  Settings s(dir / "evac_settings.json", status);
  s.load();
  Paths p(dir, s, status);

  REQUIRE(p.custom_userdata().empty());
  REQUIRE(p.is_default_userdata_active());

  // Trailing slashes and ".." are cleaned up; nothing is created on disk.
  REQUIRE(p.add_userdata(dir / "x/../modded/", "Modded").empty());
  REQUIRE(!fs::exists(dir / "modded"));
  REQUIRE(p.custom_userdata().size() == 1);
  REQUIRE(p.custom_userdata()[0].path == dir / "modded");
  REQUIRE(p.userdata() == dir / "dda/userdata");

  REQUIRE(!p.add_userdata(dir / "modded", "Other").empty());  // Already there.
  REQUIRE(
      !p.add_userdata(dir / "dda/userdata", "Again").empty());  // The default.
  REQUIRE(!p.add_userdata(dir / "clean", "Modded").empty());    // Same name.
  REQUIRE(!p.add_userdata(dir / "clean", "Default").empty());
  REQUIRE(!p.add_userdata(dir / "clean", "  ").empty());
  REQUIRE(p.add_userdata(dir / "clean", "Clean").empty());

  p.set_active_userdata(dir / "modded");
  REQUIRE(p.userdata() == dir / "modded");
  REQUIRE(p.savegames() == dir / "modded/save");
  REQUIRE(p.save_backups() == dir / "modded/evac/backups");
  REQUIRE(p.mod_repo() == dir / "dda/mod_repo");  // Not part of userdata.

  // Renaming changes the label only.
  REQUIRE(!p.rename_userdata(dir / "modded", "Clean").empty());
  REQUIRE(p.rename_userdata(dir / "modded", "Modded")
              .empty());  // Its own name is fine.
  REQUIRE(p.rename_userdata(dir / "modded", "Heavy mods").empty());
  REQUIRE(p.custom_userdata()[0].label == "Heavy mods");
  REQUIRE(p.userdata() == dir / "modded");

  // Each game has its own list.
  s.set_string("game", "bn");
  REQUIRE(p.custom_userdata().empty());
  REQUIRE(p.userdata() == dir / "bn/userdata");
  s.set_string("game", "dda");
  REQUIRE(p.userdata() == dir / "modded");

  // Removing the active one forgets it and goes back to the default; files
  // stay.
  fs::create_directories(dir / "modded/save");
  p.remove_userdata(dir / "modded");
  REQUIRE(p.custom_userdata().size() == 1);
  REQUIRE(p.is_default_userdata_active());
  REQUIRE(s.get_string("active_userdata_dda").empty());
  REQUIRE(fs::exists(dir / "modded/save"));
}

TEST_CASE("a stale active userdata falls back to the default") {
  const fs::path dir = make_temp_dir("userdata_stale");
  std::ofstream(dir / "evac_settings.json")
      << R"({"active_userdata_dda": "/nowhere"})";
  Status status;
  Settings s(dir / "evac_settings.json", status);
  s.load();
  Paths p(dir, s, status);
  REQUIRE(p.userdata() == dir / "dda/userdata");
}
