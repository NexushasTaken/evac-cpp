# To be implemented

Features from the [Kitten CDDA Game Launcher](https://github.com/Fris0uman/CDDA-Game-Launcher)
(Windows only, Python/Qt; local clone at `~/dev/repos/CDDA-Game-Launcher`) that catapult-cpp does not have yet.
Paths starting with `cddagl/` point into that clone. Line numbers are from 2026-10-18 and may drift.

"Tackle #N" means explain it first, not implement it: say what the feature is for, who would use
it and when, and roughly what the change would touch, then wait for a go-ahead before writing code.

Work through these one at a time, one commit each. When a feature is done, remove its entry
from this file (its section, its line in "Suggested order", and any matching "Nice to have" item)
in the same commit, and renumber the sections and their `#N` references so there are no gaps.

## Suggested order

1. Automatic backups, "before update" and "before launch" first (#1)
2. Tracking the game process, then "backup after game end" (#2)
3. Later: keeping user content on update (#3), changelog (#5), switching userdata folders (#6)

## Worth porting

### 1. Automatic backups
- **Original:** checkboxes for "backup before launch", "after game end" and "before update".
  Names are `auto_before_launch`, `auto_after_end` and `auto_before_update_<build>v`.
  `max_auto_backups` (default 6) caps the count. Pruning removes the oldest auto backups
  until there are max-1, then makes the new one.
- **Reference:** `cddagl/ui/views/backups.py:265-290` (settings), `:730` (`prune_auto_backups`);
  call sites in `cddagl/ui/views/main.py:432, 580, 1419`.
- **In catapult-cpp:** reuse `BackupManager::backup_current` and `default_backup_name`
  (`src/core/backup_manager.h`). Add setting keys in `Settings::defaults()` (`src/core/settings.cpp`),
  a prune function plus tests in `tests/test_backups.cpp`, and checkboxes in `src/ui/tab_backups.cpp`.
- **Note:** catapult-cpp backups are a directory per backup (one .zip per world), not a single .zip,
  so prune by directory mtime. "After game end" needs #2.

### 2. Tracking the game process
- **Original:** watches the running game. The launch button becomes "Show current game",
  the status says "Game process has ended", and the post-game hooks (auto backup) run.
- **Reference:** `cddagl/ui/views/main.py:440-580, 898-970`.
- **In catapult-cpp:** `src/core/game_launcher.cpp` forks twice and only waits on the middle child
  (`waitpid` around line 77), so the launcher never learns when the game exits. Needed: keep the
  game's pid and get notified on exit (e.g. `Glib::signal_child_watch` in the UI, or a watcher
  thread). Decide what happens to "keep launcher open after starting the game" and to
  updating or deleting an install while the game runs.
- **Note:** this is a concurrency/design change. Settle the design before writing code.

### 3. Keeping user content on update
- **Original:** the old install moves to `previous_version/`. After extracting, it copies over
  custom tilesets, soundpacks, fonts, mods (comparing mod `ident`s so the bundled version wins)
  and `user-default-mods.json`. There's also a "Restore previous version" button.
- **Reference:** `cddagl/ui/views/main.py:2143-2560` (`mod_ident`, `post_extraction*`,
  `preserve_custom_fonts`), `restore_previous` at `:345`.
- **In catapult-cpp:** `ReleaseInstaller::install_release` (`src/core/release_installer.cpp`)
  sends the old install to the trash when updating (deleted outright if `use_trash` is off), and
  the update dialog already warns about user content via `check_game_dir_for_userdata`. Nothing
  is copied into the new install yet, and restoring means doing it by hand from the Trash tab.
  Saves live in the separate user data folder.

### 4. Richer backup list
- **Original:** sortable columns for name, worlds, characters, uncompressed size, compressed size,
  compression ratio and modified date.
- **Reference:** `cddagl/ui/views/backups.py:1137-1270`.
- **In catapult-cpp:** extend `BackupSummary` (`src/core/backup_manager.h`) and show the columns in
  `src/ui/tab_backups.cpp` (for example with a `Gtk::ColumnView`).

### 5. Changelog
- **Original:** lists recently merged PRs from the game repo, using the GitHub search API
  (`CHANGELOG_URL` in `cddagl/constants.py:22`). There's an option to reverse the sort.
- **Reference:** `cddagl/ui/views/main.py:3106-3200`.
- **In catapult-cpp:** already listed as "not done" in the README. Reuse `Http`
  (`src/core/http.h`) and the game definitions in `src/core/games.cpp` for the repo per game.

### 6. Switching userdata folders
- **Original:** none, this is not from the Kitten launcher. Use case: one userdata on the Windows
  partition and one on the Linux side, same layout, picked from the launcher.
- **In catapult-cpp:** everything already goes through `Paths::userdata()` (`src/core/paths.h`),
  and the game gets it as `--userdir` (`src/core/game_launcher.cpp:120`). The change is to make
  `userdata()` come from a setting (a list of folders plus the active one, per game) instead of
  always `game_base_dir()/userdata`, add a selector in the UI, and reload every tab after a switch.
- **Depends on:** nothing outside userdata may belong to one userdata. Done: zip backups and
  snapshots live in `<userdata>/catapult/` (`Paths::launcher_dir()`, see `docs/git-backups.md`).
- **Note:** the trash, settings and installs stay shared. Things to check: config files in a Windows
  userdata that hold Windows paths (for example fonts in `config/fonts.json`), a game version older
  than the one that wrote the saves, and an NTFS partition mounted read-only.

## Nice to have
- GitHub rate-limit warning from the `X-RateLimit-Remaining`/`X-RateLimit-Reset` headers
  (`cddagl/ui/views/main.py:2707`). Today `src/core/release_manager.cpp:59` only parses the error message.
- Refresh the builds list every N minutes (`cddagl/ui/views/settings.py:336`).
- Don't copy or move saves on update above a set size (`cddagl/ui/views/settings.py:309-355`).
- Soundpack details: size, home page link, download speed.
- "Download in your browser" guide dialog for packs that can't be downloaded directly
  (`cddagl/ui/views/dialogs.py:46-143`). It goes on top of the existing "Install From File...".
- Check for a newer launcher version, with an opt-out (`cddagl/ui/views/tabbed.py:167-345`).
  No self-update, since Linux packages handle that.
- Crash dialog with the error and a link to report it (`cddagl/ui/views/dialogs.py:304`).
- Help menu with a FAQ and a "Game issue" link.
- Translations (already listed in the README).

## Not porting
- `cddagl/win32.py`: recycle bin, Windows process detection, reading the version from the exe.
- Dark theme `.qss`: catapult-cpp follows the system GTK theme on purpose.
- SQLite settings and alembic: the JSON settings stay compatible with the original Catapult.
- Single-instance named pipe: `Gtk::Application` already does this.
- Already in catapult-cpp: stable/experimental, keep download cache, install soundpack from file,
  keep launcher open, licenses dialog.
