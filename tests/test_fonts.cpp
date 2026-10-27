#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "core/fs.h"
#include "core/font_manager.h"
#include "core/helpers.h"
#include "core/paths.h"
#include "core/settings.h"

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

// The game's default fonts, as in its data/fontdata.json. Entries are paths
// or objects with a "path".
static const char GAME_FONTS[] =
    "{\"typeface\": [{\"path\": \"data/font/Terminus.ttf\", \"hinting\": \"Bitmap\"}, \"data/font/unifont.ttf\"],"
    " \"gui_typeface\": [{\"path\": \"data/font/Terminus.ttf\", \"hinting\": \"Bitmap\"}, \"data/font/unifont.ttf\"],"
    " \"map_typeface\": [{\"path\": \"data/font/Terminus.ttf\", \"hinting\": \"Bitmap\"}, \"data/font/unifont.ttf\"],"
    " \"overmap_typeface\": [{\"path\": \"data/font/Terminus.ttf\", \"hinting\": \"Bitmap\"}, \"data/font/unifont.ttf\"]}";

// Same shape as what the game creates.
static void write_game_config(const fs::path& config_dir)
{
    write_file(config_dir / "fonts.json", GAME_FONTS);
    write_file(config_dir / "options.json",
               "[{\"name\": \"FONT_SIZE\", \"value\": \"16\"}, {\"name\": \"FONT_HEIGHT\", \"value\": \"16\"},"
               " {\"name\": \"FONT_WIDTH\", \"value\": \"8\"}, {\"name\": \"MAP_FONT_SIZE\", \"value\": \"16\"},"
               " {\"name\": \"MAP_FONT_HEIGHT\", \"value\": \"16\"}, {\"name\": \"MAP_FONT_WIDTH\", \"value\": \"16\"},"
               " {\"name\": \"OVERMAP_FONT_SIZE\", \"value\": \"16\"}, {\"name\": \"OVERMAP_FONT_HEIGHT\", \"value\": \"16\"},"
               " {\"name\": \"OVERMAP_FONT_WIDTH\", \"value\": \"16\"}, {\"name\": \"FONT_BLENDING\", \"value\": \"false\"}]");
}

struct FontFixture
{
    FontFixture(const std::string& name)
        : dir(make_temp_dir(name)), settings(dir / "s.json", status), paths(dir, settings, status), fonts(paths, status)
    {
        settings.load();
    }

    fs::path dir;
    Status status;
    Settings settings;
    Paths paths;
    FontManager fonts;
};

TEST_CASE("bundled font info is valid and the files exist")
{
    FontFixture f("fonts_info");
    REQUIRE(f.fonts.load_available_fonts());
    REQUIRE(f.fonts.available_fonts().size() == 10);

    for (size_t i = 0; i < f.fonts.available_fonts().size(); ++i)
    {
        const FontInfo& font = f.fonts.available_fonts()[i];
        REQUIRE(fs::exists(f.paths.data_dir() / "fonts" / font.file));
        REQUIRE(!font.description.empty());
    }
    REQUIRE(f.fonts.available_fonts()[2].name == "Hack");
    REQUIRE(f.fonts.available_fonts()[2].height == 1.2);
}

TEST_CASE("missing config files are detected")
{
    FontFixture f("fonts_missing");
    REQUIRE(!f.fonts.font_config_file_exists());
    REQUIRE(!f.fonts.options_file_exists());
    REQUIRE(!f.fonts.load_font_config());
    REQUIRE(!f.fonts.load_game_options());
}

static const std::vector<std::string> ALL_FIELDS = {"typeface", "gui_typeface", "map_typeface", "overmap_typeface"};

