#ifndef EVAC_RELEASE_INSTALLER_H
#define EVAC_RELEASE_INSTALLER_H

#include <string>
#include <vector>

#include "core/http.h"
#include "core/paths.h"
#include "core/release_data.h"
#include "core/settings.h"
#include "core/status.h"
#include "core/trash.h"

// Installs, updates and removes game releases. These block, so use a worker
// thread. Nothing else should touch the settings while they run.
class ReleaseInstaller
{
public:
    ReleaseInstaller(Settings& settings, Paths& paths, Status& status, Trash& trash);

    // Downloads (unless the archive is cached) and installs the release. If
    // update_in is not empty, that existing install is replaced instead of
    // creating a new one.
    bool install_release(const Release& release, const std::string& update_in);

    bool remove_release_by_name(const std::string& release_name);

    // Returns the user data directories found inside the game directory of
    // the install, which would be lost when it gets deleted.
    std::vector<std::string> check_game_dir_for_userdata(const std::string& release_name);

    void cancel();

private:
    Settings& settings_;
    Paths& paths_;
    Status& status_;
    Trash& trash_;
    Http http_;
};

#endif
