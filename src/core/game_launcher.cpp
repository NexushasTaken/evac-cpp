#include "core/game_launcher.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <filesystem>

#include "core/helpers.h"

namespace fs = std::filesystem;

bool split_command_line(const std::string& text, std::vector<std::string>& words, std::string& error)
{
    words.clear();
    std::string word;
    bool in_word = false;  // Separate from word.empty(), so that '' gives an empty word.
    size_t i = 0;
    while (i < text.size())
    {
        char c = text[i];
        if (c == ' ' || c == '\t' || c == '\n')
        {
            if (in_word)
                words.push_back(word);
            word.clear();
            in_word = false;
            ++i;
        }
        else if (c == '\'')
        {
            size_t end = text.find('\'', i + 1);
            if (end == std::string::npos)
            {
                error = "A single quote (') is not closed.";
                return false;
            }
            word += text.substr(i + 1, end - i - 1);
            in_word = true;
            i = end + 1;
        }
        else if (c == '"')
        {
            ++i;
            while (i < text.size() && text[i] != '"')
            {
                if (text[i] == '\\' && i + 1 < text.size() && (text[i + 1] == '"' || text[i + 1] == '\\'))
                    ++i;
                word += text[i];
                ++i;
            }
            if (i >= text.size())
            {
                error = "A double quote (\") is not closed.";
                return false;
            }
            in_word = true;
            ++i;
        }
        else if (c == '\\' && i + 1 < text.size())
        {
            word += text[i + 1];
            in_word = true;
            i += 2;
        }
        else
        {
            word += c;
            in_word = true;
            ++i;
        }
    }
    if (in_word)
        words.push_back(word);
    return true;
}

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

    std::vector<std::string> extra_args;
    std::string error;
    if (!split_command_line(settings_.get_string("game_extra_args"), extra_args, error))
    {
        status_.post("Cannot start the game, the extra command-line parameters are wrong: " + error, MSG_ERROR);
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
    args.insert(args.end(), extra_args.begin(), extra_args.end());

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
