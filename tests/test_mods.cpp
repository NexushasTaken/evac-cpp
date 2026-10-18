#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/archive.h"
#include "core/helpers.h"
#include "core/mod_manager.h"
#include "core/paths.h"
#include "core/settings.h"

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

static std::string modinfo(const std::string& id, const std::string& name, const std::string& extra = "")
{
    return "{\"type\": \"MOD_INFO\", \"id\": \"" + id + "\", \"name\": \"" + name + "\"" + extra + "}";
}

struct ModFixture
{
    ModFixture(const std::string& name)
        : dir(make_temp_dir(name)), settings(dir + "/s.json", status), paths(dir, settings, status),
          trash(settings, paths, status), mods(settings, paths, status, trash)
    {
        settings.load();
        // A registered install, so that the stock mods directory is inside it.
        write_file(dir + "/dda/game0/" + INFO_FILENAME, "{\"name\": \"Test Build\"}");
        paths.installs_summary();
    }

    std::string dir;
    Status status;
    Settings settings;
    Paths paths;
    Trash trash;
    ModManager mods;
};

TEST_CASE("html tags are stripped")
{
    REQUIRE(ModManager::strip_html_tags("A <color_red>red</color> mod") == "A red mod");
    REQUIRE(ModManager::strip_html_tags("no tags") == "no tags");
    REQUIRE(ModManager::strip_html_tags("1 < 2") == "1 < 2");
}

TEST_CASE("parse_mods_dir understands the different modinfo.json shapes")
{
    std::string dir = make_temp_dir("mods_parse");
    Status status;

    write_file(dir + "/plain/modinfo.json", modinfo("plain_id", "Plain Mod"));
    write_file(dir + "/list/modinfo.json",
               "[{\"type\": \"something_else\", \"id\": \"nope\"}, " + modinfo("list_id", "<color_blue>List</color> Mod", ", \"description\": \"<b>Bold</b> text\"") + "]");
    write_file(dir + "/ident/modinfo.json", "{\"type\": \"MOD_INFO\", \"ident\": \"ident_id\", \"name\": \"Ident Mod\"}");
    write_file(dir + "/noid/modinfo.json", "{\"type\": \"MOD_INFO\", \"name\": \"Name As Id\"}");
    write_file(dir + "/comments/modinfo.json", "// a comment\n" + modinfo("comment_id", "Commented"));
    write_file(dir + "/broken/modinfo.json", "{ this is not json");
    write_file(dir + "/nomod/modinfo.json", "[{\"type\": \"other\"}]");
    write_file(dir + "/empty/readme.txt", "no modinfo here");

    std::map<std::string, ModEntry> mods = ModManager::parse_mods_dir(dir, status);
    REQUIRE(mods.size() == 5);
    REQUIRE(mods["plain_id"].name == "Plain Mod");
    REQUIRE(mods["plain_id"].location == dir + "/plain");
    REQUIRE(mods["list_id"].name == "List Mod");
    REQUIRE(mods["list_id"].modinfo["description"] == "Bold text");
    REQUIRE(mods.count("ident_id") == 1);
    REQUIRE(mods["ident_id"].modinfo["id"] == "ident_id");
    REQUIRE(mods.count("Name As Id") == 1);
    REQUIRE(mods.count("comment_id") == 1);

    REQUIRE(ModManager::parse_mods_dir(dir + "/missing", status).empty());
}

