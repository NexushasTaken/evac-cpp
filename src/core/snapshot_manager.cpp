#include "core/snapshot_manager.h"

#include <boost/algorithm/string/trim.hpp>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "absl/strings/match.h"
#include "core/fs.h"
#include "core/git.h"
#include "core/paths.h"
#include "core/platform.h"
#include "core/status.h"
#include "core/trash.h"

namespace fs = std::filesystem;

static const std::string_view REPO_SUFFIX = ".git";

namespace algo = boost::algorithm;

// Splits git's -z output on NUL. Empty parts are dropped.
static std::vector<std::string> split_nul(const std::string& text) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (start < text.size()) {
    size_t end = text.find('\0', start);
    if (end == std::string::npos) {
      end = text.size();
    }
    if (end > start) {
      parts.push_back(text.substr(start, end - start));
    }
    start = end + 1;
  }
  return parts;
}

static bool ends_with(const std::string& text, const std::string& end) {
  return text.size() >= end.size() && text.ends_with(end);
}

SnapshotManager::SnapshotManager(Paths& paths, Status& status, Trash& trash)
    : m_paths(paths), m_status(status), m_trash(trash) {}

std::string SnapshotManager::default_snapshot_name(const std::tm& time) {
  static const char* const DAYS[] = {"Sun", "Mon", "Tue", "Wed",
                                     "Thu", "Fri", "Sat"};
  static const char* const MONTHS[] = {"Jan", "Feb", "Mar", "Apr",
                                       "May", "Jun", "Jul", "Aug",
                                       "Sep", "Oct", "Nov", "Dec"};
  // Not strftime: the name should not depend on the locale.
  char buf[48];
  std::snprintf(buf, sizeof(buf), "%s %s %d %02d:%02d %d",
                DAYS[((time.tm_wday % 7) + 7) % 7],
                MONTHS[((time.tm_mon % 12) + 12) % 12], time.tm_mday,
                time.tm_hour, time.tm_min, time.tm_year + 1900);
  return buf;
}

fs::path SnapshotManager::world_dir(const std::string& world) {
  return m_paths.savegames() / to_path(world);
}

fs::path SnapshotManager::repo_path(const std::string& world) {
  std::string name = world;
  name += REPO_SUFFIX;
  return m_paths.snapshots() / to_path(name);
}

// The work tree only when the world folder exists: git refuses a missing one,
// and reading the history of a deleted world does not need it.
std::vector<std::string> SnapshotManager::repo_args(const std::string& world) {
  std::vector<std::string> args;
  const fs::path dir = world_dir(world);
  std::error_code ec;
  if (fs::is_directory(dir, ec)) {
    args.emplace_back("-C");
    args.push_back(to_utf8(dir));
    args.push_back("--work-tree=" + to_utf8(dir));
  }
  args.push_back("--git-dir=" + to_utf8(repo_path(world)));
  return args;
}

git::Result SnapshotManager::run(const std::string& world,
                                 const std::vector<std::string>& args) {
  std::vector<std::string> all = repo_args(world);
  all.insert(all.end(), args.begin(), args.end());
  return git::run(all);
}

bool SnapshotManager::run_or_report(const std::string& world,
                                    const std::vector<std::string>& args,
                                    const std::string& what) {
  const git::Result r = run(world, args);
  if (r.ok()) {
    return true;
  }
  const std::string detail = algo::trim_copy(r.err);
  m_status.post(
      what + " failed for " + world + (detail.empty() ? "." : ": " + detail),
      MSG_ERROR);
  return false;
}

bool SnapshotManager::has_commits(const std::string& world) {
  std::error_code ec;
  if (!fs::is_directory(repo_path(world), ec)) {
    return false;
  }
  return run(world, {"rev-parse", "--verify", "-q", "HEAD"}).ok();
}

bool SnapshotManager::ensure_repo(const std::string& world) {
  const fs::path repo = repo_path(world);
  std::error_code ec;
  if (fs::is_directory(repo, ec)) {
    return true;
  }
  fs::create_directories(m_paths.snapshots(), ec);
  if (ec) {
    m_status.post(
        "Cannot create " + to_utf8(m_paths.snapshots()) + ": " + ec.message(),
        MSG_ERROR);
    return false;
  }
  git::Result r = git::run({"init", "-q", "--bare", to_utf8(repo)});
  if (!r.ok()) {
    m_status.post("Cannot create the snapshot repository for " + world + ": " +
                      algo::trim_copy(r.err),
                  MSG_ERROR);
    return false;
  }
  return true;
}

