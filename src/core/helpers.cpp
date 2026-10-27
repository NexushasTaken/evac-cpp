#include "core/helpers.h"

#include <filesystem>
#include <fstream>
#include <sstream>

const char* const INFO_FILENAME = "evac_install_info.json";

bool load_json_file(const std::string& file, nlohmann::json& out, Status& status)
{
    std::ifstream f(file);
    if (!f)
    {
        status.post("Failed to read file: " + file, MSG_ERROR);
        return false;
    }

    std::stringstream ss;
    ss << f.rdbuf();

    nlohmann::json parsed = nlohmann::json::parse(ss.str(), nullptr, false);
    if (parsed.is_discarded())
    {
        status.post("Failed to parse JSON file: " + file, MSG_ERROR);
        return false;
    }

    out = parsed;
    return true;
}

bool save_json_file(const nlohmann::json& data, const std::string& file, Status& status)
{
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(file).parent_path(), ec);
    std::ofstream f(file);
    if (!f)
    {
        status.post("Failed to write file: " + file, MSG_ERROR);
        return false;
    }
    f << data.dump(4);
    return true;
}

bool create_info_file(const std::string& location, const std::string& install_name, Status& status)
{
    nlohmann::json info;
    info["name"] = install_name;
    return save_json_file(info, location + "/" + INFO_FILENAME, status);
}
