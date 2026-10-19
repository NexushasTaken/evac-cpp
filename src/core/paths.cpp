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

std::string Paths::userdata() { return game_base_dir() + "/userdata"; }
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
    std::string new_dir = save_backups();
    std::error_code ec;
    if (!fs::is_directory(old_dir, ec) || fs::exists(fs::symlink_status(new_dir, ec)))
        return true;

    status_.post("Moving the backups from " + old_dir + " to " + new_dir + "...");
    fs::create_directories(launcher_dir(), ec);
    if (ec)
    {
        status_.post("Cannot create " + launcher_dir() + ": " + ec.message(), MSG_ERROR);
        return false;
    }
    return fs_helper::move_dir(old_dir, new_dir, status_);
}