int SnapshotManager::stage_all(const std::string& world) {
  // -A also records the files the game deleted, so a restore brings them back.
  if (!run_or_report(world, {"add", "-A"}, "git add")) {
    return -1;
  }
  // On a repository without commits this compares with an empty tree.
  git::Result r = run(world, {"diff", "--cached", "--quiet"});
  if (r.started && r.exit_code == 0) {
    return 0;
  }
  if (r.started && r.exit_code == 1) {
    return 1;
  }
  m_status.post("git diff failed for " + world + ": " + algo::trim_copy(r.err),
                MSG_ERROR);
  return -1;
}

bool SnapshotManager::commit(const std::string& world, const std::string& name,
                             const std::string& description) {
  // --cleanup=whitespace: the default would drop lines starting with '#', such
  // as a name "#3".
  std::vector<std::string> args = {
      "commit", "-q", "--no-verify", "--cleanup=whitespace", "-m", name,
  };
  if (!algo::trim_copy(description).empty()) {
    args.emplace_back("-m");
    args.push_back(description);
  }
  return run_or_report(world, args, "git commit");
}

// Makes the index and the world folder match rev: tracked files not in rev are
// removed by restore, untracked ones by clean.
bool SnapshotManager::reset_to(const std::string& world,
                               const std::string& rev) {
  return run_or_report(world,
                       {"restore", "--source=" + rev, "--staged", "--worktree",
                        "--", ":/"},
                       "git restore") &&
         run_or_report(world, {"clean", "-fdq", "--", ":/"}, "git clean");
}

std::vector<WorldInfo> SnapshotManager::list_worlds() {
  std::vector<WorldInfo> result;
  std::set<std::string> present;

  std::vector<std::string> worlds = Paths::list_dir(m_paths.savegames());
  for (const auto& world : worlds) {
    WorldInfo info;
    info.name = world;
    present.insert(info.name);
    if (!has_commits(info.name)) {
      info.state = WorldInfo::NO_SNAPSHOTS;
    } else {
      git::Result r = run(info.name, {"status", "--porcelain", "-z"});
      if (!r.ok()) {
        m_status.post("git status failed for " + info.name + ": " +
                          algo::trim_copy(r.err),
                      MSG_WARN);
      }
      info.state =
          (r.ok() && r.out.empty()) ? WorldInfo::UNCHANGED : WorldInfo::CHANGED;
    }
    result.push_back(info);
  }

  std::vector<std::string> repos = Paths::list_dir(m_paths.snapshots());
  for (const auto& repo : repos) {
    if (!repo.ends_with(REPO_SUFFIX)) {
      continue;
    }
    WorldInfo info;
    info.name = repo.substr(0, repo.size() - REPO_SUFFIX.size());
    if (info.name.empty() || (present.contains(info.name) != 0u)) {
      continue;
    }
    info.state = WorldInfo::DELETED;
    git::Result r = run(info.name, {"config", "--get", "--bool", "evac.keep"});
    info.kept = r.ok() && algo::trim_copy(r.out) == "true";
    result.push_back(info);
  }
  return result;
}

std::vector<Snapshot> SnapshotManager::list_snapshots(
    const std::string& world) {
  std::vector<Snapshot> result;
  if (!has_commits(world)) {
    return result;
  }

  // Commits end with NUL (-z), fields with 0x1f.
  git::Result r = run(world, {"log", "-z", "--format=%H%x1f%ct%x1f%B"});
  if (!r.ok()) {
    m_status.post("git log failed for " + world + ": " + algo::trim_copy(r.err),
                  MSG_ERROR);
    return result;
  }

  std::vector<std::string> records = split_nul(r.out);
  for (const auto& record : records) {
    const size_t first = record.find('\x1f');
    const size_t second =
        (first == std::string::npos) ? first : record.find('\x1f', first + 1);
    if (second == std::string::npos) {
      continue;
    }

    Snapshot snap;
    snap.hash = record.substr(0, first);
    snap.time =
        std::atoll(record.substr(first + 1, second - first - 1).c_str());
    const std::string message = record.substr(second + 1);
    const size_t newline = message.find('\n');
    snap.name = algo::trim_copy(message.substr(0, newline));
    if (newline != std::string::npos) {
      snap.description = algo::trim_copy(message.substr(newline + 1));
    }
    result.push_back(snap);
  }
  return result;
}

int SnapshotManager::changed_files(const std::string& world,
                                   const std::string& hash) {
  git::Result r = run(world, {"diff-tree", "--no-commit-id", "-r",
                              "--name-only", "-z", "--root", hash});
  if (!r.ok()) {
    return -1;
  }
  return static_cast<int>(split_nul(r.out).size());
}

