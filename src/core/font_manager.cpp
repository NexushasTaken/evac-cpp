#include "core/font_manager.h"

#include <algorithm>
#include <filesystem>

#include "core/fs.h"
#include "core/helpers.h"

namespace fs = std::filesystem;

const std::vector<std::string>& FontManager::font_fields() {
  static const std::vector<std::string> fields = {
      "typeface", "gui_typeface", "map_typeface", "overmap_typeface"};
  return fields;
}

// As read by the game in src/font_loader.cpp.
const std::vector<std::string>& FontManager::hinting_modes() {
  static const std::vector<std::string> modes = {"Default", "Light", "Auto",
                                                 "NoAuto",  "None",  "Bitmap"};
  return modes;
}

// The game opens the map and overmap fonts with SDL_ttf, which also takes
// .fon, but the menus use FreeType through ImGui: these work for both.
bool FontManager::is_font_file(const std::string& name) {
  std::string ext = to_utf8(to_path(name).extension());
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  return ext == ".ttf" || ext == ".otf" || ext == ".ttc";
}

// An entry of a fonts.json field is either a path or an object with a "path"
// (see the game's doc/user-guides/FONT_OPTIONS.md). Returns "" for anything
// else.
static std::string entry_path(const nlohmann::json& entry) {
  if (entry.is_string()) return entry.get<std::string>();
  if (entry.is_object() && entry.contains("path") && entry["path"].is_string())
    return entry["path"].get<std::string>();
  return "";
}

static std::string describe(const std::string& desc_key) {
  if (desc_key == "font_desc_fantasque")
    return "A somewhat whimsical monospace font by Jany Belluz.";
  if (desc_key == "font_desc_hack")
    return "A workhorse font primarily designed for source code,\nwith focus "
           "on legibility. By Source Foundry Authors.";
  if (desc_key == "font_desc_inconsolata")
    return "A variant of Inconsolata font (originally by Raph Levien)\nwith "
           "addition of Greek and Cyrillic characters.";
  if (desc_key == "font_desc_pt")
    return "A typewriter font by ParaType with excellent Cyrillic typography.";
  if (desc_key == "font_desc_ubuntu")
    return "A font by Canonical with a very distinct style,\ndesigned for the "
           "eponymous Linux distribution.";
  return "";
}

FontManager::FontManager(Paths& paths, Status& status)
    : paths_(paths),
      status_(status),
      font_config_(nlohmann::json::object()),
      game_options_(nlohmann::json::array()) {}

bool FontManager::font_config_file_exists() {
  return fs::exists(paths_.config() / "fonts.json");
}

bool FontManager::options_file_exists() {
  return fs::exists(paths_.config() / "options.json");
}

