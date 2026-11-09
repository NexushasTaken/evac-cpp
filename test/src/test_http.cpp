#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "core/http.h"
#include "core/settings.h"
#include "core/status.h"

TEST_CASE("progress string") {
  REQUIRE(Http::format_progress({512L * 1024, 1024L * 1024,
                                 Http::calc_bps(2000L, 512.0 * 1024.0)}) ==
          "Download progress: 512.00 KiB (50.00%) at 256.00 KiB/s");
  REQUIRE(Http::format_progress(
              {10L * 1024 * 1024, 0, Http::calc_bps(1000, 5 * 1024 * 1024)}) ==
          "Download progress: 10.00 MiB at 5.00 MiB/s");
}

TEST_CASE("size string") {
  REQUIRE(Http::format_size(512 * 1024) == "512.00 KiB");
  REQUIRE(Http::format_size(137811281) == "131.43 MiB");
  REQUIRE(Http::format_size(3LL * 1024 * 1024 * 1024) == "3.00 GiB");
}

TEST_CASE(
    "a cached archive is not downloaded, so there is no size to look up") {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "evac_test_describe";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  {
    std::ofstream f(dir / "game.zip");
    f << std::string(2048, 'x');
  }

  Status status;
  Settings settings((dir / "s.json").string(), status);
  settings.load();
  Http http(status);

  // The URL is never contacted while the archive is in the cache.
  REQUIRE(http.describe_download(settings, dir.string(),
                                 "http://127.0.0.1:1/game.zip", "game.zip") ==
          "Nothing to download: game.zip is already in the cache (2.00 KiB).");

  std::filesystem::remove_all(dir);
}

// Network tests only run when EVAC_TEST_URL points at a server that serves
// a file called hello.txt containing "hello\n" (see the README).
TEST_CASE("get and download against a local server", "[network]") {
  const char* base = std::getenv("EVAC_TEST_URL");
  if (base == nullptr) {
    SKIP("EVAC_TEST_URL is not set");
  }

  Status status;
  Http http(status);

  HttpResponse response;
  REQUIRE(http.get(std::string(base) + "/hello.txt", std::vector<std::string>(),
                   response));
  REQUIRE(response.code == 200);
  REQUIRE(response.body == "hello\n");

  REQUIRE(http.get(std::string(base) + "/missing", std::vector<std::string>(),
                   response));
  REQUIRE(response.code == 404);

  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "evac_test_http";
  std::filesystem::remove_all(dir);
  REQUIRE(http.download_file(std::string(base) + "/hello.txt", dir.string(),
                             "got.txt"));
  const std::ifstream f(dir / "got.txt");
  std::stringstream ss;
  ss << f.rdbuf();
  REQUIRE(ss.str() == "hello\n");
  REQUIRE(!std::filesystem::exists(dir / "got.txt.part"));

  REQUIRE(!http.download_file(std::string(base) + "/missing", dir.string(),
                              "nope.txt"));
  REQUIRE(!std::filesystem::exists(dir / "nope.txt"));
}

// Checks that HTTPS and the system certificates work, e.g.
// EVAC_TEST_HTTPS_URL=https://api.github.com/zen
TEST_CASE("https request", "[network]") {
  const char* url = std::getenv("EVAC_TEST_HTTPS_URL");
  if (url == nullptr) {
    SKIP("EVAC_TEST_HTTPS_URL is not set");
  }

  Status status;
  Http http(status);
  HttpResponse response;
  REQUIRE(http.get(url, std::vector<std::string>(), response));
  REQUIRE(response.code == 200);
  REQUIRE(!response.body.empty());
}
