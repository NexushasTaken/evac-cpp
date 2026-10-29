#include <catch2/catch_test_macros.hpp>

#include "core/version.h"

TEST_CASE("version is not empty") { REQUIRE(!evac_version().empty()); }