bool FontManager::load_available_fonts() {
  available_fonts_.clear();

  nlohmann::json data;
  if (!load_json_file(paths_.data_dir() / "fonts" / "font_info.json", data,
                      status_) ||
      !data.is_array()) {
    status_.post("Could not read the bundled font info.", MSG_ERROR);
    return false;
  }

  for (size_t i = 0; i < data.size(); ++i) {
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
  size_t bundled_count = available_fonts_.size();

  // The other fonts in the user font directory were added from files.
  std::vector<std::string> added;
  std::error_code ec;
  for (fs::directory_iterator it(paths_.font_user(), ec), end; !ec && it != end;
       it.increment(ec)) {
    std::string file = to_utf8(it->path().filename());
    if (!it->is_regular_file(ec) || !is_font_file(file)) continue;
    bool is_bundled = false;
    for (size_t i = 0; i < bundled_count; ++i)
      is_bundled = is_bundled || available_fonts_[i].file == file;
    if (!is_bundled) added.push_back(file);
  }
  std::sort(added.begin(), added.end());
  for (size_t i = 0; i < added.size(); ++i) {
    FontInfo font;
    font.name = to_utf8(to_path(added[i]).stem());
    font.file = added[i];
    font.description =
        "Added from a file. The launcher does not know its proportions,\n"
        "so check the width and height in the font sizes.";
    font.bundled = false;
    available_fonts_.push_back(font);
  }
  return true;
}

fs::path FontManager::font_file_path(const FontInfo& font) const {
  if (font.bundled) return paths_.data_dir() / "fonts" / to_path(font.file);
  return paths_.font_user() / to_path(font.file);
}

bool FontManager::load_font_config() {
  fs::path file = paths_.config() / "fonts.json";
  font_config_ = nlohmann::json::object();

  if (!fs::exists(file)) {
    status_.post("Font config file " + to_utf8(file) + " is not found!",
                 MSG_ERROR);
    return false;
  }

  nlohmann::json data;
  if (!load_json_file(file, data, status_) || !data.is_object()) return false;
  font_config_ = data;
  return true;
}

bool FontManager::write_font_config() {
  return save_json_file(font_config_, paths_.config() / "fonts.json", status_);
}

bool FontManager::load_game_options() {
  fs::path file = paths_.config() / "options.json";
  game_options_ = nlohmann::json::array();

  if (!fs::exists(file)) {
    status_.post("Game options file " + to_utf8(file) + " is not found!",
                 MSG_ERROR);
    return false;
  }

  nlohmann::json data;
  if (!load_json_file(file, data, status_) || !data.is_array()) return false;
  game_options_ = data;
  return true;
}

bool FontManager::write_game_options() {
  if (!save_json_file(game_options_, paths_.config() / "options.json", status_))
    return false;
  status_.post("Saved game options to options.json.");
  return true;
}

bool FontManager::has_font_field(const std::string& field) const {
  return font_config_.contains(field);
}

std::vector<FontEntry> FontManager::font_entries(
    const std::string& field) const {
  std::vector<FontEntry> result;
  if (!font_config_.contains(field)) return result;

  // A field is a list of entries, or a single entry.
  const nlohmann::json& value = font_config_[field];
  std::vector<nlohmann::json> items;
  if (value.is_array())
    items.assign(value.begin(), value.end());
  else
    items.push_back(value);

  for (size_t i = 0; i < items.size(); ++i) {
    FontEntry entry;
    entry.path = entry_path(items[i]);
    if (entry.path.empty()) continue;
    if (items[i].is_object()) {
      if (items[i].contains("hinting") && items[i]["hinting"].is_string())
        entry.hinting = items[i]["hinting"].get<std::string>();
      if (items[i].contains("antialiasing") &&
          items[i]["antialiasing"].is_boolean())
        entry.antialiasing = items[i]["antialiasing"].get<bool>();
    }
    result.push_back(entry);
  }
  return result;
}

std::vector<std::string> FontManager::font_files(
    const std::string& field) const {
  std::vector<FontEntry> entries = font_entries(field);
  std::vector<std::string> result;
  for (size_t i = 0; i < entries.size(); ++i) result.push_back(entries[i].path);
  return result;
}

bool FontManager::has_game_option(const std::string& option_name) const {
  for (size_t i = 0; i < game_options_.size(); ++i) {
    const nlohmann::json& item = game_options_[i];
    if (item.is_object() && item.value("name", std::string()) == option_name)
      return true;
  }
  return false;
}

std::string FontManager::get_game_option(const std::string& option_name) {
  for (size_t i = 0; i < game_options_.size(); ++i) {
    const nlohmann::json& item = game_options_[i];
    if (item.is_object() && item.value("name", std::string()) == option_name &&
        item.contains("value") && item["value"].is_string())
      return item["value"].get<std::string>();
  }
  status_.post(
      "Tried to get game option \"" + option_name + "\", but it wasn't found.",
      MSG_ERROR);
  return "";
}

bool FontManager::set_game_option(const std::string& option_name,
                                  const std::string& value) {
  for (size_t i = 0; i < game_options_.size(); ++i) {
    nlohmann::json& item = game_options_[i];
    if (item.is_object() && item.value("name", std::string()) == option_name) {
      item["value"] = value;
      return true;
    }
  }
  status_.post(
      "Tried to set game option \"" + option_name + "\", but it wasn't found.",
      MSG_ERROR);
  return false;
}

bool FontManager::install_font(const FontInfo& font) {
  if (!font.bundled) return true;  // Already in the user font directory.

  fs::path source = paths_.data_dir() / "fonts" / to_path(font.file);
  fs::path dest = paths_.font_user() / to_path(font.file);

  std::error_code ec;
  fs::create_directories(dest.parent_path(), ec);
  fs::copy_file(source, dest, fs::copy_options::overwrite_existing, ec);
  if (ec) {
    status_.post(
        "Could not install font file " + font.file + ": " + ec.message(),
        MSG_ERROR);
    status_.post("Font source: " + to_utf8(source) +
                     ". Destination: " + to_utf8(dest) + ".",
                 MSG_DEBUG);
    return false;
  }
  return true;
}

// Whether a fonts.json path is a font in the user font directory, where the
// launcher puts its fonts. Older launchers wrote just the file name, which
// the game also looks for there.
bool FontManager::is_user_font(const std::string& path) {
  fs::path p = to_path(path);
  std::error_code ec;
  if (!p.has_parent_path()) return fs::exists(paths_.font_user() / p, ec);
  return fs::equivalent(p.parent_path(), paths_.font_user(), ec);
}

bool FontManager::set_font(size_t font_index,
                           const std::vector<std::string>& fields) {
  if (font_index >= available_fonts_.size()) {
    status_.post("set_font was passed an invalid font index.", MSG_ERROR);
    return false;
  }
  const FontInfo& font = available_fonts_[font_index];

  // The game may have changed the file since we read it.
  if (!load_font_config()) return false;
  if (!install_font(font)) return false;

  // The full path: the game looks in the user font directory for the UI
  // and map fonts, but the menus open the path as it is.
  std::string path = to_utf8(paths_.font_user() / to_path(font.file));
  nlohmann::json entry = {{"path", path}, {"hinting", "Default"}};

  for (size_t i = 0; i < fields.size(); ++i) {
    const std::string& field = fields[i];
    if (!font_config_.contains(field)) {
      status_.post("Font config does not have field \"" + field + "\".",
                   MSG_ERROR);
      continue;
    }

    // A single entry becomes a list, so our font can go in front of it.
    nlohmann::json& list = font_config_[field];
    if (!list.is_array()) list = nlohmann::json::array({list});

    // Our font goes in front of the game's fonts, and replaces the
    // previous font set by the launcher.
    if (!list.empty() && is_user_font(entry_path(list[0])))
      list[0] = entry;
    else
      list.insert(list.begin(), entry);
  }

  return write_font_config();
}

bool FontManager::reset_font() {
  if (!load_font_config()) return false;

  // The game keeps its default fonts in data/fontdata.json.
  fs::path file = paths_.game_dir() / "data" / "fontdata.json";
  nlohmann::json defaults;
  if (!load_json_file(file, defaults, status_) || !defaults.is_object()) {
    status_.post("Could not read the default fonts from " + to_utf8(file) + ".",
                 MSG_ERROR);
    return false;
  }

  const std::vector<std::string>& fields = font_fields();
  for (size_t i = 0; i < fields.size(); ++i) {
    if (defaults.contains(fields[i]))
      font_config_[fields[i]] = defaults[fields[i]];
  }
  return write_font_config();
}

// Re-reads fonts.json and finds an entry to edit, if it is still the one
// with that path. A single entry is made a list first.
nlohmann::json* FontManager::entry_for_edit(const std::string& field,
                                            size_t index,
                                            const std::string& path) {
  if (!load_font_config()) return nullptr;
  if (font_config_.contains(field)) {
    nlohmann::json& list = font_config_[field];
    if (!list.is_array()) list = nlohmann::json::array({list});
    if (index < list.size() && entry_path(list[index]) == path)
      return &list[index];
  }
  status_.post(
      "The font list changed since it was shown. Check it and try again.",
      MSG_ERROR);
  return nullptr;
}

bool FontManager::move_entry(const std::string& field, size_t index,
                             const std::string& path, int offset) {
  if (!entry_for_edit(field, index, path)) return false;
  nlohmann::json& list = font_config_[field];
  long target = (long)index + offset;
  if (target < 0 || target >= (long)list.size()) return false;
  std::swap(list[index], list[(size_t)target]);
  return write_font_config();
}

bool FontManager::remove_entry(const std::string& field, size_t index,
                               const std::string& path) {
  if (!entry_for_edit(field, index, path)) return false;
  font_config_[field].erase(index);
  return write_font_config();
}

bool FontManager::set_entry_style(const std::string& field, size_t index,
                                  const std::string& path,
                                  const std::string& hinting,
                                  bool antialiasing) {
  const std::vector<std::string>& modes = hinting_modes();
  if (std::find(modes.begin(), modes.end(), hinting) == modes.end()) {
    status_.post("\"" + hinting + "\" is not a hinting mode of the game.",
                 MSG_ERROR);
    return false;
  }

  nlohmann::json* entry = entry_for_edit(field, index, path);
  if (!entry) return false;
  // Only objects have the settings; anything else in the object is kept.
  if (!entry->is_object()) *entry = {{"path", path}};
  (*entry)["hinting"] = hinting;
  (*entry)["antialiasing"] = antialiasing;
  return write_font_config();
}

bool FontManager::add_would_replace(const fs::path& source) {
  fs::path target = paths_.font_user() / source.filename();
  std::error_code ec;
  return fs::exists(target, ec) && !fs::equivalent(source, target, ec);
}

int FontManager::add_font_file(const fs::path& source) {
  std::string file = to_utf8(source.filename());
  if (!is_font_file(file)) {
    status_.post(
        file + " is not a font file the game can use (.ttf, .otf or .ttc).",
        MSG_ERROR);
    return -1;
  }
  for (size_t i = 0; i < available_fonts_.size(); ++i) {
    if (available_fonts_[i].bundled && available_fonts_[i].file == file) {
      status_.post(file +
                       " has the name of a font that comes with the launcher. "
                       "Rename it to add it.",
                   MSG_ERROR);
      return -1;
    }
  }

  fs::path target = paths_.font_user() / source.filename();
  std::error_code ec;
  if (!fs::equivalent(source, target, ec)) {
    fs::create_directories(target.parent_path(), ec);
    fs::copy_file(source, target, fs::copy_options::overwrite_existing, ec);
    if (ec) {
      status_.post("Could not copy " + to_utf8(source) + " to " +
                       to_utf8(target) + ": " + ec.message(),
                   MSG_ERROR);
      return -1;
    }
  }
  status_.post("Added font " + file + ".");

  if (!load_available_fonts()) return -1;
  for (size_t i = 0; i < available_fonts_.size(); ++i) {
    if (!available_fonts_[i].bundled && available_fonts_[i].file == file)
      return (int)i;
  }
  return -1;
}

// Index in available_fonts_ of the font that is set first for a field, or -1
// if it is some other font.
int FontManager::current_font_index(const std::string& field) const {
  std::vector<std::string> files = font_files(field);
  if (files.empty()) return -1;
  std::string file = to_utf8(to_path(files[0]).filename());
  for (size_t i = 0; i < available_fonts_.size(); ++i) {
    if (available_fonts_[i].file == file) return (int)i;
  }
  return -1;
}

// The options.json names of a field's sizes; the menus use the UI's.
static std::string option_prefix(const std::string& field) {
  if (field == "map_typeface") return "MAP_FONT";
  if (field == "overmap_typeface") return "OVERMAP_FONT";
  return "FONT";
}

// Game options are strings; a broken value falls back to the game's minimum.
static int to_int(const std::string& text, int minimum) {
  int value = std::atoi(text.c_str());
  return value < minimum ? minimum : value;
}

FontDims FontManager::game_dims(const std::string& field) {
  std::string prefix = option_prefix(field);
  FontDims dims;
  dims.size = to_int(get_game_option(prefix + "_SIZE"), 8);
  dims.width = to_int(get_game_option(prefix + "_WIDTH"), 6);
  dims.height = to_int(get_game_option(prefix + "_HEIGHT"), 8);
  return dims;
}

FontDims FontManager::auto_dims(const std::string& field, int size) const {
  double height_ratio = 1.0;
  double width_ratio = 0.5;
  int index = current_font_index(field == "gui_typeface" ? "typeface" : field);
  if (index >= 0) {
    height_ratio = available_fonts_[index].height;
    width_ratio = available_fonts_[index].width;
  }

  FontDims dims;
  dims.size = size;
  dims.height = (int)(size * height_ratio);
  // For the map and the overmap the width is the same as the height.
  dims.width =
      option_prefix(field) == "FONT" ? (int)(size * width_ratio) : dims.height;
  return dims;
}

bool FontManager::save_font_options(const FontOptions& options) {
  // Edit the game's current options, not our copy from when the tab was shown:
  // the game saves options.json on its own (the language, the window size...).
  if (!load_game_options()) return false;

  const char* const prefixes[] = {"FONT", "MAP_FONT", "OVERMAP_FONT"};
  const FontDims* dims[] = {&options.ui, &options.map, &options.overmap};
  for (int i = 0; i < 3; ++i) {
    set_game_option(std::string(prefixes[i]) + "_SIZE",
                    std::to_string(dims[i]->size));
    set_game_option(std::string(prefixes[i]) + "_WIDTH",
                    std::to_string(dims[i]->width));
    set_game_option(std::string(prefixes[i]) + "_HEIGHT",
                    std::to_string(dims[i]->height));
  }
  set_game_option("FONT_BLENDING", options.blending ? "true" : "false");
  if (has_game_option("USE_DRAW_ASCII_LINES_ROUTINE"))
    set_game_option("USE_DRAW_ASCII_LINES_ROUTINE",
                    options.ascii_lines ? "true" : "false");
  return write_game_options();
}