TEST_CASE("set_font installs the font and edits fonts.json")
{
    FontFixture f("fonts_set");
    write_game_config(f.paths.config());
    REQUIRE(f.fonts.load_available_fonts());
    REQUIRE(f.fonts.load_font_config());

    // Index 2 is Hack; set it for the UI and the map only.
    fs::path hack = f.paths.font_user() / "Hack-Regular.ttf";
    REQUIRE(f.fonts.set_font(2, {"typeface", "map_typeface"}));
    REQUIRE(fs::exists(hack));

    nlohmann::json saved;
    REQUIRE(load_json_file(f.paths.config() / "fonts.json", saved, f.status));
    REQUIRE(saved["typeface"].size() == 3);
    // The full path, which the menus need.
    REQUIRE(saved["typeface"][0]["path"] == to_utf8(hack));
    REQUIRE(saved["typeface"][0]["hinting"] == "Default");
    // The game's entries are kept as they were.
    REQUIRE(saved["typeface"][1]["path"] == "data/font/Terminus.ttf");
    REQUIRE(saved["typeface"][1]["hinting"] == "Bitmap");
    REQUIRE(saved["typeface"][2] == "data/font/unifont.ttf");
    REQUIRE(saved["map_typeface"][0]["path"] == to_utf8(hack));
    REQUIRE(saved["overmap_typeface"].size() == 2);
    REQUIRE(saved["gui_typeface"].size() == 2);

    // Setting another font replaces ours instead of piling up.
    REQUIRE(f.fonts.set_font(0, {"typeface"}));
    REQUIRE(f.fonts.font_files("typeface").size() == 3);
    REQUIRE(f.fonts.font_files("typeface")[0] == f.paths.font_user() / "FantasqueSansMono-Regular.ttf");

    // The menus too.
    REQUIRE(f.fonts.set_font(2, ALL_FIELDS));
    REQUIRE(f.fonts.font_files("gui_typeface")[0] == hack);

    REQUIRE(!f.fonts.set_font(99, ALL_FIELDS));
}

TEST_CASE("a font set by an older launcher by its file name is replaced too")
{
    FontFixture f("fonts_old_name");
    write_game_config(f.paths.config());
    write_file(f.paths.font_user() / "Hack-Regular.ttf", "font");
    write_file(f.paths.config() / "fonts.json",
               "{\"typeface\": [\"Hack-Regular.ttf\", \"data/font/unifont.ttf\"]}");
    REQUIRE(f.fonts.load_available_fonts());
    REQUIRE(f.fonts.set_font(0, {"typeface"}));
    std::vector<std::string> files = f.fonts.font_files("typeface");
    REQUIRE(files.size() == 2);
    REQUIRE(files[0] == f.paths.font_user() / "FantasqueSansMono-Regular.ttf");
}

TEST_CASE("font_files reads every entry shape of the game's format")
{
    FontFixture f("fonts_shapes");
    write_game_config(f.paths.config());
    write_file(f.paths.config() / "fonts.json",
               "{\"typeface\": [{\"path\": \"Hack-Regular.ttf\", \"hinting\": \"Light\", \"antialiasing\": false},"
               " \"data/font/unifont.ttf\"],"
               " \"map_typeface\": {\"path\": \"data/font/Terminus.ttf\", \"hinting\": \"Bitmap\"},"
               " \"overmap_typeface\": \"unifont.ttf\"}");
    REQUIRE(f.fonts.load_available_fonts());
    REQUIRE(f.fonts.load_font_config());

    std::vector<std::string> files = f.fonts.font_files("typeface");
    REQUIRE(files.size() == 2);
    REQUIRE(files[0] == "Hack-Regular.ttf");
    REQUIRE(files[1] == "data/font/unifont.ttf");
    REQUIRE(f.fonts.font_files("map_typeface") == std::vector<std::string>{"data/font/Terminus.ttf"});
    REQUIRE(f.fonts.font_files("overmap_typeface") == std::vector<std::string>{"unifont.ttf"});
    REQUIRE(!f.fonts.has_font_field("gui_typeface"));

    std::vector<FontEntry> entries = f.fonts.font_entries("typeface");
    REQUIRE(entries[0].hinting == "Light");
    REQUIRE(!entries[0].antialiasing);
    // A path alone does not say; the game decides.
    REQUIRE(entries[1].hinting == "");
    REQUIRE(entries[1].antialiasing);

    // A single entry is turned into a list with our font in front.
    REQUIRE(f.fonts.set_font(0, {"map_typeface"}));
    files = f.fonts.font_files("map_typeface");
    REQUIRE(files.size() == 2);
    REQUIRE(files[0] == f.paths.font_user() / "FantasqueSansMono-Regular.ttf");
    REQUIRE(files[1] == "data/font/Terminus.ttf");
}

