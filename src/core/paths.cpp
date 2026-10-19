#include "core/paths.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>

#include "core/fs.h"
#include "core/helpers.h"

namespace fs = std::filesystem;

#ifndef CATAPULT_DATA_DIR
#define CATAPULT_DATA_DIR ""
#endif

static const char* const GAMES[] = {"dda", "bn", "eod", "tish", "tlg"};

static std::string env_or_empty(const char* name)
{
    const char* value = std::getenv(name);
    return value ? value : "";
}

Locations portable_locations(const std::string& dir)
{
    Locations loc;
    loc.config_dir = dir;
    loc.games_dir = dir;
    loc.cache_dir = dir + "/cache";
    return loc;
}

// The spec says to ignore values that are not absolute paths.
static std::string xdg_base(const std::string& value, const std::string& fallback)
{
    if (!value.empty() && value[0] == '/')
        return value;
    return fallback;
}

Locations xdg_locations(const std::string& home, const std::string& config_home,
                        const std::string& data_home, const std::string& cache_home)
{
    Locations loc;
    loc.config_dir = xdg_base(config_home, home + "/.config") + "/catapult";
    loc.games_dir = xdg_base(data_home, home + "/.local/share") + "/catapult";
    loc.cache_dir = xdg_base(cache_home, home + "/.cache") + "/catapult";
    return loc;
}

Locations default_locations()
{
    std::string exe_dir = Paths::executable_dir();
    if (fs::exists(exe_dir + "/catapult_settings.json"))
        return portable_locations(exe_dir);

    std::string home = env_or_empty("HOME");
    if (home.empty())
        return portable_locations(exe_dir);
    return xdg_locations(home, env_or_empty("XDG_CONFIG_HOME"), env_or_empty("XDG_DATA_HOME"),
                         env_or_empty("XDG_CACHE_HOME"));
}

Paths::Paths(const Locations& locations, Settings& settings, Status& status)
    : games_dir_(locations.games_dir), cache_dir_(locations.cache_dir), settings_(settings), status_(status)
{
}

Paths::Paths(const std::string& root, Settings& settings, Status& status)
    : games_dir_(root), cache_dir_(root + "/cache"), settings_(settings), status_(status)
{
}

std::string Paths::executable_dir()
{
    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (ec)
        return fs::current_path().string();
    return exe.parent_path().string();
}

std::vector<std::string> Paths::list_dir(const std::string& dir)
{
    std::vector<std::string> result;
    std::error_code ec;
    fs::directory_iterator it(dir, ec);
    if (ec)
        return result;
    for (; it != fs::directory_iterator(); it.increment(ec))
    {
        if (it->is_directory())
            result.push_back(it->path().filename().string());
    }
    std::sort(result.begin(), result.end());
    return result;
}

Paths::InstallsSummary Paths::installs_summary()
{
    InstallsSummary result;

    for (size_t g = 0; g < sizeof(GAMES) / sizeof(GAMES[0]); ++g)
    {
        std::string game = GAMES[g];
        std::string base_dir = games_dir_ + "/" + game;
        std::map<std::string, std::string> installs;

        std::vector<std::string> subdirs = list_dir(base_dir);
        for (size_t i = 0; i < subdirs.size(); ++i)
        {
            std::string dir = base_dir + "/" + subdirs[i];
            std::string info_file = dir + "/" + INFO_FILENAME;
            nlohmann::json info;
            if (fs::exists(info_file) && load_json_file(info_file, info, status_)
                && info.is_object() && info.contains("name") && info["name"].is_string())
            {
                installs[info["name"].get<std::string>()] = dir;
            }
        }
        if (!installs.empty())
            result[game] = installs;
    }

    // Ensure that some installation of the game is set as active.
    std::string game = settings_.get_string("game");
    InstallsSummary::const_iterator it = result.find(game);
    if (it != result.end())
    {
        std::string active = settings_.get_string("active_install_" + game);
        if (active.empty() || it->second.find(active) == it->second.end())
            settings_.set_string("active_install_" + game, it->second.begin()->first);
    }

    return result;
}

