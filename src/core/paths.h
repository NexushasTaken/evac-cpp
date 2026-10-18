#ifndef CATAPULT_PATHS_H
#define CATAPULT_PATHS_H

#include <map>
#include <string>
#include <vector>

#include "core/settings.h"
#include "core/status.h"

// Where the launcher keeps its own files.
//   config_dir: catapult_settings.json
//   games_dir:  one subdirectory per game ("dda", "bn", ...) with installs, userdata, ...
//   cache_dir:  downloaded archives
struct Locations
{
    std::string config_dir;
    std::string games_dir;
    std::string cache_dir;
};

// Everything in one directory (the layout of the original launcher).
Locations portable_locations(const std::string& dir);
// XDG base directories. An empty or relative *_home is replaced by the default under home.
Locations xdg_locations(const std::string& home, const std::string& config_home,
                        const std::string& data_home, const std::string& cache_home);
// Portable if catapult_settings.json sits next to the executable, XDG otherwise.
Locations default_locations();

// The one place for all path resolution logic.
class Paths
{
public:
    // game name -> (install name -> install directory)
    typedef std::map<std::string, std::map<std::string, std::string> > InstallsSummary;

    Paths(const Locations& locations, Settings& settings, Status& status);
    // Portable layout in root (handy for tests).
    Paths(const std::string& root, Settings& settings, Status& status);

    static std::string executable_dir();
    static std::vector<std::string> list_dir(const std::string& dir);  // Subdirectories, sorted.

    InstallsSummary installs_summary();

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
    // Files that ship with the launcher (bundled fonts, ...): <executable dir>/data if
    // it exists, otherwise the data directory the build was made from.
    std::string data_dir() const;
    std::string save_backups();
    // Shared by all games, so it is next to the game directories, not in one.
    std::string trash_dir() const;

private:
    std::string game_base_dir();  // games_dir/<current game>
    std::string find_active_game_dir();

    std::string games_dir_;
    std::string cache_dir_;
    Settings& settings_;
    Status& status_;
};

#endif
