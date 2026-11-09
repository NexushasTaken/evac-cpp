#include "core/release_installer.h"

#include <array>
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <system_error>
#include <vector>

#include "core/archive.h"
#include "core/fs.h"
#include "core/helpers.h"
#include "core/paths.h"
#include "core/release_data.h"
#include "core/settings.h"
#include "core/status.h"
#include "core/trash.h"

namespace fs = std::filesystem;

ReleaseInstaller::ReleaseInstaller(Settings& settings, Paths& paths,
                                   Status& status, Trash& trash)
    : m_settings(settings),
      m_paths(paths),
      m_status(status),
      m_trash(trash),
      m_http(status) {}

// The release name in the install's info file, or the directory name if there
// is none.
static std::string install_name(const fs::path& dir, Status& status) {
  nlohmann::json info;
  const fs::path info_file = dir / INFO_FILENAME;
  if (fs::exists(info_file) && load_json_file(info_file, info, status) &&
      info.is_object() && info.contains("name") && info["name"].is_string()) {
    return info["name"].get<std::string>();
  }
  return to_utf8(dir.filename());
}

void ReleaseInstaller::cancel() { m_http.cancel(); }

bool ReleaseInstaller::install_release(const Release& release,
                                       const fs::path& update_in) {
  if (!update_in.empty()) {
    m_status.post("Updating to " + release.name + "...");
  } else {
    m_status.post("Installing " + release.name + "...");
  }

  const fs::path cache_dir = m_paths.cache_dir();
  const fs::path archive_path = cache_dir / to_path(release.filename);
  std::error_code ec;

  if (m_settings.get_bool("ignore_cache") || !fs::exists(archive_path)) {
    m_http.apply_proxy_settings(m_settings, true);
    if (!m_http.download_file(release.url, cache_dir, release.filename)) {
      return false;
    }
  }

  // Always extract into an empty directory, so that the one directory inside it
  // is ours.
  const fs::path tmp_dir = m_paths.tmp_dir();
  fs::remove_all(tmp_dir, ec);

  const bool extracted =
      archive_helper::extract(archive_path, tmp_dir, m_status);
  if (!m_settings.get_bool("keep_cache")) {
    fs::remove(archive_path, ec);
  }
  if (!extracted) {
    fs::remove_all(tmp_dir, ec);
    return false;
  }

  std::vector<std::string> extracted_items = Paths::list_dir(tmp_dir);
  if (extracted_items.empty()) {
    m_status.post("The archive did not contain a game directory.", MSG_ERROR);
    return false;
  }
  const fs::path extracted_root = tmp_dir / to_path(extracted_items.at(0));

  if (!create_info_file(extracted_root, release.name, m_status)) {
    return false;
  }

  fs::path target_dir;
  if (!update_in.empty()) {
    target_dir = update_in;
    if (!m_trash.remove(target_dir, "install",
                        install_name(target_dir, m_status))) {
      return false;
    }
  } else {
    target_dir = m_paths.next_install_dir();
  }

  if (!fs_helper::move_dir(extracted_root, target_dir, m_status)) {
    return false;
  }

  if (!update_in.empty()) {
    m_settings.set_string("active_install_" + m_settings.get_string("game"),
                          release.name);
    m_status.post("Update finished.");
  } else {
    m_status.post("Installation finished.");
  }
  m_settings.save();
  return true;
}

bool ReleaseInstaller::remove_release_by_name(const std::string& release_name) {
  Paths::InstallsSummary installs = m_paths.installs_summary();
  const std::string game = m_settings.get_string("game");

  const auto g = installs.find(game);
  if (g == installs.end() || !g->second.contains(release_name)) {
    m_status.post("Attempted to remove release \"" + release_name +
                      "\", but could not find it on disk.",
                  MSG_ERROR);
    return false;
  }

  m_status.post("Removing " + release_name + "...");
  if (!m_trash.remove(g->second.find(release_name)->second, "install",
                      release_name)) {
    return false;
  }
  m_status.post("Removal finished.");
  return true;
}

std::vector<fs::path> ReleaseInstaller::check_game_dir_for_userdata(
    const std::string& release_name) {
  std::vector<fs::path> result;

  Paths::InstallsSummary installs = m_paths.installs_summary();
  const auto g = installs.find(m_settings.get_string("game"));
  if (g == installs.end() || !g->second.contains(release_name)) {
    return result;
  }

  const fs::path game_path = g->second.find(release_name)->second;
  const std::array<std::string, 8> subdirs = {
      "config", "font", "graveyard", "memorial",
      "mods",   "save", "sound",     "templates",
  };
  for (const auto& subdir : subdirs) {
    const fs::path full = game_path / subdir;
    if (fs::is_directory(full) && !fs::is_empty(full)) {
      result.push_back(full);
    }
  }
  return result;
}
