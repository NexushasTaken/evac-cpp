#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "core/game_launcher.h"

static std::vector<std::string> split(const std::string& text) {
  std::vector<std::string> words;
  std::string error;
  REQUIRE(split_command_line(text, words, error));
  return words;
}

TEST_CASE("command line is split on spaces") {
  REQUIRE(split("").empty());
  REQUIRE(split("   ").empty());
  REQUIRE(split("--debug") == std::vector<std::string>{"--debug"});
  REQUIRE(split("  --a   b\t--c\n") ==
          std::vector<std::string>{"--a", "b", "--c"});
}

TEST_CASE("command line quotes keep spaces") {
  REQUIRE(split("--x 'a b'") == std::vector<std::string>{"--x", "a b"});
  REQUIRE(split("--x \"a b\"") == std::vector<std::string>{"--x", "a b"});
  REQUIRE(split("--x=\"a b\"c") == std::vector<std::string>{"--x=a bc"});
  REQUIRE(split("'' x") == std::vector<std::string>{"", "x"});
}

TEST_CASE("command line escapes") {
  REQUIRE(split("a\\ b") == std::vector<std::string>{"a b"});
  REQUIRE(split("\"say \\\"hi\\\"\"") ==
          std::vector<std::string>{"say \"hi\""});
  REQUIRE(split("\"C:\\dir\"") ==
          std::vector<std::string>{"C:\\dir"});  // Other backslashes stay.
  REQUIRE(split("'a\\b'") == std::vector<std::string>{"a\\b"});
  REQUIRE(split("$HOME ~ *") == std::vector<std::string>{"$HOME", "~", "*"});
}

TEST_CASE("command line with an open quote is an error") {
  std::vector<std::string> words;
  std::string error;
  REQUIRE_FALSE(split_command_line("--x 'a b", words, error));
  REQUIRE_FALSE(error.empty());
  error.clear();
  REQUIRE_FALSE(split_command_line("--x \"a b", words, error));
  REQUIRE_FALSE(error.empty());
}
