#ifndef CATAPULT_SOUNDPACK_MANAGER_H
#define CATAPULT_SOUNDPACK_MANAGER_H

#include <string>
#include <vector>

#include "core/http.h"
#include "core/paths.h"
#include "core/settings.h"
#include "core/status.h"

// A soundpack that can be downloaded.
struct SoundpackSource
{
    std::string name;           // Also the name of the directory it gets installed to.
    std::string url;
    std::string filename;       // Name of the downloaded archive.
    std::string internal_path;  // Where the pack is inside the archive.
};

// All soundpacks that can be downloaded, in the order they are shown.
const std::vector<SoundpackSource>& available_soundpacks();

// A soundpack found on disk.
struct InstalledSoundpack
{
    std::string name;
    std::string description;
    std::string location;
    bool is_stock = false;  // Comes with the game, so it can't be deleted.
};

// Installs and removes soundpacks. These block, so use a worker thread.
class SoundpackManager
{
public:
    SoundpackManager(Settings& settings, Paths& paths, Status& status);

    // Finds the soundpacks in a directory: every subdirectory that has a
    // soundpack.txt. Static, so it can be tested without a game install.
    static std::vector<InstalledSoundpack> parse_sound_dir(const std::string& sound_dir);

    // The packs in the user's sound directory, plus the ones that come with the game.
    std::vector<InstalledSoundpack> get_installed(bool include_stock);

    bool delete_pack(const std::string& pack_name);

    // Downloads the pack (or uses from_file, if not empty) and installs it.
    // With reinstall, an existing copy is removed first.
    bool install_pack(const SoundpackSource& pack, const std::string& from_file, bool reinstall, bool keep_archive);

    void cancel();

private:
    Settings& settings_;
    Paths& paths_;
    Status& status_;
    Http http_;
};

#endif
