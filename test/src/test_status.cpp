#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "core/status.h"

TEST_CASE("status buffers messages until a listener is set") {
  Status status;
  status.post("one");
  status.post("two", MSG_ERROR);

  std::vector<StatusMessage> got;
  status.set_listener([&got](const StatusMessage& m) { got.push_back(m); });
  REQUIRE(got.size() == 2);
  REQUIRE(got[1].type == MSG_ERROR);

  status.post("three");
  REQUIRE(got.size() == 3);
}

TEST_CASE("debug messages are dropped unless enabled") {
  Status status;
  std::vector<StatusMessage> got;
  status.set_listener([&got](const StatusMessage& m) { got.push_back(m); });

  status.post("hidden", MSG_DEBUG);
  REQUIRE(got.empty());

  status.set_debug_enabled(true);
  status.post("shown", MSG_DEBUG);
  REQUIRE(got.size() == 1);
}

TEST_CASE("message format") {
  REQUIRE(Status::format("hi", MSG_INFO, "[T]") == "[T] hi");
  REQUIRE(Status::format("hi", MSG_WARN, "[T]") == "[T] [Warning] hi");
}
