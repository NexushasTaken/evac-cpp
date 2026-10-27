#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/archive.h"
#include "core/fs.h"

namespace fs = std::filesystem;

static fs::path make_temp_dir(const std::string& name)
{
    fs::path p = fs::temp_directory_path() / ("evac_test_" + name);
    fs::remove_all(p);
    fs::create_directories(p);
    return p;
}

static void write_file(const fs::path& path, const std::string& content)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path) << content;
}

static std::string read_file(const fs::path& path)
{
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

TEST_CASE("list_dir lists, recurses and filters")
{
    fs::path dir = make_temp_dir("list");
    Status status;
    write_file(dir / "a.txt", "a");
    write_file(dir / "sub/b.json", "b");

    std::vector<std::string> flat = fs_helper::list_dir(dir, false, "", status);
    REQUIRE(flat.size() == 2);  // a.txt and sub

    std::vector<std::string> all = fs_helper::list_dir(dir, true, "", status);
    REQUIRE(all.size() == 3);

    std::vector<std::string> json = fs_helper::list_dir(dir, true, "\\.json$", status);
    REQUIRE(json.size() == 1);
    REQUIRE(json[0] == "sub/b.json");
}

TEST_CASE("copy_dir copies into the destination")
{
    fs::path dir = make_temp_dir("copy");
    Status status;
    write_file(dir / "src/x.txt", "x");
    write_file(dir / "src/deep/y.txt", "y");

    REQUIRE(fs_helper::copy_dir(dir / "src", dir / "dest", status));
    REQUIRE(read_file(dir / "dest/src/x.txt") == "x");
    REQUIRE(read_file(dir / "dest/src/deep/y.txt") == "y");
    REQUIRE(fs::exists(dir / "src/x.txt"));
}

TEST_CASE("rm_dir removes recursively")
{
    fs::path dir = make_temp_dir("rm");
    Status status;
    write_file(dir / "d/a/b.txt", "b");
    REQUIRE(fs_helper::rm_dir(dir / "d", status));
    REQUIRE(!fs::exists(dir / "d"));
}

TEST_CASE("move_dir renames and merges")
{
    fs::path dir = make_temp_dir("move");
    Status status;
    write_file(dir / "from/a.txt", "a");
    write_file(dir / "from/sub/b.txt", "b");

    REQUIRE(fs_helper::move_dir(dir / "from", dir / "to", status));
    REQUIRE(!fs::exists(dir / "from"));
    REQUIRE(read_file(dir / "to/sub/b.txt") == "b");

    // Destination exists now: contents get merged.
    write_file(dir / "from2/c.txt", "c");
    REQUIRE(fs_helper::move_dir(dir / "from2", dir / "to", status));
    REQUIRE(read_file(dir / "to/c.txt") == "c");
    REQUIRE(read_file(dir / "to/a.txt") == "a");
}

TEST_CASE("zip then extract round trip")
{
    fs::path dir = make_temp_dir("zip");
    Status status;
    write_file(dir / "save/World/data.bin", "hello");
    write_file(dir / "save/World/sub/more.txt", "more");

    REQUIRE(archive_helper::zip(dir / "save", "World", dir / "World.zip", status));
    REQUIRE(archive_helper::extract(dir / "World.zip", dir / "out", status));
    REQUIRE(read_file(dir / "out/World/data.bin") == "hello");
    REQUIRE(read_file(dir / "out/World/sub/more.txt") == "more");
}

TEST_CASE("names outside ASCII survive a zip round trip")
{
    fs::path dir = make_temp_dir("zip_utf8");
    Status status;
    // Not in the Windows ANSI code pages of Western Europe, so this fails if a
    // name goes through one.
    std::string world = "\xd0\x9c\xd0\xb8\xd1\x80 \xc3\x84";  // "Мир Ä" in UTF-8.
    write_file(dir / "save" / to_path(world) / "\xc3\xa9t\xc3\xa9.txt", "summer");

    REQUIRE(archive_helper::zip(dir / "save", world, dir / to_path(world + ".zip"), status));
    REQUIRE(archive_helper::extract(dir / to_path(world + ".zip"), dir / "out", status));
    REQUIRE(read_file(dir / "out" / to_path(world) / to_path("\xc3\xa9t\xc3\xa9.txt")) == "summer");
    REQUIRE(fs_helper::list_dir(dir / "out", false, "", status) == std::vector<std::string>{world});
}

TEST_CASE("extract handles tar.gz")
{
    fs::path dir = make_temp_dir("targz");
    Status status;
    write_file(dir / "pack/game/file.txt", "game");

    std::string cmd = "tar -czf '" + to_utf8(dir / "pack.tar.gz") + "' -C '" + to_utf8(dir / "pack") + "' game";
    if (std::system(cmd.c_str()) != 0)
        SKIP("tar is not available");

    REQUIRE(archive_helper::extract(dir / "pack.tar.gz", dir / "out", status));
    REQUIRE(read_file(dir / "out/game/file.txt") == "game");
}

TEST_CASE("extract fails on garbage")
{
    fs::path dir = make_temp_dir("garbage");
    Status status;
    write_file(dir / "bad.zip", "this is not an archive");
    REQUIRE(!archive_helper::extract(dir / "bad.zip", dir / "out", status));
}
