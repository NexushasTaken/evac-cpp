#include "core/settings.h"

#include <filesystem>

#include "core/helpers.h"

Settings::Settings(const std::string& settings_file, Status& status)
    : file_(settings_file), status_(status), current_(nlohmann::json::object())
{
}

nlohmann::json Settings::defaults()
{
    nlohmann::json d;
    d["game"] = "dda";
    d["channel"] = "stable";
    d["active_install_dda"] = "";
    d["active_install_bn"] = "";
    d["active_install_eod"] = "";
    d["active_install_tish"] = "";
    d["active_install_tlg"] = "";
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
    d["ui_scale_override"] = 1.0;  // Unused, kept for compatibility.
    d["ui_scale_override_enabled"] = false;  // Unused, kept for compatibility.
    d["show_stock_mods"] = false;
    d["show_installed_mods_in_available"] = false;
    d["show_obsolete_mods"] = false;
    d["install_archived_mods"] = false;
    d["show_stock_sound"] = false;
    d["font_preview_cyrillic"] = false;
    d["show_game_desc"] = true;
    d["keep_open_after_starting_game"] = true;
    d["keep_cache"] = false;
    d["ignore_cache"] = false;
    d["proxy_option"] = "off";
    d["proxy_host"] = "";
    d["proxy_port"] = 0;
    d["debug_mode"] = false;
    return d;
}

void Settings::load()
{
    bool created = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (std::filesystem::exists(file_))
        {
            nlohmann::json data;
            if (load_json_file(file_, data, status_) && data.is_object())
                current_ = data;
            else
                current_ = nlohmann::json::object();
        }
        else
        {
            current_ = defaults();
            created = true;
        }
    }
    if (created)
    {
        status_.post("Creating settings file: " + file_);
        save();
    }
    status_.set_debug_enabled(get_bool("debug_mode"));
}

bool Settings::save()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return save_json_file(current_, file_, status_);
}

nlohmann::json Settings::lookup(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json def = defaults();
    if (!def.contains(name))
    {
        // Not one of ours; allow it only if it is in the file.
        if (current_.contains(name))
            return current_[name];
        status_.post("Requested a non-existing setting: " + name, MSG_ERROR);
        return nlohmann::json();
    }

    if (current_.contains(name))
    {
        const nlohmann::json& v = current_[name];
        const nlohmann::json& d = def[name];
        bool same_type = (v.is_number() && d.is_number()) || (v.type() == d.type());
        if (same_type)
            return v;
    }
    return def[name];
}

std::string Settings::get_string(const std::string& name)
{
    nlohmann::json v = lookup(name);
    return v.is_string() ? v.get<std::string>() : std::string();
}

bool Settings::get_bool(const std::string& name)
{
    nlohmann::json v = lookup(name);
    return v.is_boolean() ? v.get<bool>() : false;
}

int Settings::get_int(const std::string& name)
{
    nlohmann::json v = lookup(name);
    return v.is_number() ? v.get<int>() : 0;
}

double Settings::get_double(const std::string& name)
{
    nlohmann::json v = lookup(name);
    return v.is_number() ? v.get<double>() : 0.0;
}

nlohmann::json Settings::get_json(const std::string& name)
{
    return lookup(name);
}

void Settings::set_string(const std::string& name, const std::string& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    current_[name] = value;
}

void Settings::set_bool(const std::string& name, bool value)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_[name] = value;
    }
    if (name == "debug_mode")
        status_.set_debug_enabled(value);
}

void Settings::set_int(const std::string& name, int value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    current_[name] = value;
}

void Settings::set_double(const std::string& name, double value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    current_[name] = value;
}

void Settings::set_json(const std::string& name, const nlohmann::json& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    current_[name] = value;
}
