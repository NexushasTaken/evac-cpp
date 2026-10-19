#include "core/snapshot_manager.h"

#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>

namespace fs = std::filesystem;

static const char REPO_SUFFIX[] = ".git";

static std::vector<std::string> args_of(std::initializer_list<std::string> list)
{
    return std::vector<std::string>(list);
}

static std::string trim(const std::string& text)
{
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

// Splits git's -z output on NUL. Empty parts are dropped.
static std::vector<std::string> split_nul(const std::string& text)
{
    std::vector<std::string> parts;
    size_t start = 0;
    while (start < text.size())
    {
        size_t end = text.find('\0', start);
        if (end == std::string::npos)
            end = text.size();
        if (end > start)
            parts.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}

static bool ends_with(const std::string& text, const std::string& end)
{
    return text.size() >= end.size() && text.compare(text.size() - end.size(), end.size(), end) == 0;
}

SnapshotManager::SnapshotManager(Paths& paths, Status& status, Trash& trash)
    : paths_(paths), status_(status), trash_(trash)
{
}

std::string SnapshotManager::default_snapshot_name(const std::tm& time)
{
    static const char* const DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* const MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    // Not strftime: the name should not depend on the locale.
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%s %s %d %02d:%02d %d", DAYS[((time.tm_wday % 7) + 7) % 7],
                  MONTHS[((time.tm_mon % 12) + 12) % 12], time.tm_mday, time.tm_hour, time.tm_min,
                  time.tm_year + 1900);
    return buf;
}

std::string SnapshotManager::world_dir(const std::string& world) { return paths_.savegames() + "/" + world; }

std::string SnapshotManager::repo_path(const std::string& world)
{
    return paths_.snapshots() + "/" + world + REPO_SUFFIX;
}

// The work tree only when the world folder exists: git refuses a missing one,
// and reading the history of a deleted world does not need it.
std::vector<std::string> SnapshotManager::repo_args(const std::string& world)
{
    std::vector<std::string> args;
    std::string dir = world_dir(world);
    std::error_code ec;
    if (fs::is_directory(dir, ec))
    {
        args.push_back("-C");
        args.push_back(dir);
        args.push_back("--work-tree=" + dir);
    }
    args.push_back("--git-dir=" + repo_path(world));
    return args;
}

git::Result SnapshotManager::run(const std::string& world, const std::vector<std::string>& args)
{
    std::vector<std::string> all = repo_args(world);
    all.insert(all.end(), args.begin(), args.end());
    return git::run(all);
}

bool SnapshotManager::run_or_report(const std::string& world, const std::vector<std::string>& args,
                                    const std::string& what)
{
    git::Result r = run(world, args);
    if (r.ok())
        return true;
    std::string detail = trim(r.err);
    status_.post(what + " failed for " + world + (detail.empty() ? "." : ": " + detail), MSG_ERROR);
    return false;
}

bool SnapshotManager::has_commits(const std::string& world)
{
    std::error_code ec;
    if (!fs::is_directory(repo_path(world), ec))
        return false;
    return run(world, args_of({"rev-parse", "--verify", "-q", "HEAD"})).ok();
}

bool SnapshotManager::ensure_repo(const std::string& world)
{
    std::string repo = repo_path(world);
    std::error_code ec;
    if (fs::is_directory(repo, ec))
        return true;
    fs::create_directories(paths_.snapshots(), ec);
    if (ec)
    {
        status_.post("Cannot create " + paths_.snapshots() + ": " + ec.message(), MSG_ERROR);
        return false;
    }
    git::Result r = git::run(args_of({"init", "-q", "--bare", repo}));
    if (!r.ok())
    {
        status_.post("Cannot create the snapshot repository for " + world + ": " + trim(r.err), MSG_ERROR);
        return false;
    }
    return true;
}

int SnapshotManager::stage_all(const std::string& world)
{
    // -A also records the files the game deleted, so a restore brings them back.
    if (!run_or_report(world, args_of({"add", "-A"}), "git add"))
        return -1;
    // On a repository without commits this compares with an empty tree.
    git::Result r = run(world, args_of({"diff", "--cached", "--quiet"}));
    if (r.started && r.exit_code == 0)
        return 0;
    if (r.started && r.exit_code == 1)
        return 1;
    status_.post("git diff failed for " + world + ": " + trim(r.err), MSG_ERROR);
    return -1;
}

bool SnapshotManager::commit(const std::string& world, const std::string& name, const std::string& description)
{
    // --cleanup=whitespace: the default would drop lines starting with '#', such as a name "#3".
    std::vector<std::string> args = args_of({"commit", "-q", "--no-verify", "--cleanup=whitespace", "-m", name});
    if (!trim(description).empty())
    {
        args.push_back("-m");
        args.push_back(description);
    }
    return run_or_report(world, args, "git commit");
}

// Makes the index and the world folder match rev: tracked files not in rev are
// removed by restore, untracked ones by clean.
bool SnapshotManager::reset_to(const std::string& world, const std::string& rev)
{
    return run_or_report(world, args_of({"restore", "--source=" + rev, "--staged", "--worktree", "--", ":/"}),
                         "git restore")
           && run_or_report(world, args_of({"clean", "-fdq", "--", ":/"}), "git clean");
}

std::vector<WorldInfo> SnapshotManager::list_worlds()
{
    std::vector<WorldInfo> result;
    std::set<std::string> present;

    std::vector<std::string> worlds = Paths::list_dir(paths_.savegames());
    for (size_t i = 0; i < worlds.size(); ++i)
    {
        WorldInfo info;
        info.name = worlds[i];
        present.insert(info.name);
        if (!has_commits(info.name))
        {
            info.state = WorldInfo::NO_SNAPSHOTS;
        }
        else
        {
            git::Result r = run(info.name, args_of({"status", "--porcelain", "-z"}));
            if (!r.ok())
                status_.post("git status failed for " + info.name + ": " + trim(r.err), MSG_WARN);
            info.state = (r.ok() && r.out.empty()) ? WorldInfo::UNCHANGED : WorldInfo::CHANGED;
        }
        result.push_back(info);
    }

    std::vector<std::string> repos = Paths::list_dir(paths_.snapshots());
    for (size_t i = 0; i < repos.size(); ++i)
    {
        if (!ends_with(repos[i], REPO_SUFFIX))
            continue;
        WorldInfo info;
        info.name = repos[i].substr(0, repos[i].size() - (sizeof(REPO_SUFFIX) - 1));
        if (info.name.empty() || present.count(info.name))
            continue;
        info.state = WorldInfo::DELETED;
        git::Result r = run(info.name, args_of({"config", "--get", "--bool", "catapult.keep"}));
        info.kept = r.ok() && trim(r.out) == "true";
        result.push_back(info);
    }
    return result;
}

std::vector<Snapshot> SnapshotManager::list_snapshots(const std::string& world)
{
    std::vector<Snapshot> result;
    if (!has_commits(world))
        return result;

    // Commits end with NUL (-z), fields with 0x1f.
    git::Result r = run(world, args_of({"log", "-z", "--format=%H%x1f%ct%x1f%B"}));
    if (!r.ok())
    {
        status_.post("git log failed for " + world + ": " + trim(r.err), MSG_ERROR);
        return result;
    }

    std::vector<std::string> records = split_nul(r.out);
    for (size_t i = 0; i < records.size(); ++i)
    {
        const std::string& record = records[i];
        size_t first = record.find('\x1f');
        size_t second = (first == std::string::npos) ? first : record.find('\x1f', first + 1);
        if (second == std::string::npos)
            continue;

        Snapshot snap;
        snap.hash = record.substr(0, first);
        snap.time = std::atoll(record.substr(first + 1, second - first - 1).c_str());
        std::string message = record.substr(second + 1);
        size_t newline = message.find('\n');
        snap.name = trim(message.substr(0, newline));
        if (newline != std::string::npos)
            snap.description = trim(message.substr(newline + 1));
        result.push_back(snap);
    }
    return result;
}

int SnapshotManager::changed_files(const std::string& world, const std::string& hash)
{
    git::Result r = run(world, args_of({"diff-tree", "--no-commit-id", "-r", "--name-only", "-z", "--root", hash}));
    if (!r.ok())
        return -1;
    return (int)split_nul(r.out).size();
}

std::uint64_t SnapshotManager::history_size(const std::string& world)
{
    std::uint64_t total = 0;
    std::error_code ec;
    fs::recursive_directory_iterator it(repo_path(world), ec);
    for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        std::error_code item_ec;
        if (fs::is_regular_file(it->symlink_status(item_ec)))
        {
            std::uintmax_t size = it->file_size(item_ec);
            if (!item_ec)
                total += size;
        }
    }
    return total;
}

