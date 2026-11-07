#ifndef EVAC_PATHS_H
#define EVAC_PATHS_H

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "core/settings.h"
#include "core/status.h"

// Where the launcher keeps its own files.
//   config_dir: evac_settings.json
//   games_dir:  one subdirectory per game ("dda", "bn", ...) with installs,
//   userdata, ... cache_dir:  downloaded archives
struct Locations {
  std::filesystem::path config_dir;
  std::filesystem::path games_dir;
  std::filesystem::path cache_dir;
};

// Everything in one directory (the layout of the original launcher).
Locations portable_locations(const std::filesystem::path& dir);
// XDG base directories, from the values of HOME and XDG_*_HOME (UTF-8). An
// empty or relative *_home is replaced by the default under home.
Locations xdg_locations(const std::string& home, const std::string& config_home,
                        const std::string& data_home,
                        const std::string& cache_home);
// %APPDATA%\evac for the settings, %LOCALAPPDATA%\evac for games and cache.
Locations windows_locations(const std::string& appdata,
                            const std::string& local_appdata);
// ~/Library/Application Support/evac, and ~/Library/Caches/evac for the cache.
Locations macos_locations(const std::string& home);
// Portable if evac_settings.json sits next to the executable, otherwise the
// usual place for the platform: XDG on Linux, see above for Windows and macOS.
Locations default_locations();

// A userdata folder the user added for the current game. The label is only a
// name for the list: renaming never touches the folder.
struct UserdataFolder {
  std::filesystem::path path;
  std::string label;
};

// The one place for all path resolution logic.
class Paths {
 public:
  // game name -> (install name -> install directory)
  using InstallsSummary =
      std::map<std::string_view, std::map<std::string, std::filesystem::path> >;

  Paths(const Locations& locations, Settings& settings, Status& status);
  // Portable layout in root (handy for tests).
  Paths(const std::filesystem::path& root, Settings& settings, Status& status);

  static std::filesystem::path executable_dir();
  // Names of the subdirectories (UTF-8), sorted.
  static std::vector<std::string> list_dir(const std::filesystem::path& dir);

  InstallsSummary installs_summary();

  std::filesystem::path cache_dir() const;
  std::filesystem::path game_dir();
  std::filesystem::path next_install_dir();
  // The active userdata folder of the current game: a custom one if chosen, the
  // default otherwise. The launcher never creates it, the game fills it in.
  std::filesystem::path userdata();
  std::filesystem::path
  default_userdata();  // <game base>/userdata, always there and never removed.
  std::vector<UserdataFolder> custom_userdata();
  // Each says why it can't be done, or "" once done. Paths are made absolute.
  std::string add_userdata(const std::filesystem::path& path,
                           const std::string& label);
  std::string rename_userdata(const std::filesystem::path& path,
                              const std::string& label);
  std::string check_userdata_label(const std::string& label,
                                   const std::filesystem::path& own_path = {});
  // Forgets a custom folder; its files stay. The default becomes active if it
  // was.
  void remove_userdata(const std::filesystem::path& path);
  void set_active_userdata(
      const std::filesystem::path&
          path);  // Empty (or the default's path) for the default.
  bool is_default_userdata_active();
  std::filesystem::path config();
  std::filesystem::path savegames();
  std::filesystem::path mods_stock();
  std::filesystem::path mods_user();
  std::filesystem::path sound_stock();
  std::filesystem::path sound_user();
  std::filesystem::path gfx_default();
  std::filesystem::path gfx_user();
  std::filesystem::path font_user();
  std::filesystem::path templates();
  std::filesystem::path memorial();
  std::filesystem::path graveyard();
  std::filesystem::path mod_repo();
  std::filesystem::path tmp_dir();
  // Files that ship with the launcher (bundled fonts, ...): <executable
  // dir>/data if it exists, otherwise the data directory the build was made
  // from.
  std::filesystem::path data_dir() const;
  // Everything the launcher adds to a userdata folder goes in launcher_dir, so
  // it moves with its worlds. The game never looks there.
  std::filesystem::path launcher_dir();
  std::filesystem::path save_backups();  // Zip backups (the Backups tab).
  std::filesystem::path
  snapshots();  // One bare git repository per world (the Snapshots tab).
  // Zip backups used to be in <game base>/save_backups, from the time there was
  // only the default userdata. Moves them into the default's backups folder,
  // whatever userdata is active, if that does not exist yet. Returns false only
  // if the move failed.
  bool move_old_backups();
  // Shared by all games, so it is next to the game directories, not in one.
  std::filesystem::path trash_dir() const;

 private:
  std::filesystem::path game_base_dir();  // games_dir/<current game>
  std::filesystem::path find_active_game_dir();
  void store_custom_userdata(const std::vector<UserdataFolder>& folders);

  std::filesystem::path m_games_dir;
  std::filesystem::path m_cache_dir;
  Settings& m_settings;
  Status& m_status;
};

#endif
