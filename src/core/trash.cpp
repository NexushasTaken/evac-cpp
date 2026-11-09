#include "core/trash.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <format>
#include <map>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "core/backup_manager.h"
#include "core/fs.h"
#include "core/helpers.h"
#include "core/mod_manager.h"
#include "core/paths.h"
#include "core/settings.h"
#include "core/status.h"

using std::uint64_t;

namespace fs = std::filesystem;

static const std::string_view INFO_FILE = "info.json";
static const std::string_view ITEM_DIR = "item";

Trash::Trash(Settings& settings, Paths& paths, Status& status)
    : m_settings(settings), m_paths(paths), m_status(status) {}

// True if path is dir itself or somewhere inside it.
static bool is_within(const fs::path& path, const fs::path& dir) {
  const fs::path rel = path.lexically_relative(dir);
  return !rel.empty() && *rel.begin() != "..";
}

static bool exists_no_follow(const fs::path& path) {
  std::error_code ec;
  return fs::exists(fs::symlink_status(path, ec));
}

// Bytes in all the regular files under path. Symlinks are not followed.
static std::uint64_t measure(const fs::path& path) {
  std::error_code ec;
  const fs::file_status st = fs::symlink_status(path, ec);
  if (fs::is_regular_file(st)) {
    return fs::file_size(path, ec);
  }
  if (!fs::is_directory(st)) {
    return 0;
  }

  std::uint64_t total = 0;
  fs::recursive_directory_iterator it(path, ec);
  for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
    std::error_code item_ec;
    if (fs::is_regular_file(it->symlink_status(item_ec))) {
      std::uintmax_t size = it->file_size(item_ec);
      if (!item_ec) {
        total += size;
      }
    }
  }
  return total;
}

// An id from list(), and nothing that points outside the trash. Backslash is
// a separator on Windows; make_id never uses it.
static bool is_valid_id(const std::string& id) {
  return !id.empty() && id != "." && id != ".." &&
         id.find_first_of("/\\") == std::string::npos;
}

// path if it is free, else path + " (restored)", " (restored 2)", ...
static fs::path free_path(const fs::path& path) {
  if (!exists_no_follow(path)) {
    return path;
  }
  fs::path candidate = path;
  candidate += " (restored)";
  for (int n = 2; exists_no_follow(candidate); ++n) {
    candidate = path;
    candidate += " (restored " + std::to_string(n) + ")";
  }
  return candidate;
}

// The first free gameN in a game's directory, like Paths::next_install_dir.
static fs::path free_install_dir(const fs::path& base_dir) {
  int n = 0;
  while (exists_no_follow(base_dir / ("game" + std::to_string(n)))) {
    ++n;
  }
  return base_dir / ("game" + std::to_string(n));
}

// The install of that release in a game's directory, or empty. Like
// Paths::installs_summary, but without its side effect on the settings.
static fs::path find_install(const fs::path& base_dir,
                             const std::string& release_name, Status& status) {
  const std::vector<std::string> subdirs = Paths::list_dir(base_dir);
  for (const auto& subdir : subdirs) {
    fs::path dir = base_dir / to_path(subdir);
    const fs::path info_file = dir / INFO_FILENAME;
    nlohmann::json info;
    if (fs::exists(info_file) && load_json_file(info_file, info, status) &&
        info.is_object() && info.contains("name") &&
        info["name"] == release_name) {
      return dir;
    }
  }
  return {};
}

// A directory with move_dir, anything else (a file, a symlink) with a rename.
static bool move_any(const fs::path& src, const fs::path& dest,
                     Status& status) {
  std::error_code ec;
  const fs::file_status st = fs::symlink_status(src, ec);
  if (fs::is_directory(st)) {
    return fs_helper::move_dir(src, dest, status);
  }
  fs::rename(src, dest, ec);
  if (ec) {
    status.post("Failed to move " + to_utf8(src) + " to " + to_utf8(dest) +
                    ": " + ec.message(),
                MSG_ERROR);
    return false;
  }
  return true;
}

std::string Trash::make_id(const std::string& kind, const std::string& name,
                           Clock::time_point when) {
  static const size_t SAFE_NAME_MAX = 40;
  namespace chr = std::chrono;
  const auto secs = chr::floor<chr::seconds>(when);
  const auto milli = chr::duration_cast<chr::milliseconds>(when - secs);

  const auto stamp =
      std::format("{:%Y%m%d-%H-%M-%S}-{:03}", secs, milli.count());

  // Only safe characters, and not too long: the id is a directory name.
  std::string safe_name;
  safe_name.reserve(SAFE_NAME_MAX);
  for (size_t i = 0; i < name.size() && safe_name.size() < SAFE_NAME_MAX; ++i) {
    const char c = name.at(i);
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '-' || c == '.';
    safe_name += ok ? c : '_';
  }
  return std::string(stamp) + "_" + kind + "_" + safe_name;
}