std::string SnapshotManager::git_command(const std::string& world)
{
    return "git " + git::shell_quote("--git-dir=" + repo_path(world)) + " "
           + git::shell_quote("--work-tree=" + world_dir(world));
}

bool SnapshotManager::can_write(std::string& reason)
{
    std::string dir = paths_.savegames();
    std::error_code ec;
    if (!fs::is_directory(dir, ec))
        dir = paths_.userdata();
    if (access(dir.c_str(), W_OK) == 0)
        return true;
    if (errno == EROFS)
        reason = "The userdata folder is read-only (" + paths_.userdata()
                 + "). On a Windows partition, this happens when Windows was hibernated or shut down with Fast Startup.";
    else
        reason = "Cannot write to " + dir + ".";
    return false;
}

SnapshotManager::Result SnapshotManager::take_snapshot(const std::string& world, const std::string& name,
                                                       const std::string& description)
{
    return take(world, name, description, true);
}

SnapshotManager::Result SnapshotManager::take(const std::string& world, const std::string& name,
                                              const std::string& description, bool report_unchanged)
{
    std::string title = trim(name);
    if (title.empty())
    {
        status_.post("A snapshot needs a name.", MSG_ERROR);
        return FAILED;
    }
    std::error_code ec;
    if (world.empty() || !fs::is_directory(world_dir(world), ec))
    {
        status_.post("World \"" + world + "\" not found.", MSG_ERROR);
        return FAILED;
    }
    std::string reason;
    if (!can_write(reason))
    {
        status_.post(reason, MSG_ERROR);
        return FAILED;
    }
    if (!ensure_repo(world))
        return FAILED;

    int staged = stage_all(world);
    if (staged < 0)
        return FAILED;
    if (staged == 0)
    {
        if (report_unchanged)
            status_.post(world + ": no changes since the last snapshot.");
        return NOTHING_TO_DO;
    }
    if (!commit(world, title, description))
        return FAILED;
    status_.post("Snapshot \"" + title + "\" of " + world + " taken.");
    return DONE;
}

