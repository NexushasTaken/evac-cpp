#include "core/paths.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "core/fs.h"
#include "core/helpers.h"
#include "core/platform.h"
#include "core/settings.h"
#include "core/status.h"

namespace fs = std::filesystem;

#ifndef EVAC_DATA_DIR
#define EVAC_DATA_DIR ""
#endif

static const std::array<std::string_view, 5> GAMES = {"dda", "bn", "eod",
                                                      "tish", "tlg"};

Locations portable_locations(const fs::path& dir) {
  Locations loc;
  loc.config_dir = dir;
  loc.games_dir = dir;
  loc.cache_dir = dir / "cache";
  return loc;
}

// The spec says to ignore values that are not absolute paths.
static fs::path xdg_base(const std::string& value, const fs::path& fallback) {
  if (!value.empty() && value.at(0) == '/') {
    return to_path(value);
  }
  return fallback;
}

Locations xdg_locations(const std::string& home, const std::string& config_home,
                        const std::string& data_home,
                        const std::string& cache_home) {
  const fs::path home_dir = to_path(home);
  Locations loc;
  loc.config_dir = xdg_base(config_home, home_dir / ".config") / "evac";
  loc.games_dir = xdg_base(data_home, home_dir / ".local" / "share") / "evac";
  loc.cache_dir = xdg_base(cache_home, home_dir / ".cache") / "evac";
  return loc;
}

Locations windows_locations(const std::string& appdata,
                            const std::string& local_appdata) {
  Locations loc;
  loc.config_dir = to_path(appdata) / "evac";
  // The games are big, so they stay on this machine instead of a roaming
  // profile.
  loc.games_dir = to_path(local_appdata) / "evac";
  loc.cache_dir = loc.games_dir / "cache";
  return loc;
}

Locations macos_locations(const std::string& home) {
  const fs::path home_dir = to_path(home);
  Locations loc;
  loc.config_dir = home_dir / "Library" / "Application Support" / "evac";
  loc.games_dir = loc.config_dir;
  loc.cache_dir = home_dir / "Library" / "Caches" / "evac";
  return loc;
}

Locations default_locations() {
  const fs::path exe_dir = Paths::executable_dir();
  if (fs::exists(exe_dir / "evac_settings.json")) {
    return portable_locations(exe_dir);
  }

  const std::string_view os = platform::os_name();
  if (os == "win") {
    const std::string appdata = platform::get_env("APPDATA");
    const std::string local_appdata = platform::get_env("LOCALAPPDATA");
    if (appdata.empty() || local_appdata.empty()) {
      return portable_locations(exe_dir);
    }
    return windows_locations(appdata, local_appdata);
  }

  const std::string home = platform::get_env("HOME");
  if (home.empty()) {
    return portable_locations(exe_dir);
  }
  if (os == "mac") {
    return macos_locations(home);
  }
  return xdg_locations(home, platform::get_env("XDG_CONFIG_HOME"),
                       platform::get_env("XDG_DATA_HOME"),
                       platform::get_env("XDG_CACHE_HOME"));
}

Paths::Paths(const Locations& locations, Settings& settings, Status& status)
    : m_games_dir(locations.games_dir),
      m_cache_dir(locations.cache_dir),
      m_settings(settings),
      m_status(status) {}

Paths::Paths(const fs::path& root, Settings& settings, Status& status)
    : m_games_dir(root),
      m_cache_dir(root / "cache"),
      m_settings(settings),
      m_status(status) {}

fs::path Paths::executable_dir() {
  const fs::path exe = platform::executable_path();
  if (exe.empty()) {
    return fs::current_path();
  }
  return exe.parent_path();
}

std::vector<std::string> Paths::list_dir(const fs::path& dir) {
  std::vector<std::string> result;
  std::error_code ec;
  fs::directory_iterator it(dir, ec);
  if (ec) {
    return result;
  }
  for (; it != fs::directory_iterator(); it.increment(ec)) {
    if (it->is_directory()) {
      result.push_back(to_utf8(it->path().filename()));
    }
  }
  std::ranges::sort(result);
  return result;
}

