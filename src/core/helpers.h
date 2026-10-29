#ifndef EVAC_HELPERS_H
#define EVAC_HELPERS_H

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

#include "core/status.h"

extern const char* const INFO_FILENAME;

// Reads a JSON file. Returns false (and posts an error) on failure.
bool load_json_file(const std::filesystem::path& file, nlohmann::json& out,
                    Status& status);

// Writes a JSON file, indented with 4 spaces like the original launcher.
bool save_json_file(const nlohmann::json& data,
                    const std::filesystem::path& file, Status& status);

// Writes evac_install_info.json into the given install directory.
bool create_info_file(const std::filesystem::path& location,
                      const std::string& install_name, Status& status);

#endif
