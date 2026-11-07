#include "core/font_manager.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "core/fs.h"
#include "core/helpers.h"

namespace fs = std::filesystem;

const std::vector<std::string>& FontManager::font_fields() {
  static const std::vector<std::string> fields = {
      "typeface",
      "gui_typeface",
      "map_typeface",
      "overmap_typeface",
  };
  return fields;
}

// As read by the game in src/font_loader.cpp.
const std::vector<std::string>& FontManager::hinting_modes() {
  static const std::vector<std::string> modes = {
      "Default", "Light", "Auto", "NoAuto", "None", "Bitmap",
  };
  return modes;
}

// The game opens the map and overmap fonts with SDL_ttf, which also takes
// .fon, but the menus use FreeType through ImGui: these work for both.
bool FontManager::is_font_file(const std::string& name) {
  std::string ext = to_utf8(to_path(name).extension());
  std::ranges::transform(ext, ext.begin(),
                         [](unsigned char c) { return std::tolower(c); });
  return ext == ".ttf" || ext == ".otf" || ext == ".ttc";
}

// An entry of a fonts.json field is either a path or an object with a "path"
// (see the game's doc/user-guides/FONT_OPTIONS.md). Returns "" for anything
// else.
static std::string entry_path(const nlohmann::json& entry) {
  if (entry.is_string()) {
    return entry.get<std::string>();
  }
  if (entry.is_object() && entry.contains("path") &&
      entry["path"].is_string()) {
    return entry["path"].get<std::string>();
  }
  return "";
}

static std::string_view describe(const std::string& desc_key) {
  if (desc_key == "font_desc_fantasque") {
    return "A somewhat whimsical monospace font by Jany Belluz.";
  }
  if (desc_key == "font_desc_hack") {
    return "A workhorse font primarily designed for source code,\nwith focus "
           "on legibility. By Source Foundry Authors.";
  }
  if (desc_key == "font_desc_inconsolata") {
    return "A variant of Inconsolata font (originally by Raph Levien)\nwith "
           "addition of Greek and Cyrillic characters.";
  }
  if (desc_key == "font_desc_pt") {
    return "A typewriter font by ParaType with excellent Cyrillic typography.";
  }
  if (desc_key == "font_desc_ubuntu") {
    return "A font by Canonical with a very distinct style,\ndesigned for the "
           "eponymous Linux distribution.";
  }
  return "";
}

FontManager::FontManager(Paths& paths, Status& status)
    : m_paths(paths),
      m_status(status),
      m_font_config(nlohmann::json::object()),
      m_game_options(nlohmann::json::array()) {}

bool FontManager::font_config_file_exists() {
  return fs::exists(m_paths.config() / "fonts.json");
}

bool FontManager::options_file_exists() {
  return fs::exists(m_paths.config() / "options.json");
}

bool FontManager::load_available_fonts() {
  m_available_fonts.clear();

  nlohmann::json data;
  if (!load_json_file(m_paths.data_dir() / "fonts" / "font_info.json", data,
                      m_status) ||
      !data.is_array()) {
    m_status.post("Could not read the bundled font info.", MSG_ERROR);
    return false;
  }

  for (const auto& item : data) {
    if (!item.is_object() || !item.contains("name") || !item.contains("file")) {
      continue;
    }
    FontInfo font;
    font.name = item["name"].get<std::string>();
    font.file = item["file"].get<std::string>();
    font.description = describe(item.value("desc_key", std::string()));
    font.height = item.value("height", 1.0);
    const double width = 0.5;
    font.width = item.value("width", width);
    m_available_fonts.push_back(font);
  }
  size_t bundled_count = m_available_fonts.size();

  // The other fonts in the user font directory were added from files.
  std::vector<std::string> added;
  std::error_code ec;
  for (fs::directory_iterator it(m_paths.font_user(), ec), end;
       !ec && it != end; it.increment(ec)) {
    std::string file = to_utf8(it->path().filename());
    if (!it->is_regular_file(ec) || !is_font_file(file)) {
      continue;
    }
    bool is_bundled = false;
    for (size_t i = 0; i < bundled_count; ++i) {
      is_bundled = is_bundled || m_available_fonts.at(i).file == file;
    }
    if (!is_bundled) {
      added.push_back(file);
    }
  }
  std::ranges::sort(added);
  for (const auto& i : added) {
    FontInfo font;
    font.name = to_utf8(to_path(i).stem());
    font.file = i;
    font.description =
        "Added from a file. The launcher does not know its proportions,\n"
        "so check the width and height in the font sizes.";
    font.bundled = false;
    m_available_fonts.push_back(font);
  }
  return true;
}

