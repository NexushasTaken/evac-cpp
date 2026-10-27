#ifndef EVAC_APP_CONTEXT_H
#define EVAC_APP_CONTEXT_H

#include <string>

#include "core/backup_manager.h"
#include "core/font_manager.h"
#include "core/game_launcher.h"
#include "core/mod_manager.h"
#include "core/paths.h"
#include "core/release_installer.h"
#include "core/release_manager.h"
#include "core/settings.h"
#include "core/snapshot_manager.h"
#include "core/soundpack_manager.h"
#include "core/status.h"
#include "core/trash.h"

// Everything the UI needs from the core, created once in main().
// The order of the members matters: they use the ones above them.
struct AppContext
{
    explicit AppContext(const Locations& locations)
        : settings(locations.config_dir + "/catapult_settings.json", status),
          paths(locations, settings, status),
          trash(settings, paths, status),
          releases(settings, status),
          installer(settings, paths, status, trash),
          launcher(settings, paths, status),
          fonts(paths, status),
          soundpacks(settings, paths, status, trash),
          mods(settings, paths, status, trash),
          backups(settings, paths, status, trash),
          snapshots(paths, status, trash)
    {
        settings.load();
    }

    Status status;
    Settings settings;
    Paths paths;
    Trash trash;
    ReleaseManager releases;
    ReleaseInstaller installer;
    GameLauncher launcher;
    FontManager fonts;
    SoundpackManager soundpacks;
    ModManager mods;
    BackupManager backups;
    SnapshotManager snapshots;
};

#endif