Paths::InstallsSummary Paths::installs_summary() {
  InstallsSummary result;

  for (const auto& game : GAMES) {
    const fs::path base_dir = m_games_dir / game;
    std::map<std::string, fs::path> installs;

    const std::vector<std::string> subdirs = list_dir(base_dir);
    for (const auto& subdir : subdirs) {
      const fs::path dir = base_dir / to_path(subdir);
      const fs::path info_file = dir / INFO_FILENAME;
      nlohmann::json info;
      if (fs::exists(info_file) && load_json_file(info_file, info, m_status) &&
          info.is_object() && info.contains("name") &&
          info["name"].is_string()) {
        installs[info["name"].get<std::string>()] = dir;
      }
    }
    if (!installs.empty()) {
      result[game] = installs;
    }
  }

  // Ensure that some installation of the game is set as active.
  const std::string game = m_settings.get_string("game");
  const auto it = result.find(game);
  if (it != result.end()) {
    const std::string active = m_settings.get_string("active_install_" + game);
    if (active.empty() || !it->second.contains(active)) {
      m_settings.set_string("active_install_" + game,
                            it->second.begin()->first);
    }
  }

  return result;
}

fs::path Paths::game_base_dir() {
  return m_games_dir / to_path(m_settings.get_string("game"));
}

fs::path Paths::cache_dir() const { return m_cache_dir; }

fs::path Paths::data_dir() {
  fs::path local = executable_dir() / "data";
  if (fs::is_directory(local)) {
    return local;
  }
  return to_path(EVAC_DATA_DIR);
}

fs::path Paths::game_dir() {
  const std::string active =
      m_settings.get_string("active_install_" + m_settings.get_string("game"));
  if (active.empty()) {
    return next_install_dir();
  }
  return find_active_game_dir();
}

fs::path Paths::find_active_game_dir() {
  const std::string active =
      m_settings.get_string("active_install_" + m_settings.get_string("game"));
  const fs::path base_dir = game_base_dir();

  const std::vector<std::string> subdirs = list_dir(base_dir);
  for (const auto& subdir : subdirs) {
    fs::path dir = base_dir / to_path(subdir);
    const fs::path info_file = dir / INFO_FILENAME;
    nlohmann::json info;
    if (fs::exists(info_file) && load_json_file(info_file, info, m_status) &&
        info.is_object() && info.contains("name") && info["name"] == active) {
      return dir;
    }
  }
  return {};
}

// Names for new installs follow the pattern "game0", "game1", ...
fs::path Paths::next_install_dir() {
  const fs::path base_dir = game_base_dir();
  int n = 0;
  while (fs::exists(base_dir / ("game" + std::to_string(n)))) {
    ++n;
  }
  return base_dir / ("game" + std::to_string(n));
}

fs::path Paths::default_userdata() { return game_base_dir() / "userdata"; }

// The active one must still be in the list, a stale value means the default.
fs::path Paths::userdata() {
  fs::path active = to_path(m_settings.get_string(
      "active_userdata_" + m_settings.get_string("game")));
  if (!active.empty()) {
    const std::vector<UserdataFolder> folders = custom_userdata();
    for (const auto& folder : folders) {
      if (folder.path == active) {
        return active;
      }
    }
  }
  return default_userdata();
}

std::vector<UserdataFolder> Paths::custom_userdata() {
  std::vector<UserdataFolder> result;
  const nlohmann::json list =
      m_settings.get_json("userdata_folders_" + m_settings.get_string("game"));
  if (!list.is_array()) {
    return result;
  }
  for (const auto& item : list) {
    if (!item.is_object() || !item.contains("path") ||
        !item["path"].is_string()) {
      continue;
    }
    UserdataFolder folder;
    folder.path = to_path(item["path"].get<std::string>());
    if (item.contains("label") && item["label"].is_string()) {
      folder.label = item["label"].get<std::string>();
    }
    if (folder.label.empty()) {
      folder.label = to_utf8(folder.path.filename());
    }
    result.push_back(folder);
  }
  return result;
}

void Paths::store_custom_userdata(const std::vector<UserdataFolder>& folders) {
  nlohmann::json list = nlohmann::json::array();
  for (const auto& folder : folders) {
    list.push_back({{"path", to_utf8(folder.path)}, {"label", folder.label}});
  }
  m_settings.set_json("userdata_folders_" + m_settings.get_string("game"),
                      list);
}

static fs::path clean_path(const fs::path& path) {
  std::error_code ec;
  fs::path p = fs::absolute(path, ec);
  if (ec) {
    p = path;
  }
  p = p.lexically_normal();
  // "dir/" has an empty file name; drop that separator ("/" and "C:\" stay).
  if (!p.has_filename() && p.has_relative_path()) {
    p = p.parent_path();
  }
  return p;
}