fs::path FontManager::font_file_path(const FontInfo& font) const {
  if (font.bundled) {
    return m_paths.data_dir() / "fonts" / to_path(font.file);
  }
  return m_paths.font_user() / to_path(font.file);
}

bool FontManager::load_font_config() {
  fs::path file = m_paths.config() / "fonts.json";
  m_font_config = nlohmann::json::object();

  if (!fs::exists(file)) {
    m_status.post("Font config file " + to_utf8(file) + " is not found!",
                  MSG_ERROR);
    return false;
  }

  nlohmann::json data;
  if (!load_json_file(file, data, m_status) || !data.is_object()) {
    return false;
  }
  m_font_config = data;
  return true;
}

bool FontManager::write_font_config() {
  return save_json_file(m_font_config, m_paths.config() / "fonts.json",
                        m_status);
}

bool FontManager::load_game_options() {
  fs::path file = m_paths.config() / "options.json";
  m_game_options = nlohmann::json::array();

  if (!fs::exists(file)) {
    m_status.post("Game options file " + to_utf8(file) + " is not found!",
                  MSG_ERROR);
    return false;
  }

  nlohmann::json data;
  if (!load_json_file(file, data, m_status) || !data.is_array()) {
    return false;
  }
  m_game_options = data;
  return true;
}

bool FontManager::write_game_options() {
  if (!save_json_file(m_game_options, m_paths.config() / "options.json",
                      m_status)) {
    return false;
  }
  m_status.post("Saved game options to options.json.");
  return true;
}

bool FontManager::has_font_field(const std::string& field) const {
  return m_font_config.contains(field);
}

std::vector<FontEntry> FontManager::font_entries(
    const std::string& field) const {
  std::vector<FontEntry> result;
  if (!m_font_config.contains(field)) {
    return result;
  }

  // A field is a list of entries, or a single entry.
  const nlohmann::json& value = m_font_config[field];
  std::vector<nlohmann::json> items;
  if (value.is_array()) {
    items.assign(value.begin(), value.end());
  } else {
    items.push_back(value);
  }

  for (auto& item : items) {
    FontEntry entry;
    entry.path = entry_path(item);
    if (entry.path.empty()) {
      continue;
    }
    if (item.is_object()) {
      if (item.contains("hinting") && item["hinting"].is_string()) {
        entry.hinting = item["hinting"].get<std::string>();
      }
      if (item.contains("antialiasing") && item["antialiasing"].is_boolean()) {
        entry.antialiasing = item["antialiasing"].get<bool>();
      }
    }
    result.push_back(entry);
  }
  return result;
}

std::vector<std::string> FontManager::font_files(
    const std::string& field) const {
  std::vector<FontEntry> entries = font_entries(field);
  std::vector<std::string> result;
  result.reserve(entries.size());
  for (auto& entry : entries) {
    result.push_back(entry.path);
  }
  return result;
}

bool FontManager::has_game_option(const std::string& option_name) const {
  for (const auto& item : m_game_options) {
    if (item.is_object() && item.value("name", std::string()) == option_name) {
      return true;
    }
  }
  return false;
}

std::string FontManager::get_game_option(const std::string& option_name) {
  for (const auto& item : m_game_options) {
    if (item.is_object() && item.value("name", std::string()) == option_name &&
        item.contains("value") && item["value"].is_string()) {
      return item["value"].get<std::string>();
    }
  }
  m_status.post(
      "Tried to get game option \"" + option_name + "\", but it wasn't found.",
      MSG_ERROR);
  return "";
}

bool FontManager::set_game_option(const std::string& option_name,
                                  const std::string& value) {
  for (auto& item : m_game_options) {
    if (item.is_object() && item.value("name", std::string()) == option_name) {
      item["value"] = value;
      return true;
    }
  }
  m_status.post(
      "Tried to set game option \"" + option_name + "\", but it wasn't found.",
      MSG_ERROR);
  return false;
}

