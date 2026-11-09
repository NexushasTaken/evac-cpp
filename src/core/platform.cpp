#include "platform.h"

#include <boost/filesystem/path.hpp>
#include <boost/process/v2/detail/config.hpp>
#include <boost/process/v2/environment.hpp>
#include <boost/uuid/basic_random_generator.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cerrno>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>

#include "boost/dll/runtime_symbol_info.hpp"
#include "boost/predef.h"

namespace platform {

namespace fs = std::filesystem;

std::string_view os_name() {
#if BOOST_OS_WINDOWS
  return "win";
#elif BOOST_OS_MACOS
  return "mac";
#elif BOOST_OS_LINUX
  return "linux";
#endif
}

std::filesystem::path executable_path() {
  std::error_code ec;
  const fs::path location = boost::dll::program_location(ec).native();
  return ec ? "" : location;
}

std::string get_env(const char* name) {
  boost::process::error_code ec;
  const auto env_value = boost::process::environment::get(name, ec);
  return ec ? "" : env_value;
}

bool is_executable_file(const std::filesystem::path& path) {
#if BOOST_OS_WINDOWS
  if (path.has_extension()) {
    return false;
  }
  std::string ext = path.extension().native();
  for (auto& c : ext) {
    c = std::tolower(c);
  }
  return ext == ".exe";
#else
  std::error_code ec;
  const auto st = fs::status(path, ec);
  if (ec || !fs::is_regular_file(st)) {
    return false;
  }

  const auto perm = st.permissions();
  using P = fs::perms;
  return (perm & (P::owner_exec | P::group_exec | P::others_exec)) != P::none;
#endif
}

std::tm local_time(std::time_t t) {
  std::tm tm_result = {};
#if BOOST_OS_WINDOWS
  localtime_s(&tm_result, &t);
#else
  localtime_r(&t, &tm_result);
#endif
  return tm_result;
}

static fs::path make_probe_path(const fs::path& dir) {
  static thread_local boost::uuids::random_generator gen;
  return dir / (".write_probe_" + boost::uuids::to_string(gen()));
}

bool dir_writable(const fs::path& dir, bool& read_only_fs) {
  read_only_fs = false;

  std::error_code ec;
  if (!fs::is_directory(dir, ec)) {
    return false;
  }

  const fs::path probe = make_probe_path(dir);

  errno = 0;
#if BOOST_OS_WINDOWS
  SetLastError(0);
#endif

  std::ofstream f(probe, std::ios::binary);
  if (!f) {
#if BOOST_OS_WINDOWS
    read_only_fs = (GetLastError() == ERROR_WRITE_PROTECT);
#else
    read_only_fs = (errno == EROFS);
#endif
    return false;
  }

  f.close();
  fs::remove(probe, ec);  // best-effort cleanup
  return true;
}

}  // namespace platform