bool Trash::remove(const fs::path& path, const std::string& kind,
                   const std::string& name) {
  return remove(path, kind, name, Clock::now());
}

bool Trash::remove(const fs::path& path, const std::string& kind,
                   const std::string& name, Clock::time_point when) {
  std::error_code ec;
  if (!exists_no_follow(path)) {
    m_status.post("Nothing to delete at " + to_utf8(path), MSG_ERROR);
    return false;
  }

  if (!m_settings.get_bool("use_trash")) {
    m_status.post("Deleting " + to_utf8(path));
    return fs_helper::rm_dir(path, m_status);
  }

  // Moving the trash into itself, or a directory that holds the trash, can't
  // work.
  const fs::path trash_dir = fs::weakly_canonical(m_paths.trash_dir(), ec);
  const fs::path target = fs::weakly_canonical(path, ec);
  if (is_within(target, trash_dir) || is_within(trash_dir, target)) {
    m_status.post("Refusing to move " + to_utf8(path) +
                      " to the trash: it is or contains the trash.",
                  MSG_ERROR);
    return false;
  }

  const std::string base_id = make_id(kind, name, when);
  std::string id = base_id;
  for (int n = 2; exists_no_follow(m_paths.trash_dir() / to_path(id)); ++n) {
    id = base_id + "-" + std::to_string(n);
  }
  const fs::path entry = m_paths.trash_dir() / to_path(id);

  namespace chr = std::chrono;
  const auto secs = chr::floor<chr::seconds>(when);
  const auto milli = chr::duration_cast<chr::milliseconds>(when - secs);

  const auto deleted_at =
      std::format("{:%Y-%m-%d %H:%M:%S}.{:06}", secs, milli.count());

  nlohmann::json info;
  info["kind"] = kind;
  info["name"] = name;
  info["game"] = m_settings.get_string("game");
  info["original_path"] = to_utf8(fs::absolute(path, ec));
  info["deleted_at"] = deleted_at;
  info["size"] = measure(path);

  m_status.post("Moving " + name + " to the trash...");
  fs::create_directories(entry, ec);
  if (ec) {
    m_status.post("Failed to create " + to_utf8(entry) + ": " + ec.message(),
                  MSG_ERROR);
    return false;
  }
  if (!save_json_file(info, entry / INFO_FILE, m_status)) {
    fs::remove_all(entry, ec);
    return false;
  }

  if (!fs_helper::move_dir(path, entry / ITEM_DIR, m_status)) {
    // Across filesystems move_dir goes file by file, so part of it may be in
    // the trash already. Only throw the entry away if nothing got there.
    if (!exists_no_follow(entry / ITEM_DIR)) {
      fs::remove_all(entry, ec);
    } else {
      m_status.post("Part of " + name +
                        " was moved to the trash. It is kept there, in " +
                        to_utf8(entry),
                    MSG_WARN);
    }
    return false;
  }
  return true;
}

bool Trash::load_item(const std::string& id, TrashItem& out) {
  if (!is_valid_id(id)) {
    return false;
  }
  const fs::path entry = m_paths.trash_dir() / to_path(id);
  if (!exists_no_follow(entry / INFO_FILE) ||
      !exists_no_follow(entry / ITEM_DIR)) {
    return false;
  }
  nlohmann::json info;
  if (!load_json_file(entry / INFO_FILE, info, m_status) || !info.is_object()) {
    return false;
  }

  out = TrashItem();
  out.id = id;
  out.kind = info.value("kind", "");
  out.name = info.value("name", id);
  out.game = info.value("game", "");
  out.original_path = to_path(info.value("original_path", ""));
  out.deleted_at = info.value("deleted_at", "");
  out.size = info.value("size", static_cast<std::uint64_t>(0));
  return true;
}

std::vector<TrashItem> Trash::list() {
  std::vector<TrashItem> result;

  const std::vector<std::string> ids = Paths::list_dir(m_paths.trash_dir());
  for (const auto& id : ids) {
    TrashItem item;
    if (load_item(id, item)) {
      result.push_back(item);
    }
  }

  std::ranges::sort(result, [](const TrashItem& a, const TrashItem& b) {
    if (a.deleted_at != b.deleted_at) {
      return a.deleted_at > b.deleted_at;
    }
    return a.id > b.id;
  });
  return result;
}