TEST_CASE("mod status")
{
    ModFixture f("mods_status");
    write_file(f.paths.mods_stock() + "/normal/modinfo.json", modinfo("stock_ok", "Stock"));
    write_file(f.paths.mods_stock() + "/old/modinfo.json", modinfo("stock_old", "Old Stock", ", \"obsolete\": true"));
    write_file(f.paths.mods_stock() + "/old2/modinfo.json", modinfo("stock_old2", "Old Stock 2", ", \"obsolete\": true"));
    write_file(f.paths.mods_user() + "/mine/modinfo.json", modinfo("mine", "Mine"));
    write_file(f.paths.mods_user() + "/old2copy/modinfo.json", modinfo("stock_old2__", "Old Stock 2*"));
    write_file(f.paths.mods_user() + "/shadowed/modinfo.json", modinfo("stock_ok", "User Copy"));

    f.mods.refresh_installed();
    REQUIRE(f.mods.mod_status("mine") == MOD_INSTALLED);
    REQUIRE(f.mods.mod_status("stock_ok") == MOD_STOCK);
    REQUIRE(f.mods.installed().at("stock_ok").name == "Stock");  // The stock mod wins.
    REQUIRE(f.mods.mod_status("stock_old") == MOD_STOCK_OBSOLETE);
    REQUIRE(f.mods.mod_status("stock_old2") == MOD_INSTALLED_MODIFIED_ID);
    REQUIRE(f.mods.mod_status("nothing") == MOD_NOT_INSTALLED);
}

TEST_CASE("install and delete mods")
{
    ModFixture f("mods_install");
    write_file(f.paths.mod_repo() + "/ModA/modinfo.json", modinfo("a", "Mod A"));
    write_file(f.paths.mod_repo() + "/ModA/items.json", "[]");
    write_file(f.paths.mod_repo() + "/ModB/modinfo.json", modinfo("b", "Mod B"));
    f.mods.refresh_installed();
    f.mods.refresh_available();
    REQUIRE(f.mods.available().size() == 2);

    std::vector<std::string> ids;
    ids.push_back("a");
    ids.push_back("b");
    REQUIRE(f.mods.install_mods(ids));
    REQUIRE(fs::exists(f.paths.mods_user() + "/ModA/items.json"));
    REQUIRE(fs::exists(f.paths.mods_user() + "/ModB/modinfo.json"));
    REQUIRE(f.mods.mod_status("a") == MOD_INSTALLED);

    std::vector<std::string> unknown(1, "zzz");
    REQUIRE(!f.mods.install_mods(unknown));

    std::vector<std::string> only_a(1, "a");
    REQUIRE(f.mods.delete_mods(only_a));
    REQUIRE(!fs::exists(f.paths.mods_user() + "/ModA"));
    REQUIRE(fs::exists(f.paths.mods_user() + "/ModB"));
    REQUIRE(f.mods.mod_status("a") == MOD_NOT_INSTALLED);
    std::vector<TrashItem> trashed = f.trash.list();
    REQUIRE(trashed.size() == 1);
    REQUIRE(trashed[0].kind == "mod");
    REQUIRE(trashed[0].original_path == f.paths.mods_user() + "/ModA");
    REQUIRE(fs::exists(f.paths.trash_dir() + "/" + trashed[0].id + "/item/items.json"));

    REQUIRE(!f.mods.delete_mods(only_a));  // Already gone.
}

TEST_CASE("stock mods are never deleted")
{
    ModFixture f("mods_stock_delete");
    write_file(f.paths.mods_stock() + "/normal/modinfo.json", modinfo("stock_ok", "Stock"));
    f.mods.refresh_installed();

    std::vector<std::string> ids(1, "stock_ok");
    REQUIRE(!f.mods.delete_mods(ids));
    REQUIRE(fs::exists(f.paths.mods_stock() + "/normal/modinfo.json"));
    REQUIRE(f.trash.list().empty());
}

TEST_CASE("a mod that clashes with an obsolete stock mod gets a new ID")
{
    ModFixture f("mods_clash");
    write_file(f.paths.mods_stock() + "/old/modinfo.json", modinfo("shared", "Old Version", ", \"obsolete\": true"));
    write_file(f.paths.mod_repo() + "/New/modinfo.json", modinfo("shared", "New Version"));
    f.mods.refresh_installed();
    f.mods.refresh_available();
    REQUIRE(f.mods.mod_status("shared") == MOD_STOCK_OBSOLETE);

    std::vector<std::string> ids(1, "shared");
    REQUIRE(f.mods.install_mods(ids));

    nlohmann::json written;
    REQUIRE(load_json_file(f.paths.mods_user() + "/New/modinfo.json", written, f.status));
    REQUIRE(written["id"] == "shared__");
    REQUIRE(written["name"] == "New Version*");
    REQUIRE(f.mods.mod_status("shared") == MOD_INSTALLED_MODIFIED_ID);

    // Deleting by the original ID removes our copy, not the stock mod.
    REQUIRE(f.mods.delete_mods(ids));
    REQUIRE(!fs::exists(f.paths.mods_user() + "/New"));
    REQUIRE(fs::exists(f.paths.mods_stock() + "/old/modinfo.json"));
    REQUIRE(f.mods.mod_status("shared") == MOD_STOCK_OBSOLETE);
}

