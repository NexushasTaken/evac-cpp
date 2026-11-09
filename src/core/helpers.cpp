#include "core/helpers.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json_fwd.hpp>
#include <sstream>
#include <string>
#include <system_error>

#include "core/fs.h"
#include "core/status.h"

const char* const INFO_FILENAME = "evac_install_info.json";

bool load_json_file(const std::filesystem::path& file, nlohmann::json& out,
                    Status& status) {
  const std::ifstream f(file);
  if (!f) {
    status.post("Failed to read file: " + to_utf8(file), MSG_ERROR);
    return false;
  }

  std::stringstream ss;
  ss << f.rdbuf();

  const nlohmann::json parsed = nlohmann::json::parse(ss.str(), nullptr, false);
  if (parsed.is_discarded()) {
    status.post("Failed to parse JSON file: " + to_utf8(file), MSG_ERROR);
    return false;
  }

  out = parsed;
  return true;
}

bool save_json_file(const nlohmann::json& data,
                    const std::filesystem::path& file, Status& status) {
  std::error_code ec;
  std::filesystem::create_directories(file.parent_path(), ec);
  std::ofstream f(file);
  if (!f) {
    status.post("Failed to write file: " + to_utf8(file), MSG_ERROR);
    return false;
  }
  f << data.dump(4);
  return true;
}

bool create_info_file(const std::filesystem::path& location,
                      const std::string& install_name, Status& status) {
  nlohmann::json info;
  info["name"] = install_name;
  return save_json_file(info, location / INFO_FILENAME, status);
}