TEST_CASE("fallback entries can be reordered, removed and styled")
{
    FontFixture f("fonts_entries");
    write_game_config(f.paths.config());
    REQUIRE(f.fonts.load_font_config());

    REQUIRE(f.fonts.move_entry("typeface", 1, "data/font/unifont.ttf", -1));
    REQUIRE(f.fonts.font_files("typeface")
            == std::vector<std::string>{"data/font/unifont.ttf", "data/font/Terminus.ttf"});
    REQUIRE(!f.fonts.move_entry("typeface", 0, "data/font/unifont.ttf", -1));

    // A plain path becomes an object when it gets settings.
    REQUIRE(f.fonts.set_entry_style("typeface", 0, "data/font/unifont.ttf", "None", false));
    nlohmann::json saved;
    REQUIRE(load_json_file(f.paths.config() / "fonts.json", saved, f.status));
    REQUIRE(saved["typeface"][0]["path"] == "data/font/unifont.ttf");
    REQUIRE(saved["typeface"][0]["hinting"] == "None");
    REQUIRE(saved["typeface"][0]["antialiasing"] == false);
    REQUIRE(!f.fonts.set_entry_style("typeface", 0, "data/font/unifont.ttf", "Sharp", true));

    // An entry that is not the one shown is not touched.
    REQUIRE(!f.fonts.remove_entry("typeface", 0, "data/font/Terminus.ttf"));
    REQUIRE(f.fonts.remove_entry("typeface", 1, "data/font/Terminus.ttf"));
    REQUIRE(f.fonts.font_files("typeface") == std::vector<std::string>{"data/font/unifont.ttf"});
    // The other lists are as the game wrote them.
    REQUIRE(load_json_file(f.paths.config() / "fonts.json", saved, f.status));
    REQUIRE(saved["map_typeface"] == nlohmann::json::parse(GAME_FONTS)["map_typeface"]);
}

TEST_CASE("fonts added from a file are kept in the user font directory")
{
    FontFixture f("fonts_add");
    write_game_config(f.paths.config());
    REQUIRE(f.fonts.load_available_fonts());
    size_t bundled = f.fonts.available_fonts().size();

    write_file(f.dir / "elsewhere/MyFont.otf", "font");
    write_file(f.dir / "elsewhere/notes.txt", "text");
    REQUIRE(f.fonts.add_font_file(f.dir / "elsewhere/notes.txt") == -1);
    REQUIRE(!f.fonts.add_would_replace(f.dir / "elsewhere/MyFont.otf"));
    int index = f.fonts.add_font_file(f.dir / "elsewhere/MyFont.otf");
    REQUIRE(index == (int)bundled);
    REQUIRE(fs::exists(f.paths.font_user() / "MyFont.otf"));
    REQUIRE(f.fonts.add_would_replace(f.dir / "elsewhere/MyFont.otf"));

    // Found again later, but bundled fonts installed there are not listed twice.
    REQUIRE(f.fonts.load_font_config());
    REQUIRE(f.fonts.set_font(2, {"typeface"}));
    REQUIRE(f.fonts.load_available_fonts());
    REQUIRE(f.fonts.available_fonts().size() == bundled + 1);
    REQUIRE(f.fonts.available_fonts()[bundled].name == "MyFont");
    REQUIRE(!f.fonts.available_fonts()[bundled].bundled);

    REQUIRE(f.fonts.set_font(bundled, {"typeface"}));
    REQUIRE(f.fonts.font_files("typeface")[0] == f.paths.font_user() / "MyFont.otf");

    // The bundled fonts' names are taken.
    write_file(f.dir / "elsewhere/Hack-Regular.ttf", "font");
    REQUIRE(f.fonts.add_font_file(f.dir / "elsewhere/Hack-Regular.ttf") == -1);
}

