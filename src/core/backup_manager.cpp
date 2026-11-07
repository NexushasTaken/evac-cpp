#include "core/backup_manager.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <format>

#include "core/archive.h"
#include "core/fs.h"

namespace fs = std::filesystem;

static bool ends_with_zip(const std::string& name) {
  if (name.size() < 4) {
    return false;
  }
  std::string end = name.substr(name.size() - 4);
  for (char& i : end) {
    i = static_cast<char>(std::tolower(i));
  }
  return end == ".zip";
}

BackupManager::BackupManager(Settings& settings, Paths& paths, Status& status,
                             Trash& trash)
    : m_settings(settings), m_paths(paths), m_status(status), m_trash(trash) {}

bool BackupManager::is_valid_backup_name(const std::string& name) {
  if (name.empty()) {
    return false;
  }
  if (name.find_first_of(":/\\?*\"|%<>") != std::string::npos) {
    return false;
  }
  // No leading or trailing whitespace.
  return (isspace(name.at(0)) == 0) && (isspace(name.at(name.size() - 1)) == 0);
}

std::string BackupManager::default_backup_name(const std::tm& time) {
  auto result = std::format("Manual_{:02}-{:02}-{:02}_{:02}-{:02}",
                            (time.tm_year + 1900) % 100, time.tm_mon + 1,
                            time.tm_mday, time.tm_hour, time.tm_min);
  return result;
}

std::vector<BackupSummary> BackupManager::list() {
  std::vector<BackupSummary> result;
  m_paths.move_old_backups();

  std::vector<std::string> names = Paths::list_dir(m_paths.save_backups());
  for (const auto& name : names) {
    BackupSummary summary;
    summary.name = name;
    summary.path = m_paths.save_backups() / to_path(name);

    // One .zip per world.
    std::error_code ec;
    for (fs::directory_iterator it(summary.path, ec);
         !ec && it != fs::directory_iterator(); it.increment(ec)) {
      std::string file = to_utf8(it->path().filename());
      if (ends_with_zip(file)) {
        summary.worlds.push_back(file.substr(0, file.size() - 4));
      }
    }
    std::ranges::sort(summary.worlds);
    result.push_back(summary);
  }
  return result;
}

bool BackupManager::backup_current(const std::string& backup_name) {
  if (!is_valid_backup_name(backup_name)) {
    m_status.post("\"" + backup_name + "\" is not a valid backup name.",
                  MSG_ERROR);
    return false;
  }

  m_paths.move_old_backups();
  fs::path dest_dir = m_paths.save_backups() / to_path(backup_name);
  if (fs::exists(dest_dir)) {
    m_status.post("A backup named \"" + backup_name + "\" already exists.",
                  MSG_ERROR);
    return false;
  }

  fs::path saves = m_paths.savegames();
  std::vector<std::string> worlds = Paths::list_dir(saves);
  if (worlds.empty()) {
    m_status.post("There are no saved games to back up.", MSG_WARN);
    return false;
  }

  m_status.post("Backing up current saves to \"" + backup_name + "\"...");
  std::error_code ec;
  fs::create_directories(dest_dir, ec);
  if (ec) {
    m_status.post("Cannot create " + to_utf8(dest_dir) + ": " + ec.message(),
                  MSG_ERROR);
    return false;
  }

  for (const auto& world : worlds) {
    if (!archive_helper::zip(saves, world, dest_dir / to_path(world + ".zip"),
                             m_status)) {
      // Don't leave a backup that is missing worlds.
      fs::remove_all(dest_dir, ec);
      return false;
    }
  }

  m_status.post("Backup created.");
  return true;
}

bool BackupManager::restore(const std::string& backup_name) {
  fs::path source_dir = m_paths.save_backups() / to_path(backup_name);
  if (!is_valid_backup_name(backup_name) || !fs::is_directory(source_dir)) {
    m_status.post("Backup \"" + backup_name + "\" not found.", MSG_ERROR);
    return false;
  }

  std::vector<fs::path> zips;
  std::error_code ec;
  for (fs::directory_iterator it(source_dir, ec);
       !ec && it != fs::directory_iterator(); it.increment(ec)) {
    if (ends_with_zip(to_utf8(it->path().filename()))) {
      zips.push_back(it->path());
    }
  }
  if (zips.empty()) {
    m_status.post("Backup \"" + backup_name + "\" has no worlds in it.",
                  MSG_ERROR);
    return false;
  }

  m_status.post("Restoring backup \"" + backup_name + "\"...");

  // Unpack next to the saves first, and only replace them when that worked.
  fs::path stage = m_paths.tmp_dir() / "restore";
  fs::remove_all(stage, ec);
  fs::create_directories(stage, ec);
  for (const auto& zip : zips) {
    if (!archive_helper::extract(zip, stage, m_status)) {
      fs::remove_all(stage, ec);
      m_status.post("The current saves were not changed.", MSG_WARN);
      return false;
    }
  }

  fs::path saves = m_paths.savegames();
  if (fs::exists(saves) &&
      !m_trash.remove(saves, "saves",
                      "Saves before restoring " + backup_name)) {
    fs::remove_all(stage, ec);
    return false;
  }
  fs::create_directories(saves.parent_path(), ec);
  bool moved = fs_helper::move_dir(stage, saves, m_status);
  fs::remove_all(stage, ec);
  if (!moved) {
    return false;
  }

  m_status.post("Backup restored.");
  return true;
}

bool BackupManager::delete_backup(const std::string& backup_name) {
  fs::path target = m_paths.save_backups() / to_path(backup_name);
  if (!is_valid_backup_name(backup_name) || !fs::is_directory(target)) {
    m_status.post("Backup \"" + backup_name + "\" not found.", MSG_ERROR);
    return false;
  }

  m_status.post("Deleting backup \"" + backup_name + "\"...");
  if (!m_trash.remove(target, "backup", backup_name)) {
    return false;
  }
  m_status.post("Backup deleted.");
  return true;
}
