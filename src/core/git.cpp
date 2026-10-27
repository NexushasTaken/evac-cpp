#include "core/git.h"

#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

extern char** environ;

namespace git
{

// Settings that must not come from the user's config. See docs/git-backups.md.
static const char* const FORCED_CONFIG[] = {
    "user.name=evac",
    "user.email=evac@localhost",
    "core.hooksPath=/dev/null",
    "core.autocrlf=false",
    "core.filemode=false",
    "core.quotePath=false",
    // Git reads ~/.config/git/ignore and attributes even without a global config file.
    "core.excludesFile=/dev/null",
    "core.attributesFile=/dev/null",
    "commit.gpgsign=false",
    "init.defaultBranch=main",
    "safe.directory=*",
    // Housekeeping in the background could hold a lock while the next command runs.
    "gc.autoDetach=false",
    "maintenance.autoDetach=false",
};

static std::vector<std::string> make_environment()
{
    std::vector<std::string> env;
    for (char** e = environ; e && *e; ++e)
    {
        // GIT_DIR, GIT_WORK_TREE, GIT_INDEX_FILE, ... would point git somewhere else.
        if (std::strncmp(*e, "GIT_", 4) != 0)
            env.push_back(*e);
    }
    env.push_back("GIT_CONFIG_GLOBAL=/dev/null");
    env.push_back("GIT_CONFIG_NOSYSTEM=1");
    env.push_back("GIT_TERMINAL_PROMPT=0");
    return env;
}

static std::vector<char*> c_strings(std::vector<std::string>& strings)
{
    std::vector<char*> result;
    for (size_t i = 0; i < strings.size(); ++i)
        result.push_back(&strings[i][0]);
    result.push_back(nullptr);
    return result;
}

static void close_pipe(int fds[2])
{
    if (fds[0] >= 0)
        close(fds[0]);
    if (fds[1] >= 0)
        close(fds[1]);
}

// Reads both pipes until git closes them. Reading only one at a time could
// hang when git fills the other one.
static void read_all(int out_fd, int err_fd, std::string& out, std::string& err)
{
    struct pollfd fds[2];
    fds[0].fd = out_fd;
    fds[0].events = POLLIN;
    fds[1].fd = err_fd;
    fds[1].events = POLLIN;
    std::string* targets[2] = {&out, &err};
    int open_count = 2;

    char buf[65536];
    while (open_count > 0)
    {
        if (poll(fds, 2, -1) < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }
        for (int i = 0; i < 2; ++i)
        {
            if (fds[i].fd < 0 || fds[i].revents == 0)
                continue;
            ssize_t n = read(fds[i].fd, buf, sizeof(buf));
            if (n > 0)
            {
                targets[i]->append(buf, (size_t)n);
            }
            else if (n == 0 || errno != EINTR)
            {
                fds[i].fd = -1;  // poll ignores negative fds.
                --open_count;
            }
        }
    }
}

Result run(const std::vector<std::string>& args)
{
    Result result;

    std::vector<std::string> argv_strings;
    argv_strings.push_back("git");
    for (size_t i = 0; i < sizeof(FORCED_CONFIG) / sizeof(FORCED_CONFIG[0]); ++i)
    {
        argv_strings.push_back("-c");
        argv_strings.push_back(FORCED_CONFIG[i]);
    }
    argv_strings.insert(argv_strings.end(), args.begin(), args.end());
    std::vector<std::string> env_strings = make_environment();
    std::vector<char*> argv = c_strings(argv_strings);
    std::vector<char*> envp = c_strings(env_strings);

    // O_CLOEXEC, so other threads that spawn processes don't inherit our pipes.
    int out_pipe[2] = {-1, -1};
    int err_pipe[2] = {-1, -1};
    if (pipe2(out_pipe, O_CLOEXEC) != 0 || pipe2(err_pipe, O_CLOEXEC) != 0)
    {
        result.err = std::string("Cannot run git: ") + std::strerror(errno);
        close_pipe(out_pipe);
        close_pipe(err_pipe);
        return result;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, out_pipe[1], 1);
    posix_spawn_file_actions_adddup2(&actions, err_pipe[1], 2);

    pid_t pid = -1;
    int spawn_error = posix_spawnp(&pid, "git", &actions, nullptr, argv.data(), envp.data());
    posix_spawn_file_actions_destroy(&actions);
    close(out_pipe[1]);
    close(err_pipe[1]);
    out_pipe[1] = err_pipe[1] = -1;

    if (spawn_error != 0)
    {
        result.err = std::string("Cannot run git: ") + std::strerror(spawn_error);
        close_pipe(out_pipe);
        close_pipe(err_pipe);
        return result;
    }

    read_all(out_pipe[0], err_pipe[0], result.out, result.err);
    close_pipe(out_pipe);
    close_pipe(err_pipe);

    int wstatus = 0;
    while (waitpid(pid, &wstatus, 0) < 0)
    {
        if (errno != EINTR)
        {
            result.err += std::string("waitpid failed: ") + std::strerror(errno);
            return result;
        }
    }
    // glibc's posix_spawnp reports a missing program as exit status 127.
    if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 127 && result.out.empty() && result.err.empty())
    {
        result.err = "git is not installed.";
        return result;
    }
    result.started = true;
    result.exit_code = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1;
    return result;
}

bool parse_version(const std::string& text, int& major, int& minor)
{
    return std::sscanf(text.c_str(), "git version %d.%d", &major, &minor) == 2;
}

bool available()
{
    std::vector<std::string> args;
    args.push_back("--version");
    Result r = run(args);
    int major = 0;
    int minor = 0;
    if (!r.ok() || !parse_version(r.out, major, minor))
        return false;
    return major > 2 || (major == 2 && minor >= 23);
}

std::string shell_quote(const std::string& word)
{
    if (!word.empty() && word.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-./=:@%+,")
                             == std::string::npos)
        return word;
    std::string result = "'";
    for (size_t i = 0; i < word.size(); ++i)
    {
        if (word[i] == '\'')
            result += "'\\''";
        else
            result += word[i];
    }
    return result + "'";
}

}  // namespace git
