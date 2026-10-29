#include "core/git.h"

#include <cstdio>

namespace git {

// Settings that must not come from the user's config. See docs/git-backups.md.
// "/dev/null" works on Windows too: Git for Windows opens it as "nul".
static const char* const FORCED_CONFIG[] = {
    "user.name=evac",
    "user.email=evac@localhost",
    "core.hooksPath=/dev/null",
    "core.autocrlf=false",
    "core.filemode=false",
    "core.quotePath=false",
    // Git reads ~/.config/git/ignore and attributes even without a global
    // config file.
    "core.excludesFile=/dev/null",
    "core.attributesFile=/dev/null",
    "commit.gpgsign=false",
    "init.defaultBranch=main",
    "safe.directory=*",
    // Housekeeping in the background could hold a lock while the next command
    // runs.
    "gc.autoDetach=false",
    "maintenance.autoDetach=false",
};

Result run(const std::vector<std::string>& args) {
  std::vector<std::string> all;
  for (size_t i = 0; i < sizeof(FORCED_CONFIG) / sizeof(FORCED_CONFIG[0]);
       ++i) {
    all.push_back("-c");
    all.push_back(FORCED_CONFIG[i]);
  }
  all.insert(all.end(), args.begin(), args.end());

  process::Environment env;
  // GIT_DIR, GIT_WORK_TREE, GIT_INDEX_FILE, ... would point git somewhere else.
  env.remove_prefixes.push_back("GIT_");
  env.set.push_back(std::make_pair("GIT_CONFIG_GLOBAL", "/dev/null"));
  env.set.push_back(std::make_pair("GIT_CONFIG_NOSYSTEM", "1"));
  env.set.push_back(std::make_pair("GIT_TERMINAL_PROMPT", "0"));

  Result result = process::run("git", all, env);
  if (result.not_found) result.err = "git is not installed.";
  return result;
}

bool parse_version(const std::string& text, int& major, int& minor) {
  return std::sscanf(text.c_str(), "git version %d.%d", &major, &minor) == 2;
}

bool available() {
  std::vector<std::string> args;
  args.push_back("--version");
  Result r = run(args);
  int major = 0;
  int minor = 0;
  if (!r.ok() || !parse_version(r.out, major, minor)) return false;
  return major > 2 || (major == 2 && minor >= 23);
}

std::string shell_quote(const std::string& word) {
  if (!word.empty() &&
      word.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVW"
                             "XYZ0123456789_-./=:@%+,") == std::string::npos)
    return word;
  std::string result = "'";
  for (size_t i = 0; i < word.size(); ++i) {
    if (word[i] == '\'')
      result += "'\\''";
    else
      result += word[i];
  }
  return result + "'";
}

}  // namespace git
