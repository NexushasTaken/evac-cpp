#include "core/mod_manager.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

#include "core/archive.h"
#include "core/fs.h"
#include "core/helpers.h"

namespace fs = std::filesystem;

static std::vector<Modpack> make_modpacks()
{
    std::vector<Modpack> packs;
    Modpack p;

    p.name = "CDDA Kenan Modpack";
    p.url = "https://github.com/Kenan2000/CDDA-Kenan-Modpack/archive/refs/heads/master.zip";
    p.filename = "CDDA-Kenan-Modpack-master.zip";
    p.internal_paths.push_back("CDDA-Structured-Kenan-Modpack-master/Kenan-Structured-Modpack/High-Maintenance-Huge-Mods");
    p.archived_path = "CDDA-Structured-Kenan-Modpack-master/Kenan-Structured-Modpack/Archived-Mods";
    packs.push_back(p);

    p = Modpack();
    p.name = "BN Kenan Modpack";
    p.url = "https://github.com/Zlorthishen/BrightNights-Structured-Kenan-Modpack/archive/refs/heads/master.zip";
    p.filename = "BrightNights-Structured-Kenan-Modpack-master.zip";
    p.internal_paths.push_back(
        "BrightNights-Structured-Kenan-Modpack-master/Kenan-BrightNights-Structured-Modpack/High-Maintenance-Huge-Mods");
    p.internal_paths.push_back(
        "BrightNights-Structured-Kenan-Modpack-master/Kenan-BrightNights-Structured-Modpack/Medium-Maintenance-Small-Mods");
    p.archived_path = "BrightNights-Structured-Kenan-Modpack-master/Kenan-BrightNights-Structured-Modpack/Archived-Mods";
    packs.push_back(p);

    return packs;
}

const Modpack* ModManager::modpack_for_game(const std::string& game)
{
    static const std::vector<Modpack> packs = make_modpacks();
    if (game == "dda")
        return &packs[0];
    if (game == "bn")
        return &packs[1];
    return nullptr;
}

ModManager::ModManager(Settings& settings, Paths& paths, Status& status, Trash& trash)
    : settings_(settings), paths_(paths), status_(status), trash_(trash), http_(status)
{
}

void ModManager::cancel()
{
    http_.cancel();
}

std::string ModManager::strip_html_tags(const std::string& text)
{
    static const std::regex tag("<[^<>]+>");
    return std::regex_replace(text, tag, "");
}

std::map<std::string, ModEntry> ModManager::parse_mods_dir(const std::string& mods_dir, Status& status)
{
    std::map<std::string, ModEntry> result;

    std::vector<std::string> subdirs = Paths::list_dir(mods_dir);
    for (size_t i = 0; i < subdirs.size(); ++i)
    {
        std::string location = mods_dir + "/" + subdirs[i];
        std::string modinfo_file = location + "/modinfo.json";
        std::ifstream f(modinfo_file);
        if (!f)
            continue;

        std::stringstream ss;
        ss << f.rdbuf();
        // Some mods have comments in their JSON, which the game allows.
        nlohmann::json json = nlohmann::json::parse(ss.str(), nullptr, false, true);
        if (json.is_discarded())
        {
            status.post("Could not parse " + modinfo_file, MSG_ERROR);
            continue;
        }

        // The file has either one object or a list of them; the one we want is MOD_INFO.
        std::vector<nlohmann::json> items;
        if (json.is_object())
            items.push_back(json);
        else if (json.is_array())
            items.assign(json.begin(), json.end());

        for (size_t k = 0; k < items.size(); ++k)
        {
            nlohmann::json info = items[k];
            if (!info.is_object() || info.value("type", std::string()) != "MOD_INFO")
                continue;

            std::string name = info.contains("name") && info["name"].is_string() ? info["name"].get<std::string>() : "";
            std::string id;
            if (info.contains("id") && info["id"].is_string())
                id = info["id"].get<std::string>();
            else if (info.contains("ident") && info["ident"].is_string())  // Not all mods have IDs, apparently.
                id = info["ident"].get<std::string>();
            else
                id = name;
            if (id.empty())
                break;  // Nothing to identify the mod by.

            name = strip_html_tags(name.empty() ? id : name);
            info["name"] = name;
            if (info.contains("description") && info["description"].is_string())
                info["description"] = strip_html_tags(info["description"].get<std::string>());
            info["id"] = id;

            ModEntry mod;
            mod.id = id;
            mod.name = name;
            mod.location = location;
            mod.modinfo = info;
            result[id] = mod;
            break;
        }
    }
    return result;
}

