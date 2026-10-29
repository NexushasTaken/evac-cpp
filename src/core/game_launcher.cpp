#include "core/game_launcher.h"

#include <filesystem>

#include "core/fs.h"
#include "core/helpers.h"
#include "core/platform.h"
#include "core/process.h"

namespace fs = std::filesystem;

// The file to start in an install, in the order they are tried. Linux builds
// come with a script that sets up the library path. On Windows, DDA calls the
// game cataclysm-tiles.exe and newer BN builds cataclysm-bn-tiles.exe.
static std::vector<std::string> executable_names() {
  std::vector<std::string> names;
  std::string os = platform::os_name();
  if (os == "linux") {
    names.push_back("cataclysm-launcher");
  } else if (os == "win") {
    names.push_back("cataclysm-tiles.exe");
    names.push_back("cataclysm-bn-tiles.exe");
  }
  return names;
}

bool split_command_line(const std::string& text,
                        std::vector<std::string>& words, std::string& error) {
  words.clear();
  std::string word;
  bool in_word =
      false;  // Separate from word.empty(), so that '' gives an empty word.
  size_t i = 0;
  while (i < text.size()) {
    char c = text[i];
    if (c == ' ' || c == '\t' || c == '\n') {
      if (in_word) words.push_back(word);
      word.clear();
      in_word = false;
      ++i;
    } else if (c == '\'') {
      size_t end = text.find('\'', i + 1);
      if (end == std::string::npos) {
        error = "A single quote (') is not closed.";
        return false;
      }
      word += text.substr(i + 1, end - i - 1);
      in_word = true;
      i = end + 1;
    } else if (c == '"') {
      ++i;
      while (i < text.size() && text[i] != '"') {
        if (text[i] == '\\' && i + 1 < text.size() &&
            (text[i + 1] == '"' || text[i + 1] == '\\'))
          ++i;
        word += text[i];
        ++i;
      }
      if (i >= text.size()) {
        error = "A double quote (\") is not closed.";
        return false;
      }
      in_word = true;
      ++i;
    } else if (c == '\\' && i + 1 < text.size()) {
      word += text[i + 1];
      in_word = true;
      i += 2;
    } else {
      word += c;
      in_word = true;
      ++i;
    }
  }
  if (in_word) words.push_back(word);
  return true;
}

GameLauncher::GameLauncher(Settings& settings, Paths& paths, Status& status)
    : m_settings(settings), m_paths(paths), m_status(status) {}

bool GameLauncher::has_last_world() {
  return fs::exists(m_paths.config() / "lastworld.json");
}

std::string GameLauncher::last_world() {
  nlohmann::json info;
  if (!load_json_file(m_paths.config() / "lastworld.json", info, m_status))
    return "";
  if (!info.is_object() || !info.contains("world_name") ||
      !info["world_name"].is_string())
    return "";
  return info["world_name"].get<std::string>();
}

bool GameLauncher::start(const std::string& world) {
  fs::path game_dir = m_paths.game_dir();
  // The game wants a trailing separator.
  std::string userdata = to_utf8(m_paths.userdata() / "");

  std::vector<std::string> names = executable_names();
  if (names.empty()) {
    m_status.post(
        "Cannot start the game: starting games is not supported on this "
        "platform yet.",
        MSG_ERROR);
    return false;
  }
  fs::path executable;
  for (size_t i = 0; i < names.size() && executable.empty(); ++i) {
    if (platform::is_executable_file(game_dir / names[i]))
      executable = game_dir / names[i];
  }
  if (executable.empty()) {
    m_status.post("Cannot start the game, no executable found: " +
                      to_utf8(game_dir / names[0]),
                  MSG_ERROR);
    return false;
  }

  std::vector<std::string> extra_args;
  std::string error;
  if (!split_command_line(m_settings.get_string("game_extra_args"), extra_args,
                          error)) {
    m_status.post(
        "Cannot start the game, the extra command-line parameters are wrong: " +
            error,
        MSG_ERROR);
    return false;
  }

  std::vector<std::string> args;
  args.push_back("--userdir");
  args.push_back(userdata);
  if (!world.empty()) {
    args.push_back("--world");
    args.push_back(world);
  }
  args.insert(args.end(), extra_args.begin(), extra_args.end());

  // The game finds its data folder relative to the working directory.
  if (!process::spawn_detached(executable, args, game_dir, error)) {
    m_status.post("Cannot start the game: " + error, MSG_ERROR);
    return false;
  }

  m_status.post("Started the game.", MSG_DEBUG);
  return true;
}
