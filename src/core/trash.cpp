#include "core/trash.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>

#include "core/fs.h"
#include "core/helpers.h"

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

std::vector<TrashItem> Trash::list()
{
    std::vector<TrashItem> result;

    std::vector<std::string> ids = Paths::list_dir(paths_.trash_dir());
    for (size_t i = 0; i < ids.size(); ++i)
    {
        std::string entry = paths_.trash_dir() + "/" + ids[i];
        if (!exists_no_follow(entry + "/" + INFO_FILE) || !exists_no_follow(entry + "/" + ITEM_DIR))
            continue;
        nlohmann::json info;
        if (!load_json_file(entry + "/" + INFO_FILE, info, status_) || !info.is_object())
            continue;

        TrashItem item;
        item.id = ids[i];
        item.kind = info.value("kind", "");
        item.name = info.value("name", ids[i]);
        item.game = info.value("game", "");
        item.original_path = info.value("original_path", "");
        item.deleted_at = info.value("deleted_at", "");
        item.size = info.value("size", (std::uint64_t)0);
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
        if (id.empty() || id == "." || id == ".." || id.find('/') != std::string::npos || !fs::is_directory(entry))
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
