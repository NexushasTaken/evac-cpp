#ifndef EVAC_PATHS_H
#define EVAC_PATHS_H

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

// A userdata folder the user added for the current game. The label is only a
// name for the list: renaming never touches the folder.
struct UserdataFolder
{
    std::string path;
    std::string label;
};

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
    // The active userdata folder of the current game: a custom one if chosen, the
    // default otherwise. The launcher never creates it, the game fills it in.
    std::string userdata();
    std::string default_userdata();  // <game base>/userdata, always there and never removed.
    std::vector<UserdataFolder> custom_userdata();
    // Each says why it can't be done, or "" once done. Paths are made absolute.
    std::string add_userdata(const std::string& path, const std::string& label);
    std::string rename_userdata(const std::string& path, const std::string& label);
    std::string check_userdata_label(const std::string& label, const std::string& own_path = "");
    // Forgets a custom folder; its files stay. The default becomes active if it was.
    void remove_userdata(const std::string& path);
    void set_active_userdata(const std::string& path);  // "" (or the default's path) for the default.
    bool is_default_userdata_active();
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
    // Everything the launcher adds to a userdata folder goes in launcher_dir, so it moves
    // with its worlds. The game never looks there.
    std::string launcher_dir();
    std::string save_backups();  // Zip backups (the Backups tab).
    std::string snapshots();     // One bare git repository per world (the Snapshots tab).
    // Zip backups used to be in <game base>/save_backups, from the time there was only
    // the default userdata. Moves them into the default's backups folder, whatever
    // userdata is active, if that does not exist yet. Returns false only if the move failed.
    bool move_old_backups();
    // Shared by all games, so it is next to the game directories, not in one.
    std::string trash_dir() const;

private:
    std::string game_base_dir();  // games_dir/<current game>
    std::string find_active_game_dir();
    void store_custom_userdata(const std::vector<UserdataFolder>& folders);

    std::string games_dir_;
    std::string cache_dir_;
    Settings& settings_;
    Status& status_;
};

#endif
