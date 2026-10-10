#ifndef CATAPULT_GAME_LAUNCHER_H
#define CATAPULT_GAME_LAUNCHER_H

#include <string>

#include "core/paths.h"
#include "core/settings.h"
#include "core/status.h"

class GameLauncher
{
public:
    GameLauncher(Settings& settings, Paths& paths, Status& status);

    // Starts the active install, optionally loading a world right away.
    // The game is started detached, so it keeps running if the launcher exits.
    bool start(const std::string& world);

    // Name of the world that was played last, empty if there is none.
    std::string last_world();
    bool has_last_world();

private:
    Settings& settings_;
    Paths& paths_;
    Status& status_;
};

#endif
