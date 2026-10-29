#ifndef EVAC_SETTINGS_H
#define EVAC_SETTINGS_H

#include <filesystem>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

#include "core/status.h"

// Reads and writes evac_settings.json (same keys as the original launcher's
// catapult_settings.json). It may be used from several threads at once (the UI
// and a worker). Keys that are missing from the file fall back to the defaults.
// Keys that we don't know about (like launcher_theme) are kept in the file
// untouched.
class Settings {
 public:
  Settings(const std::filesystem::path& settings_file, Status& status);

  // Loads the file, or creates it with the defaults if it doesn't exist.
  void load();
  bool save();

  std::string get_string(const std::string& name);
  bool get_bool(const std::string& name);
  int get_int(const std::string& name);
  double get_double(const std::string& name);
  nlohmann::json get_json(const std::string& name);

  void set_string(const std::string& name, const std::string& value);
  void set_bool(const std::string& name, bool value);
  void set_int(const std::string& name, int value);
  void set_double(const std::string& name, double value);
  void set_json(const std::string& name, const nlohmann::json& value);

  static nlohmann::json defaults();

 private:
  // Returns the stored value, or the default if the key is missing or has
  // a type that doesn't match the default's. Null if the key is unknown.
  nlohmann::json lookup(const std::string& name);

  std::filesystem::path m_file;
  Status& m_status;
  std::mutex m_mutex;  // Guards m_current.
  nlohmann::json m_current;
};

#endif
