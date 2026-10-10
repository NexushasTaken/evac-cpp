#include "core/fs.h"

#include <algorithm>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

namespace fs_helper
{

std::vector<std::string> list_dir(const std::string& path, bool recursive, const std::string& filter_regex,
                                  Status& status)
{
    std::vector<std::string> result;

    std::regex filter;
    bool use_filter = !filter_regex.empty();
    if (use_filter)
    {
        try
        {
            filter = std::regex(filter_regex);
        }
        catch (const std::regex_error&)
        {
            status.post("Failed to list directory " + path + ": invalid filter", MSG_ERROR);
            return result;
        }
    }

    std::error_code ec;
    fs::path base(path);
    if (!fs::is_directory(base, ec))
    {
        status.post("Failed to list directory: " + path, MSG_ERROR);
        return result;
    }

    // Symlinks are listed but never followed.
    fs::recursive_directory_iterator it(base, ec);
    if (ec)
    {
        status.post("Failed to list directory " + path + ": " + ec.message(), MSG_ERROR);
        return result;
    }
    for (; it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec)
            break;
        std::string rel = fs::relative(it->path(), base).generic_string();
        if (!use_filter || std::regex_search(rel, filter))
            result.push_back(rel);
        if (!recursive)
            it.disable_recursion_pending();
    }
    std::sort(result.begin(), result.end());
    return result;
}

static bool copy_dir_internal(const fs::path& src, const fs::path& dest, Status& status)
{
    bool ok = true;
    std::error_code ec;

    fs::create_directories(dest, ec);
    if (ec)
    {
        status.post("Cannot create target directory " + dest.string() + ": " + ec.message(), MSG_ERROR);
        return false;
    }

    fs::directory_iterator it(src, ec);
    if (ec)
    {
        status.post("Failed to list directory " + src.string() + ": " + ec.message(), MSG_ERROR);
        return false;
    }
    for (; it != fs::directory_iterator(); it.increment(ec))
    {
        fs::path target = dest / it->path().filename();
        if (it->is_symlink())
        {
            fs::copy_symlink(it->path(), target, ec);
        }
        else if (it->is_directory())
        {
            if (!copy_dir_internal(it->path(), target, status))
                ok = false;
            continue;
        }
        else
        {
            fs::copy_file(it->path(), target, fs::copy_options::overwrite_existing, ec);
        }
        if (ec)
        {
            status.post("Failed to copy " + it->path().filename().string() + ": " + ec.message(), MSG_ERROR);
            ok = false;
            ec.clear();
        }
    }
    return ok;
}

bool copy_dir(const std::string& path, const std::string& dest_dir, Status& status)
{
    fs::path src(path);
    if (src.filename().empty())
        src = src.parent_path();  // Handle a trailing slash.
    return copy_dir_internal(src, fs::path(dest_dir) / src.filename(), status);
}

bool rm_dir(const std::string& path, Status& status)
{
    std::error_code ec;
    fs::remove_all(path, ec);
    if (ec)
    {
        status.post("Failed to remove directory " + path + ": " + ec.message(), MSG_ERROR);
        return false;
    }
    return true;
}

static bool move_dir_internal(const fs::path& src, const fs::path& dest, Status& status)
{
    bool ok = true;
    std::error_code ec;

    // Fast path: a plain rename when the destination is free.
    if (!fs::exists(fs::symlink_status(dest, ec)))
    {
        fs::create_directories(dest.parent_path(), ec);
        ec.clear();
        fs::rename(src, dest, ec);
        if (!ec)
            return true;
        ec.clear();  // E.g. a different filesystem. Fall through to item by item.
    }

    fs::create_directories(dest, ec);
    if (ec)
    {
        status.post("Failed to create directory " + dest.string() + ": " + ec.message(), MSG_ERROR);
        return false;
    }

    fs::directory_iterator it(src, ec);
    for (; !ec && it != fs::directory_iterator(); it.increment(ec))
    {
        fs::path target = dest / it->path().filename();
        if (it->is_directory() && !it->is_symlink())
        {
            if (!move_dir_internal(it->path(), target, status))
                ok = false;
            continue;
        }

        std::error_code move_ec;
        fs::rename(it->path(), target, move_ec);
        if (move_ec)
        {
            // Cross-device: copy, then remove the original.
            if (it->is_symlink())
                fs::copy_symlink(it->path(), target, move_ec);
            else
                fs::copy_file(it->path(), target, fs::copy_options::overwrite_existing, move_ec);
            if (!move_ec)
                fs::remove(it->path(), move_ec);
        }
        if (move_ec)
        {
            status.post("Failed to move " + it->path().filename().string() + ": " + move_ec.message(), MSG_ERROR);
            ok = false;
        }
    }

    if (ok)
    {
        fs::remove(src, ec);
        if (ec)
        {
            status.post("Failed to remove directory after move: " + src.string() + ": " + ec.message(), MSG_ERROR);
            ok = false;
        }
    }
    return ok;
}

bool move_dir(const std::string& path, const std::string& dest, Status& status)
{
    return move_dir_internal(fs::path(path), fs::path(dest), status);
}

}  // namespace fs_helper