std::uint64_t SnapshotManager::history_size(const std::string& world) {
  std::uint64_t total = 0;
  std::error_code ec;
  fs::recursive_directory_iterator it(repo_path(world), ec);
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

std::string SnapshotManager::git_command(const std::string& world) {
  return "git " + git::shell_quote("--git-dir=" + to_utf8(repo_path(world))) +
         " " + git::shell_quote("--work-tree=" + to_utf8(world_dir(world)));
}

bool SnapshotManager::can_write(std::string& reason) {
  fs::path dir = m_paths.savegames();
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) {
    dir = m_paths.userdata();
  }
  bool read_only_fs = false;
  if (platform::dir_writable(dir, read_only_fs)) {
    return true;
  }
  if (read_only_fs) {
    reason = "The userdata folder is read-only (" +
             to_utf8(m_paths.userdata()) +
             "). On a Windows partition, this happens when Windows was "
             "hibernated or shut down with Fast Startup.";
  } else {
    reason = "Cannot write to " + to_utf8(dir) + ".";
  }
  return false;
}

SnapshotManager::Result SnapshotManager::take_snapshot(
    const std::string& world, const std::string& name,
    const std::string& description) {
  return take(world, name, description, true);
}

SnapshotManager::Result SnapshotManager::take(const std::string& world,
                                              const std::string& name,
                                              const std::string& description,
                                              bool report_unchanged) {
  const std::string title = algo::trim_copy(name);
  if (title.empty()) {
    m_status.post("A snapshot needs a name.", MSG_ERROR);
    return FAILED;
  }
  std::error_code ec;
  if (world.empty() || !fs::is_directory(world_dir(world), ec)) {
    m_status.post("World \"" + world + "\" not found.", MSG_ERROR);
    return FAILED;
  }
  std::string reason;
  if (!can_write(reason)) {
    m_status.post(reason, MSG_ERROR);
    return FAILED;
  }
  if (!ensure_repo(world)) {
    return FAILED;
  }

  const int staged = stage_all(world);
  if (staged < 0) {
    return FAILED;
  }
  if (staged == 0) {
    if (report_unchanged) {
      m_status.post(world + ": no changes since the last snapshot.");
    }
    return NOTHING_TO_DO;
  }
  if (!commit(world, title, description)) {
    return FAILED;
  }
  m_status.post("Snapshot \"" + title + "\" of " + world + " taken.");
  return DONE;
}

SnapshotManager::AllResult SnapshotManager::take_snapshot_all(
    const std::string& name, const std::string& description) {
  AllResult result;
  std::vector<std::string> worlds = Paths::list_dir(m_paths.savegames());
  for (const auto& world : worlds) {
    const Result r = take(world, name, description, false);
    if (r == DONE) {
      ++result.taken;
    } else if (r == NOTHING_TO_DO) {
      ++result.unchanged;
    } else {
      ++result.failed;
    }
  }

  std::string text = std::to_string(result.taken) +
                     (result.taken == 1 ? " snapshot" : " snapshots") +
                     " taken";
  if (result.unchanged > 0) {
    text += ", " + std::to_string(result.unchanged) +
            (result.unchanged == 1 ? " world had no changes"
                                   : " worlds had no changes");
  }
  if (result.failed > 0) {
    text += ", " + std::to_string(result.failed) + " failed";
  }
  m_status.post(text + ".", result.failed > 0 ? MSG_WARN : MSG_INFO);
  return result;
}

