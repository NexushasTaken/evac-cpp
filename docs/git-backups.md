# Git backups

Design agreed on 2026-10-19. The first version (see Scope) is implemented; the "Later" items are not.

Adds a **Snapshots** tab: one git repository per world, the manual "git in userdata" workflow built
into the launcher, limited to world saves. The existing **Backups** tab (zip backups,
`src/core/backup_manager.*`, one directory per backup with one .zip per world) stays as it is,
except that its folder moves (see Storage).

## Words

The launcher uses these terms in the UI and in the code:

| Term | What it is | Where |
| --- | --- | --- |
| **Snapshot** | A git commit in a world's repository. Quick, stays on this disk. | Snapshots tab |
| **Backup** | A zip backup of all worlds, the existing feature. | Backups tab |
| **Snapshot export** | A git bundle: one file with a world's whole snapshot history, to keep elsewhere or share. | Snapshots tab, `⋮` menu |

"Save" is not used for any of these: in a CDDA launcher it already means the game's own saves
(`userdata/save/`, "Save and quit").

## Why git

- A CDDA world is many small files (`maps/`, `o.X.Y`, ...) and most of them do not change between
  two snapshots. Git stores each unique file version once, so many snapshots cost little more than
  one.
- Taking a snapshot is a commit and restoring is a checkout. No full re-zip of every world each time.

## How the game stores a world

Checked in the game's source (`src/game.cpp`, `src/worldfactory.cpp`, `src/overmapbuffer.cpp`,
`src/diary.cpp`):

```
userdata/save/<World>/          one folder per world, named after the world
├── worldoptions.json           world settings
├── mods.json                   mods this world uses
├── master.gsav                 shared world state (factions, missions, ids)
├── uistate.json
├── maps/                       changed terrain, items, monsters
├── o.X.Y                       overmaps
├── *.dict                      dictionaries for compressed saves
└── per character, named base64(character name):
    <b64>.sav  <b64>.pt  <b64>.seen.X.Y  <b64>.mm1/  base64(name + "_diary")
```

All characters of a world share its map. A character's items and the places they changed live in
the world files, so **the world is the smallest unit that can be restored safely**. Restoring one
character alone would duplicate items or desync the map.

The game only treats a folder in `save/` as a world if it has world files in it
(`worldfactory.cpp:426`). Achievements are in `userdata/achievements/`, outside `save/`.

## Storage

Snapshots and backups belong to one userdata folder, so they live inside it:

```
<userdata>/
├── save/<World>/                   the game's worlds, read and written by both tabs
└── catapult/                       the launcher's folder, the only one the game does not own
    ├── backups/<name>/             Backups tab (zip), was <game base>/save_backups/
    └── snapshots/<World>.git       Snapshots tab (git), one bare repository per world
```

- **Why inside userdata:** a later feature will switch between several userdata folders (for
  example one on the Windows partition and one on the Linux side, both with the same layout). Two
  userdata folders can each have a world called `Courtland`. If the repos stayed in
  `<game base>/`, switching would point Courtland's history at the other Courtland. Inside userdata,
  history and backups move with their worlds and nothing has to map one to the other.
- **Rule for the code:** every per-userdata path is built from `Paths::userdata()`, never from
  `game_base_dir()`. Then switching userdata only changes one path. The trash stays global
  (`games_dir/trash`).
- **One launcher folder:** `catapult/` holds everything the launcher adds, so the game's own folders
  stay untouched. Checked in the DDA source: the game never lists the userdata folder itself, it
  only builds fixed paths under it (`src/path_info.cpp`), so an extra folder is ignored. Not checked
  for Bright Nights.
- **Moving the existing zip backups:** on the first start with this version, if
  `<game base>/save_backups/` exists and `catapult/backups/` does not, move it there. This is the
  only change to the Backups tab.
- One bare repo per world (`git init --bare`), used with
  `git --git-dir=catapult/snapshots/<World>.git --work-tree=save/<World> ...`
  (the "bare repo for dotfiles" pattern). No clone and no `.git` inside the game's folders.
