#ifndef EVAC_MOD_MANAGER_H
#define EVAC_MOD_MANAGER_H

#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "core/http.h"
#include "core/paths.h"
#include "core/settings.h"
#include "core/status.h"
#include "core/trash.h"

struct ModEntry {
  std::string id;
  std::string name;
  std::filesystem::path location;  // The directory of the mod.
  nlohmann::json modinfo;
  bool is_stock = false;     // Comes with the game.
  bool is_obsolete = false;  // A stock mod that the game marks as obsolete.
};

enum ModStatus {
  MOD_NOT_INSTALLED = 0,
  MOD_INSTALLED = 1,
  MOD_STOCK = 2,
  MOD_STOCK_OBSOLETE = 3,
  MOD_INSTALLED_MODIFIED_ID = 4  // An obsolete stock mod with this ID, and our
                                 // copy of a mod with the same ID.
};

// A modpack that can be downloaded to fill the local mod repository.
struct Modpack {
  std::string name;
  std::string url;
  std::string filename;
  std::vector<std::string>
      internal_paths;         // Directories in the archive with mods.
  std::string archived_path;  // Directory with archived (old) mods.
};

// Keeps track of the mods that are installed, and of the ones in the local
// repository (a directory the Kenan modpack gets unpacked to) that can be
// installed. The install, delete and download functions block, so use a worker
// thread, and don't read the lists while they run.
class ModManager {
 public:
  ModManager(Settings& settings, Paths& paths, Status& status, Trash& trash);

  // Finds mods in a directory (id -> mod). Exposed for tests.
  static std::map<std::string, ModEntry> parse_mods_dir(
      const std::filesystem::path& mods_dir, Status& status);
  static std::string strip_html_tags(const std::string& text);

  // The modpack for a game ("dda" or "bn"), or null if there is none.
  static const Modpack* modpack_for_game(const std::string& game);

  void refresh_installed();
  void refresh_available();

  const std::map<std::string, ModEntry>& installed() const {
    return installed_;
  }
  const std::map<std::string, ModEntry>& available() const {
    return available_;
  }

  ModStatus mod_status(const std::string& id) const;

  // Installs mods from the repository into the user's mods directory.
  bool install_mods(const std::vector<std::string>& ids);

  // Deletes user mods. Stock mods are never deleted.
  bool delete_mods(const std::vector<std::string>& ids);

  // Downloads the modpack and replaces the contents of the repository with its
  // mods.
  bool retrieve_modpack(const Modpack& pack);

  void cancel();

 private:
  bool install_mod(const std::string& id);
  bool delete_mod(const std::string& id);

  Settings& settings_;
  Paths& paths_;
  Status& status_;
  Trash& trash_;
  Http http_;
  std::map<std::string, ModEntry> installed_;
  std::map<std::string, ModEntry> available_;
};

#endif
