#ifndef EVAC_GIT_H
#define EVAC_GIT_H

#include <string>
#include <vector>

// Runs the git binary. Blocks until git is done, so use a worker thread for
// anything slow.
//
// Every call ignores the user's git config (global and system) and the GIT_*
// environment variables, and forces the settings the launcher relies on (see
// docs/git-backups.md, "Running git"), so the result is the same on every machine.
namespace git
{

struct Result
{
    bool started = false;  // False if git could not be run at all.
    int exit_code = -1;
    std::string out;
    std::string err;

    bool ok() const { return started && exit_code == 0; }
};

// args are the arguments after "git", passed as they are (no shell).
Result run(const std::vector<std::string>& args);

// True if git is installed and new enough (2.23, for git restore).
bool available();

// Parses "git version 2.43.0" (and "git version 2.39.3 (Apple Git-145)").
bool parse_version(const std::string& text, int& major, int& minor);

// Quotes a word for a POSIX shell, for commands shown to the user.
std::string shell_quote(const std::string& word);

}  // namespace git

#endif
