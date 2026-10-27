#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "core/paths.h"
#include "core/platform.h"
#include "core/process.h"
#include "core/release_data.h"
#include "core/release_manager.h"

TEST_CASE("windows arguments are quoted for CommandLineToArgvW")
{
    REQUIRE(process::windows_quote_arg("plain") == "plain");
    REQUIRE(process::windows_quote_arg("C:\\games\\dda") == "C:\\games\\dda");
    REQUIRE(process::windows_quote_arg("") == "\"\"");
    REQUIRE(process::windows_quote_arg("My World") == "\"My World\"");
    REQUIRE(process::windows_quote_arg("say \"hi\"") == "\"say \\\"hi\\\"\"");
    // Backslashes before the closing quote are doubled, so it stays a quote.
    REQUIRE(process::windows_quote_arg("C:\\My Games\\") == "\"C:\\My Games\\\\\"");
    // Backslashes before a quote: doubled, plus one for the quote.
    REQUIRE(process::windows_quote_arg("a\\\"b c") == "\"a\\\\\\\"b c\"");
}

TEST_CASE("windows command line")
{
    std::vector<std::string> args = {"--userdir", "C:\\Users\\Me\\My Data\\"};
    REQUIRE(process::windows_command_line("C:\\Program Files\\dda\\cataclysm-tiles.exe", args)
            == "\"C:\\Program Files\\dda\\cataclysm-tiles.exe\" --userdir \"C:\\Users\\Me\\My Data\\\\\"");
    REQUIRE(process::windows_command_line("git", {"status"}) == "git status");
}

TEST_CASE("environment changes")
{
    process::Environment env;
    env.remove_prefixes.push_back("GIT_");
    env.set.push_back(std::make_pair("HOME", "/new"));
    env.set.push_back(std::make_pair("EXTRA", "1"));
    std::vector<std::string> current = {"PATH=/bin", "GIT_DIR=/x", "git_dir=/y", "HOME=/old", "HOMEPAGE=z"};

    std::vector<std::string> result = process::apply_environment(current, env, false);
    std::vector<std::string> expected = {"PATH=/bin", "git_dir=/y", "HOMEPAGE=z", "HOME=/new", "EXTRA=1"};
    REQUIRE(result == expected);

    // Windows: names are not case-sensitive.
    result = process::apply_environment(current, env, true);
    expected = {"PATH=/bin", "HOMEPAGE=z", "HOME=/new", "EXTRA=1"};
    REQUIRE(result == expected);
}

TEST_CASE("windows and macos locations")
{
    Locations loc = windows_locations("C:\\Users\\Me\\AppData\\Roaming", "C:\\Users\\Me\\AppData\\Local");
    REQUIRE(loc.config_dir == "C:\\Users\\Me\\AppData\\Roaming/evac");
    REQUIRE(loc.games_dir == "C:\\Users\\Me\\AppData\\Local/evac");
    REQUIRE(loc.cache_dir == "C:\\Users\\Me\\AppData\\Local/evac/cache");

    loc = macos_locations("/Users/me");
    REQUIRE(loc.config_dir == "/Users/me/Library/Application Support/evac");
    REQUIRE(loc.games_dir == "/Users/me/Library/Application Support/evac");
    REQUIRE(loc.cache_dir == "/Users/me/Library/Caches/evac");
}

TEST_CASE("no releases for a platform without builds")
{
    REQUIRE(stable_releases("dda", "mac").empty());
    REQUIRE(ReleaseManager::asset_substring("dda", "mac").empty());
    REQUIRE(ReleaseManager::asset_substring("bn", "win") == "cbn-windows-tiles-x64");
}

TEST_CASE("the platform knows the executable")
{
    std::string os = platform::os_name();
    REQUIRE((os == "linux" || os == "win" || os == "mac"));
    std::string exe = platform::executable_path();
    REQUIRE(!exe.empty());
    REQUIRE(platform::is_executable_file(exe));
}

#ifndef _WIN32
TEST_CASE("run collects output and the exit code")
{
    process::Result r = process::run("sh", {"-c", "echo out; echo err >&2; exit 3"}, process::Environment());
    REQUIRE(r.started);
    REQUIRE(!r.not_found);
    REQUIRE(r.exit_code == 3);
    REQUIRE(r.out == "out\n");
    REQUIRE(r.err == "err\n");
}

TEST_CASE("run passes the changed environment")
{
    process::Environment env;
    env.remove_prefixes.push_back("HOME");
    env.set.push_back(std::make_pair("EVAC_TEST_VAR", "hello"));
    process::Result r = process::run("sh", {"-c", "echo \"$EVAC_TEST_VAR ${HOME-unset}\""}, env);
    REQUIRE(r.ok());
    REQUIRE(r.out == "hello unset\n");
}

TEST_CASE("a detached program runs in its working directory")
{
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "evac_test_detached";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string error;
    REQUIRE(process::spawn_detached("/bin/sh", {"-c", "pwd > out.tmp && mv out.tmp out"}, dir.string(), error));

    // It is not our child, so poll for the file.
    for (int i = 0; i < 100 && !std::filesystem::exists(dir / "out"); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    std::ifstream in(dir / "out");
    std::string line;
    std::getline(in, line);
    REQUIRE(std::filesystem::equivalent(line, dir));
}

TEST_CASE("run reports a missing program")
{
    process::Result r = process::run("evac-no-such-program", {}, process::Environment());
    REQUIRE(!r.started);
    REQUIRE(r.not_found);
}
#endif
