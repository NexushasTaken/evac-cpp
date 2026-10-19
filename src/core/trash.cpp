#include "core/trash.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>

#include "core/fs.h"
#include "core/helpers.h"
#include "core/mod_manager.h"

namespace fs = std::filesystem;

static const char INFO_FILE[] = "info.json";
static const char ITEM_DIR[] = "item";

Trash::Trash(Settings& settings, Paths& paths, Status& status) : settings_(settings), paths_(paths), status_(status)
{
}

// True if path is dir itself or somewhere inside it.
static bool is_within(const fs::path& path, const fs::path& dir)
{
    fs::path rel = path.lexically_relative(dir);
    return !rel.empty() && *rel.begin() != "..";
}

static bool exists_no_follow(const fs::path& path)
{
    std::error_code ec;
    return fs::exists(fs::symlink_status(path, ec));
}

// Bytes in all the regular files under path. Symlinks are not followed.
static std::uint64_t measure(const fs::path& path)
{
    std::error_code ec;
    fs::file_status st = fs::symlink_status(path, ec);
    if (fs::is_regular_file(st))
        return fs::file_size(path, ec);
    if (!fs::is_directory(st))
        return 0;

    std::uint64_t total = 0;
    fs::recursive_directory_iterator it(path, ec);
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

static std::tm local_time(Trash::Clock::time_point when)
{
    std::time_t t = Trash::Clock::to_time_t(when);
    std::tm tm = {};
    localtime_r(&t, &tm);
    return tm;
}

// An id from list(), and nothing that points outside the trash.
static bool is_valid_id(const std::string& id)
{
    return !id.empty() && id != "." && id != ".." && id.find('/') == std::string::npos;
}

// path if it is free, else path + " (restored)", " (restored 2)", ...
static std::string free_path(const std::string& path)
{
    if (!exists_no_follow(path))
        return path;
    std::string candidate = path + " (restored)";
    for (int n = 2; exists_no_follow(candidate); ++n)
        candidate = path + " (restored " + std::to_string(n) + ")";
    return candidate;
}

// The first free gameN in a game's directory, like Paths::next_install_dir.
static std::string free_install_dir(const std::string& base_dir)
{
    int n = 0;
    while (exists_no_follow(base_dir + "/game" + std::to_string(n)))
        ++n;
    return base_dir + "/game" + std::to_string(n);
}

// The install of that release in a game's directory, or "". Like
// Paths::installs_summary, but without its side effect on the settings.
static std::string find_install(const std::string& base_dir, const std::string& release_name, Status& status)
{
    std::vector<std::string> subdirs = Paths::list_dir(base_dir);
    for (size_t i = 0; i < subdirs.size(); ++i)
    {
        std::string info_file = base_dir + "/" + subdirs[i] + "/" + INFO_FILENAME;
        nlohmann::json info;
        if (fs::exists(info_file) && load_json_file(info_file, info, status) && info.is_object()
            && info.contains("name") && info["name"] == release_name)
            return base_dir + "/" + subdirs[i];
    }
    return "";
}

// A directory with move_dir, anything else (a file, a symlink) with a rename.
static bool move_any(const std::string& src, const std::string& dest, Status& status)
{
    std::error_code ec;
    fs::file_status st = fs::symlink_status(src, ec);
    if (fs::is_directory(st))
        return fs_helper::move_dir(src, dest, status);
    fs::rename(src, dest, ec);
    if (ec)
    {
        status.post("Failed to move " + src + " to " + dest + ": " + ec.message(), MSG_ERROR);
        return false;
    }
    return true;
}

static int millis(Trash::Clock::time_point when)
{
    return (int)(std::chrono::duration_cast<std::chrono::milliseconds>(when.time_since_epoch()).count() % 1000);
}

std::string Trash::make_id(const std::string& kind, const std::string& name, Clock::time_point when)
{
    std::tm tm = local_time(when);
    char stamp[32];
    std::snprintf(stamp, sizeof(stamp), "%04d%02d%02d-%02d%02d%02d-%03d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec, millis(when));

    // Only safe characters, and not too long: the id is a directory name.
    std::string safe_name;
    for (size_t i = 0; i < name.size() && safe_name.size() < 40; ++i)
    {
        char c = name[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.';
        safe_name += ok ? c : '_';
    }
    return std::string(stamp) + "_" + kind + "_" + safe_name;
}

bool Trash::remove(const std::string& path, const std::string& kind, const std::string& name)
{
    return remove(path, kind, name, Clock::now());
}

bool Trash::remove(const std::string& path, const std::string& kind, const std::string& name, Clock::time_point when)
{
    std::error_code ec;
    if (!exists_no_follow(path))
    {
        status_.post("Nothing to delete at " + path, MSG_ERROR);
        return false;
    }

    if (!settings_.get_bool("use_trash"))
    {
        status_.post("Deleting " + path);
        return fs_helper::rm_dir(path, status_);
    }

    // Moving the trash into itself, or a directory that holds the trash, can't work.
    fs::path trash_dir = fs::weakly_canonical(paths_.trash_dir(), ec);
    fs::path target = fs::weakly_canonical(path, ec);
    if (is_within(target, trash_dir) || is_within(trash_dir, target))
    {
        status_.post("Refusing to move " + path + " to the trash: it is or contains the trash.", MSG_ERROR);
        return false;
    }

    std::string base_id = make_id(kind, name, when);
    std::string id = base_id;
    for (int n = 2; exists_no_follow(paths_.trash_dir() + "/" + id); ++n)
        id = base_id + "-" + std::to_string(n);
    std::string entry = paths_.trash_dir() + "/" + id;

    std::tm tm = local_time(when);
    char deleted_at[40];
    std::snprintf(deleted_at, sizeof(deleted_at), "%04d-%02d-%02d %02d:%02d:%02d.%03d", tm.tm_year + 1900,
                  tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, millis(when));

    nlohmann::json info;
    info["kind"] = kind;
    info["name"] = name;
    info["game"] = settings_.get_string("game");
    info["original_path"] = fs::absolute(path, ec).string();
    info["deleted_at"] = deleted_at;
    info["size"] = measure(path);

    status_.post("Moving " + name + " to the trash...");
    fs::create_directories(entry, ec);
    if (ec)
    {
        status_.post("Failed to create " + entry + ": " + ec.message(), MSG_ERROR);
        return false;
    }
    if (!save_json_file(info, entry + "/" + INFO_FILE, status_))
    {
        fs::remove_all(entry, ec);
        return false;
    }

    if (!fs_helper::move_dir(path, entry + "/" + ITEM_DIR, status_))
    {
        // Across filesystems move_dir goes file by file, so part of it may be in
        // the trash already. Only throw the entry away if nothing got there.
        if (!exists_no_follow(entry + "/" + ITEM_DIR))
            fs::remove_all(entry, ec);
        else
            status_.post("Part of " + name + " was moved to the trash. It is kept there, in " + entry, MSG_WARN);
        return false;
    }
    return true;
}

bool Trash::load_item(const std::string& id, TrashItem& out)
{
    if (!is_valid_id(id))
        return false;
    std::string entry = paths_.trash_dir() + "/" + id;
    if (!exists_no_follow(entry + "/" + INFO_FILE) || !exists_no_follow(entry + "/" + ITEM_DIR))
        return false;
    nlohmann::json info;
    if (!load_json_file(entry + "/" + INFO_FILE, info, status_) || !info.is_object())
        return false;

    out = TrashItem();
    out.id = id;
    out.kind = info.value("kind", "");
    out.name = info.value("name", id);
    out.game = info.value("game", "");
    out.original_path = info.value("original_path", "");
    out.deleted_at = info.value("deleted_at", "");
    out.size = info.value("size", (std::uint64_t)0);
    return true;
}

std::vector<TrashItem> Trash::list()
{
    std::vector<TrashItem> result;

    std::vector<std::string> ids = Paths::list_dir(paths_.trash_dir());
    for (size_t i = 0; i < ids.size(); ++i)
    {
        TrashItem item;
        if (load_item(ids[i], item))
            result.push_back(item);
    }

    std::sort(result.begin(), result.end(), [](const TrashItem& a, const TrashItem& b) {
        if (a.deleted_at != b.deleted_at)
            return a.deleted_at > b.deleted_at;
        return a.id > b.id;
    });
    return result;
}

std::uint64_t Trash::total_size()
{
    std::vector<TrashItem> items = list();
    std::uint64_t total = 0;
    for (size_t i = 0; i < items.size(); ++i)
        total += items[i].size;
    return total;
}

bool Trash::delete_items(const std::vector<std::string>& ids)
{
    bool ok = true;
    for (size_t i = 0; i < ids.size(); ++i)
    {
        // Ids come from list(), but never let one point outside the trash.
        const std::string& id = ids[i];
        std::string entry = paths_.trash_dir() + "/" + id;
        if (!is_valid_id(id) || !fs::is_directory(entry))
        {
            status_.post("Not in the trash: \"" + id + "\"", MSG_ERROR);
            ok = false;
            continue;
        }
        if (!fs_helper::rm_dir(entry, status_))
            ok = false;
    }
    return ok;
}

bool Trash::empty()
{
    status_.post("Emptying the trash...");
    std::vector<std::string> entries;
    std::error_code ec;
    fs::directory_iterator it(paths_.trash_dir(), ec);
    if (ec)
        return true;  // No trash directory: nothing to do.
    for (; !ec && it != fs::directory_iterator(); it.increment(ec))
        entries.push_back(it->path().string());

    bool ok = true;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (!fs_helper::rm_dir(entries[i], status_))
            ok = false;
    }
    return ok;
}

std::vector<TrashItem> Trash::over_limit()
{
    std::vector<TrashItem> items = list();  // Newest first.
    std::uint64_t limit = (std::uint64_t)std::max(0, settings_.get_int("trash_max_size_mb")) * 1024 * 1024;
    std::uint64_t total = 0;
    for (size_t i = 0; i < items.size(); ++i)
        total += items[i].size;

    std::vector<TrashItem> result;
    for (size_t i = items.size(); i > 1 && total > limit; --i)  // Stops before items[0], the newest.
    {
        result.push_back(items[i - 1]);
        total -= items[i - 1].size;
    }
    return result;
}

RestorePlan Trash::plan_restore(const std::string& id)
{
    RestorePlan plan;
    TrashItem item;
    if (!load_item(id, item))
    {
        plan.reason = "It is not in the trash.";
        return plan;
    }
    std::string entry = paths_.trash_dir() + "/" + id;
    std::string item_dir = entry + "/" + ITEM_DIR;

    std::error_code ec;
    fs::path original(item.original_path);
    if (!original.is_absolute())
    {
        plan.reason = "Where it came from is not known.";
        return plan;
    }
    if (is_within(fs::weakly_canonical(original, ec), fs::weakly_canonical(paths_.trash_dir(), ec)))
    {
        plan.reason = "It would be restored into the trash.";
        return plan;
    }
    std::string parent = original.parent_path().string();
    if (!fs::is_directory(parent))
    {
        plan.reason = "The folder it was in (" + parent + ") no longer exists.";
        return plan;
    }

    if (item.kind == "mod")
    {
        // The game tells mods apart by ID, not by directory.
        std::map<std::string, ModEntry> mine = ModManager::parse_mods_dir(entry, status_);
        if (!mine.empty())
        {
            std::map<std::string, ModEntry> there = ModManager::parse_mods_dir(parent, status_);
            std::map<std::string, ModEntry>::const_iterator same = there.find(mine.begin()->first);
            if (same != there.end())
            {
                plan.kind = RestorePlan::CONFLICT;
                plan.conflict_path = same->second.location;
                plan.conflict_name = same->second.name;
                plan.reason = "A mod with the same ID (" + same->first + ") is installed: " + same->second.name + ".";
                return plan;
            }
        }
    }
    else if (item.kind == "install")
    {
        std::string same = find_install(parent, item.name, status_);
        if (!same.empty())
        {
            plan.kind = RestorePlan::CONFLICT;
            plan.conflict_path = same;
            plan.conflict_name = item.name;
            plan.reason = "\"" + item.name + "\" is already installed.";
            return plan;
        }
        plan.dest = exists_no_follow(original) ? free_install_dir(parent) : original.string();
        plan.kind = (plan.dest == original.string()) ? RestorePlan::ORIGINAL : RestorePlan::RENAMED;
        return plan;
    }
    else if (item.kind == "saves")
    {
        // The game only looks in one save directory, so the worlds go back into it.
        plan.dest = original.string();
        plan.kind = RestorePlan::ORIGINAL;
        if (exists_no_follow(original))
        {
            std::vector<std::string> worlds = Paths::list_dir(item_dir);
            for (size_t i = 0; i < worlds.size(); ++i)
            {
                if (exists_no_follow(plan.dest + "/" + worlds[i]))
                    plan.kind = RestorePlan::RENAMED;
            }
        }
        return plan;
    }
    else if (item.kind == "snapshots" && exists_no_follow(original))
    {
        // Renamed, it would no longer be <World>.git and the Snapshots tab
        // would not see it. Replacing keeps the newer history in the trash.
        plan.kind = RestorePlan::CONFLICT;
        plan.conflict_path = original.string();
        plan.conflict_name = item.name;
        plan.reason = "There are newer snapshots of " + item.name + " (for example after Start fresh).";
        return plan;
    }

    plan.dest = free_path(original.string());
    plan.kind = (plan.dest == original.string()) ? RestorePlan::ORIGINAL : RestorePlan::RENAMED;
    return plan;
}

bool Trash::restore(const std::string& id, bool replace)
{
    RestorePlan plan = plan_restore(id);
    TrashItem item;
    if (plan.kind == RestorePlan::IMPOSSIBLE || !load_item(id, item))
    {
        status_.post("Can't restore: " + plan.reason, MSG_ERROR);
        return false;
    }
    if (plan.kind == RestorePlan::CONFLICT)
    {
        if (!replace)
        {
            status_.post("Not restored: " + plan.reason, MSG_ERROR);
            return false;
        }
        if (!remove(plan.conflict_path, item.kind, plan.conflict_name))
            return false;
        plan = plan_restore(id);
        if (plan.kind == RestorePlan::CONFLICT || plan.kind == RestorePlan::IMPOSSIBLE)
        {
            status_.post("Can't restore: " + plan.reason, MSG_ERROR);
            return false;
        }
    }

    std::string entry = paths_.trash_dir() + "/" + id;
    std::string item_dir = entry + "/" + ITEM_DIR;
    status_.post("Restoring " + item.name + "...");

    bool ok = true;
    if (item.kind == "saves" && exists_no_follow(plan.dest))
    {
        // Into the current saves, world by world (files too, if there are any).
        std::vector<std::string> names;
        std::error_code ec;
        for (fs::directory_iterator it(item_dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
            names.push_back(it->path().filename().string());
        for (size_t i = 0; i < names.size(); ++i)
        {
            std::string target = free_path(plan.dest + "/" + names[i]);
            if (target != plan.dest + "/" + names[i])
                status_.post(names[i] + " is restored as " + fs::path(target).filename().string());
            if (!move_any(item_dir + "/" + names[i], target, status_))
                ok = false;
        }
    }
    else
    {
        ok = move_any(item_dir, plan.dest, status_);
    }

    if (!ok)
    {
        status_.post("Not everything of " + item.name + " could be restored. The rest is still in the trash.", MSG_WARN);
        return false;
    }
    fs_helper::rm_dir(entry, status_);
    status_.post("Restored " + item.name + " to " + plan.dest);
    return true;
}
