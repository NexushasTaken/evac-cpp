#include "core/font_manager.h"

#include <filesystem>

#include "core/helpers.h"

namespace fs = std::filesystem;

static const char* const FIELDS[] = {"typeface", "map_typeface", "overmap_typeface"};

static std::string describe(const std::string& desc_key)
{
    if (desc_key == "font_desc_fantasque")
        return "A somewhat whimsical monospace font by Jany Belluz.";
    if (desc_key == "font_desc_hack")
        return "A workhorse font primarily designed for source code,\nwith focus on legibility. By Source Foundry Authors.";
    if (desc_key == "font_desc_inconsolata")
        return "A variant of Inconsolata font (originally by Raph Levien)\nwith addition of Greek and Cyrillic characters.";
    if (desc_key == "font_desc_pt")
        return "A typewriter font by ParaType with excellent Cyrillic typography.";
    if (desc_key == "font_desc_ubuntu")
        return "A font by Canonical with a very distinct style,\ndesigned for the eponymous Linux distribution.";
    return "";
}

FontManager::FontManager(Paths& paths, Status& status)
    : paths_(paths), status_(status), font_config_(nlohmann::json::object()), game_options_(nlohmann::json::array())
{
}

bool FontManager::font_config_file_exists()
{
    return fs::exists(paths_.config() + "/fonts.json");
}

bool FontManager::options_file_exists()
{
    return fs::exists(paths_.config() + "/options.json");
}

bool FontManager::load_available_fonts()
{
    available_fonts_.clear();

    nlohmann::json data;
    if (!load_json_file(paths_.data_dir() + "/fonts/font_info.json", data, status_) || !data.is_array())
    {
        status_.post("Could not read the bundled font info.", MSG_ERROR);
        return false;
    }

    for (size_t i = 0; i < data.size(); ++i)
    {
        const nlohmann::json& item = data[i];
        if (!item.is_object() || !item.contains("name") || !item.contains("file"))
            continue;
        FontInfo font;
        font.name = item["name"].get<std::string>();
        font.file = item["file"].get<std::string>();
        font.description = describe(item.value("desc_key", std::string()));
        font.height = item.value("height", 1.0);
        font.width = item.value("width", 0.5);
        available_fonts_.push_back(font);
    }
    return true;
}

bool FontManager::load_font_config()
{
    std::string file = paths_.config() + "/fonts.json";
    font_config_ = nlohmann::json::object();

    if (!fs::exists(file))
    {
        status_.post("Font config file " + file + " is not found!", MSG_ERROR);
        return false;
    }

    nlohmann::json data;
    if (!load_json_file(file, data, status_) || !data.is_object())
        return false;
    font_config_ = data;
    return true;
}

bool FontManager::write_font_config()
{
    return save_json_file(font_config_, paths_.config() + "/fonts.json", status_);
}

bool FontManager::load_game_options()
{
    std::string file = paths_.config() + "/options.json";
    game_options_ = nlohmann::json::array();

    if (!fs::exists(file))
    {
        status_.post("Game options file " + file + " is not found!", MSG_ERROR);
        return false;
    }

    nlohmann::json data;
    if (!load_json_file(file, data, status_) || !data.is_array())
        return false;
    game_options_ = data;
    return true;
}

bool FontManager::write_game_options()
{
    if (!save_json_file(game_options_, paths_.config() + "/options.json", status_))
        return false;
    status_.post("Saved game options to options.json.");
    return true;
}

std::vector<std::string> FontManager::font_files(const std::string& field) const
{
    std::vector<std::string> result;
    if (font_config_.contains(field) && font_config_[field].is_array())
    {
        const nlohmann::json& list = font_config_[field];
        for (size_t i = 0; i < list.size(); ++i)
        {
            if (list[i].is_string())
                result.push_back(list[i].get<std::string>());
        }
    }
    return result;
}

std::string FontManager::get_game_option(const std::string& option_name)
{
    for (size_t i = 0; i < game_options_.size(); ++i)
    {
        const nlohmann::json& item = game_options_[i];
        if (item.is_object() && item.value("name", std::string()) == option_name && item.contains("value")
            && item["value"].is_string())
            return item["value"].get<std::string>();
    }
    status_.post("Tried to get game option \"" + option_name + "\", but it wasn't found.", MSG_ERROR);
    return "";
}

