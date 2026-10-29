#ifndef EVAC_SNAPSHOT_MANAGER_H
#define EVAC_SNAPSHOT_MANAGER_H

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#include "core/git.h"
#include "core/paths.h"
#include "core/status.h"
#include "core/trash.h"

// A world in save/, or one that no longer exists but still has snapshots.
struct WorldInfo {
  enum State {
    NO_SNAPSHOTS,  // No repository, or one without commits.
    UNCHANGED,     // Matches its last snapshot.
    CHANGED,       // Has changes that are not in a snapshot.
    DELETED,       // The world folder is gone, the repository is still there.
  };
  std::string name;
  State state = NO_SNAPSHOTS;
  bool kept = false;  // DELETED only: the user chose to keep its snapshots.
};

// A commit in a world's repository. Snapshots are told apart by hash, never by
// name, as names can repeat.
struct Snapshot {
  std::string hash;
  std::int64_t time = 0;    // Unix time of the commit.
  std::string name;         // First line of the message.
  std::string description;  // The rest of it.
};

// Git snapshots of the worlds of the current game: one bare repository per
// world in Paths::snapshots(), with the world folder as its work tree. See
// docs/git-backups.md. These block, so use a worker thread for anything that
// writes. Problems are reported to Status.
class SnapshotManager {
 public:
  enum Result {
    DONE,
    NOTHING_TO_DO,  // No changes to snapshot, or the world already matches the
                    // snapshot.
    FAILED,
  };

  struct AllResult {
    int taken = 0;
    int unchanged = 0;
    int failed = 0;
  };

  SnapshotManager(Paths& paths, Status& status, Trash& trash);

  // "Thu Oct 19 21:42 2026" for the given time.
  static std::string default_snapshot_name(const std::tm& time);

  // Folders in save/, then the worlds that no longer exist, each sorted by
  // name.
  std::vector<WorldInfo> list_worlds();
  // Worlds in save/ that have snapshots and changes since the last one, sorted
  // by name. For warnings before something replaces the saves.
  std::vector<std::string> worlds_with_changes();
  // Newest first. Empty if there are none.
  std::vector<Snapshot> list_snapshots(const std::string& world);
  // Number of files the snapshot changed compared to the one before it.
  int changed_files(const std::string& world, const std::string& hash);
  // Bytes the repository takes on disk.
  std::uint64_t history_size(const std::string& world);
  // The git command for using the repository from a terminal.
  std::string git_command(const std::string& world);
  std::filesystem::path repo_path(const std::string& world);

  // False when the save folder can't be written to (e.g. NTFS mounted
  // read-only), with a message for the user in reason.
  bool can_write(std::string& reason);

  // Creates the repository on first use. NOTHING_TO_DO if nothing changed since
  // the last snapshot.
  Result take_snapshot(const std::string& world, const std::string& name,
                       const std::string& description);
  // Every world in save/ that has changes, with the same name and description.
  AllResult take_snapshot_all(const std::string& name,
                              const std::string& description);

  // Makes the world match the snapshot. Changes not in a snapshot are first
  // committed as "Before restoring ...", then the restore is committed as
  // "Restored ...", so nothing is lost. Recreates the folder of a deleted
  // world.
  Result restore(const std::string& world, const std::string& hash);
  // Throws away the changes since the last snapshot.
  Result discard_changes(const std::string& world);

  // For a world that no longer exists: don't ask again about deleting its
  // snapshots.
  bool mark_kept(const std::string& world);
  // Moves the repository to the trash.
  bool delete_snapshots(const std::string& world);
  // Moves the world's snapshots to the trash and starts a new history with one
  // snapshot of the world as it is now. Shrinks the history.
  Result start_fresh(const std::string& world, const std::string& name,
                     const std::string& description);

  // Writes every snapshot of the world to one git bundle file at path, to keep
  // elsewhere or share. Replaces the file if it exists.
  bool export_snapshots(const std::string& world,
                        const std::filesystem::path& path);

  // The world name an imported bundle gets by default: its file name without
  // ".bundle".
  static std::string bundle_world_name(const std::filesystem::path& path);
  // Why world can't be the name of an imported world, or "" if it can. The
  // game's rules for world names, and no world or snapshots by that name yet.
  std::string check_import_name(const std::string& world);
  // Makes a new world from a bundle made by export_snapshots: its snapshots,
  // and the world folder as in the latest one. Never replaces an existing
  // world.
  Result import_snapshots(const std::filesystem::path& bundle,
                          const std::string& world);

 private:
  std::filesystem::path world_dir(const std::string& world);
  std::vector<std::string> repo_args(const std::string& world);
  git::Result run(const std::string& world,
                  const std::vector<std::string>& args);
  bool run_or_report(const std::string& world,
                     const std::vector<std::string>& args,
                     const std::string& what);
  Result take(const std::string& world, const std::string& name,
              const std::string& description, bool report_unchanged);
  bool has_commits(const std::string& world);
  bool ensure_repo(const std::string& world);
  // git add -A, then: 1 if there is something to commit, 0 if not, -1 on error.
  int stage_all(const std::string& world);
  bool commit(const std::string& world, const std::string& name,
              const std::string& description);
  bool reset_to(const std::string& world, const std::string& rev);

  Paths& m_paths;
  Status& m_status;
  Trash& m_trash;
};

#endif