SnapshotManager::Result SnapshotManager::restore(const std::string& world,
                                                 const std::string& hash) {
  if (!has_commits(world)) {
    m_status.post(world + " has no snapshots.", MSG_ERROR);
    return FAILED;
  }
  git::Result found =
      run(world, {"log", "-1", "--format=%H%x1f%s", hash + "^{commit}", "--"});
  const size_t sep = found.out.find('\x1f');
  if (!found.ok() || sep == std::string::npos) {
    m_status.post("Snapshot " + hash + " of " + world + " not found.",
                  MSG_ERROR);
    return FAILED;
  }
  const std::string full_hash = found.out.substr(0, sep);
  const std::string snap_name = algo::trim_copy(found.out.substr(sep + 1));
  std::string reason;
  if (!can_write(reason)) {
    m_status.post(reason, MSG_ERROR);
    return FAILED;
  }

  // A world that no longer exists gets its folder back. There is nothing in
  // it to keep, so no "Before restoring" commit for it.
  std::error_code ec;
  const bool existed = fs::is_directory(world_dir(world), ec);
  if (!existed) {
    fs::create_directories(world_dir(world), ec);
    if (ec) {
      m_status.post(
          "Cannot create " + to_utf8(world_dir(world)) + ": " + ec.message(),
          MSG_ERROR);
      return FAILED;
    }
  }

  int staged = stage_all(world);
  if (staged < 0) {
    return FAILED;
  }
  git::Result same =
      run(world, {"diff", "--cached", "--quiet", full_hash, "--"});
  if (same.started && same.exit_code == 0) {
    m_status.post(world + " already matches the snapshot \"" + snap_name +
                  "\". Nothing to restore.");
    return NOTHING_TO_DO;
  }
  if (staged == 1 && existed &&
      !commit(world, "Before restoring \"" + snap_name + "\"", "")) {
    return FAILED;
  }

  m_status.post("Restoring the snapshot \"" + snap_name + "\" of " + world +
                "...");
  if (!reset_to(world, full_hash)) {
    return FAILED;
  }
  staged = stage_all(world);
  if (staged < 0) {
    return FAILED;
  }
  if (staged == 1 && !commit(world, "Restored \"" + snap_name + "\"", "")) {
    return FAILED;
  }
  m_status.post("Snapshot \"" + snap_name + "\" of " + world + " restored.");
  return DONE;
}

SnapshotManager::Result SnapshotManager::discard_changes(
    const std::string& world) {
  std::error_code ec;
  if (!has_commits(world) || !fs::is_directory(world_dir(world), ec)) {
    m_status.post(world + " has no snapshot to go back to.", MSG_ERROR);
    return FAILED;
  }
  std::string reason;
  if (!can_write(reason)) {
    m_status.post(reason, MSG_ERROR);
    return FAILED;
  }
  git::Result r = run(world, {"status", "--porcelain", "-z"});
  if (r.ok() && r.out.empty()) {
    m_status.post(world + " has no changes to discard.");
    return NOTHING_TO_DO;
  }
  if (!reset_to(world, "HEAD")) {
    return FAILED;
  }
  m_status.post("Changes to " + world + " discarded.");
  return DONE;
}

bool SnapshotManager::mark_kept(const std::string& world) {
  return run_or_report(world, {"config", "evac.keep", "true"}, "git config");
}

bool SnapshotManager::delete_snapshots(const std::string& world) {
  const fs::path repo = repo_path(world);
  std::error_code ec;
  if (world.empty() || !fs::is_directory(repo, ec)) {
    m_status.post("No snapshots of \"" + world + "\" found.", MSG_ERROR);
    return false;
  }
  if (!m_trash.remove(repo, "snapshots", world)) {
    return false;
  }
  m_status.post("Snapshots of " + world + " deleted.");
  return true;
}

bool SnapshotManager::export_snapshots(const std::string& world,
                                       const fs::path& path) {
  if (!has_commits(world)) {
    m_status.post(world + " has no snapshots to export.", MSG_ERROR);
    return false;
  }
  // git runs in the world folder, so a relative path would land there.
  std::error_code ec;
  fs::path target = fs::absolute(path, ec);
  if (ec) {
    target = path;
  }
  m_status.post("Exporting the snapshots of " + world + "...");
  if (!run_or_report(world,
                     {"bundle", "create", "-q", to_utf8(target), "--all"},
                     "Export")) {
    return false;
  }
  m_status.post("Snapshots of " + world + " exported to " + to_utf8(target) +
                ".");
  return true;
}

std::string SnapshotManager::bundle_world_name(const fs::path& path) {
  std::string name = to_utf8(path.filename());
  if (ends_with(name, ".bundle")) {
    name.resize(name.size() - 7);
  }
  return name;
}

// Same rules as the game's worldfactory::valid_worldname and is_char_allowed.
std::string SnapshotManager::check_import_name(const std::string& world) {
  if (world.empty()) {
    return "World name cannot be empty.";
  }
  if (world == "save" || world == "TUTORIAL" || world == "DEFENSE") {
    return world + " is a reserved name.";
  }
  if (world == "." || world == "..") {
    return "World name cannot be " + world + ".";
  }
  for (char i : world) {
    const auto c = static_cast<unsigned char>(i);
    if (c >= 128) {
      continue;  // Part of a UTF-8 character.
    }
    if (std::isprint(c) == 0) {
      return "World name contains an invalid character.";
    }
    if (absl::StrContains(std::string("\\/:*?\"<>|"), (char)c)) {
      return std::string("World name contains invalid character: '") +
             static_cast<char>(c) + "'.";
    }
  }
  std::error_code ec;
  if (fs::exists(world_dir(world), ec)) {
    return "A world named " + world + " already exists.";
  }
  if (fs::exists(repo_path(world), ec)) {
    return "There are already snapshots of a world named " + world + ".";
  }
  return "";
}

