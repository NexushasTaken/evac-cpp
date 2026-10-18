#include "core/soundpack_manager.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/archive.h"
#include "core/fs.h"

namespace fs = std::filesystem;

static const char NAME_PREFIX[] = "VIEW: ";
static const char DESC_PREFIX[] = "DESCRIPTION: ";
static const char CONFIG_FILE[] = "soundpack.txt";
static const char DISABLED_SUFFIX[] = ".disabled";

static bool starts_with(const std::string& text, const char* prefix)
{
    return text.compare(0, std::string(prefix).size(), prefix) == 0;
}

SoundpackManager::SoundpackManager(Settings& settings, Paths& paths, Status& status, Trash& trash)
    : settings_(settings), paths_(paths), status_(status), trash_(trash), http_(status)
{
}

void SoundpackManager::cancel()
{
    http_.cancel();
}

std::vector<InstalledSoundpack> SoundpackManager::parse_sound_dir(const std::string& sound_dir)
{
    std::vector<InstalledSoundpack> result;

    std::vector<std::string> subdirs = Paths::list_dir(sound_dir);
    for (size_t i = 0; i < subdirs.size(); ++i)
    {
        std::string location = sound_dir + "/" + subdirs[i];
        std::string config = location + "/" + CONFIG_FILE;
        bool enabled = true;
        std::error_code ec;
        if (!fs::exists(config, ec))
        {
            if (ec)
                continue;  // Can't look inside, so skip it like an unreadable file.
            config += DISABLED_SUFFIX;
            enabled = false;
        }
        std::ifstream f(config);
        if (!f)
            continue;

        InstalledSoundpack pack;
        pack.location = location;
        pack.enabled = enabled;
        std::string line;
        while (std::getline(f, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            if (starts_with(line, NAME_PREFIX))
                pack.name = line.substr(sizeof(NAME_PREFIX) - 1);
            else if (starts_with(line, DESC_PREFIX))
                pack.description = line.substr(sizeof(DESC_PREFIX) - 1);
        }
        if (pack.name.empty())
            pack.name = subdirs[i];  // No VIEW line: better than an empty name.
        result.push_back(pack);
    }
    return result;
}

std::vector<InstalledSoundpack> SoundpackManager::get_installed(bool include_stock)
{
    std::vector<InstalledSoundpack> packs = parse_sound_dir(paths_.sound_user());

    if (include_stock)
    {
        std::vector<InstalledSoundpack> stock = parse_sound_dir(paths_.sound_stock());
        for (size_t i = 0; i < stock.size(); ++i)
        {
            stock[i].is_stock = true;
            packs.push_back(stock[i]);
        }
    }
    return packs;
}

bool SoundpackManager::find_user_pack(const std::string& location, InstalledSoundpack& out)
{
    std::vector<InstalledSoundpack> packs = get_installed(false);
    for (size_t i = 0; i < packs.size(); ++i)
    {
        if (packs[i].location == location)
        {
            out = packs[i];
            return true;
        }
    }
    status_.post("Could not find a soundpack in " + location, MSG_ERROR);
    return false;
}

bool SoundpackManager::delete_pack(const std::string& location)
{
    InstalledSoundpack pack;
    if (!find_user_pack(location, pack))
        return false;
    return trash_.remove(pack.location, "soundpack", pack.name);
}

bool SoundpackManager::set_enabled(const std::string& location, bool enabled)
{
    InstalledSoundpack pack;
    if (!find_user_pack(location, pack))
        return false;
    if (pack.enabled == enabled)
        return true;

    std::string config = pack.location + "/" + CONFIG_FILE;
    std::string disabled = config + DISABLED_SUFFIX;
    std::string from = enabled ? disabled : config;
    std::string to = enabled ? config : disabled;

    // rename() replaces an existing file, so check first. symlink_status also
    // sees a dangling symlink, which exists() would not.
    std::error_code ec;
    fs::file_status to_status = fs::symlink_status(to, ec);
    if (fs::exists(to_status))
    {
        status_.post("Not renaming the config of soundpack \"" + pack.name + "\": " + to + " already exists.",
                     MSG_ERROR);
        return false;
    }
    ec.clear();
    fs::rename(from, to, ec);
    if (ec)
    {
        status_.post("Could not rename the config of soundpack \"" + pack.name + "\": " + ec.message(), MSG_ERROR);
        return false;
    }

    status_.post(std::string(enabled ? "Enabled" : "Disabled") + " soundpack \"" + pack.name + "\".");
    return true;
}

bool SoundpackManager::install_pack(const SoundpackSource& pack, const std::string& from_file, bool reinstall,
                                    bool keep_archive)
{
    std::string sound_dir = paths_.sound_user();
    std::string tmp_dir = paths_.tmp_dir() + "/" + pack.name;
    std::string archive_path;
    std::error_code ec;

    if (reinstall)
        status_.post("Reinstalling soundpack \"" + pack.name + "\"...");
    else
        status_.post("Installing soundpack \"" + pack.name + "\"...");

    if (!from_file.empty())
    {
        archive_path = from_file;
    }
    else
    {
        archive_path = paths_.cache_dir() + "/" + pack.filename;
        if (settings_.get_bool("ignore_cache") || !fs::exists(archive_path))
        {
            http_.apply_proxy_settings(settings_, true);
            if (!http_.download_file(pack.url, paths_.cache_dir(), pack.filename))
            {
                status_.post("Could not download soundpack archive.", MSG_ERROR);
                return false;
            }
        }
    }

    // Always extract into an empty directory.
    fs::remove_all(tmp_dir, ec);
    bool extracted = archive_helper::extract(archive_path, tmp_dir, status_);

    if (!keep_archive && !settings_.get_bool("keep_cache"))
        fs::remove(archive_path, ec);

    std::string source = tmp_dir + "/" + pack.internal_path;
    if (extracted && !fs::is_directory(source))
    {
        status_.post("The archive does not contain \"" + pack.internal_path + "\".", MSG_ERROR);
        extracted = false;
    }
    if (!extracted)
    {
        fs::remove_all(tmp_dir, ec);
        return false;
    }

    // Only now is it safe to remove the old copy. The directory may not be
    // there even when reinstalling, if the installed copy has another name.
    std::string old_copy = sound_dir + "/" + pack.name;
    if (reinstall && fs::exists(fs::symlink_status(old_copy, ec)) && !trash_.remove(old_copy, "soundpack", pack.name))
    {
        fs::remove_all(tmp_dir, ec);
        return false;
    }

    bool moved = fs_helper::move_dir(source, sound_dir + "/" + pack.name, status_);
    fs::remove_all(tmp_dir, ec);
    if (!moved)
        return false;

    status_.post("Soundpack installed.");
    return true;
}