void ModManager::refresh_installed()
{
    std::map<std::string, ModEntry> result = parse_mods_dir(paths_.mods_user(), status_);
    for (std::map<std::string, ModEntry>::iterator it = result.begin(); it != result.end(); ++it)
    {
        it->second.is_stock = false;
        it->second.is_obsolete = false;
    }

    // Stock mods come last, so they win if the ID is the same.
    std::map<std::string, ModEntry> stock = parse_mods_dir(paths_.mods_stock(), status_);
    for (std::map<std::string, ModEntry>::iterator it = stock.begin(); it != stock.end(); ++it)
    {
        it->second.is_stock = true;
        it->second.is_obsolete = it->second.modinfo.value("obsolete", false);
        result[it->first] = it->second;
    }

    installed_ = result;
}

void ModManager::refresh_available()
{
    available_ = parse_mods_dir(paths_.mod_repo(), status_);
}

ModStatus ModManager::mod_status(const std::string& id) const
{
    if (installed_.find(id + "__") != installed_.end())
        return MOD_INSTALLED_MODIFIED_ID;

    std::map<std::string, ModEntry>::const_iterator it = installed_.find(id);
    if (it == installed_.end())
        return MOD_NOT_INSTALLED;
    if (!it->second.is_stock)
        return MOD_INSTALLED;
    return it->second.is_obsolete ? MOD_STOCK_OBSOLETE : MOD_STOCK;
}

// Gives the MOD_INFO entry of a modinfo.json file a modified ID ("id__") and
// name ("name*"). The rest of the file is written back as the mod has it.
static bool rename_mod(const std::string& file, const std::string& id, Status& status)
{
    std::ifstream f(file);
    std::stringstream ss;
    ss << f.rdbuf();
    nlohmann::json json = nlohmann::json::parse(ss.str(), nullptr, false, true);
    if (!f || json.is_discarded())
    {
        status.post("Could not parse " + file, MSG_ERROR);
        return false;
    }

    // Same lookup as parse_mods_dir: the first MOD_INFO entry, alone or in a list.
    nlohmann::json* info = nullptr;
    if (json.is_object())
        info = &json;
    else if (json.is_array())
    {
        for (size_t i = 0; i < json.size() && !info; ++i)
        {
            if (json[i].is_object() && json[i].value("type", std::string()) == "MOD_INFO")
                info = &json[i];
        }
    }
    if (!info)
    {
        status.post("No MOD_INFO entry in " + file, MSG_ERROR);
        return false;
    }

    // Change the ID where the mod keeps it ("id", or the older "ident").
    if (!info->contains("id") && info->contains("ident"))
        (*info)["ident"] = id + "__";
    else
        (*info)["id"] = id + "__";
    std::string name = info->contains("name") && (*info)["name"].is_string() ? (*info)["name"].get<std::string>() : id;
    (*info)["name"] = name + "*";

    return save_json_file(json, file, status);
}

bool ModManager::install_mod(const std::string& id)
{
    std::map<std::string, ModEntry>::const_iterator found = available_.find(id);
    if (found == available_.end())
    {
        status_.post("Could not find mod with ID \"" + id + "\"", MSG_ERROR);
        return false;
    }
    const ModEntry& mod = found->second;
    std::string mods_dir = paths_.mods_user();

    if (!fs_helper::copy_dir(mod.location, mods_dir, status_))
        return false;

    // If a stock mod with this ID is obsolete, both would clash in the game.
    // Our copy gets another ID (and a star in the name to tell them apart).
    std::map<std::string, ModEntry>::const_iterator clash = installed_.find(id);
    if (clash != installed_.end() && clash->second.is_obsolete)
    {
        status_.post("There is already an obsoleted mod with ID " + id + ". " + mod.name
                     + " will be installed with modified ID and name to avoid collisions.");
        std::string target = mods_dir + "/" + fs::path(mod.location).filename().string() + "/modinfo.json";
        if (!rename_mod(target, id, status_))
            return false;
    }

    status_.post("Installed " + mod.name);
    return true;
}

