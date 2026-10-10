#include "core/game_launcher.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <filesystem>

#include "core/helpers.h"

namespace fs = std::filesystem;

GameLauncher::GameLauncher(Settings& settings, Paths& paths, Status& status)
    : settings_(settings), paths_(paths), status_(status)
{
}

bool GameLauncher::has_last_world()
{
    return fs::exists(paths_.config() + "/lastworld.json");
}

std::string GameLauncher::last_world()
{
    nlohmann::json info;
    if (!load_json_file(paths_.config() + "/lastworld.json", info, status_))
        return "";
    if (!info.is_object() || !info.contains("world_name") || !info["world_name"].is_string())
        return "";
    return info["world_name"].get<std::string>();
}

bool GameLauncher::start(const std::string& world)
{
    std::string game_dir = paths_.game_dir();
    std::string executable = game_dir + "/cataclysm-launcher";
    std::string userdata = paths_.userdata() + "/";

    if (access(executable.c_str(), X_OK) != 0)
    {
        status_.post("Cannot start the game, no executable found: " + executable, MSG_ERROR);
        return false;
    }

    std::vector<std::string> args;
    args.push_back(executable);
    args.push_back("--userdir");
    args.push_back(userdata);
    if (!world.empty())
    {
        args.push_back("--world");
        args.push_back(world);
    }

    std::vector<char*> argv;
    for (size_t i = 0; i < args.size(); ++i)
        argv.push_back(const_cast<char*>(args[i].c_str()));
    argv.push_back(nullptr);

    // Fork twice, so that the game is not our child and never becomes a zombie.
    pid_t pid = fork();
    if (pid < 0)
    {
        status_.post("Cannot start the game: fork failed", MSG_ERROR);
        return false;
    }
    if (pid == 0)
    {
        if (fork() == 0)
        {
            setsid();
            execv(executable.c_str(), argv.data());
            _exit(127);
        }
        _exit(0);
    }
    waitpid(pid, nullptr, 0);

    status_.post("Started the game.", MSG_DEBUG);
    return true;
}
