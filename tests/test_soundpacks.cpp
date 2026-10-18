#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/archive.h"
#include "core/helpers.h"
#include "core/paths.h"
#include "core/settings.h"
#include "core/soundpack_manager.h"

namespace fs = std::filesystem;

static std::string make_temp_dir(const std::string& name)
{
    fs::path p = fs::temp_directory_path() / ("catapult_test_" + name);
    fs::remove_all(p);
    fs::create_directories(p);
    return p.string();
}

static void write_file(const std::string& path, const std::string& content)
{
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream(path) << content;
}

static std::string read_file(const std::string& path)
{
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct SoundFixture
{
    SoundFixture(const std::string& name)
        : dir(make_temp_dir(name)),
          settings(dir + "/s.json", status),
          paths(dir, settings, status),
          sounds(settings, paths, status)
    {
        settings.load();
    }

    // Makes a zip like the real soundpack archives: a directory with a soundpack.txt in it.
    std::string make_pack_zip(const std::string& dir_in_zip, const std::string& marker)
    {
        write_file(dir + "/zipsrc/" + dir_in_zip + "/soundpack.txt", "VIEW: Test Pack\nDESCRIPTION: " + marker + "\n");
        write_file(dir + "/zipsrc/" + dir_in_zip + "/sfx/boom.ogg", marker);
        std::string zip = dir + "/" + marker + ".zip";
        REQUIRE(archive_helper::zip(dir + "/zipsrc", dir_in_zip, zip, status));
        fs::remove_all(dir + "/zipsrc");
        return zip;
    }

    std::string dir;
    Status status;
    Settings settings;
    Paths paths;
    SoundpackManager sounds;
};

TEST_CASE("soundpack table")
{
    const std::vector<SoundpackSource>& packs = available_soundpacks();
    REQUIRE(packs.size() == 12);
    REQUIRE(packs[0].name == "CC-Sounds");
    for (size_t i = 0; i < packs.size(); ++i)
    {
        REQUIRE(!packs[i].url.empty());
        REQUIRE(!packs[i].filename.empty());
        REQUIRE(!packs[i].internal_path.empty());
    }
}

TEST_CASE("parse_sound_dir reads soundpack.txt")
{
    std::string dir = make_temp_dir("sound_parse");
    write_file(dir + "/a/soundpack.txt", "VIEW: Pack A\r\nDESCRIPTION: Loud and clear\r\nMODS: x\r\n");
    write_file(dir + "/b/soundpack.txt", "DESCRIPTION: No name here\n");
    write_file(dir + "/c/readme.txt", "not a soundpack");
    write_file(dir + "/d/soundpack.txt.disabled", "VIEW: Pack D\n");

    std::vector<InstalledSoundpack> packs = SoundpackManager::parse_sound_dir(dir);
    REQUIRE(packs.size() == 3);
    REQUIRE(packs[0].name == "Pack A");
    REQUIRE(packs[0].description == "Loud and clear");
    REQUIRE(packs[0].location == dir + "/a");
    REQUIRE(packs[0].enabled);
    REQUIRE(packs[1].name == "b");  // Falls back to the directory name.
    REQUIRE(packs[2].name == "Pack D");
    REQUIRE(!packs[2].enabled);

    REQUIRE(SoundpackManager::parse_sound_dir(dir + "/missing").empty());
}

TEST_CASE("get_installed marks stock packs")
{
    SoundFixture f("sound_installed");
    // A registered install, so that the stock directory is inside it.
    write_file(f.dir + "/dda/game0/" + INFO_FILENAME, "{\"name\": \"Test Build\"}");
    f.paths.installs_summary();
    write_file(f.paths.sound_user() + "/mine/soundpack.txt", "VIEW: Mine\n");
    write_file(f.paths.sound_stock() + "/stock/soundpack.txt", "VIEW: Stock\n");

    std::vector<InstalledSoundpack> user_only = f.sounds.get_installed(false);
    REQUIRE(user_only.size() == 1);
    REQUIRE(!user_only[0].is_stock);

    std::vector<InstalledSoundpack> all = f.sounds.get_installed(true);
    REQUIRE(all.size() == 2);
    REQUIRE(all[1].is_stock);
    REQUIRE(all[1].name == "Stock");
}

TEST_CASE("disable and enable a soundpack")
{
    SoundFixture f("sound_toggle");
    write_file(f.dir + "/dda/game0/" + INFO_FILENAME, "{\"name\": \"Test Build\"}");
    f.paths.installs_summary();
    std::string mine = f.paths.sound_user() + "/mine";
    write_file(mine + "/soundpack.txt", "VIEW: Mine\n");
    write_file(f.paths.sound_stock() + "/stock/soundpack.txt", "VIEW: Stock\n");

    REQUIRE(f.sounds.set_enabled("Mine", false));
    REQUIRE(!fs::exists(mine + "/soundpack.txt"));
    REQUIRE(fs::exists(mine + "/soundpack.txt.disabled"));
    std::vector<InstalledSoundpack> installed = f.sounds.get_installed(false);
    REQUIRE(installed.size() == 1);
    REQUIRE(!installed[0].enabled);

    REQUIRE(f.sounds.set_enabled("Mine", false));  // Already disabled: nothing to do.
    REQUIRE(fs::exists(mine + "/soundpack.txt.disabled"));

    REQUIRE(f.sounds.set_enabled("Mine", true));
    REQUIRE(fs::exists(mine + "/soundpack.txt"));
    REQUIRE(!fs::exists(mine + "/soundpack.txt.disabled"));
    REQUIRE(f.sounds.get_installed(false)[0].enabled);

    REQUIRE(!f.sounds.set_enabled("No Such Pack", false));
    REQUIRE(!f.sounds.set_enabled("Stock", false));  // Stock packs are left alone.
    REQUIRE(fs::exists(f.paths.sound_stock() + "/stock/soundpack.txt"));

    // A disabled pack can still be deleted.
    REQUIRE(f.sounds.set_enabled("Mine", false));
    REQUIRE(f.sounds.delete_pack("Mine"));
    REQUIRE(!fs::exists(mine));
}

TEST_CASE("install from a local file, reinstall and delete")
{
    SoundFixture f("sound_install");
    SoundpackSource pack;
    pack.name = "Test Pack";
    pack.internal_path = "Inner-1";

    std::string zip1 = f.make_pack_zip("Inner-1", "first");
    REQUIRE(f.sounds.install_pack(pack, zip1, false, true));
    REQUIRE(read_file(f.paths.sound_user() + "/Test Pack/sfx/boom.ogg") == "first");
    REQUIRE(fs::exists(zip1));  // keep_archive: the user's file stays.
    REQUIRE(!fs::exists(f.paths.tmp_dir() + "/Test Pack"));

    // Reinstall replaces the old contents.
    std::string zip2 = f.make_pack_zip("Inner-1", "second");
    REQUIRE(f.sounds.install_pack(pack, zip2, true, false));
    REQUIRE(read_file(f.paths.sound_user() + "/Test Pack/sfx/boom.ogg") == "second");
    REQUIRE(!fs::exists(zip2));  // Not kept.

    std::vector<InstalledSoundpack> installed = f.sounds.get_installed(false);
    REQUIRE(installed.size() == 1);
    REQUIRE(installed[0].name == "Test Pack");

    REQUIRE(!f.sounds.delete_pack("No Such Pack"));
    REQUIRE(f.sounds.delete_pack("Test Pack"));
    REQUIRE(!fs::exists(f.paths.sound_user() + "/Test Pack"));
}

TEST_CASE("install downloads the archive and uses the cache")
{
    SoundFixture f("sound_download");
    std::string zip = f.make_pack_zip("Inner-2", "downloaded");

    SoundpackSource pack;
    pack.name = "Test Pack";
    pack.url = "file://" + zip;
    pack.filename = "test-pack.zip";
    pack.internal_path = "Inner-2";

    REQUIRE(f.sounds.install_pack(pack, "", false, false));
    REQUIRE(read_file(f.paths.sound_user() + "/Test Pack/sfx/boom.ogg") == "downloaded");
    REQUIRE(!fs::exists(f.paths.cache_dir() + "/test-pack.zip"));  // keep_cache is off.

    f.settings.set_bool("keep_cache", true);
    REQUIRE(f.sounds.install_pack(pack, "", true, false));
    REQUIRE(fs::exists(f.paths.cache_dir() + "/test-pack.zip"));
}

TEST_CASE("failed installs leave the old copy alone")
{
    SoundFixture f("sound_fail");
    SoundpackSource pack;
    pack.name = "Test Pack";
    pack.url = "file://" + f.dir + "/nope.zip";
    pack.filename = "nope.zip";
    pack.internal_path = "Inner-1";

    std::string zip = f.make_pack_zip("Inner-1", "good");
    REQUIRE(f.sounds.install_pack(pack, zip, false, true));

    // Download fails.
    REQUIRE(!f.sounds.install_pack(pack, "", true, false));
    // The archive is not a zip.
    write_file(f.dir + "/bad.zip", "garbage");
    REQUIRE(!f.sounds.install_pack(pack, f.dir + "/bad.zip", true, true));
    // The archive does not have the expected directory.
    std::string wrong = f.make_pack_zip("Other-Dir", "wrong");
    REQUIRE(!f.sounds.install_pack(pack, wrong, true, true));

    REQUIRE(read_file(f.paths.sound_user() + "/Test Pack/sfx/boom.ogg") == "good");
}