SnapshotManager::AllResult SnapshotManager::take_snapshot_all(const std::string& name, const std::string& description)
{
    AllResult result;
    std::vector<std::string> worlds = Paths::list_dir(paths_.savegames());
    for (size_t i = 0; i < worlds.size(); ++i)
    {
        Result r = take(worlds[i], name, description, false);
        if (r == DONE)
            ++result.taken;
        else if (r == NOTHING_TO_DO)
            ++result.unchanged;
        else
            ++result.failed;
    }

    std::string text = std::to_string(result.taken) + (result.taken == 1 ? " snapshot" : " snapshots") + " taken";
    if (result.unchanged > 0)
        text += ", " + std::to_string(result.unchanged)
                + (result.unchanged == 1 ? " world had no changes" : " worlds had no changes");
    if (result.failed > 0)
        text += ", " + std::to_string(result.failed) + " failed";
    status_.post(text + ".", result.failed > 0 ? MSG_WARN : MSG_INFO);
    return result;
}

SnapshotManager::Result SnapshotManager::restore(const std::string& world, const std::string& hash)
{
    if (!has_commits(world))
    {
        status_.post(world + " has no snapshots.", MSG_ERROR);
        return FAILED;
    }
    git::Result found = run(world, args_of({"log", "-1", "--format=%H%x1f%s", hash + "^{commit}", "--"}));
    size_t sep = found.out.find('\x1f');
    if (!found.ok() || sep == std::string::npos)
    {
        status_.post("Snapshot " + hash + " of " + world + " not found.", MSG_ERROR);
        return FAILED;
    }
    std::string full_hash = found.out.substr(0, sep);
    std::string snap_name = trim(found.out.substr(sep + 1));
    std::string reason;
    if (!can_write(reason))
    {
        status_.post(reason, MSG_ERROR);
        return FAILED;
    }

    // A world that no longer exists gets its folder back. There is nothing in
    // it to keep, so no "Before restoring" commit for it.
    std::error_code ec;
    bool existed = fs::is_directory(world_dir(world), ec);
    if (!existed)
    {
        fs::create_directories(world_dir(world), ec);
        if (ec)
        {
            status_.post("Cannot create " + world_dir(world) + ": " + ec.message(), MSG_ERROR);
            return FAILED;
        }
    }

    int staged = stage_all(world);
    if (staged < 0)
        return FAILED;
    git::Result same = run(world, args_of({"diff", "--cached", "--quiet", full_hash, "--"}));
    if (same.started && same.exit_code == 0)
    {
        status_.post(world + " already matches the snapshot \"" + snap_name + "\". Nothing to restore.");
        return NOTHING_TO_DO;
    }
    if (staged == 1 && existed && !commit(world, "Before restoring \"" + snap_name + "\"", ""))
        return FAILED;

    status_.post("Restoring the snapshot \"" + snap_name + "\" of " + world + "...");
    if (!reset_to(world, full_hash))
        return FAILED;
    staged = stage_all(world);
    if (staged < 0)
        return FAILED;
    if (staged == 1 && !commit(world, "Restored \"" + snap_name + "\"", ""))
        return FAILED;
    status_.post("Snapshot \"" + snap_name + "\" of " + world + " restored.");
    return DONE;
}

