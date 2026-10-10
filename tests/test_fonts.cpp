#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "core/font_manager.h"
#include "core/helpers.h"
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

// Same shape as what the game creates.
static void write_game_config(const std::string& config_dir)
{
    write_file(config_dir + "/fonts.json",
               "{\"typeface\": [\"data/font/Terminus.ttf\", \"data/font/unifont.ttf\"],"
               " \"map_typeface\": [\"data/font/Terminus.ttf\", \"data/font/unifont.ttf\"],"
               " \"overmap_typeface\": [\"data/font/Terminus.ttf\", \"data/font/unifont.ttf\"]}");
    write_file(config_dir + "/options.json",
               "[{\"name\": \"FONT_SIZE\", \"value\": \"16\"}, {\"name\": \"FONT_HEIGHT\", \"value\": \"16\"},"
               " {\"name\": \"FONT_WIDTH\", \"value\": \"8\"}, {\"name\": \"MAP_FONT_SIZE\", \"value\": \"16\"},"
               " {\"name\": \"MAP_FONT_HEIGHT\", \"value\": \"16\"}, {\"name\": \"MAP_FONT_WIDTH\", \"value\": \"16\"},"
               " {\"name\": \"OVERMAP_FONT_SIZE\", \"value\": \"16\"}, {\"name\": \"OVERMAP_FONT_HEIGHT\", \"value\": \"16\"},"
               " {\"name\": \"OVERMAP_FONT_WIDTH\", \"value\": \"16\"}, {\"name\": \"FONT_BLENDING\", \"value\": \"false\"}]");
}

struct FontFixture
{
    FontFixture(const std::string& name)
        : dir(make_temp_dir(name)), settings(dir + "/s.json", status), paths(dir, settings, status), fonts(paths, status)
    {
        settings.load();
    }

    std::string dir;
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
        REQUIRE(fs::exists(f.paths.data_dir() + "/fonts/" + font.file));
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

TEST_CASE("set_font installs the font and edits fonts.json")
{
    FontFixture f("fonts_set");
    write_game_config(f.paths.config());
    REQUIRE(f.fonts.load_available_fonts());
    REQUIRE(f.fonts.load_font_config());

    // Index 2 is Hack; set it for the UI and the map only.
    REQUIRE(f.fonts.set_font(2, true, true, false));
    REQUIRE(fs::exists(f.paths.font_user() + "/Hack-Regular.ttf"));

    nlohmann::json saved;
    REQUIRE(load_json_file(f.paths.config() + "/fonts.json", saved, f.status));
    REQUIRE(saved["typeface"].size() == 3);
    REQUIRE(saved["typeface"][0] == "Hack-Regular.ttf");
    REQUIRE(saved["typeface"][1] == "data/font/Terminus.ttf");
    REQUIRE(saved["map_typeface"][0] == "Hack-Regular.ttf");
    REQUIRE(saved["overmap_typeface"].size() == 2);

    // Setting another font replaces ours instead of piling up.
    REQUIRE(f.fonts.set_font(0, true, false, false));
    REQUIRE(f.fonts.font_files("typeface").size() == 3);
    REQUIRE(f.fonts.font_files("typeface")[0] == "FantasqueSansMono-Regular.ttf");

    REQUIRE(!f.fonts.set_font(99, true, true, true));
}

TEST_CASE("reset_font brings back the defaults")
{
    FontFixture f("fonts_reset");
    write_game_config(f.paths.config());
    REQUIRE(f.fonts.load_available_fonts());
    REQUIRE(f.fonts.load_font_config());
    REQUIRE(f.fonts.set_font(2, true, true, true));

    REQUIRE(f.fonts.reset_font());
    std::vector<std::string> files = f.fonts.font_files("overmap_typeface");
    REQUIRE(files.size() == 2);
    REQUIRE(files[0] == "data/font/Terminus.ttf");
    REQUIRE(files[1] == "data/font/unifont.ttf");
}

TEST_CASE("font sizes use the ratios of the selected fonts")
{
    FontFixture f("fonts_sizes");
    write_game_config(f.paths.config());
    REQUIRE(f.fonts.load_available_fonts());
    REQUIRE(f.fonts.load_font_config());
    REQUIRE(f.fonts.load_game_options());

    // Hack: height 1.2, width 0.55. Terminus (not bundled): 1.0 and 0.5.
    REQUIRE(f.fonts.set_font(2, true, false, false));
    f.fonts.set_font_sizes(20, 10, 12);

    REQUIRE(f.fonts.get_game_option("FONT_SIZE") == "20");
    REQUIRE(f.fonts.get_game_option("FONT_HEIGHT") == "24");
    REQUIRE(f.fonts.get_game_option("FONT_WIDTH") == "11");
    REQUIRE(f.fonts.get_game_option("MAP_FONT_HEIGHT") == "10");
    REQUIRE(f.fonts.get_game_option("MAP_FONT_WIDTH") == "10");
    REQUIRE(f.fonts.get_game_option("OVERMAP_FONT_SIZE") == "12");

    REQUIRE(f.fonts.write_game_options());
    nlohmann::json saved;
    REQUIRE(load_json_file(f.paths.config() + "/options.json", saved, f.status));
    REQUIRE(saved[0]["value"] == "20");
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
