#ifndef EVAC_TRASH_H
#define EVAC_TRASH_H

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

// What restoring an item would do. See Trash::plan_restore.
struct RestorePlan
{
    enum Kind
    {
        ORIGINAL,    // Goes back where it was.
        RENAMED,     // Goes back next to where it was, with " (restored)" added. For saves:
                     // the worlds go back, and the ones whose names are taken get renamed.
        CONFLICT,    // Something the game would mix up with it is there. Needs replace.
        IMPOSSIBLE,  // Can't be restored, see reason.
    };
    Kind kind = IMPOSSIBLE;
    std::string dest;           // ORIGINAL, RENAMED: where it goes.
    std::string conflict_path;  // CONFLICT: what replacing would move to the trash...
    std::string conflict_name;  // ...and its name there.
    std::string reason;         // CONFLICT, IMPOSSIBLE: for the user.
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

    // Only looks, changes nothing. By kind:
    //  - "mod": CONFLICT when a mod with the same ID is in the mods directory.
    //  - "install": CONFLICT when the same version is installed for that game;
    //    otherwise its old directory, or the next free gameN.
    //  - "saves": the whole directory if there are no saves now, else world by world.
    //  - anything else: its old path, or RENAMED when that is taken.
    // IMPOSSIBLE when the directory it was in is gone.
    RestorePlan plan_restore(const std::string& id);

    // Restores the item. A CONFLICT is only restored with replace, which first
    // moves the conflicting one to the trash. If something fails, what was not
    // moved back stays in the trash. A name (see check_restore_name) is the
    // folder name it goes back as, next to where it was, instead of plan.dest.
    bool restore(const std::string& id, bool replace, const std::string& name = "");

    // Whether a RENAMED item of this kind can be given a name by the user. Not
    // saves (the worlds go back one by one) or installs (gameN folders).
    static bool can_choose_name(const std::string& kind);
    // Why the item can't be restored as name, or "" if it can.
    std::string check_restore_name(const std::string& id, const std::string& name);

    static std::string make_id(const std::string& kind, const std::string& name, Clock::time_point when);

private:
    bool load_item(const std::string& id, TrashItem& out);

    Settings& settings_;
    Paths& paths_;
    Status& status_;
};

#endif
