#include "core/process.h"

// Windows 7 or newer, for the handle list in process_windows.cpp (MinGW
// defaults lower).
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

#include <algorithm>
#include <cwchar>
#include <functional>
#include <thread>

namespace process {

static std::wstring widen(const std::string& text) {
  if (text.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(),
                              nullptr, 0);
  std::wstring result(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), &result[0], n);
  return result;
}

static std::string narrow(const wchar_t* text, size_t size) {
  if (size == 0) return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, text, (int)size, nullptr, 0, nullptr,
                              nullptr);
  std::string result(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, (int)size, &result[0], n, nullptr,
                      nullptr);
  return result;
}

static std::string error_message(DWORD code) {
  wchar_t* buf = nullptr;
  DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                               FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           nullptr, code, 0, (LPWSTR)&buf, 0, nullptr);
  std::string text = n ? narrow(buf, n) : "error " + std::to_string(code);
  LocalFree(buf);
  while (!text.empty() &&
         (text.back() == '\n' || text.back() == '\r' || text.back() == '.'))
    text.pop_back();
  return text;
}

static std::vector<std::string> current_environment() {
  std::vector<std::string> result;
  wchar_t* block = GetEnvironmentStringsW();
  if (!block) return result;
  for (const wchar_t* p = block; *p; p += std::wcslen(p) + 1)
    result.push_back(narrow(p, std::wcslen(p)));
  FreeEnvironmentStringsW(block);
  return result;
}

// "A=1\0B=2\0\0", sorted by name without case, as CreateProcess wants it.
static std::wstring environment_block(const std::vector<std::string>& vars) {
  std::vector<std::wstring> wide;
  for (size_t i = 0; i < vars.size(); ++i) wide.push_back(widen(vars[i]));
  std::sort(wide.begin(), wide.end(),
            [](const std::wstring& a, const std::wstring& b) {
              return _wcsicmp(a.c_str(), b.c_str()) < 0;
            });
  std::wstring block;
  for (size_t i = 0; i < wide.size(); ++i) {
    block += wide[i];
    block += L'\0';
  }
  block += L'\0';
  if (wide.empty()) block += L'\0';
  return block;
}

static void read_pipe(HANDLE pipe, std::string& target) {
  char buf[65536];
  DWORD n = 0;
  while (ReadFile(pipe, buf, sizeof(buf), &n, nullptr) && n > 0)
    target.append(buf, n);
}

static void close_handle(HANDLE& h) {
  if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
  h = nullptr;
}

Result run(const std::string& program, const std::vector<std::string>& args,
           const Environment& env) {
  Result result;

  SECURITY_ATTRIBUTES inheritable = {sizeof(SECURITY_ATTRIBUTES), nullptr,
                                     TRUE};
  HANDLE out_read = nullptr, out_write = nullptr, err_read = nullptr,
         err_write = nullptr;
  if (!CreatePipe(&out_read, &out_write, &inheritable, 0) ||
      !CreatePipe(&err_read, &err_write, &inheritable, 0)) {
    result.err = "Cannot run " + program + ": " + error_message(GetLastError());
    close_handle(out_read);
    close_handle(out_write);
    close_handle(err_read);
    close_handle(err_write);
    return result;
  }
  // Only the child's ends are inherited.
  SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0);
  HANDLE null_in =
      CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  &inheritable, OPEN_EXISTING, 0, nullptr);

  // List the handles to inherit, so a process another thread starts at the
  // same time doesn't get our pipes (the Windows version of close-on-exec).
  SIZE_T list_size = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &list_size);
  std::vector<char> list_buf(list_size);
  LPPROC_THREAD_ATTRIBUTE_LIST list =
      (LPPROC_THREAD_ATTRIBUTE_LIST)list_buf.data();
  InitializeProcThreadAttributeList(list, 1, 0, &list_size);
  HANDLE inherit[3] = {null_in, out_write, err_write};
  UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit,
                            sizeof(inherit), nullptr, nullptr);

  STARTUPINFOEXW si;
  ZeroMemory(&si, sizeof(si));
  si.StartupInfo.cb = sizeof(si);
  si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  si.StartupInfo.hStdInput = null_in;
  si.StartupInfo.hStdOutput = out_write;
  si.StartupInfo.hStdError = err_write;
  si.lpAttributeList = list;

  std::wstring command = widen(windows_command_line(program, args));
  std::wstring env_block =
      environment_block(apply_environment(current_environment(), env, true));
  PROCESS_INFORMATION pi;
  ZeroMemory(&pi, sizeof(pi));
  // No application name: CreateProcess then searches PATH and adds ".exe".
  BOOL created =
      CreateProcessW(nullptr, &command[0], nullptr, nullptr, TRUE,
                     EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW |
                         CREATE_UNICODE_ENVIRONMENT,
                     &env_block[0], nullptr, &si.StartupInfo, &pi);
  DWORD create_error = GetLastError();
  DeleteProcThreadAttributeList(list);
  close_handle(out_write);
  close_handle(err_write);
  close_handle(null_in);

  if (!created) {
    result.not_found = (create_error == ERROR_FILE_NOT_FOUND ||
                        create_error == ERROR_PATH_NOT_FOUND);
    result.err = "Cannot run " + program + ": " + error_message(create_error);
    close_handle(out_read);
    close_handle(err_read);
    return result;
  }

  // One thread per pipe: reading only one could hang when the child fills the
  // other.
  std::thread err_reader(read_pipe, err_read, std::ref(result.err));
  read_pipe(out_read, result.out);
  err_reader.join();
  close_handle(out_read);
  close_handle(err_read);

  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 0;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  result.started = true;
  result.exit_code = (int)code;
  return result;
}

bool spawn_detached(const std::filesystem::path& executable,
                    const std::vector<std::string>& args,
                    const std::filesystem::path& working_dir,
                    std::string& error) {
  std::wstring app = executable.wstring();
  std::wstring command =
      widen(windows_command_line(executable.u8string(), args));
  std::wstring dir = working_dir.wstring();
  STARTUPINFOW si;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi;
  ZeroMemory(&pi, sizeof(pi));
  if (!CreateProcessW(app.c_str(), &command[0], nullptr, nullptr, FALSE,
                      CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS, nullptr,
                      dir.c_str(), &si, &pi)) {
    error = error_message(GetLastError());
    return false;
  }
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return true;
}

}  // namespace process