bool ModManager::install_mods(const std::vector<std::string>& ids)
{
    if (ids.empty())
        return true;
    if (ids.size() > 1)
        status_.post("Installing " + std::to_string(ids.size()) + " mods...");

    bool ok = true;
    for (size_t i = 0; i < ids.size(); ++i)
    {
        if (!install_mod(ids[i]))
            ok = false;
    }
    refresh_installed();
    return ok;
}

bool ModManager::delete_mod(const std::string& id)
{
    std::map<std::string, ModEntry>::const_iterator found = installed_.find(id);
    if (found == installed_.end())
    {
        status_.post("Could not find mod with ID \"" + id + "\"", MSG_ERROR);
        return false;
    }
    if (found->second.is_stock)
    {
        status_.post(found->second.name + " comes with the game and cannot be deleted.", MSG_ERROR);
        return false;
    }

    std::string name = found->second.name;
    if (!trash_.remove(found->second.location, "mod", name))
        return false;
    status_.post("Deleted " + name);
    return true;
}

bool ModManager::delete_mods(const std::vector<std::string>& ids)
{
    if (ids.empty())
        return true;
    if (ids.size() > 1)
        status_.post("Deleting " + std::to_string(ids.size()) + " mods...");

    bool ok = true;
    for (size_t i = 0; i < ids.size(); ++i)
    {
        // For these, the installed copy has the modified ID.
        std::string id = (mod_status(ids[i]) == MOD_INSTALLED_MODIFIED_ID) ? ids[i] + "__" : ids[i];
        if (!delete_mod(id))
            ok = false;
    }
    refresh_installed();
    return ok;
}

bool ModManager::retrieve_modpack(const Modpack& pack)
{
    status_.post("Retrieving " + pack.name + "...");

    std::string archive_path = paths_.cache_dir() + "/" + pack.filename;
    std::error_code ec;

    if (settings_.get_bool("ignore_cache") || !fs::exists(archive_path))
    {
        http_.apply_proxy_settings(settings_, true);
        if (!http_.download_file(pack.url, paths_.cache_dir(), pack.filename))
            return false;
    }

    // Always extract into an empty directory.
    std::string tmp_dir = paths_.tmp_dir();
    fs::remove_all(tmp_dir, ec);
    bool extracted = archive_helper::extract(archive_path, tmp_dir, status_);
    if (!settings_.get_bool("keep_cache"))
        fs::remove(archive_path, ec);
    if (!extracted)
    {
        fs::remove_all(tmp_dir, ec);
        return false;
    }

    // Look before wiping the repository, so that a surprising archive does not leave it empty.
    for (size_t i = 0; i < pack.internal_paths.size(); ++i)
    {
        if (!fs::is_directory(tmp_dir + "/" + pack.internal_paths[i]))
        {
            status_.post("The archive does not contain \"" + pack.internal_paths[i] + "\".", MSG_ERROR);
            fs::remove_all(tmp_dir, ec);
            return false;
        }
    }

    bool ok = true;
    std::string repo = paths_.mod_repo();

    status_.post("Wiping the repository...");
    if (fs::exists(repo) && !fs_helper::rm_dir(repo, status_))
        ok = false;

    status_.post("Adding mods from the pack to repository...");
    for (size_t i = 0; i < pack.internal_paths.size(); ++i)
    {
        if (!fs_helper::move_dir(tmp_dir + "/" + pack.internal_paths[i], repo, status_))
            ok = false;
    }

    if (settings_.get_bool("install_archived_mods"))
    {
        status_.post("Adding archived mods to repository...");
        if (!fs_helper::move_dir(tmp_dir + "/" + pack.archived_path, repo, status_))
            ok = false;
    }

    status_.post("Cleaning up...");
    fs::remove_all(tmp_dir, ec);

    refresh_available();
    status_.post(ok ? "All finished." : "Finished with errors.", ok ? MSG_INFO : MSG_WARN);
    return ok;
}