std::uint64_t Trash::total_size() {
  const std::vector<TrashItem> items = list();
  uint64_t total = 0;
  for (const auto& item : items) {
    total += item.size;
  }
  return total;
}

bool Trash::delete_items(const std::vector<std::string>& ids) {
  bool ok = true;
  for (const auto& id : ids) {
    // Ids come from list(), but never let one point outside the trash.
    if (!is_valid_id(id) ||
        !fs::is_directory(m_paths.trash_dir() / to_path(id))) {
      m_status.post("Not in the trash: \"" + id + "\"", MSG_ERROR);
      ok = false;
      continue;
    }
    if (!fs_helper::rm_dir(m_paths.trash_dir() / to_path(id), m_status)) {
      ok = false;
    }
  }
  return ok;
}

bool Trash::empty() {
  m_status.post("Emptying the trash...");
  std::vector<fs::path> entries;
  std::error_code ec;
  fs::directory_iterator it(m_paths.trash_dir(), ec);
  if (ec) {
    return true;  // No trash directory: nothing to do.
  }
  for (; !ec && it != fs::directory_iterator(); it.increment(ec)) {
    entries.push_back(it->path());
  }

  bool ok = true;
  for (const auto& entrie : entries) {
    if (!fs_helper::rm_dir(entrie, m_status)) {
      ok = false;
    }
  }
  return ok;
}

std::vector<TrashItem> Trash::over_limit() {
  std::vector<TrashItem> items = list();  // Newest first.
  constexpr int MiB = 1024 * 1024;
  std::uint64_t limit = static_cast<std::uint64_t>(std::max(
                            0, m_settings.get_int("trash_max_size_mb"))) *
                        MiB;
  std::uint64_t total = 0;
  for (const auto& item : items) {
    total += item.size;
  }

  std::vector<TrashItem> result;
  for (size_t i = items.size(); i > 1 && total > limit;
       --i)  // Stops before items[0], the newest.
  {
    result.push_back(items[i - 1]);
    total -= items[i - 1].size;
  }
  return result;
}

RestorePlan Trash::plan_restore(const std::string& id) {
  RestorePlan plan;
  TrashItem item;
  if (!load_item(id, item)) {
    plan.reason = "It is not in the trash.";
    return plan;
  }
  const fs::path entry = m_paths.trash_dir() / to_path(id);
  const fs::path item_dir = entry / ITEM_DIR;

  std::error_code ec;
  const fs::path& original = item.original_path;
  if (!original.is_absolute()) {
    plan.reason = "Where it came from is not known.";
    return plan;
  }
  if (is_within(fs::weakly_canonical(original, ec),
                fs::weakly_canonical(m_paths.trash_dir(), ec))) {
    plan.reason = "It would be restored into the trash.";
    return plan;
  }
  const fs::path parent = original.parent_path();
  if (!fs::is_directory(parent)) {
    plan.reason =
        "The folder it was in (" + to_utf8(parent) + ") no longer exists.";
    return plan;
  }

  if (item.kind == "mod") {
    // The game tells mods apart by ID, not by directory.
    std::map<std::string, ModEntry> mine =
        ModManager::parse_mods_dir(entry, m_status);
    if (!mine.empty()) {
      std::map<std::string, ModEntry> there =
          ModManager::parse_mods_dir(parent, m_status);
      const auto same = there.find(mine.begin()->first);
      if (same != there.end()) {
        plan.kind = RestorePlan::CONFLICT;
        plan.conflict_path = same->second.location;
        plan.conflict_name = same->second.name;
        plan.reason = "A mod with the same ID (" + same->first +
                      ") is installed: " + same->second.name + ".";
        return plan;
      }
    }
  } else if (item.kind == "install") {
    fs::path same = find_install(parent, item.name, m_status);
    if (!same.empty()) {
      plan.kind = RestorePlan::CONFLICT;
      plan.conflict_path = std::move(same);
      plan.conflict_name = item.name;
      plan.reason = "\"" + item.name + "\" is already installed.";
      return plan;
    }
    plan.dest =
        exists_no_follow(original) ? free_install_dir(parent) : original;
    plan.kind =
        (plan.dest == original) ? RestorePlan::ORIGINAL : RestorePlan::RENAMED;
    return plan;
  } else if (item.kind == "saves") {
    // The game only looks in one save directory, so the worlds go back into it.
    plan.dest = original;
    plan.kind = RestorePlan::ORIGINAL;
    if (exists_no_follow(original)) {
      const std::vector<std::string> worlds = Paths::list_dir(item_dir);
      for (const auto& world : worlds) {
        if (exists_no_follow(plan.dest / to_path(world))) {
          plan.kind = RestorePlan::RENAMED;
        }
      }
    }
    return plan;
  } else if (item.kind == "snapshots" && exists_no_follow(original)) {
    // Renamed, it would no longer be <World>.git and the Snapshots tab
    // would not see it. Replacing keeps the newer history in the trash.
    plan.kind = RestorePlan::CONFLICT;
    plan.conflict_path = original;
    plan.conflict_name = item.name;
    plan.reason = "There are newer snapshots of " + item.name +
                  " (for example after Start fresh).";
    return plan;
  }

  plan.dest = free_path(original);
  plan.kind =
      (plan.dest == original) ? RestorePlan::ORIGINAL : RestorePlan::RENAMED;
  return plan;
}