bool FontManager::install_font(const FontInfo& font) {
  if (!font.bundled) {
    return true;  // Already in the user font directory.
  }

  fs::path source = m_paths.data_dir() / "fonts" / to_path(font.file);
  fs::path dest = m_paths.font_user() / to_path(font.file);

  std::error_code ec;
  fs::create_directories(dest.parent_path(), ec);
  fs::copy_file(source, dest, fs::copy_options::overwrite_existing, ec);
  if (ec) {
    m_status.post(
        "Could not install font file " + font.file + ": " + ec.message(),
        MSG_ERROR);
    m_status.post("Font source: " + to_utf8(source) +
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
  if (!p.has_parent_path()) {
    return fs::exists(m_paths.font_user() / p, ec);
  }
  return fs::equivalent(p.parent_path(), m_paths.font_user(), ec);
}

bool FontManager::set_font(size_t font_index,
                           const std::vector<std::string>& fields) {
  if (font_index >= m_available_fonts.size()) {
    m_status.post("set_font was passed an invalid font index.", MSG_ERROR);
    return false;
  }
  const FontInfo& font = m_available_fonts.at(font_index);

  // The game may have changed the file since we read it.
  if (!load_font_config()) {
    return false;
  }
  if (!install_font(font)) {
    return false;
  }

  // The full path: the game looks in the user font directory for the UI
  // and map fonts, but the menus open the path as it is.
  std::string path = to_utf8(m_paths.font_user() / to_path(font.file));
  nlohmann::json entry = {{"path", path}, {"hinting", "Default"}};

  for (const auto& field : fields) {
    if (!m_font_config.contains(field)) {
      m_status.post("Font config does not have field \"" + field + "\".",
                    MSG_ERROR);
      continue;
    }

    // A single entry becomes a list, so our font can go in front of it.
    nlohmann::json& list = m_font_config.at(field);
    if (!list.is_array()) {
      list = nlohmann::json::array({list});
    }

    // Our font goes in front of the game's fonts, and replaces the
    // previous font set by the launcher.
    if (!list.empty() && is_user_font(entry_path(list[0]))) {
      list[0] = entry;
    } else {
      list.insert(list.begin(), entry);
    }
  }

  return write_font_config();
}

bool FontManager::reset_font() {
  if (!load_font_config()) {
    return false;
  }

  // The game keeps its default fonts in data/fontdata.json.
  fs::path file = m_paths.game_dir() / "data" / "fontdata.json";
  nlohmann::json defaults;
  if (!load_json_file(file, defaults, m_status) || !defaults.is_object()) {
    m_status.post(
        "Could not read the default fonts from " + to_utf8(file) + ".",
        MSG_ERROR);
    return false;
  }

  const std::vector<std::string>& fields = font_fields();
  for (const auto& field : fields) {
    if (defaults.contains(field)) {
      m_font_config[field] = defaults[field];
    }
  }
  return write_font_config();
}

// Re-reads fonts.json and finds an entry to edit, if it is still the one
// with that path. A single entry is made a list first.
nlohmann::json* FontManager::entry_for_edit(const std::string& field,
                                            size_t index,
                                            const std::string& path) {
  if (!load_font_config()) {
    return nullptr;
  }
  if (m_font_config.contains(field)) {
    nlohmann::json& list = m_font_config[field];
    if (!list.is_array()) {
      list = nlohmann::json::array({list});
    }
    if (index < list.size() && entry_path(list[index]) == path) {
      return &list[index];
    }
  }
  m_status.post(
      "The font list changed since it was shown. Check it and try again.",
      MSG_ERROR);
  return nullptr;
}

bool FontManager::move_entry(const std::string& field, size_t index,
                             const std::string& path, int offset) {
  if (entry_for_edit(field, index, path) == nullptr) {
    return false;
  }
  nlohmann::json& list = m_font_config[field];
  size_t target = index + offset;
  if (target < 0 || target >= list.size()) {
    return false;
  }
  std::swap(list[index], list[target]);
  return write_font_config();
}

bool FontManager::remove_entry(const std::string& field, size_t index,
                               const std::string& path) {
  if (entry_for_edit(field, index, path) == nullptr) {
    return false;
  }
  m_font_config[field].erase(index);
  return write_font_config();
}

bool FontManager::set_entry_style(const std::string& field, size_t index,
                                  const std::string& path,
                                  const std::string& hinting,
                                  bool antialiasing) {
  const std::vector<std::string>& modes = hinting_modes();
  if (std::ranges::find(modes, hinting) == modes.end()) {
    m_status.post("\"" + hinting + "\" is not a hinting mode of the game.",
                  MSG_ERROR);
    return false;
  }

  nlohmann::json* entry = entry_for_edit(field, index, path);
  if (entry == nullptr) {
    return false;
  }
  // Only objects have the settings; anything else in the object is kept.
  if (!entry->is_object()) {
    *entry = {{"path", path}};
  }
  (*entry)["hinting"] = hinting;
  (*entry)["antialiasing"] = antialiasing;
  return write_font_config();
}

bool FontManager::add_would_replace(const fs::path& source) {
  fs::path target = m_paths.font_user() / source.filename();
  std::error_code ec;
  return fs::exists(target, ec) && !fs::equivalent(source, target, ec);
}

int FontManager::add_font_file(const fs::path& source) {
  std::string file = to_utf8(source.filename());
  if (!is_font_file(file)) {
    m_status.post(
        file + " is not a font file the game can use (.ttf, .otf or .ttc).",
        MSG_ERROR);
    return -1;
  }
  for (auto& m_available_font : m_available_fonts) {
    if (m_available_font.bundled && m_available_font.file == file) {
      m_status.post(file +
                        " has the name of a font that comes with the launcher. "
                        "Rename it to add it.",
                    MSG_ERROR);
      return -1;
    }
  }

  fs::path target = m_paths.font_user() / source.filename();
  std::error_code ec;
  if (!fs::equivalent(source, target, ec)) {
    fs::create_directories(target.parent_path(), ec);
    fs::copy_file(source, target, fs::copy_options::overwrite_existing, ec);
    if (ec) {
      m_status.post("Could not copy " + to_utf8(source) + " to " +
                        to_utf8(target) + ": " + ec.message(),
                    MSG_ERROR);
      return -1;
    }
  }
  m_status.post("Added font " + file + ".");

  if (!load_available_fonts()) {
    return -1;
  }
  for (size_t i = 0; i < m_available_fonts.size(); ++i) {
    if (!m_available_fonts.at(i).bundled &&
        m_available_fonts.at(i).file == file) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

// Index in m_available_fonts of the font that is set first for a field, or -1
// if it is some other font.
int FontManager::current_font_index(const std::string& field) const {
  std::vector<std::string> files = font_files(field);
  if (files.empty()) {
    return -1;
  }
  std::string file = to_utf8(to_path(files.at(0)).filename());
  for (size_t i = 0; i < m_available_fonts.size(); ++i) {
    if (m_available_fonts.at(i).file == file) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

// The options.json names of a field's sizes; the menus use the UI's.
static std::string_view option_prefix(const std::string& field) {
  if (field == "map_typeface") {
    return "MAP_FONT";
  }
  if (field == "overmap_typeface") {
    return "OVERMAP_FONT";
  }
  return "FONT";
}

// Game options are strings; a broken value falls back to the game's minimum.
static int to_int(const std::string& text, int minimum) {
  char* end = nullptr;
  errno = 0;

  const int base = 10;
  long val = std::strtol(text.c_str(), &end, base);

  // Check if no conversion took place, or if an overflow/underflow occurred
  if (end == text.c_str() || errno == ERANGE) {
    return minimum;
  }

  return std::max(static_cast<int>(val), minimum);
}

FontDims FontManager::game_dims(const std::string& field) {
  std::string prefix{option_prefix(field)};
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
    height_ratio = m_available_fonts.at(index).height;
    width_ratio = m_available_fonts.at(index).width;
  }

  FontDims dims;
  dims.size = size;
  dims.height = static_cast<int>(std::floor(size * height_ratio));
  // For the map and the overmap the width is the same as the height.
  dims.width = option_prefix(field) == "FONT"
                   ? static_cast<int>(std::floor(size * width_ratio))
                   : dims.height;
  return dims;
}

bool FontManager::save_font_options(const FontOptions& options) {
  // Edit the game's current options, not our copy from when the tab was shown:
  // the game saves options.json on its own (the language, the window size...).
  if (!load_game_options()) {
    return false;
  }

  const int N = 3;
  const std::array<std::string_view, N> prefixes = {
      "FONT",
      "MAP_FONT",
      "OVERMAP_FONT",
  };
  const std::array<const FontDims*, N> dims = {
      &options.ui,
      &options.map,
      &options.overmap,
  };

  for (int i = 0; i < N; ++i) {
    std::string prefix{prefixes.at(i)};
    const auto* dim{dims.at(i)};
    set_game_option(prefix + "_SIZE", std::to_string(dim->size));
    set_game_option(prefix + "_WIDTH", std::to_string(dim->width));
    set_game_option(prefix + "_HEIGHT", std::to_string(dim->height));
  }
  set_game_option("FONT_BLENDING", options.blending ? "true" : "false");
  if (has_game_option("USE_DRAW_ASCII_LINES_ROUTINE")) {
    set_game_option("USE_DRAW_ASCII_LINES_ROUTINE",
                    options.ascii_lines ? "true" : "false");
  }
  return write_game_options();
}
