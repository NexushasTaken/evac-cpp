#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/licenses.h"
#include "core/paths.h"
#include "core/settings.h"
#include "core/tips.h"
#include "core/window_state.h"

namespace fs = std::filesystem;

static std::string make_temp_dir(const std::string& name)
{
    fs::path p = fs::temp_directory_path() / ("catapult_test_" + name);
    fs::remove_all(p);
    fs::create_directories(p);
    return p.string();
}

TEST_CASE("tips")
{
    REQUIRE(tips_of_the_day().size() == 10);
    for (size_t i = 0; i < tips_of_the_day().size(); ++i)
        REQUIRE(!tips_of_the_day()[i].empty());

    REQUIRE(pick_tip(0) == tips_of_the_day()[0]);
    REQUIRE(pick_tip(13) == tips_of_the_day()[3]);
}

TEST_CASE("window state round trip keeps other keys")
{
    std::string dir = make_temp_dir("window_state");
    Status status;
    Settings settings(dir + "/s.json", status);
    settings.load();

    REQUIRE(!load_window_state(settings).valid);  // Nothing saved yet.

    // What the original launcher wrote.
    nlohmann::json old = nlohmann::json::object();
    old["size_x"] = 900.0;
    old["size_y"] = 700.0;
    old["position_x"] = 100;
    old["position_y"] = 50;
    settings.set_json("window_state", old);

    WindowState loaded = load_window_state(settings);
    REQUIRE(loaded.valid);
    REQUIRE(loaded.width == 900);
    REQUIRE(loaded.height == 700);
    REQUIRE(!loaded.maximized);

    WindowState state;
    state.width = 1024;
    state.height = 768;
    state.maximized = true;
    store_window_state(settings, state);

    loaded = load_window_state(settings);
    REQUIRE(loaded.width == 1024);
    REQUIRE(loaded.maximized);
    REQUIRE(settings.get_json("window_state")["position_x"] == 100);
}

TEST_CASE("nonsensical window sizes are ignored")
{
    std::string dir = make_temp_dir("window_state_bad");
    Status status;
    Settings settings(dir + "/s.json", status);
    settings.load();

    nlohmann::json bad = nlohmann::json::object();
    bad["size_x"] = 5;
    bad["size_y"] = 700;
    settings.set_json("window_state", bad);
    REQUIRE(!load_window_state(settings).valid);

    bad["size_x"] = "wide";
    settings.set_json("window_state", bad);
    REQUIRE(!load_window_state(settings).valid);
}

TEST_CASE("license sections can all be read")
{
    Status status;
    Settings settings(make_temp_dir("licenses") + "/s.json", status);
    Paths paths(make_temp_dir("licenses_paths"), settings, status);

    std::vector<LicenseSection> sections = load_license_sections(paths.data_dir());
    REQUIRE(sections.size() == 7);
    REQUIRE(sections[0].heading == "Catapult");
    REQUIRE(sections[0].text.find("MIT License") != std::string::npos);
    for (size_t i = 0; i < sections.size(); ++i)
    {
        REQUIRE(!sections[i].text.empty());
        REQUIRE(sections[i].text.find("(Could not read") == std::string::npos);
    }
}
