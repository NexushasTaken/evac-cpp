#ifndef CATAPULT_FONT_MANAGER_H
#define CATAPULT_FONT_MANAGER_H

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/paths.h"
#include "core/status.h"

// A font that comes with the launcher.
struct FontInfo
{
    std::string name;
    std::string file;
    std::string description;
    double height = 1.0;  // Height and width of a character as a part of the font size.
    double width = 0.5;
};

// Manages the fonts of the game: copies bundled fonts to the user's font
// directory and edits <config>/fonts.json and <config>/options.json.
// These are small files, so everything here is quick and may be called from
// the main thread.
class FontManager
{
public:
    FontManager(Paths& paths, Status& status);

    bool font_config_file_exists();
    bool options_file_exists();

    bool load_available_fonts();
    bool load_font_config();
    bool load_game_options();
    bool write_game_options();

    const std::vector<FontInfo>& available_fonts() const { return available_fonts_; }

    // The font files currently set for a field ("typeface", "map_typeface"
    // or "overmap_typeface"): the first is the font, the rest are fallbacks.
    std::vector<std::string> font_files(const std::string& field) const;

    // Game options are stored as strings, like in options.json. A missing
    // option is reported to Status.
    std::string get_game_option(const std::string& option_name);
    bool set_game_option(const std::string& option_name, const std::string& value);

    bool set_font(size_t font_index, bool ui, bool map, bool overmap);
    bool reset_font();
    void set_font_sizes(int ui, int map, int overmap);

private:
    bool write_font_config();
    bool install_font(const FontInfo& font);
    int current_font_index(const std::string& field) const;

    Paths& paths_;
    Status& status_;
    std::vector<FontInfo> available_fonts_;
    nlohmann::json font_config_;
    nlohmann::json game_options_;
};

#endif