TEST_CASE("reset_font brings back the defaults")
{
    FontFixture f("fonts_reset");
    write_game_config(f.paths.config());
    REQUIRE(f.fonts.load_available_fonts());
    REQUIRE(f.fonts.load_font_config());
    REQUIRE(f.fonts.set_font(2, ALL_FIELDS));

    // No data/fontdata.json in the game: nothing to reset to.
    REQUIRE(!f.fonts.reset_font());
    REQUIRE(f.fonts.font_files("typeface")[0] == f.paths.font_user() / "Hack-Regular.ttf");

    write_file(f.dir / "dda/game0" / INFO_FILENAME, "{\"name\": \"Test Build\"}");
    f.paths.installs_summary();
    write_file(f.dir / "dda/game0/data/fontdata.json", GAME_FONTS);
    REQUIRE(f.fonts.reset_font());
    nlohmann::json saved;
    REQUIRE(load_json_file(f.paths.config() / "fonts.json", saved, f.status));
    REQUIRE(saved == nlohmann::json::parse(GAME_FONTS));
}

TEST_CASE("font sizes use the proportions of the selected fonts")
{
    FontFixture f("fonts_sizes");
    write_game_config(f.paths.config());
    REQUIRE(f.fonts.load_available_fonts());
    REQUIRE(f.fonts.load_font_config());
    REQUIRE(f.fonts.load_game_options());

    // Hack: height 1.2, width 0.55. Terminus (not one of ours): 1.0 and 0.5.
    REQUIRE(f.fonts.set_font(2, {"typeface"}));
    FontDims ui = f.fonts.auto_dims("typeface", 20);
    REQUIRE(ui.height == 24);
    REQUIRE(ui.width == 11);
    // The menus go by the UI font.
    REQUIRE(f.fonts.auto_dims("gui_typeface", 20).height == 24);
    FontDims map = f.fonts.auto_dims("map_typeface", 10);
    REQUIRE(map.height == 10);
    REQUIRE(map.width == 10);

    FontOptions options;
    options.ui = ui;
    options.map = map;
    options.overmap = {12, 7, 13};  // Set by hand.
    options.blending = true;
    options.ascii_lines = false;
    REQUIRE(f.fonts.save_font_options(options));

    REQUIRE(f.fonts.get_game_option("FONT_SIZE") == "20");
    REQUIRE(f.fonts.get_game_option("FONT_HEIGHT") == "24");
    REQUIRE(f.fonts.get_game_option("FONT_WIDTH") == "11");
    REQUIRE(f.fonts.get_game_option("MAP_FONT_HEIGHT") == "10");
    REQUIRE(f.fonts.get_game_option("OVERMAP_FONT_WIDTH") == "7");
    REQUIRE(f.fonts.get_game_option("FONT_BLENDING") == "true");
    // This game has no ASCII lines option, so none is added.
    REQUIRE(!f.fonts.has_game_option("USE_DRAW_ASCII_LINES_ROUTINE"));

    FontDims overmap = f.fonts.game_dims("overmap_typeface");
    REQUIRE(overmap.size == 12);
    REQUIRE(overmap.width == 7);
    REQUIRE(overmap.height == 13);

    nlohmann::json saved;
    REQUIRE(load_json_file(f.paths.config() / "options.json", saved, f.status));
    REQUIRE(saved[0]["value"] == "20");
}