std::string Paths::check_userdata_label(const std::string& label,
                                        const fs::path& own_path) {
  if (label.find_first_not_of(" \t") == std::string::npos) {
    return "The name can't be empty.";
  }
  if (label == "Default") {
    return "\"Default\" is the name of the default user data folder.";
  }
  const std::vector<UserdataFolder> folders = custom_userdata();
  for (const auto& folder : folders) {
    if (folder.label == label && folder.path != own_path) {
      return "Another user data folder is already called \"" + label + "\".";
    }
  }
  return "";
}

std::string Paths::add_userdata(const fs::path& path,
                                const std::string& label) {
  if (path.empty()) {
    return "No folder given.";
  }
  fs::path clean = clean_path(path);
  if (clean == clean_path(default_userdata())) {
    return "This is the default user data folder, it is always in the list.";
  }
  std::vector<UserdataFolder> folders = custom_userdata();
  for (const auto& folder : folders) {
    if (folder.path == clean) {
      return "This folder is already in the list as \"" + folder.label + "\".";
    }
  }
  std::string problem = check_userdata_label(label);
  if (!problem.empty()) {
    return problem;
  }

  UserdataFolder folder;
  folder.path = std::move(clean);
  folder.label = label;
  folders.push_back(folder);
  store_custom_userdata(folders);
  return "";
}

std::string Paths::rename_userdata(const fs::path& path,
                                   const std::string& label) {
  std::vector<UserdataFolder> folders = custom_userdata();
  for (size_t i = 0; i < folders.size(); ++i) {
    if (folders.at(i).path != path) {
      continue;
    }
    std::string problem = check_userdata_label(label, path);
    if (!problem.empty()) {
      return problem;
    }
    folders.at(i).label = label;
    store_custom_userdata(folders);
    return "";
  }
  return "This user data folder is not in the list.";
}

void Paths::remove_userdata(const fs::path& path) {
  const std::vector<UserdataFolder> folders = custom_userdata();
  std::vector<UserdataFolder> kept;
  for (const auto& folder : folders) {
    if (folder.path != path) {
      kept.push_back(folder);
    }
  }
  store_custom_userdata(kept);
  const std::string key = "active_userdata_" + m_settings.get_string("game");
  if (to_path(m_settings.get_string(key)) == path) {
    m_settings.set_string(key, "");
  }
}

void Paths::set_active_userdata(const fs::path& path) {
  const std::string key = "active_userdata_" + m_settings.get_string("game");
  m_settings.set_string(key, path == default_userdata() ? "" : to_utf8(path));
}

bool Paths::is_default_userdata_active() {
  return userdata() == default_userdata();
}

fs::path Paths::config() { return userdata() / "config"; }
fs::path Paths::savegames() { return userdata() / "save"; }
fs::path Paths::mods_stock() { return game_dir() / "data" / "mods"; }
fs::path Paths::mods_user() { return userdata() / "mods"; }
fs::path Paths::sound_stock() { return game_dir() / "data" / "sound"; }
fs::path Paths::sound_user() { return userdata() / "sound"; }
fs::path Paths::gfx_default() { return game_dir() / "gfx"; }
fs::path Paths::gfx_user() { return userdata() / "gfx"; }
fs::path Paths::font_user() { return userdata() / "font"; }
fs::path Paths::templates() { return userdata() / "templates"; }
fs::path Paths::memorial() { return userdata() / "memorial"; }
fs::path Paths::graveyard() { return userdata() / "graveyard"; }
fs::path Paths::mod_repo() { return game_base_dir() / "mod_repo"; }
fs::path Paths::tmp_dir() { return game_base_dir() / "tmp"; }
fs::path Paths::launcher_dir() { return userdata() / "evac"; }
fs::path Paths::save_backups() { return launcher_dir() / "backups"; }
fs::path Paths::snapshots() { return launcher_dir() / "snapshots"; }
fs::path Paths::trash_dir() const { return m_games_dir / "trash"; }

bool Paths::move_old_backups() {
  const fs::path old_dir = game_base_dir() / "save_backups";
  const fs::path parent_dir = default_userdata() / "evac";
  const fs::path new_dir = parent_dir / "backups";
  std::error_code ec;
  if (!fs::is_directory(old_dir, ec) ||
      fs::exists(fs::symlink_status(new_dir, ec))) {
    return true;
  }

  m_status.post("Moving the backups from " + to_utf8(old_dir) + " to " +
                to_utf8(new_dir) + "...");
  fs::create_directories(parent_dir, ec);
  if (ec) {
    m_status.post("Cannot create " + to_utf8(parent_dir) + ": " + ec.message(),
                  MSG_ERROR);
    return false;
  }
  return fs_helper::move_dir(old_dir, new_dir, m_status);
}
