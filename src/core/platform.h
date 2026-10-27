#ifndef EVAC_PLATFORM_H
#define EVAC_PLATFORM_H

#include <ctime>
#include <filesystem>
#include <string>

// The things the core needs to ask the operating system. Implemented in
// platform_posix.cpp (Linux, macOS, BSDs) and platform_windows.cpp; CMake
// builds one of them.
//
// All strings are UTF-8, on every platform.
namespace platform
{

// "linux", "win" or "mac". The same names the release lists use. Other POSIX
// systems count as "linux" (they run Linux builds, or none).
std::string os_name();

// Full path of the running executable, empty if it can't be found.
std::filesystem::path executable_path();

// The value of an environment variable, empty if it is not set.
std::string get_env(const char* name);

// True if path is a regular file this user may run.
bool is_executable_file(const std::filesystem::path& path);

// t in the local time zone (localtime_r is not on Windows, localtime is not thread-safe).
std::tm local_time(std::time_t t);

// True if new files can be made in dir. read_only_fs is set when the whole
// file system is read-only (as opposed to a permission problem).
bool dir_writable(const std::filesystem::path& dir, bool& read_only_fs);

}  // namespace platform

#endif
