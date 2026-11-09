#include "core/settings.h"

#include <filesystem>
#include <mutex>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <utility>

#include "core/fs.h"
#include "core/helpers.h"
#include "core/status.h"

Settings::Settings(std::filesystem::path settings_file, Status& status)
    : m_file(std::move(settings_file)),
      m_status(status),
      m_current(nlohmann::json::object()) {}

nlohmann::json Settings::defaults() {
  nlohmann::json d;
  d["game"] = "dda";
  d["channel"] = "stable";
  d["active_install_dda"] = "";
  d["active_install_bn"] = "";
  d["active_install_eod"] = "";
  d["active_install_tish"] = "";
  d["active_install_tlg"] = "";
  // Userdata folders the user added, per game: [{"path": ..., "label": ...}].
  // The active one is its path, or "" for the default (<game base>/userdata).
  const char* const games[] = {"dda", "bn", "eod", "tish", "tlg"};
  for (const char* game : games) {
    d[std::string("userdata_folders_") + game] = nlohmann::json::array();
    d[std::string("active_userdata_") + game] = "";
  }
  d["update_current_when_installing"] = true;
  d["launcher_locale"] = "";
  d["launcher_theme"] = "Godot_4.tres";  // Unused, kept for compatibility.
  d["window_state"] = nlohmann::json::object();
  d["print_tips_of_the_day"] = true;
  d["update_to_same_build_allowed"] = false;
  d["shorten_release_names"] = false;
  d["always_show_installs"] = false;
  d["num_releases_to_request"] = 10;
  d["num_prs_to_request"] = 50;
  d["ui_scale_override"] = 1.0;            // Unused, kept for compatibility.
  d["ui_scale_override_enabled"] = false;  // Unused, kept for compatibility.
  d["show_stock_mods"] = false;
  d["show_installed_mods_in_available"] = false;
  d["show_obsolete_mods"] = false;
  d["install_archived_mods"] = false;
  d["show_stock_sound"] = false;
  d["font_preview_cyrillic"] = false;
  d["show_game_desc"] = true;
  d["keep_open_after_starting_game"] = true;
  d["game_extra_args"] =
      "";  // Added to the game's command line, see split_command_line().
  d["keep_cache"] = false;
  d["ignore_cache"] = false;
  d["proxy_option"] = "off";
  d["proxy_host"] = "";
  d["proxy_port"] = 0;
  d["debug_mode"] = false;
  d["use_trash"] =
      true;  // Deleted things go to the trash instead of being removed.
  d["trash_max_size_mb"] = 5120;
  d["trash_auto_prune"] =
      false;  // Offer to delete the oldest items when over the limit.
  return d;
}

void Settings::load() {
  bool created = false;
  {
    const std::scoped_lock lock(m_mutex);
    if (std::filesystem::exists(m_file)) {
      nlohmann::json data;
      if (load_json_file(m_file, data, m_status) && data.is_object()) {
        m_current = data;
      } else {
        m_current = nlohmann::json::object();
      }
    } else {
      m_current = defaults();
      created = true;
    }
  }
  if (created) {
    m_status.post("Creating settings file: " + to_utf8(m_file));
    save();
  }
  m_status.set_debug_enabled(get_bool("debug_mode"));
}

bool Settings::save() {
  const std::scoped_lock lock(m_mutex);
  return save_json_file(m_current, m_file, m_status);
}

nlohmann::json Settings::lookup(const std::string& name) {
  const std::scoped_lock lock(m_mutex);
  nlohmann::json def = defaults();
  if (!def.contains(name)) {
    // Not one of ours; allow it only if it is in the file.
    if (m_current.contains(name)) {
      return m_current[name];
    }
    m_status.post("Requested a non-existing setting: " + name, MSG_ERROR);
    return {};
  }

  if (m_current.contains(name)) {
    const nlohmann::json& v = m_current[name];
    const nlohmann::json& d = def[name];
    const bool same_type =
        (v.is_number() && d.is_number()) || (v.type() == d.type());
    if (same_type) {
      return v;
    }
  }
  return def[name];
}

std::string Settings::get_string(const std::string& name) {
  const nlohmann::json v = lookup(name);
  return v.is_string() ? v.get<std::string>() : std::string();
}

bool Settings::get_bool(const std::string& name) {
  const nlohmann::json v = lookup(name);
  return v.is_boolean() ? v.get<bool>() : false;
}

int Settings::get_int(const std::string& name) {
  const nlohmann::json v = lookup(name);
  return v.is_number() ? v.get<int>() : 0;
}

double Settings::get_double(const std::string& name) {
  const nlohmann::json v = lookup(name);
  return v.is_number() ? v.get<double>() : 0.0;
}

nlohmann::json Settings::get_json(const std::string& name) {
  return lookup(name);
}

void Settings::set_string(const std::string& name, const std::string& value) {
  const std::scoped_lock lock(m_mutex);
  m_current[name] = value;
}

void Settings::set_bool(const std::string& name, bool value) {
  {
    const std::scoped_lock lock(m_mutex);
    m_current[name] = value;
  }
  if (name == "debug_mode") {
    m_status.set_debug_enabled(value);
  }
}

void Settings::set_int(const std::string& name, int value) {
  const std::scoped_lock lock(m_mutex);
  m_current[name] = value;
}

void Settings::set_double(const std::string& name, double value) {
  const std::scoped_lock lock(m_mutex);
  m_current[name] = value;
}

void Settings::set_json(const std::string& name, const nlohmann::json& value) {
  const std::scoped_lock lock(m_mutex);
  m_current[name] = value;
}