std::string Paths::game_base_dir()
{
    return games_dir_ + "/" + settings_.get_string("game");
}

std::string Paths::cache_dir() const { return cache_dir_; }

std::string Paths::data_dir() const
{
    std::string local = executable_dir() + "/data";
    if (fs::is_directory(local))
        return local;
    return CATAPULT_DATA_DIR;
}

std::string Paths::game_dir()
{
    std::string active = settings_.get_string("active_install_" + settings_.get_string("game"));
    if (active.empty())
        return next_install_dir();
    return find_active_game_dir();
}

std::string Paths::find_active_game_dir()
{
    std::string active = settings_.get_string("active_install_" + settings_.get_string("game"));
    std::string base_dir = game_base_dir();

    std::vector<std::string> subdirs = list_dir(base_dir);
    for (size_t i = 0; i < subdirs.size(); ++i)
    {
        std::string dir = base_dir + "/" + subdirs[i];
        std::string info_file = dir + "/" + INFO_FILENAME;
        nlohmann::json info;
        if (fs::exists(info_file) && load_json_file(info_file, info, status_)
            && info.is_object() && info.contains("name") && info["name"] == active)
        {
            return dir;
        }
    }
    return "";
}

// Names for new installs follow the pattern "game0", "game1", ...
std::string Paths::next_install_dir()
{
    std::string base_dir = game_base_dir();
    int n = 0;
    while (fs::exists(base_dir + "/game" + std::to_string(n)))
        ++n;
    return base_dir + "/game" + std::to_string(n);
}

std::string Paths::default_userdata() { return game_base_dir() + "/userdata"; }

// The active one must still be in the list, a stale value means the default.
std::string Paths::userdata()
{
    std::string active = settings_.get_string("active_userdata_" + settings_.get_string("game"));
    if (!active.empty())
    {
        std::vector<UserdataFolder> folders = custom_userdata();
        for (size_t i = 0; i < folders.size(); ++i)
        {
            if (folders[i].path == active)
                return active;
        }
    }
    return default_userdata();
}

std::vector<UserdataFolder> Paths::custom_userdata()
{
    std::vector<UserdataFolder> result;
    nlohmann::json list = settings_.get_json("userdata_folders_" + settings_.get_string("game"));
    if (!list.is_array())
        return result;
    for (size_t i = 0; i < list.size(); ++i)
    {
        const nlohmann::json& item = list[i];
        if (!item.is_object() || !item.contains("path") || !item["path"].is_string())
            continue;
        UserdataFolder folder;
        folder.path = item["path"].get<std::string>();
        if (item.contains("label") && item["label"].is_string())
            folder.label = item["label"].get<std::string>();
        if (folder.label.empty())
            folder.label = fs::path(folder.path).filename().string();
        result.push_back(folder);
    }
    return result;
}

void Paths::store_custom_userdata(const std::vector<UserdataFolder>& folders)
{
    nlohmann::json list = nlohmann::json::array();
    for (size_t i = 0; i < folders.size(); ++i)
        list.push_back({{"path", folders[i].path}, {"label", folders[i].label}});
    settings_.set_json("userdata_folders_" + settings_.get_string("game"), list);
}

static std::string clean_path(const std::string& path)
{
    std::error_code ec;
    fs::path p = fs::absolute(path, ec);
    if (ec)
        p = path;
    std::string s = p.lexically_normal().string();
    while (s.size() > 1 && s.back() == '/')
        s.pop_back();
    return s;
}

std::string Paths::check_userdata_label(const std::string& label, const std::string& own_path)
{
    if (label.find_first_not_of(" \t") == std::string::npos)
        return "The name can't be empty.";
    if (label == "Default")
        return "\"Default\" is the name of the default user data folder.";
    std::vector<UserdataFolder> folders = custom_userdata();
    for (size_t i = 0; i < folders.size(); ++i)
    {
        if (folders[i].label == label && folders[i].path != own_path)
            return "Another user data folder is already called \"" + label + "\".";
    }
    return "";
}

