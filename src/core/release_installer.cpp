#include "core/release_installer.h"

#include <filesystem>

#include "core/archive.h"
#include "core/fs.h"
#include "core/helpers.h"

namespace fs = std::filesystem;

ReleaseInstaller::ReleaseInstaller(Settings& settings, Paths& paths, Status& status)
    : settings_(settings), paths_(paths), status_(status), http_(status)
{
}

void ReleaseInstaller::cancel()
{
    http_.cancel();
}

bool ReleaseInstaller::install_release(const Release& release, const std::string& update_in)
{
    if (!update_in.empty())
        status_.post("Updating to " + release.name + "...");
    else
        status_.post("Installing " + release.name + "...");

    std::string cache_dir = paths_.cache_dir();
    std::string archive_path = cache_dir + "/" + release.filename;
    std::error_code ec;

    if (settings_.get_bool("ignore_cache") || !fs::exists(archive_path))
    {
        http_.apply_proxy_settings(settings_, true);
        if (!http_.download_file(release.url, cache_dir, release.filename))
            return false;
    }

    // Always extract into an empty directory, so that the one directory inside it is ours.
    std::string tmp_dir = paths_.tmp_dir();
    fs::remove_all(tmp_dir, ec);

    bool extracted = archive_helper::extract(archive_path, tmp_dir, status_);
    if (!settings_.get_bool("keep_cache"))
        fs::remove(archive_path, ec);
    if (!extracted)
    {
        fs::remove_all(tmp_dir, ec);
        return false;
    }

    std::vector<std::string> extracted_items = Paths::list_dir(tmp_dir);
    if (extracted_items.empty())
    {
        status_.post("The archive did not contain a game directory.", MSG_ERROR);
        return false;
    }
    std::string extracted_root = tmp_dir + "/" + extracted_items[0];

    if (!create_info_file(extracted_root, release.name, status_))
        return false;

    std::string target_dir;
    if (!update_in.empty())
    {
        target_dir = update_in;
        if (!fs_helper::rm_dir(target_dir, status_))
            return false;
    }
    else
    {
        target_dir = paths_.next_install_dir();
    }

    if (!fs_helper::move_dir(extracted_root, target_dir, status_))
        return false;

    if (!update_in.empty())
    {
        settings_.set_string("active_install_" + settings_.get_string("game"), release.name);
        status_.post("Update finished.");
    }
    else
    {
        status_.post("Installation finished.");
    }
    settings_.save();
    return true;
}

bool ReleaseInstaller::remove_release_by_name(const std::string& release_name)
{
    Paths::InstallsSummary installs = paths_.installs_summary();
    std::string game = settings_.get_string("game");

    Paths::InstallsSummary::const_iterator g = installs.find(game);
    if (g == installs.end() || g->second.find(release_name) == g->second.end())
    {
        status_.post("Attempted to remove release \"" + release_name + "\", but could not find it on disk.", MSG_ERROR);
        return false;
    }

    status_.post("Removing " + release_name + "...");
    if (!fs_helper::rm_dir(g->second.find(release_name)->second, status_))
        return false;
    status_.post("Removal finished.");
    return true;
}

std::vector<std::string> ReleaseInstaller::check_game_dir_for_userdata(const std::string& release_name)
{
    std::vector<std::string> result;

    Paths::InstallsSummary installs = paths_.installs_summary();
    Paths::InstallsSummary::const_iterator g = installs.find(settings_.get_string("game"));
    if (g == installs.end() || g->second.find(release_name) == g->second.end())
        return result;

    std::string game_path = g->second.find(release_name)->second;
    const char* const subdirs[] = {"config", "font", "graveyard", "memorial", "mods", "save", "sound", "templates"};
    for (size_t i = 0; i < sizeof(subdirs) / sizeof(subdirs[0]); ++i)
    {
        std::string full = game_path + "/" + subdirs[i];
        if (fs::is_directory(full) && !fs::is_empty(full))
            result.push_back(full);
    }
    return result;
}
