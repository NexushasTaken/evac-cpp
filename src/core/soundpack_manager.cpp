#include "core/soundpack_manager.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "core/archive.h"
#include "core/fs.h"
#include "core/paths.h"
#include "core/settings.h"
#include "core/status.h"
#include "core/trash.h"

namespace fs = std::filesystem;

static const std::string_view NAME_PREFIX = "VIEW: ";
static const std::string_view DESC_PREFIX = "DESCRIPTION: ";
static const std::string_view CONFIG_FILE = "soundpack.txt";
static const std::string_view DISABLED_SUFFIX = ".disabled";

static bool starts_with(const std::string& text,
                        const std::string_view& prefix) {
  return text.compare(0, std::string(prefix).size(), prefix) == 0;
}

SoundpackManager::SoundpackManager(Settings& settings, Paths& paths,
                                   Status& status, Trash& trash)
    : m_settings(settings),
      m_paths(paths),
      m_status(status),
      m_trash(trash),
      m_http(status) {}

void SoundpackManager::cancel() { m_http.cancel(); }

std::vector<InstalledSoundpack> SoundpackManager::parse_sound_dir(
    const fs::path& sound_dir) {
  std::vector<InstalledSoundpack> result;

  const std::vector<std::string> subdirs = Paths::list_dir(sound_dir);
  for (const auto& subdir : subdirs) {
    const fs::path location = sound_dir / to_path(subdir);
    fs::path config = location / CONFIG_FILE;
    bool enabled = true;
    std::error_code ec;
    if (!fs::exists(config, ec)) {
      if (ec) {
        continue;  // Can't look inside, so skip it like an unreadable file.
      }
      config += DISABLED_SUFFIX;
      enabled = false;
    }
    std::ifstream f(config);
    if (!f) {
      continue;
    }

    InstalledSoundpack pack;
    pack.location = location;
    pack.enabled = enabled;
    std::string line;
    while (std::getline(f, line)) {
      if (!line.empty() && line.at(line.size() - 1) == '\r') {
        line.erase(line.size() - 1);
      }
      if (starts_with(line, NAME_PREFIX)) {
        pack.name = line.substr(NAME_PREFIX.size());
      } else if (starts_with(line, DESC_PREFIX)) {
        pack.description = line.substr(DESC_PREFIX.size());
      }
    }
    if (pack.name.empty()) {
      pack.name = subdir;  // No VIEW line: better than an empty name.
    }
    result.push_back(pack);
  }
  return result;
}

std::vector<InstalledSoundpack> SoundpackManager::get_installed(
    bool include_stock) {
  std::vector<InstalledSoundpack> packs = parse_sound_dir(m_paths.sound_user());

  if (include_stock) {
    std::vector<InstalledSoundpack> stock =
        parse_sound_dir(m_paths.sound_stock());
    for (auto& i : stock) {
      i.is_stock = true;
      packs.push_back(i);
    }
  }
  return packs;
}

bool SoundpackManager::find_user_pack(const fs::path& location,
                                      InstalledSoundpack& out) {
  const std::vector<InstalledSoundpack> packs = get_installed(false);
  for (const auto& pack : packs) {
    if (pack.location == location) {
      out = pack;
      return true;
    }
  }
  m_status.post("Could not find a soundpack in " + to_utf8(location),
                MSG_ERROR);
  return false;
}

bool SoundpackManager::delete_pack(const fs::path& location) {
  InstalledSoundpack pack;
  if (!find_user_pack(location, pack)) {
    return false;
  }
  return m_trash.remove(pack.location, "soundpack", pack.name);
}