SnapshotManager::Result SnapshotManager::discard_changes(const std::string& world)
{
    std::error_code ec;
    if (!has_commits(world) || !fs::is_directory(world_dir(world), ec))
    {
        status_.post(world + " has no snapshot to go back to.", MSG_ERROR);
        return FAILED;
    }
    std::string reason;
    if (!can_write(reason))
    {
        status_.post(reason, MSG_ERROR);
        return FAILED;
    }
    git::Result r = run(world, args_of({"status", "--porcelain", "-z"}));
    if (r.ok() && r.out.empty())
    {
        status_.post(world + " has no changes to discard.");
        return NOTHING_TO_DO;
    }
    if (!reset_to(world, "HEAD"))
        return FAILED;
    status_.post("Changes to " + world + " discarded.");
    return DONE;
}

bool SnapshotManager::mark_kept(const std::string& world)
{
    return run_or_report(world, args_of({"config", "catapult.keep", "true"}), "git config");
}

bool SnapshotManager::delete_snapshots(const std::string& world)
{
    std::string repo = repo_path(world);
    std::error_code ec;
    if (world.empty() || !fs::is_directory(repo, ec))
    {
        status_.post("No snapshots of \"" + world + "\" found.", MSG_ERROR);
        return false;
    }
    if (!trash_.remove(repo, "snapshots", world))
        return false;
    status_.post("Snapshots of " + world + " deleted.");
    return true;
}
