#ifndef CATAPULT_TRASH_H
#define CATAPULT_TRASH_H

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "core/paths.h"
#include "core/settings.h"
#include "core/status.h"

// Something in the trash. On disk it is trash_dir/<id>/ with info.json and
// item/ (the thing itself) in it.
struct TrashItem
{
    std::string id;             // Directory name: 20261018-140322-123_soundpack_CC-Sounds
    std::string kind;           // "soundpack", "mod", "backup", "install", "saves"
    std::string name;           // What to show on the list.
    std::string game;           // The game that was selected when it was deleted.
    std::string original_path;
    std::string deleted_at;     // Local time, "2026-10-18 14:03:22.123". Sorts by time.
    std::uint64_t size = 0;     // In bytes, measured when it was moved here.
};

// Where deleted things go, so they can be restored. These block, so use a
// worker thread.
class Trash
{
public:
    typedef std::chrono::system_clock Clock;

    Trash(Settings& settings, Paths& paths, Status& status);

    // Moves path to the trash, or removes it for good when "use_trash" is off.
    bool remove(const std::string& path, const std::string& kind, const std::string& name);
    // The same, with the time to record (for tests).
    bool remove(const std::string& path, const std::string& kind, const std::string& name, Clock::time_point when);

    // Newest first. Entries without info.json and item/ (half written) are left out.
    std::vector<TrashItem> list();
    std::uint64_t total_size();

    bool delete_items(const std::vector<std::string>& ids);
    // Removes everything in the trash directory, half-written entries too.
    bool empty();

    // The oldest items that would have to go to get under "trash_max_size_mb",
    // oldest first. Never the newest item, so what was just deleted stays.
    // Only answers the question: the UI asks the user, then calls delete_items.
    std::vector<TrashItem> over_limit();

    static std::string make_id(const std::string& kind, const std::string& name, Clock::time_point when);

private:
    Settings& settings_;
    Paths& paths_;
    Status& status_;
};

#endif
