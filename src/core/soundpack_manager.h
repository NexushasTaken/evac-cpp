#ifndef EVAC_SOUNDPACK_MANAGER_H
#define EVAC_SOUNDPACK_MANAGER_H

#include <filesystem>
#include <string>
#include <vector>

#include "core/http.h"
#include "core/paths.h"
#include "core/settings.h"
#include "core/status.h"
#include "core/trash.h"

// A soundpack that can be downloaded.
struct SoundpackSource {
  std::string name;  // Also the name of the directory it gets installed to.
  std::string url;
  std::string filename;       // Name of the downloaded archive.
  std::string internal_path;  // Where the pack is inside the archive.
};

// All soundpacks that can be downloaded, in the order they are shown.
const std::vector<SoundpackSource>& available_soundpacks();

// A soundpack found on disk.
struct InstalledSoundpack {
  std::string name;
  std::string description;
  std::filesystem::path location;
  bool is_stock = false;  // Comes with the game, so it can't be deleted.
  bool enabled =
      true;  // False when soundpack.txt is renamed to soundpack.txt.disabled.
};

// Installs and removes soundpacks. These block, so use a worker thread.
class SoundpackManager {
 public:
  SoundpackManager(Settings& settings, Paths& paths, Status& status,
                   Trash& trash);

  // Finds the soundpacks in a directory: every subdirectory that has a
  // soundpack.txt (or a soundpack.txt.disabled). Static, so it can be tested
  // without a game install.
  static std::vector<InstalledSoundpack> parse_sound_dir(
      const std::filesystem::path& sound_dir);

  // The packs in the user's sound directory, plus the ones that come with the
  // game.
  std::vector<InstalledSoundpack> get_installed(bool include_stock);

  // Packs are picked by location, not name: two folders can have the same VIEW
  // name.
  bool delete_pack(const std::filesystem::path& location);

  // Renames soundpack.txt to soundpack.txt.disabled, or back, so the game
  // skips the pack. Only for the user's packs, not the stock ones.
  bool set_enabled(const std::filesystem::path& location, bool enabled);

  // True if the pack's directory is in the user's sound directory, so that
  // installing it again replaces that copy. Stock packs don't count: they
  // are never touched.
  bool has_user_copy(const SoundpackSource& pack);

  // Downloads the pack (or uses from_file, if not empty) and installs it.
  // With reinstall, an existing copy is removed first. Without it, an
  // existing copy is an error, so nothing gets merged into it.
  bool install_pack(const SoundpackSource& pack,
                    const std::filesystem::path& from_file, bool reinstall,
                    bool keep_archive);

  void cancel();

 private:
  // Looks up one of the user's packs by its directory. Posts an error if there
  // is none.
  bool find_user_pack(const std::filesystem::path& location,
                      InstalledSoundpack& out);

  Settings& settings_;
  Paths& paths_;
  Status& status_;
  Trash& trash_;
  Http http_;
};

#endif
