#include "core/process.h"

#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

extern char** environ;

namespace process
{

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

// Close-on-exec, so other threads that spawn processes don't inherit our pipes.
static bool make_pipe(int fds[2])
{
#ifdef __APPLE__
    // No pipe2 on macOS. There is a short window where another thread's spawn
    // could inherit the pipe, which only delays the EOF.
    if (pipe(fds) != 0)
        return false;
    fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    return true;
#else
    return pipe2(fds, O_CLOEXEC) == 0;
#endif
}

// Reads both pipes until the child closes them. Reading only one at a time
// could hang when the child fills the other one.
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

Result run(const std::string& program, const std::vector<std::string>& args, const Environment& env)
{
    Result result;

    std::vector<std::string> argv_strings;
    argv_strings.push_back(program);
    argv_strings.insert(argv_strings.end(), args.begin(), args.end());
    std::vector<std::string> current;
    for (char** e = environ; e && *e; ++e)
        current.push_back(*e);
    std::vector<std::string> env_strings = apply_environment(current, env, false);
    std::vector<char*> argv = c_strings(argv_strings);
    std::vector<char*> envp = c_strings(env_strings);

    int out_pipe[2] = {-1, -1};
    int err_pipe[2] = {-1, -1};
    if (!make_pipe(out_pipe) || !make_pipe(err_pipe))
    {
        result.err = "Cannot run " + program + ": " + std::strerror(errno);
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
    int spawn_error = posix_spawnp(&pid, program.c_str(), &actions, nullptr, argv.data(), envp.data());
    posix_spawn_file_actions_destroy(&actions);
    close(out_pipe[1]);
    close(err_pipe[1]);
    out_pipe[1] = err_pipe[1] = -1;

    if (spawn_error != 0)
    {
        // macOS and musl report a missing program here.
        result.not_found = (spawn_error == ENOENT);
        result.err = "Cannot run " + program + ": " + std::strerror(spawn_error);
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
        result.not_found = true;
        result.err = "Cannot run " + program + ": not found";
        return result;
    }
    result.started = true;
    result.exit_code = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1;
    return result;
}

bool spawn_detached(const std::filesystem::path& executable, const std::vector<std::string>& args,
                    const std::filesystem::path& working_dir, std::string& error)
{
    std::vector<std::string> argv_strings;
    argv_strings.push_back(executable.string());
    argv_strings.insert(argv_strings.end(), args.begin(), args.end());
    std::vector<char*> argv = c_strings(argv_strings);

    // Fork twice, so that the child is not ours and never becomes a zombie.
    pid_t pid = fork();
    if (pid < 0)
    {
        error = std::string("fork failed: ") + std::strerror(errno);
        return false;
    }
    if (pid == 0)
    {
        if (fork() == 0)
        {
            setsid();
            if (chdir(working_dir.c_str()) != 0)
                _exit(127);
            execv(executable.c_str(), argv.data());
            _exit(127);
        }
        _exit(0);
    }
    while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR)
    {
    }
    return true;
}

}  // namespace process
