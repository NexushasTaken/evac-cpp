#include "core/process.h"

#include <unistd.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/process/environment.hpp>
#include <boost/process/process.hpp>
#include <boost/process/start_dir.hpp>
#include <boost/process/stdio.hpp>
#include <boost/process/v2/default_launcher.hpp>
#include <boost/process/v2/detail/config.hpp>
#include <boost/process/v2/environment.hpp>
#include <boost/process/v2/exit_code.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/start_dir.hpp>
#include <boost/process/v2/stdio.hpp>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <format>
#include <string>
#include <system_error>
#include <vector>

namespace process {

struct NewSession {
  template <class Launcher>
  std::error_code on_exec_setup(Launcher& /*unused*/,
                                const boost::filesystem::path& /*unused*/,
                                const char* const*& /*unused*/) {
    ::setsid();
    return {};
  }
};

static Result run_process(const std::string& program,
                          const std::vector<std::string>& args,
                          const Environment& env,
                          const std::filesystem::path& working_dir,
                          bool background) {
  namespace proc = boost::process;
  namespace asio = boost::asio;
  namespace environ = boost::process::environment;
  Result result = {};

  asio::io_context ctx{};
  asio::readable_pipe pout{ctx};
  asio::readable_pipe perr{ctx};
  proc::error_code ec;

  std::vector<std::string> current_env{};

  for (const auto v : environ::current()) {
    current_env.push_back(
        std::format("{}={}", v.key().string(), v.value().string()));
  }

  const auto new_env = apply_environment(current_env, env, false);
  proc::default_process_launcher launcher;
  if (background) {
    proc::process child = launcher(
        ctx, ec, program, args,
        proc::process_stdio{.in = nullptr, .out = nullptr, .err = nullptr},
        proc::process_environment(new_env),
        proc::process_start_dir(working_dir.native()), NewSession{});
    if (ec) {
      result.not_found = ec == std::errc::no_such_file_or_directory;
      result.err = std::format("Cannot run {}:{}", program, ec.message());
      return result;
    }
    child.detach();
    result.started = true;
    return result;
  }
  proc::process child =
      launcher(ctx, ec, program, args,
               proc::process_stdio{.in = nullptr, .out = pout, .err = perr},
               proc::process_environment(new_env),
               proc::process_start_dir(working_dir.native()));
  if (ec) {
    result.not_found = ec == std::errc::no_such_file_or_directory;
    result.started = false;
    result.err = std::format("Cannot run {}:{}", program, ec.message());
    return result;
  }
  result.started = true;

  proc::error_code out_ec;
  proc::error_code err_ec;
  proc::error_code wait_ec;

  // eof is the normal completion, so it is filtered after ctx.run()
  asio::async_read(pout, asio::dynamic_buffer(result.out),
                   [&](proc::error_code e, std::size_t /*_*/) { out_ec = e; });

  asio::async_read(perr, asio::dynamic_buffer(result.err),
                   [&](proc::error_code e, std::size_t /*_*/) { err_ec = e; });

  child.async_wait([&](proc::error_code e, int code) {
    wait_ec = e;
    result.exit_code = proc::evaluate_exit_code(code);
  });

  ctx.run();  // returns when all three operations have completed

  // Error handling
  if (out_ec && out_ec != asio::error::eof) {
    result.err =
        std::format("Cannot read stdout: {}:{}", program, out_ec.message());
  }
  if (err_ec && err_ec != asio::error::eof) {
    result.err =
        std::format("Cannot read stderr: {}:{}", program, err_ec.message());
  }
  if (wait_ec) {
    result.err = std::format("Cannot wait on process: {}:{}", program,
                             wait_ec.message());
    return result;
  }

  return result;
}

Result run(const std::string& program, const std::vector<std::string>& args,
           const Environment& env) {
  const auto program_path =
      boost::process::environment::find_executable(program).native();
  return run_process(program_path, args, env,
                     boost::filesystem::current_path().native(), false);
}

bool spawn_detached(const std::filesystem::path& executable,
                    const std::vector<std::string>& args,
                    const std::filesystem::path& working_dir,
                    std::string& error) {
  const Result result = run_process(executable, args, {}, working_dir, true);
  error = result.err;
  return result.started;
}

static bool starts_with(const std::string& text, const std::string& prefix,
                        bool ignore_case) {
  if (text.size() < prefix.size()) {
    return false;
  }
  for (size_t i = 0; i < prefix.size(); ++i) {
    char a = text[i];
    char b = prefix[i];
    if (ignore_case) {
      a = static_cast<char>(std::toupper(static_cast<unsigned char>(a)));
      b = static_cast<char>(std::toupper(static_cast<unsigned char>(b)));
    }
    if (a != b) {
      return false;
    }
  }
  return true;
}

std::vector<std::string> apply_environment(
    const std::vector<std::string>& current, const Environment& env,
    bool ignore_case) {
  std::vector<std::string> result;
  for (const auto& i : current) {
    bool drop = false;
    for (size_t p = 0; p < env.remove_prefixes.size() && !drop; ++p) {
      drop = starts_with(i, env.remove_prefixes[p], ignore_case);
    }
    for (size_t s = 0; s < env.set.size() && !drop; ++s) {
      drop = starts_with(i, env.set[s].first + "=", ignore_case);
    }
    if (!drop) {
      result.push_back(i);
    }
  }
  for (const auto& s : env.set) {
    result.push_back(s.first + "=" + s.second);
  }
  return result;
}

// The rules are those of CommandLineToArgvW: backslashes are only special
// right before a double quote (or the closing quote).
std::string windows_quote_arg(const std::string& arg) {
  if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string::npos) {
    return arg;
  }

  std::string result = "\"";
  size_t i = 0;
  while (true) {
    size_t backslashes = 0;
    while (i < arg.size() && arg[i] == '\\') {
      ++backslashes;
      ++i;
    }
    if (i == arg.size()) {
      // Double them, so the closing quote stays a quote.
      result.append(backslashes * 2, '\\');
      break;
    }
    if (arg[i] == '"') {
      result.append((backslashes * 2) + 1, '\\');
      result += '"';
    } else {
      result.append(backslashes, '\\');
      result += arg[i];
    }
    ++i;
  }
  return result + "\"";
}

std::string windows_command_line(const std::string& program,
                                 const std::vector<std::string>& args) {
  // The program name is read differently: up to the next quote, no escapes.
  // Windows file names can't contain a quote, so plain quotes are enough.
  std::string line = program.find_first_of(" \t") == std::string::npos
                         ? program
                         : "\"" + program + "\"";
  for (const auto& arg : args) {
    line += " " + windows_quote_arg(arg);
  }
  return line;
}

}  // namespace process