std::string Paths::add_userdata(const std::string& path, const std::string& label)
{
    if (path.empty())
        return "No folder given.";
    std::string clean = clean_path(path);
    if (clean == clean_path(default_userdata()))
        return "This is the default user data folder, it is always in the list.";
    std::vector<UserdataFolder> folders = custom_userdata();
    for (size_t i = 0; i < folders.size(); ++i)
    {
        if (folders[i].path == clean)
            return "This folder is already in the list as \"" + folders[i].label + "\".";
    }
    std::string problem = check_userdata_label(label);
    if (!problem.empty())
        return problem;

    UserdataFolder folder;
    folder.path = clean;
    folder.label = label;
    folders.push_back(folder);
    store_custom_userdata(folders);
    return "";
}

std::string Paths::rename_userdata(const std::string& path, const std::string& label)
{
    std::vector<UserdataFolder> folders = custom_userdata();
    for (size_t i = 0; i < folders.size(); ++i)
    {
        if (folders[i].path != path)
            continue;
        std::string problem = check_userdata_label(label, path);
        if (!problem.empty())
            return problem;
        folders[i].label = label;
        store_custom_userdata(folders);
        return "";
    }
    return "This user data folder is not in the list.";
}

void Paths::remove_userdata(const std::string& path)
{
    std::vector<UserdataFolder> folders = custom_userdata();
    std::vector<UserdataFolder> kept;
    for (size_t i = 0; i < folders.size(); ++i)
    {
        if (folders[i].path != path)
            kept.push_back(folders[i]);
    }
    store_custom_userdata(kept);
    std::string key = "active_userdata_" + settings_.get_string("game");
    if (settings_.get_string(key) == path)
        settings_.set_string(key, "");
}

void Paths::set_active_userdata(const std::string& path)
{
    std::string key = "active_userdata_" + settings_.get_string("game");
    settings_.set_string(key, path == default_userdata() ? "" : path);
}

bool Paths::is_default_userdata_active() { return userdata() == default_userdata(); }

std::string Paths::config() { return userdata() + "/config"; }
std::string Paths::savegames() { return userdata() + "/save"; }
std::string Paths::mods_stock() { return game_dir() + "/data/mods"; }
std::string Paths::mods_user() { return userdata() + "/mods"; }
std::string Paths::sound_stock() { return game_dir() + "/data/sound"; }
std::string Paths::sound_user() { return userdata() + "/sound"; }
std::string Paths::gfx_default() { return game_dir() + "/gfx"; }
std::string Paths::gfx_user() { return userdata() + "/gfx"; }
std::string Paths::font_user() { return userdata() + "/font"; }
std::string Paths::templates() { return userdata() + "/templates"; }
std::string Paths::memorial() { return userdata() + "/memorial"; }
std::string Paths::graveyard() { return userdata() + "/graveyard"; }
std::string Paths::mod_repo() { return game_base_dir() + "/mod_repo"; }
std::string Paths::tmp_dir() { return game_base_dir() + "/tmp"; }
std::string Paths::launcher_dir() { return userdata() + "/catapult"; }
std::string Paths::save_backups() { return launcher_dir() + "/backups"; }
std::string Paths::snapshots() { return launcher_dir() + "/snapshots"; }
std::string Paths::trash_dir() const { return games_dir_ + "/trash"; }

bool Paths::move_old_backups()
{
    std::string old_dir = game_base_dir() + "/save_backups";
    std::string parent_dir = default_userdata() + "/catapult";
    std::string new_dir = parent_dir + "/backups";
    std::error_code ec;
    if (!fs::is_directory(old_dir, ec) || fs::exists(fs::symlink_status(new_dir, ec)))
        return true;

    status_.post("Moving the backups from " + old_dir + " to " + new_dir + "...");
    fs::create_directories(parent_dir, ec);
    if (ec)
    {
        status_.post("Cannot create " + parent_dir + ": " + ec.message(), MSG_ERROR);
        return false;
    }
    return fs_helper::move_dir(old_dir, new_dir, status_);
}
