#include "core/platform.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <filesystem>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

namespace platform
{

std::string os_name()
{
#ifdef __APPLE__
    return "mac";
#else
    return "linux";
#endif
}

std::string executable_path()
{
    std::error_code ec;
#ifdef __APPLE__
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buf(size, '\0');
    if (_NSGetExecutablePath(&buf[0], &size) != 0)
        return "";
    buf.resize(buf.find('\0'));
    fs::path exe = fs::weakly_canonical(buf, ec);  // It can contain symlinks and "..".
    return ec ? buf : exe.string();
#else
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    return ec ? "" : exe.string();
#endif
}

std::string get_env(const char* name)
{
    const char* value = std::getenv(name);
    return value ? value : "";
}

bool is_executable_file(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && access(path.c_str(), X_OK) == 0;
}

bool dir_writable(const std::string& dir, bool& read_only_fs)
{
    read_only_fs = false;
    if (access(dir.c_str(), W_OK) == 0)
        return true;
    read_only_fs = (errno == EROFS);
    return false;
}

}  // namespace platform