// A zip that looks like the Kenan modpack archives.
static std::string make_modpack_zip(ModFixture& f, const std::string& marker)
{
    write_file(f.dir + "/zipsrc/Pack-master/Sub/High/ModA/modinfo.json", modinfo("a", "Mod A", ", \"description\": \"" + marker + "\""));
    write_file(f.dir + "/zipsrc/Pack-master/Sub/High/ModB/modinfo.json", modinfo("b", "Mod B"));
    write_file(f.dir + "/zipsrc/Pack-master/Sub/Archived/OldMod/modinfo.json", modinfo("old", "Old Mod"));
    std::string zip = f.dir + "/" + marker + ".zip";
    REQUIRE(archive_helper::zip(f.dir + "/zipsrc", "Pack-master", zip, f.status));
    fs::remove_all(f.dir + "/zipsrc");
    return zip;
}

static Modpack test_modpack(const std::string& zip)
{
    Modpack pack;
    pack.name = "Test Pack";
    pack.url = "file://" + zip;
    pack.filename = "test-pack.zip";
    pack.internal_paths.push_back("Pack-master/Sub/High");
    pack.archived_path = "Pack-master/Sub/Archived";
    return pack;
}

TEST_CASE("retrieving a modpack fills the repository")
{
    ModFixture f("mods_pack");
    write_file(f.paths.mod_repo() + "/Stale/modinfo.json", modinfo("stale", "Stale Mod"));
    Modpack pack = test_modpack(make_modpack_zip(f, "first"));

    REQUIRE(f.mods.retrieve_modpack(pack));
    REQUIRE(f.mods.available().size() == 2);
    REQUIRE(f.mods.available().count("a") == 1);
    REQUIRE(f.mods.available().count("old") == 0);    // Archived mods are optional.
    REQUIRE(f.mods.available().count("stale") == 0);  // The repository was wiped.
    REQUIRE(!fs::exists(f.paths.tmp_dir()));
    REQUIRE(!fs::exists(f.paths.cache_dir() + "/test-pack.zip"));  // keep_cache is off.

    f.settings.set_bool("install_archived_mods", true);
    REQUIRE(f.mods.retrieve_modpack(pack));
    REQUIRE(f.mods.available().size() == 3);
    REQUIRE(f.mods.available().count("old") == 1);
}

TEST_CASE("a bad modpack leaves the repository alone")
{
    ModFixture f("mods_pack_bad");
    write_file(f.paths.mod_repo() + "/Keep/modinfo.json", modinfo("keep", "Keep Me"));

    // The download fails.
    Modpack pack = test_modpack(f.dir + "/does-not-exist.zip");
    REQUIRE(!f.mods.retrieve_modpack(pack));

    // The archive is fine, but not what we expect.
    Modpack wrong = test_modpack(make_modpack_zip(f, "second"));
    wrong.internal_paths[0] = "Pack-master/Elsewhere";
    REQUIRE(!f.mods.retrieve_modpack(wrong));

    REQUIRE(fs::exists(f.paths.mod_repo() + "/Keep/modinfo.json"));
}

TEST_CASE("modpacks exist for dda and bn only")
{
    REQUIRE(ModManager::modpack_for_game("dda") != nullptr);
    REQUIRE(ModManager::modpack_for_game("bn") != nullptr);
    REQUIRE(ModManager::modpack_for_game("bn")->internal_paths.size() == 2);
    REQUIRE(ModManager::modpack_for_game("eod") == nullptr);
}
