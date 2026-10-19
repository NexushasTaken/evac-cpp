#ifndef CATAPULT_GAME_LAUNCHER_H
#define CATAPULT_GAME_LAUNCHER_H

#include <string>
#include <vector>

#include "core/paths.h"
#include "core/settings.h"
#include "core/status.h"

// Splits extra command-line parameters into words, like a shell does:
// spaces separate words, '...' keeps everything as it is, "..." keeps spaces
// and allows \" and \\, and a backslash outside quotes escapes the next character.
// Nothing else is expanded (no $VAR, ~ or *). Returns false and sets error
// when a quote is not closed.
bool split_command_line(const std::string& text, std::vector<std::string>& words, std::string& error);

class GameLauncher
{
public:
    GameLauncher(Settings& settings, Paths& paths, Status& status);

    // Starts the active install, optionally loading a world right away.
    // The words of the game_extra_args setting are added after the launcher's own arguments.
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