bool SoundpackManager::set_enabled(const fs::path& location, bool enabled) {
  InstalledSoundpack pack;
  if (!find_user_pack(location, pack)) {
    return false;
  }
  if (pack.enabled == enabled) {
    return true;
  }

  const fs::path config = pack.location / CONFIG_FILE;
  fs::path disabled = config;
  disabled += DISABLED_SUFFIX;
  const fs::path from = enabled ? disabled : config;
  const fs::path to = enabled ? config : disabled;

  // rename() replaces an existing file, so check first. symlink_status also
  // sees a dangling symlink, which exists() would not.
  std::error_code ec;
  const fs::file_status to_status = fs::symlink_status(to, ec);
  if (fs::exists(to_status)) {
    m_status.post("Not renaming the config of soundpack \"" + pack.name +
                      "\": " + to_utf8(to) + " already exists.",
                  MSG_ERROR);
    return false;
  }
  ec.clear();
  fs::rename(from, to, ec);
  if (ec) {
    m_status.post("Could not rename the config of soundpack \"" + pack.name +
                      "\": " + ec.message(),
                  MSG_ERROR);
    return false;
  }

  m_status.post(std::string(enabled ? "Enabled" : "Disabled") +
                " soundpack \"" + pack.name + "\".");
  return true;
}

bool SoundpackManager::has_user_copy(const SoundpackSource& pack) {
  std::error_code ec;
  return fs::exists(
      fs::symlink_status(m_paths.sound_user() / to_path(pack.name), ec));
}

bool SoundpackManager::install_pack(const SoundpackSource& pack,
                                    const fs::path& from_file, bool reinstall,
                                    bool keep_archive) {
  const fs::path sound_dir = m_paths.sound_user();
  const fs::path tmp_dir = m_paths.tmp_dir() / to_path(pack.name);
  fs::path archive_path;
  std::error_code ec;

  if (!reinstall && has_user_copy(pack)) {
    m_status.post("Soundpack \"" + pack.name + "\" is already installed in " +
                      to_utf8(sound_dir / to_path(pack.name)) +
                      ". Reinstall it instead.",
                  MSG_ERROR);
    return false;
  }

  if (reinstall) {
    m_status.post("Reinstalling soundpack \"" + pack.name + "\"...");
  } else {
    m_status.post("Installing soundpack \"" + pack.name + "\"...");
  }

  if (!from_file.empty()) {
    archive_path = from_file;
  } else {
    archive_path = m_paths.cache_dir() / to_path(pack.filename);
    if (m_settings.get_bool("ignore_cache") || !fs::exists(archive_path)) {
      m_http.apply_proxy_settings(m_settings, true);
      if (!m_http.download_file(pack.url, m_paths.cache_dir(), pack.filename)) {
        m_status.post("Could not download soundpack archive.", MSG_ERROR);
        return false;
      }
    }
  }

  // Always extract into an empty directory.
  fs::remove_all(tmp_dir, ec);
  bool extracted = archive_helper::extract(archive_path, tmp_dir, m_status);

  if (!keep_archive && !m_settings.get_bool("keep_cache")) {
    fs::remove(archive_path, ec);
  }

  const fs::path source = tmp_dir / to_path(pack.internal_path);
  if (extracted && !fs::is_directory(source)) {
    m_status.post(
        "The archive does not contain \"" + pack.internal_path + "\".",
        MSG_ERROR);
    extracted = false;
  }
  if (!extracted) {
    fs::remove_all(tmp_dir, ec);
    return false;
  }

  // Only now is it safe to remove the old copy. The directory may not be
  // there even when reinstalling, if the installed copy has another name.
  const fs::path old_copy = sound_dir / to_path(pack.name);
  if (reinstall && fs::exists(fs::symlink_status(old_copy, ec)) &&
      !m_trash.remove(old_copy, "soundpack", pack.name)) {
    fs::remove_all(tmp_dir, ec);
    return false;
  }

  const bool moved =
      fs_helper::move_dir(source, sound_dir / to_path(pack.name), m_status);
  fs::remove_all(tmp_dir, ec);
  if (!moved) {
    return false;
  }

  m_status.post("Soundpack installed.");
  return true;
}
