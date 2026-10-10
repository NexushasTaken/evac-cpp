#ifndef CATAPULT_APP_CONTEXT_H
#define CATAPULT_APP_CONTEXT_H

#include <string>

#include "core/backup_manager.h"
#include "core/font_manager.h"
#include "core/game_launcher.h"
#include "core/mod_manager.h"
#include "core/paths.h"
#include "core/release_installer.h"
#include "core/release_manager.h"
#include "core/settings.h"
#include "core/soundpack_manager.h"
#include "core/status.h"

// Everything the UI needs from the core, created once in main().
// The order of the members matters: they use the ones above them.
struct AppContext
{
    explicit AppContext(const std::string& own_dir)
        : settings(own_dir + "/catapult_settings.json", status),
          paths(own_dir, settings, status),
          releases(settings, status),
          installer(settings, paths, status),
          launcher(settings, paths, status),
          fonts(paths, status),
          soundpacks(settings, paths, status),
          mods(settings, paths, status),
          backups(settings, paths, status)
    {
        settings.load();
    }

    Status status;
    Settings settings;
    Paths paths;
    ReleaseManager releases;
    ReleaseInstaller installer;
    GameLauncher launcher;
    FontManager fonts;
    SoundpackManager soundpacks;
    ModManager mods;
    BackupManager backups;
};

#endif