// Picks the latest snapshot from the bundle's HEAD, or its only branch.
static std::string bundle_head(const std::string& list_heads) {
  std::vector<std::string> refs;
  size_t start = 0;
  while (start < list_heads.size()) {
    size_t end = list_heads.find('\n', start);
    if (end == std::string::npos) {
      end = list_heads.size();
    }
    const std::string line = list_heads.substr(start, end - start);
    const size_t space = line.find(' ');
    if (space != std::string::npos) {
      refs.push_back(line.substr(space + 1));
    }
    start = end + 1;
  }
  for (const auto& ref : refs) {
    if (ref == "HEAD") {
      return "HEAD";
    }
  }
  return refs.size() == 1 ? refs[0] : "";
}

SnapshotManager::Result SnapshotManager::import_snapshots(
    const fs::path& bundle_path, const std::string& world) {
  const std::string problem = check_import_name(world);
  if (!problem.empty()) {
    m_status.post("Cannot import as \"" + world + "\": " + problem, MSG_ERROR);
    return FAILED;
  }
  std::string reason;
  if (!can_write(reason)) {
    m_status.post(reason, MSG_ERROR);
    return FAILED;
  }
  const std::string bundle = to_utf8(bundle_path);
  git::Result heads = git::run({"bundle", "list-heads", bundle});
  const std::string head = heads.ok() ? bundle_head(heads.out) : "";
  if (head.empty()) {
    const std::string detail =
        heads.ok() ? "it does not say which snapshot is the latest."
                   : algo::trim_copy(heads.err);
    m_status.post("Cannot import " + bundle + ": " + detail, MSG_ERROR);
    return FAILED;
  }

  // Both are new (check_import_name), so a failed import can remove them.
  const fs::path repo = repo_path(world);
  const fs::path dir = world_dir(world);
  std::error_code ec;
  if (!ensure_repo(world)) {
    fs::remove_all(repo, ec);
    return FAILED;
  }
  m_status.post("Importing the snapshots of " + world + "...");
  bool ok = run_or_report(world, {"bundle", "verify", "-q", bundle},
                          "Checking the bundle") &&
            run_or_report(
                world, {"fetch", "-q", bundle, "+" + head + ":refs/heads/main"},
                "Import");
  if (ok) {
    fs::create_directories(dir, ec);
    if (ec) {
      m_status.post("Cannot create " + to_utf8(dir) + ": " + ec.message(),
                    MSG_ERROR);
    }
    ok = !ec && reset_to(world, "HEAD");
  }
  if (!ok) {
    fs::remove_all(dir, ec);
    fs::remove_all(repo, ec);
    return FAILED;
  }
  const size_t count = list_snapshots(world).size();
  m_status.post("Imported " + world + " with " + std::to_string(count) +
                (count == 1 ? " snapshot." : " snapshots."));
  return DONE;
}

SnapshotManager::Result SnapshotManager::start_fresh(
    const std::string& world, const std::string& name,
    const std::string& description) {
  std::error_code ec;
  if (!fs::is_directory(world_dir(world), ec) || !has_commits(world)) {
    m_status.post(world + " has no snapshots to start fresh from.", MSG_ERROR);
    return FAILED;
  }
  if (algo::trim_copy(name).empty()) {
    m_status.post("A snapshot needs a name.", MSG_ERROR);
    return FAILED;
  }
  std::string reason;
  if (!can_write(reason)) {
    m_status.post(reason, MSG_ERROR);
    return FAILED;
  }
  if (!m_trash.remove(repo_path(world), "snapshots", world)) {
    return FAILED;
  }
  const Result result = take(world, name, description, true);
  if (result == FAILED) {
    m_status.post("The old snapshots of " + world +
                      " are in the trash, but the new snapshot failed.",
                  MSG_ERROR);
  } else if (result == NOTHING_TO_DO) {
    m_status.post("Started fresh: " + world +
                  " is empty, so it has no snapshots now.");
  } else {
    m_status.post("Started fresh: " + world + " has one snapshot, \"" +
                  algo::trim_copy(name) + "\".");
  }
  return result;
}

std::vector<std::string> SnapshotManager::worlds_with_changes() {
  std::vector<std::string> names;
  std::vector<WorldInfo> worlds = list_worlds();
  for (auto& world : worlds) {
    if (world.state == WorldInfo::CHANGED) {
      names.push_back(world.name);
    }
  }
  return names;
}
