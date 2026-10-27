#include "core/platform.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace platform
{

static std::wstring widen(const std::string& text)
{
    if (text.empty())
        return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
    std::wstring result(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), &result[0], n);
    return result;
}

static std::string narrow(const std::wstring& text)
{
    if (text.empty())
        return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string result(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), &result[0], n, nullptr, nullptr);
    return result;
}

std::string os_name()
{
    return "win";
}

std::string executable_path()
{
    // The path can be longer than MAX_PATH, so grow until it fits.
    std::vector<wchar_t> buf(MAX_PATH);
    while (true)
    {
        DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        if (n == 0)
            return "";
        if (n < buf.size())
            return narrow(std::wstring(buf.data(), n));
        buf.resize(buf.size() * 2);
    }
}

std::string get_env(const char* name)
{
    std::wstring wname = widen(name);
    DWORD n = GetEnvironmentVariableW(wname.c_str(), nullptr, 0);
    if (n == 0)
        return "";
    std::wstring value(n, L'\0');
    n = GetEnvironmentVariableW(wname.c_str(), &value[0], n);
    value.resize(n);
    return narrow(value);
}

bool is_executable_file(const std::string& path)
{
    // Windows has no execute bit; any regular file can be tried.
    std::error_code ec;
    return fs::is_regular_file(fs::path(widen(path)), ec);
}

std::tm local_time(std::time_t t)
{
    std::tm result = {};
    localtime_s(&result, &t);
    return result;
}

bool dir_writable(const std::string& dir, bool& read_only_fs)
{
    // Attributes say nothing about directories, so try to make a file.
    read_only_fs = false;
    std::wstring probe = widen(dir) + L"\\.evac-write-test-" + std::to_wstring(GetCurrentProcessId());
    HANDLE h = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_EXISTS)  // Left over from a crash; the directory is writable.
            return true;
        read_only_fs = (error == ERROR_WRITE_PROTECT);
        return false;
    }
    CloseHandle(h);
    return true;
}

}  // namespace platform