bool Trash::can_choose_name(const std::string& kind) {
  return kind != "saves" && kind != "install";
}

std::string Trash::check_restore_name(const std::string& id,
                                      const std::string& name) {
  TrashItem item;
  if (!load_item(id, item)) {
    return "It is not in the trash.";
  }
  if (!can_choose_name(item.kind)) {
    return "Its name can't be chosen.";
  }
  if (name.empty()) {
    return "The name cannot be empty.";
  }
  // Backslash is a separator on Windows.
  if (name == "." || name == ".." ||
      name.find_first_of(std::string("/\\\0", 3)) != std::string::npos) {
    return "The name cannot be " + name + " or contain / or \\.";
  }
  if (item.kind == "backup" && !BackupManager::is_valid_backup_name(name)) {
    return "A backup name cannot contain :/\\?*\"|%<> or start or end with a "
           "space.";
  }
  if (exists_no_follow(item.original_path.parent_path() / to_path(name))) {
    return "Something named " + name + " is already there.";
  }
  return "";
}

bool Trash::restore(const std::string& id, bool replace,
                    const std::string& name) {
  RestorePlan plan = plan_restore(id);
  TrashItem item;
  if (plan.kind == RestorePlan::IMPOSSIBLE || !load_item(id, item)) {
    m_status.post("Can't restore: " + plan.reason, MSG_ERROR);
    return false;
  }
  if (!name.empty()) {
    // Checked again: something may have taken the name since it was asked for.
    const std::string problem = plan.kind == RestorePlan::CONFLICT
                                    ? plan.reason
                                    : check_restore_name(id, name);
    if (!problem.empty()) {
      m_status.post("Not restored as \"" + name + "\": " + problem, MSG_ERROR);
      return false;
    }
    plan.dest = item.original_path.parent_path() / to_path(name);
  }
  if (plan.kind == RestorePlan::CONFLICT) {
    if (!replace) {
      m_status.post("Not restored: " + plan.reason, MSG_ERROR);
      return false;
    }
    if (!remove(plan.conflict_path, item.kind, plan.conflict_name)) {
      return false;
    }
    plan = plan_restore(id);
    if (plan.kind == RestorePlan::CONFLICT ||
        plan.kind == RestorePlan::IMPOSSIBLE) {
      m_status.post("Can't restore: " + plan.reason, MSG_ERROR);
      return false;
    }
  }

  const fs::path entry = m_paths.trash_dir() / to_path(id);
  const fs::path item_dir = entry / ITEM_DIR;
  m_status.post("Restoring " + item.name + "...");

  bool ok = true;
  if (item.kind == "saves" && exists_no_follow(plan.dest)) {
    // Into the current saves, world by world (files too, if there are any).
    std::vector<std::string> names;
    std::error_code ec;
    for (fs::directory_iterator it(item_dir, ec);
         !ec && it != fs::directory_iterator(); it.increment(ec)) {
      names.push_back(to_utf8(it->path().filename()));
    }
    for (const auto& name : names) {
      const fs::path wanted = plan.dest / to_path(name);
      const fs::path target = free_path(wanted);
      if (target != wanted) {
        m_status.post(name + " is restored as " + to_utf8(target.filename()));
      }
      if (!move_any(item_dir / to_path(name), target, m_status)) {
        ok = false;
      }
    }
  } else {
    ok = move_any(item_dir, plan.dest, m_status);
  }

  if (!ok) {
    m_status.post("Not everything of " + item.name +
                      " could be restored. The rest is still in the trash.",
                  MSG_WARN);
    return false;
  }
  fs_helper::rm_dir(entry, m_status);
  m_status.post("Restored " + item.name + " to " + to_utf8(plan.dest));
  return true;
}