- Only the world folder is tracked. Config, fonts, sound, achievements, memorial and graveyard are not.
- Snapshot names and descriptions are commit messages: the name is the first line, the description
  is the body. Any text is allowed except an empty name (they are not folder names).

## Running git

- Shell out to the `git` binary (no libgit2, no new Conan dependency). Slow work runs on the worker
  thread, one job at a time, like every other slow job in the launcher.
- Run git directly with an argv list, never through a shell (world names can have spaces and
  quotes). For example `Glib::spawn_sync`, capturing stdout, stderr and the exit status.
- The user's git config must not interfere. A global ignore file (`core.excludesFile`) could
  silently leave world files out of every snapshot. So every call runs with the environment
  `GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_NOSYSTEM=1` and forces:
  `-c user.name=catapult -c user.email=catapult@localhost -c core.hooksPath=/dev/null
  -c core.autocrlf=false -c core.filemode=false -c core.quotePath=false -c commit.gpgsign=false
  -c init.defaultBranch=main -c safe.directory=*`.
  It also forces `core.excludesFile=/dev/null` and `core.attributesFile=/dev/null` (git reads
  `~/.config/git/ignore` even without a global config file), `gc.autoDetach=false` and
  `maintenance.autoDetach=false` (no background job holding a lock), and drops every `GIT_*`
  variable from the environment (`GIT_DIR` and friends would point git elsewhere).
  - `core.filemode=false`: an NTFS partition can show every file as executable, which git would
    otherwise report as a change to every file.
  - `safe.directory=*`: an NTFS partition can be mounted with another owner, and git then refuses
    the repo ("dubious ownership"). Git accepts this setting on the command line.
  - Commits never carry the user's real name or email, which matters when sharing an export.
- Parse machine output (`status --porcelain -z`, `log -z` with a `--format`), not human output.
  Snapshots are identified by commit hash in the code, never by name (names can repeat).
- Needs git 2.23 or newer (`git restore`).
- If `git` is not installed, the Snapshots tab says so and is disabled.
- The tab shows the exact `git --git-dir=... --work-tree=...` command for the selected world with a
  Copy button, so the repo can be used from a terminal. No "open terminal" button: Linux has no
  reliable way to find the default terminal.

## Actions

| Action | What it does |
| --- | --- |
| **Take snapshot** (selected world) | `git add -A` then commit. Name defaults to the date, description is optional. If nothing changed: "No changes since the last snapshot" and no commit. Creates the repo on first use. |
| **Take snapshot of all worlds** | Take a snapshot of every world that has changes, with the same name and description. Worlds without changes are skipped. Reports the result, e.g. "2 snapshots taken, 1 world had no changes". |
| **Restore snapshot X** | 1. If the world changed since the last snapshot, commit it as "Before restoring X". 2. `git restore --source=X --staged --worktree -- .` 3. `git clean -fd` (removes files X does not have). 4. Commit "Restored X". History is never rewritten, nothing is lost. Asks for confirmation first. |
| **Discard changes** | Steps 2 and 3 of Restore with X = `HEAD`, no commits. This is the one action that throws away changes, so it asks for confirmation and says so. |

`git add -A` records files the game deleted, so a restore brings them back.

- **Nothing to restore:** if the world already matches X, say so and make no commits. In general
  every commit step is skipped when there is nothing to commit, so there are never empty commits.
- **No snapshots yet** (repo missing or without commits): only Take snapshot is possible.
- **Empty folders:** git does not track them, so a restore does not bring back an empty folder.
  That is fine: the game creates the folders it writes to (`maps/` in `src/game.cpp:8537`, map
  memory in `src/map_memory.cpp:471`, compressed overmaps in `src/overmap.cpp:7817`).

There is no "delete one snapshot": that would rewrite history. **Start fresh** (later) is the way to
shrink a world's history.

### Worlds that no longer exist

A world folder can disappear while its repo stays: deleted in the game, or removed by restoring a zip
backup that does not contain it.

