#include "core/paths.h"

#include <algorithm>
#include <filesystem>

#include "core/helpers.h"

namespace fs = std::filesystem;

#ifndef CATAPULT_DATA_DIR
#define CATAPULT_DATA_DIR ""
#endif

static const char* const GAMES[] = {"dda", "bn", "eod", "tish", "tlg"};

Paths::Paths(const std::string& own_dir, Settings& settings, Status& status)
    : own_dir_(own_dir), settings_(settings), status_(status)
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
        std::string base_dir = own_dir_ + "/" + game;
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
    return own_dir_ + "/" + settings_.get_string("game");
}

std::string Paths::own_dir() const { return own_dir_; }
std::string Paths::cache_dir() const { return own_dir_ + "/cache"; }
std::string Paths::utils_dir() const { return own_dir_ + "/utils"; }

std::string Paths::data_dir() const
{
    std::string local = own_dir_ + "/data";
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
std::string Paths::save_backups() { return game_base_dir() + "/save_backups"; }
