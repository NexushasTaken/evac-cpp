#ifndef EVAC_PROCESS_H
#define EVAC_PROCESS_H

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

// Starting other programs, without a shell. run() and spawn_detached() are
// implemented in process_posix.cpp and process_windows.cpp; the helpers below
// are plain string work (process.cpp), so they are tested on every platform.
namespace process
{

struct Result
{
    bool started = false;    // False if the program could not be run at all.
    bool not_found = false;  // The program does not exist (started is false then).
    int exit_code = -1;
    std::string out;
    std::string err;

    bool ok() const { return started && exit_code == 0; }
};

// Changes to the launcher's own environment for the child.
struct Environment
{
    std::vector<std::string> remove_prefixes;                  // Drop every variable whose name starts with one.
    std::vector<std::pair<std::string, std::string> > set;   // Then add these (replacing old values).
};

// Runs program (looked up in PATH if it has no directory) with args, stdin
// empty, and collects stdout and stderr. Blocks until it exits, so use a
// worker thread for anything slow.
Result run(const std::string& program, const std::vector<std::string>& args, const Environment& env);

// Starts an executable in working_dir and does not wait for it. It keeps
// running if the launcher exits. Returns false and sets error if it could not
// be started.
bool spawn_detached(const std::filesystem::path& executable, const std::vector<std::string>& args,
                    const std::filesystem::path& working_dir, std::string& error);

// --- Helpers, public for the tests ---

// current is a list of "NAME=value". ignore_case is for Windows, where
// variable names are not case-sensitive.
std::vector<std::string> apply_environment(const std::vector<std::string>& current, const Environment& env,
                                           bool ignore_case);

// Quotes one argument so that the Microsoft C runtime (CommandLineToArgvW)
// reads it back unchanged.
std::string windows_quote_arg(const std::string& arg);

// The whole Windows command line: the program, then the quoted arguments.
std::string windows_command_line(const std::string& program, const std::vector<std::string>& args);

}  // namespace process

#endif
