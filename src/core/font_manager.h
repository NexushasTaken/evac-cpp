#ifndef CATAPULT_FONT_MANAGER_H
#define CATAPULT_FONT_MANAGER_H

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/paths.h"
#include "core/status.h"

// A font that can be set for the game: one that comes with the launcher, or
// one the user added from a file (those are kept in the user font directory).
struct FontInfo
{
    std::string name;
    std::string file;  // File name, in the bundled fonts or in the user font directory.
    std::string description;
    double height = 1.0;  // Height and width of a character as a part of the font size.
    double width = 0.5;
    bool bundled = true;
};

// An entry of a font list in fonts.json (see the game's doc/user-guides/FONT_OPTIONS.md).
struct FontEntry
{
    std::string path;
    std::string hinting;  // "" when the entry does not say, and the game decides.
    bool antialiasing = true;
};

// A font size in options.json, with the width and height of a character cell in pixels.
struct FontDims
{
    int size = 16;
    int width = 8;
    int height = 16;
};

struct FontOptions
{
    FontDims ui;  // Also used for the menus (gui_typeface).
    FontDims map;
    FontDims overmap;
    bool blending = false;
    bool ascii_lines = true;  // Only saved if the game has the option.
};

// Manages the fonts of the game: copies fonts to the user's font directory
// and edits <config>/fonts.json and <config>/options.json.
// These are small files, so everything here is quick and may be called from
// the main thread.
class FontManager
{
public:
    FontManager(Paths& paths, Status& status);

    // The font lists of fonts.json, as the game names them: "typeface" (the
    // game UI), "gui_typeface" (the menus), "map_typeface", "overmap_typeface".
    static const std::vector<std::string>& font_fields();
    // The hinting modes the game accepts.
    static const std::vector<std::string>& hinting_modes();
    // Font files the game can use for every font list.
    static bool is_font_file(const std::string& path);

    bool font_config_file_exists();
    bool options_file_exists();

    bool load_available_fonts();
    bool load_font_config();
    bool load_game_options();
    bool write_game_options();

    const std::vector<FontInfo>& available_fonts() const { return available_fonts_; }
    // Where the file of an available font is: the bundled ones before they are installed.
    std::string font_file_path(const FontInfo& font) const;

    // Whether fonts.json has a font list (older games have no "gui_typeface").
    bool has_font_field(const std::string& field) const;
    // The entries of a font list: the first is the font, the rest are fallbacks.
    std::vector<FontEntry> font_entries(const std::string& field) const;
    std::vector<std::string> font_files(const std::string& field) const;

    // Game options are stored as strings, like in options.json. A missing
    // option is reported to Status.
    bool has_game_option(const std::string& option_name) const;
    std::string get_game_option(const std::string& option_name);
    bool set_game_option(const std::string& option_name, const std::string& value);

    // These re-read the game's file before editing it, so that only the font
    // settings change and whatever the game saved meanwhile is kept.
    // Entries are passed with their path as it was shown: if the file changed
    // since, the edit is refused instead of hitting another entry.
    bool set_font(size_t font_index, const std::vector<std::string>& fields);
    bool reset_font();
    bool move_entry(const std::string& field, size_t index, const std::string& path, int offset);
    bool remove_entry(const std::string& field, size_t index, const std::string& path);
    bool set_entry_style(const std::string& field, size_t index, const std::string& path, const std::string& hinting,
                         bool antialiasing);
    bool save_font_options(const FontOptions& options);

    // Adding a font file copies it to the user font directory. Returns its
    // index in available_fonts(), or -1.
    int add_font_file(const std::string& source);
    // Whether adding source would replace another file of the same name.
    bool add_would_replace(const std::string& source);

    // The size of a font list in options.json ("gui_typeface" uses the UI's),
    // and the width and height the launcher picks for a size, from the
    // proportions of the font set first in that list.
    FontDims game_dims(const std::string& field);
    FontDims auto_dims(const std::string& field, int size) const;

private:
    bool write_font_config();
    bool install_font(const FontInfo& font);
    nlohmann::json* entry_for_edit(const std::string& field, size_t index, const std::string& path);
    bool is_user_font(const std::string& path);
    int current_font_index(const std::string& field) const;

    Paths& paths_;
    Status& status_;
    std::vector<FontInfo> available_fonts_;
    nlohmann::json font_config_;
    nlohmann::json game_options_;
};

#endif