bool FontManager::set_game_option(const std::string& option_name, const std::string& value)
{
    for (size_t i = 0; i < game_options_.size(); ++i)
    {
        nlohmann::json& item = game_options_[i];
        if (item.is_object() && item.value("name", std::string()) == option_name)
        {
            item["value"] = value;
            return true;
        }
    }
    status_.post("Tried to set game option \"" + option_name + "\", but it wasn't found.", MSG_ERROR);
    return false;
}

bool FontManager::install_font(const FontInfo& font)
{
    fs::path source = fs::path(paths_.data_dir()) / "fonts" / font.file;
    fs::path dest = fs::path(paths_.font_user()) / font.file;

    std::error_code ec;
    fs::create_directories(dest.parent_path(), ec);
    fs::copy_file(source, dest, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        status_.post("Could not install font file " + font.file + ": " + ec.message(), MSG_ERROR);
        status_.post("Font source: " + source.string() + ". Destination: " + dest.string() + ".", MSG_DEBUG);
        return false;
    }
    return true;
}

bool FontManager::set_font(size_t font_index, bool ui, bool map, bool overmap)
{
    if (font_index >= available_fonts_.size())
    {
        status_.post("set_font was passed an invalid font index.", MSG_ERROR);
        return false;
    }
    const FontInfo& font = available_fonts_[font_index];

    if (!install_font(font))
        return false;

    std::vector<std::string> fields;
    if (ui)
        fields.push_back("typeface");
    if (map)
        fields.push_back("map_typeface");
    if (overmap)
        fields.push_back("overmap_typeface");

    for (size_t i = 0; i < fields.size(); ++i)
    {
        const std::string& field = fields[i];
        if (!font_config_.contains(field) || !font_config_[field].is_array())
        {
            status_.post("Font config does not have field \"" + field + "\".", MSG_ERROR);
            continue;
        }

        // Fonts that come with the game are the two last entries. Our font goes
        // in front of them, and replaces the previous choice next time.
        nlohmann::json& list = font_config_[field];
        if (list.size() <= 2)
            list.insert(list.begin(), font.file);
        else
            list[0] = font.file;
    }

    return write_font_config();
}

bool FontManager::reset_font()
{
    for (size_t i = 0; i < sizeof(FIELDS) / sizeof(FIELDS[0]); ++i)
    {
        nlohmann::json defaults = nlohmann::json::array();
        defaults.push_back("data/font/Terminus.ttf");
        defaults.push_back("data/font/unifont.ttf");
        font_config_[FIELDS[i]] = defaults;
    }
    return write_font_config();
}

// Index in available_fonts_ of the bundled font that is currently set for a
// field, or -1 if it is some other font.
int FontManager::current_font_index(const std::string& field) const
{
    std::vector<std::string> files = font_files(field);
    if (files.empty())
        return -1;
    for (size_t i = 0; i < available_fonts_.size(); ++i)
    {
        if (available_fonts_[i].file == files[0])
            return (int)i;
    }
    return -1;
}

void FontManager::set_font_sizes(int ui, int map, int overmap)
{
    double height_ratio = 1.0;
    double width_ratio = 0.5;

    int index = current_font_index("typeface");
    if (index >= 0)
    {
        height_ratio = available_fonts_[index].height;
        width_ratio = available_fonts_[index].width;
    }
    set_game_option("FONT_SIZE", std::to_string(ui));
    set_game_option("FONT_HEIGHT", std::to_string((int)(ui * height_ratio)));
    set_game_option("FONT_WIDTH", std::to_string((int)(ui * width_ratio)));

    // For the map and the overmap the width is the same as the height.
    index = current_font_index("map_typeface");
    height_ratio = (index >= 0) ? available_fonts_[index].height : 1.0;
    set_game_option("MAP_FONT_SIZE", std::to_string(map));
    set_game_option("MAP_FONT_HEIGHT", std::to_string((int)(map * height_ratio)));
    set_game_option("MAP_FONT_WIDTH", std::to_string((int)(map * height_ratio)));

    index = current_font_index("overmap_typeface");
    height_ratio = (index >= 0) ? available_fonts_[index].height : 1.0;
    set_game_option("OVERMAP_FONT_SIZE", std::to_string(overmap));
    set_game_option("OVERMAP_FONT_HEIGHT", std::to_string((int)(overmap * height_ratio)));
    set_game_option("OVERMAP_FONT_WIDTH", std::to_string((int)(overmap * height_ratio)));
}
