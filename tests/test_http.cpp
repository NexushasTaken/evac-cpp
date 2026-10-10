#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/http.h"

TEST_CASE("progress string")
{
    REQUIRE(Http::format_progress(512 * 1024, 1024 * 1024, 2000, 512 * 1024)
            == "Download progress: 512.0 KB (50.0%) at 256 KB/s");
    REQUIRE(Http::format_progress(10.0 * 1024 * 1024, 0, 1000, 5.0 * 1024 * 1024)
            == "Download progress: 10.0 MB at 5.0 MB/s");
}

// Network tests only run when CATAPULT_TEST_URL points at a server that serves
// a file called hello.txt containing "hello\n" (see the README).
TEST_CASE("get and download against a local server", "[network]")
{
    const char* base = std::getenv("CATAPULT_TEST_URL");
    if (!base)
        SKIP("CATAPULT_TEST_URL is not set");

    Status status;
    Http http(status);

    HttpResponse response;
    REQUIRE(http.get(std::string(base) + "/hello.txt", std::vector<std::string>(), response));
    REQUIRE(response.code == 200);
    REQUIRE(response.body == "hello\n");

    REQUIRE(http.get(std::string(base) + "/missing", std::vector<std::string>(), response));
    REQUIRE(response.code == 404);

    std::filesystem::path dir = std::filesystem::temp_directory_path() / "catapult_test_http";
    std::filesystem::remove_all(dir);
    REQUIRE(http.download_file(std::string(base) + "/hello.txt", dir.string(), "got.txt"));
    std::ifstream f(dir / "got.txt");
    std::stringstream ss;
    ss << f.rdbuf();
    REQUIRE(ss.str() == "hello\n");
    REQUIRE(!std::filesystem::exists(dir / "got.txt.part"));

    REQUIRE(!http.download_file(std::string(base) + "/missing", dir.string(), "nope.txt"));
    REQUIRE(!std::filesystem::exists(dir / "nope.txt"));
}

// Checks that HTTPS and the system certificates work, e.g.
// CATAPULT_TEST_HTTPS_URL=https://api.github.com/zen
TEST_CASE("https request", "[network]")
{
    const char* url = std::getenv("CATAPULT_TEST_HTTPS_URL");
    if (!url)
        SKIP("CATAPULT_TEST_HTTPS_URL is not set");

    Status status;
    Http http(status);
    HttpResponse response;
    REQUIRE(http.get(url, std::vector<std::string>(), response));
    REQUIRE(response.code == 200);
    REQUIRE(!response.body.empty());
}
