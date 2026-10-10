#ifndef CATAPULT_PATHS_H
#define CATAPULT_PATHS_H

#include <map>
#include <string>
#include <vector>

#include "core/settings.h"
#include "core/status.h"

// The one place for all path resolution logic. Everything lives under own_dir
// (the directory of the executable, so the launcher stays portable), in a
// subdirectory per game ("dda", "bn", ...).
class Paths
{
public:
    // game name -> (install name -> install directory)
    typedef std::map<std::string, std::map<std::string, std::string> > InstallsSummary;

    Paths(const std::string& own_dir, Settings& settings, Status& status);

    static std::string executable_dir();
    static std::vector<std::string> list_dir(const std::string& dir);  // Subdirectories, sorted.

    InstallsSummary installs_summary();

    std::string own_dir() const;
    std::string cache_dir() const;
    std::string game_dir();
    std::string next_install_dir();
    std::string userdata();
    std::string config();
    std::string savegames();
    std::string mods_stock();
    std::string mods_user();
    std::string sound_stock();
    std::string sound_user();
    std::string gfx_default();
    std::string gfx_user();
    std::string font_user();
    std::string templates();
    std::string memorial();
    std::string graveyard();
    std::string mod_repo();
    std::string tmp_dir();
    std::string utils_dir() const;
    std::string save_backups();

private:
    std::string game_base_dir();  // own_dir/<current game>
    std::string find_active_game_dir();

    std::string own_dir_;
    Settings& settings_;
    Status& status_;
};

#endif