1. The first time the Snapshots tab finds such a repo, it asks:
   "Courtland no longer exists. Delete its snapshots too? [Cancel] [Move to Trash]".
   Move to Trash moves the repo to the in-app trash, so it can still be recovered from the Trash tab.
2. On Cancel, the repo is marked as kept (for example `git config catapult.keep true` in the repo,
   so the mark travels with it) and the tab does not ask again. The world stays in the list, greyed
   out, labelled `deleted`. Its snapshots can be viewed, and **Restore snapshot** recreates the world
   folder (create the folder before `git restore`). Later, **Delete snapshots…** in the `⋮` menu
   moves the repo to the trash, with confirmation. Until then, the repo can be deleted from a file
   manager.

A new world created with the same name continues that name's history. Accepted.

## Living next to the Backups tab

The two tabs never touch each other's files: zip restore cannot break the repos (they are outside
`save/`), and git never touches `catapult/backups/`. They only meet in the world folders:

- **Zip restore** replaces every world. In the Snapshots tab those worlds show `● changed`, and the
  next snapshot records the restore. Correct as is.
- **Zip restore with changes not in a snapshot:** the current saves go to the trash as today. The
  restore dialog should add which worlds have changes not in a snapshot, e.g.
  "Courtland has changes not in a snapshot".
- **Zip restore removes a world** the zip does not contain: handled by "Worlds that no longer exist".

## UI

```
 Game  Mods  Soundpacks  Fonts  Snapshots  Backups  Trash  Settings
┌────────────────────────────────────────────────────────────────────────────┐
│ Worlds                    │ Snapshots of Courtland                    [⋮] │
│ ┌───────────────────────┐ │ ┌────────────────────────────────────────────┐ │
│ │ Courtland   ● changed │ │ │ Oct 19 21:04  before raiding the lab       │ │
│ │ OtherWorld            │ │ │ Oct 19 20:31  Thu Oct 19 20:31 2026        │ │
│ │ NewWorld no snapshots │ │ │ Oct 18 23:10  Restored "first night"       │ │
│ │ OldWorld      deleted │ │ │ Oct 18 23:09  Before restoring "first ..." │ │
│ │                       │ │ │ Oct 18 22:00  first night                  │ │
│ └───────────────────────┘ │ └────────────────────────────────────────────┘ │
│                           │ [ Restore snapshot ]  [ Discard changes ]      │
│                           │ ┌────────────────────────────────────────────┐ │
│                           │ │ before raiding the lab                     │ │
│                           │ │ Oct 19 2026, 21:04 · 14 files changed      │ │
│                           │ │ Cleared the house next door, going in at   │ │
│                           │ │ night with 3 molotovs.                     │ │
│                           │ └────────────────────────────────────────────┘ │
├────────────────────────────────────────────────────────────────────────────┤
│ Name:        [Thu Oct 19 21:42 2026                                      ] │
│ Description: [                                                           ] │
│              [ Take snapshot ]  [ Take snapshot of all worlds ]            │
├────────────────────────────────────────────────────────────────────────────┤
│ 41 snapshots, 12 MB   git --git-dir=…/snapshots/Courtland.git …   [Copy]   │
└────────────────────────────────────────────────────────────────────────────┘
```

- **Worlds list** (`Gtk::ListBox`): every folder in `save/` plus kept repos of worlds that no longer
  exist. Status from `git status --porcelain`: `● changed`, `no snapshots` (no repo yet), `deleted`
  (greyed out), or nothing when the world matches its last snapshot. A list rather than a dropdown,
  so it is visible which worlds need a snapshot.
- **Snapshot list:** the selected world's history, newest first, date and name per row.
- **Details:** name, full date, number of changed files, description.
- **Restore snapshot:** enabled when a snapshot is selected. **Discard changes:** enabled only when
  the world has changes. Both ask for confirmation.
- **Take snapshot:** name defaults to the date, description is a one-line optional entry.
  For a `deleted` world, Take snapshot and Discard changes are disabled.