TEST_CASE("the ASCII lines option is saved when the game has it")
{
    FontFixture f("fonts_ascii");
    write_game_config(f.paths.config());
    nlohmann::json options;
    REQUIRE(load_json_file(f.paths.config() / "options.json", options, f.status));
    options.push_back({{"name", "USE_DRAW_ASCII_LINES_ROUTINE"}, {"value", "true"}});
    REQUIRE(save_json_file(options, f.paths.config() / "options.json", f.status));

    FontOptions o;
    o.ascii_lines = false;
    REQUIRE(f.fonts.save_font_options(o));
    REQUIRE(f.fonts.get_game_option("USE_DRAW_ASCII_LINES_ROUTINE") == "false");
}

TEST_CASE("unknown game options are reported")
{
    FontFixture f("fonts_options");
    write_game_config(f.paths.config());
    REQUIRE(f.fonts.load_game_options());
    REQUIRE(!f.fonts.set_game_option("NO_SUCH_OPTION", "1"));
    REQUIRE(f.fonts.get_game_option("NO_SUCH_OPTION") == "");
    REQUIRE(f.fonts.set_game_option("FONT_BLENDING", "true"));
    REQUIRE(f.fonts.get_game_option("FONT_BLENDING") == "true");
}

TEST_CASE("font edits keep what the game saved after we read its files")
{
    FontFixture f("fonts_fresh");
    write_game_config(f.paths.config());
    REQUIRE(f.fonts.load_available_fonts());
    REQUIRE(f.fonts.load_font_config());
    REQUIRE(f.fonts.load_game_options());

    // The game runs and saves its files meanwhile.
    write_file(f.paths.config() / "options.json",
               "[{\"name\": \"USE_LANG\", \"value\": \"en\"}, {\"name\": \"FONT_SIZE\", \"value\": \"16\"},"
               " {\"name\": \"FONT_HEIGHT\", \"value\": \"16\"}, {\"name\": \"FONT_WIDTH\", \"value\": \"8\"},"
               " {\"name\": \"MAP_FONT_SIZE\", \"value\": \"16\"}, {\"name\": \"MAP_FONT_HEIGHT\", \"value\": \"16\"},"
               " {\"name\": \"MAP_FONT_WIDTH\", \"value\": \"16\"}, {\"name\": \"OVERMAP_FONT_SIZE\", \"value\": \"16\"},"
               " {\"name\": \"OVERMAP_FONT_HEIGHT\", \"value\": \"16\"}, {\"name\": \"OVERMAP_FONT_WIDTH\", \"value\": \"16\"},"
               " {\"name\": \"FONT_BLENDING\", \"value\": \"false\"}]");
    write_file(f.paths.config() / "fonts.json",
               "{\"typeface\": [\"data/font/unifont.ttf\"], \"map_typeface\": [\"data/font/unifont.ttf\"],"
               " \"overmap_typeface\": [\"data/font/unifont.ttf\"], \"gui_typeface\": [\"data/font/Roboto.ttf\"]}");

    REQUIRE(f.fonts.set_font(2, {"typeface"}));
    FontOptions options;
    options.ui = f.fonts.auto_dims("typeface", 20);
    options.blending = true;
    REQUIRE(f.fonts.save_font_options(options));

    nlohmann::json fonts;
    REQUIRE(load_json_file(f.paths.config() / "fonts.json", fonts, f.status));
    REQUIRE(fonts["gui_typeface"][0] == "data/font/Roboto.ttf");
    REQUIRE(fonts["typeface"].size() == 2);
    REQUIRE(fonts["typeface"][1] == "data/font/unifont.ttf");

    REQUIRE(f.fonts.get_game_option("USE_LANG") == "en");
    REQUIRE(f.fonts.get_game_option("FONT_SIZE") == "20");
    REQUIRE(f.fonts.get_game_option("FONT_HEIGHT") == "24");
    REQUIRE(f.fonts.get_game_option("FONT_BLENDING") == "true");
    nlohmann::json options_file;
    REQUIRE(load_json_file(f.paths.config() / "options.json", options_file, f.status));
    REQUIRE(options_file[0]["value"] == "en");
}
