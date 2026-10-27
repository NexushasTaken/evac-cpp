#ifndef EVAC_BACKUP_MANAGER_H
#define EVAC_BACKUP_MANAGER_H

#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#include "core/paths.h"
#include "core/settings.h"
#include "core/status.h"
#include "core/trash.h"

struct BackupSummary
{
    std::string name;
    std::filesystem::path path;
    std::vector<std::string> worlds;
};

// Backs up and restores the saved games of the current game. A backup is a
// directory in save_backups with one .zip per world. Backups in the old
// location are moved there first (see Paths::move_old_backups). These block, so use a
// worker thread.
class BackupManager
{
public:
    BackupManager(Settings& settings, Paths& paths, Status& status, Trash& trash);

    // Names are used as directory names, so some characters are not allowed
    // (the same rules as the original launcher, whatever the OS).
    static bool is_valid_backup_name(const std::string& name);

    // "Manual_yy-mm-dd_hh-mm" for the given time.
    static std::string default_backup_name(const std::tm& time);

    std::vector<BackupSummary> list();

    bool backup_current(const std::string& backup_name);

    // Replaces the current saves with the backup. The backup is unpacked first,
    // so if that fails the current saves stay as they are.
    bool restore(const std::string& backup_name);

    bool delete_backup(const std::string& backup_name);

private:
    Settings& settings_;
    Paths& paths_;
    Status& status_;
    Trash& trash_;
};

#endif