- **Footer:** history size of the selected world, and the git command with a Copy button.
- **`⋮` menu** (`Gtk::MenuButton`): Export snapshots…, Import snapshots…, Start fresh…,
  Delete snapshots… (worlds that no longer exist only). All of these are later items, so the first
  version has no `⋮` button.
- The tab reloads when it is shown, no Refresh button. If many large worlds make `git status` slow,
  run it on the worker thread so the tab does not stall.

## Edge cases

- **Game running:** a snapshot taken while the game is saving can be half-written. The launcher cannot
  tell when the game exits yet (`docs/to-be-implemented.md`, game process tracking). The first
  version does not check for this.
- **Compressed saves (`.zzip`):** compressed files change completely when their content changes, so
  history grows faster. Unchanged files are still stored once.
- **Read-only userdata:** Linux mounts an NTFS partition read-only when Windows was hibernated or
  shut down with Fast Startup. Taking a snapshot or restoring then fails. The tab should check
  first and say "userdata is read-only", not show a git error.
- **Trash on another partition:** moving a repo from userdata on the Windows partition to the
  global trash is a copy then delete, not a rename, so it is slow for a large history. The trash
  already handles this: `fs_helper::move_dir` falls back to copying item by item
  (`src/core/fs.cpp:139`).

## Where the code goes

Follow the zip backups as the model (`BackupManager` + `TabBackups`):

- `src/core/git.{h,cpp}`: the one function that runs git (argv, environment and forced config
  above) and returns exit status, stdout and stderr. Plus "is git installed".
- `src/core/snapshot_manager.{h,cpp}`: list worlds with their status, list snapshots, take
  snapshot, take snapshot of all worlds, restore, discard, find repos of worlds that no longer
  exist, mark kept. No GTK in here.
- `src/core/paths.{h,cpp}`: `launcher_dir()` = `userdata()/catapult`, `snapshots()` =
  `launcher_dir()/snapshots`, and `save_backups()` changed to `launcher_dir()/backups`, plus the
  one-time move of the old folder.
- `src/ui/tab_snapshots.{h,cpp}`, added to `src/ui/main_window.cpp` between Fonts and Backups. Slow
  work through the `TaskRunner`, confirmations through `dialogs::confirm`.
- `tests/test_snapshots.cpp`: real git in a temp folder. Cover snapshot, no-change snapshot,
  restore (a file added after X is removed, a file deleted after X comes back, "Before restoring"
  and "Restored" commits), discard, a world name with spaces, and the missing-world case.

Suggested order, one commit each: git runner, paths and the backups folder move, snapshot manager
with tests, the tab, the "no longer exists" prompt.

## Scope

**First version:** the Snapshots tab with the worlds list, snapshot list, details, Take snapshot,
Take snapshot of all worlds, Restore snapshot, Discard changes, and the "no longer exists" prompt with
the `deleted` state. Storage under `<userdata>/catapult/`, including the one-time move of
`save_backups/`. The Backups tab is otherwise not changed. Switching userdata is a separate, later
feature: this version only makes sure it will work.

**Later:**
- **Export snapshots…:** `git bundle create <World>.bundle --all` to a file the user picks. One file
  with every snapshot, to keep elsewhere or share with history.
- **Import snapshots…:** `git bundle verify`, then `git clone --bare` into
  `catapult/snapshots/<World>.git`, then restore the latest snapshot into `save/<World>`. If the world name
  is taken, ask for a new name or cancel, never overwrite. Could warn about mods from `mods.json`
  that are not installed.
- **Start fresh** (per world): move the repo to the trash (with confirmation) and take one snapshot
  of the current state. Shrinks the history.
- **Delete snapshots…** for worlds that no longer exist.
- The zip restore dialog naming worlds with changes not in a snapshot.

**Not planned:**
- Automatic snapshots, until the launcher can track the game process.
- A separate "Export world (.zip)": the Backups tab already makes zips.

## Effect on `docs/to-be-implemented.md`

- **Automatic backups** and **Richer backup list** still apply to the Backups tab.
- Automatic snapshots would be the cheap version of automatic backups once game process tracking
  exists.
