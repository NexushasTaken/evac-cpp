# Evac

A C++17 / GTK4 launcher and content manager for
[Cataclysm: Dark Days Ahead](https://github.com/CleverRaven/Cataclysm-DDA) and
[Cataclysm: Bright Nights](https://github.com/cataclysmbn/Cataclysm-BN) (and the forks Era of Decay,
There Is Still Hope and The Last Generation), based on [Catapult](https://github.com/qrrk/Catapult).

> **Work in progress, Linux only for now.** Not affiliated with or endorsed by the original Catapult or any of
> the games.

> **Written with AI.** The code in this project was written by Claude, an AI model by Anthropic. See
> [How this was made](#how-this-was-made).

## History

Evac started as *catapult-cpp*, a (nearly) one-to-one C++/GTK4 rewrite of
[Catapult](https://github.com/qrrk/Catapult) by qrrk. It has since been reworked (XDG folders, an in-app trash,
git snapshots of worlds, several userdata folders per game) and renamed to Evac to set it apart from the
original. The original design is still the foundation, and the credit for it goes to qrrk and Catapult's
contributors.

## How this was made

Evac's code was written by Claude (Anthropic) through Claude Code. I (NexushasTaken) picked the features, made
the design decisions and tested the app by hand; the core library also has unit tests, written along with the
code. I did not review every line of the code myself.

The features and file formats come from qrrk's Catapult (see [History](#history)). As with any young project,
back up your saves before trusting it with them, and please report anything odd.

## Credits

This project started as a (nearly) one-to-one port of **Catapult by qrrk** from Godot/GDScript to C++. The
features, the settings file format, the directory layout and the lists of stable releases and soundpacks all come
from the original. Evac is its own app now and uses its own file names (see Differences), so it does not share a
folder with Catapult.
All credit for the design goes to the original author and its contributors. The code here was written fresh in
C++, following the original's behavior.

The original project is MIT licensed, see [License](#license).

## Features

| Tab         | What it does                                                                        |
|-------------|-------------------------------------------------------------------------------------|
| Game        | Install, update, switch between and launch stable and experimental builds; switch between user data folders |
| Mods        | Install and delete mods, get the Kenan modpack into a local repository              |
| Soundpacks  | Install, reinstall and delete soundpacks, install from a downloaded file            |
| Fonts       | Set bundled or your own fonts, edit the fallback lists, sizes and rendering options |
| Backups     | Back up, restore and delete save game backups                                       |
| Settings    | Launcher options, proxy, caching                                                    |

Not done yet:

- The changelog window and the font size help
- Translations (only English) and the language setting
- Desktop integration (`.desktop` file, icon) and packaging
- Windows and macOS

Differences from the original:

- The launcher always uses your system GTK theme and display scaling. There are no launcher themes and no scale
  setting; the old `launcher_theme` and `ui_scale_*` keys in the settings file are kept but ignored.
- It is not a single self-contained executable. The `data/` folder (bundled fonts, licenses) has to be next to it.
- Files go to the XDG directories by default instead of next to the executable (see Running).
- Its own files have their own names: `evac_settings.json` (same keys as Catapult's `catapult_settings.json`),
  `evac_install_info.json` in each game install, and an `evac/` folder in each userdata folder.
- Restoring a backup unpacks it to a staging folder first and asks for confirmation, so a broken backup can't
  destroy your current saves.

## Building

You need:

- a C++17 compiler (GCC or Clang), CMake 3.23 or newer, `pkg-config`
- [Conan](https://conan.io) 2
- the gtkmm 4 and fontconfig development files (fontconfig comes with GTK on most systems), for example `gtkmm-4.0` on Arch, `libgtkmm-4.0-dev` on Debian/Ubuntu or
  `gtkmm4.0-devel` on Fedora. (gtkmm is not on Conan Center, so it comes from your system.)

The other libraries (cpr and libcurl, libarchive, nlohmann_json, spdlog, Catch2) are fetched and built by Conan.

```sh
# Once per machine, if you have never used Conan:
conan profile detect

# Get and build the dependencies. The first time this builds OpenSSL and friends from source
# and takes a while (Conan caches the result).
conan install . --build=missing -s build_type=Debug -s compiler.cppstd=17

# Build.
cmake --preset conan-debug
cmake --build --preset conan-debug
```

For an optimized build use `-s build_type=Release` in the `conan install` line and the `conan-release` presets.
The binary ends up in `build/Debug/` (or `build/Release/`), and the build copies `data/` next to it.

Configuring also writes `compile_commands.json` into the build folder. The `.clangd` file in the project root
points clangd at `build/Debug`; change it if you work in the Release build.

## Running

```sh
build/Debug/evac
```

The launcher follows the XDG base directory spec:

| What                                                        | Where                                      |
|-------------------------------------------------------------|--------------------------------------------|
| `evac_settings.json`                                        | `$XDG_CONFIG_HOME/evac` (`~/.config/evac`) |
| Games: a folder per game (`dda/`, `bn/`, ...) with the installs, user data, mod repository and save backups | `$XDG_DATA_HOME/evac` (`~/.local/share/evac`) |
| Downloaded archives                                         | `$XDG_CACHE_HOME/evac` (`~/.cache/evac`)   |

**Portable mode:** if an `evac_settings.json` sits next to the executable, the launcher keeps everything in
that folder instead, like the original (`cache/` and the game folders next to it). To go back to the XDG
folders, move or delete that file. The `data/` folder (bundled fonts, licenses) always has to be next to the
executable for now.

If you used an earlier build that kept its files next to the executable, move them by hand, for example:

```sh
mkdir -p ~/.config/evac ~/.local/share/evac ~/.cache/evac
cd build/Debug
mv catapult_settings.json ~/.config/evac/evac_settings.json
for f in */game*/catapult_install_info.json; do mv "$f" "${f%/*}/evac_install_info.json"; done
mv dda bn eod tish tlg ~/.local/share/evac/   # only the ones that exist
mv cache/* ~/.cache/evac/                     # optional, they can be downloaded again
```

If you used a build from before the rename to Evac (catapult-cpp), the launcher's own folders were called
`catapult` instead of `evac`: `~/.config/catapult`, `~/.local/share/catapult`, `~/.cache/catapult`, and a
`catapult/` folder (backups, snapshots) inside each userdata folder. Rename them to `evac`, change any paths in
the settings file that point into the old folders, and in each snapshot repository that was marked as kept, run
`git config evac.keep true`. The files were renamed too: `catapult_settings.json` is now `evac_settings.json`,
and each game install's `catapult_install_info.json` is now `evac_install_info.json`. Without that file an
install is not recognized.

## Tests

```sh
ctest --preset conan-debug
# or, for the details:
build/Debug/core_tests
```

The tests cover the core library (everything except the GTK interface) and need no network access: downloads
are tested with `file://` URLs and archives built on the fly. Two extra tests are skipped unless you point them
at a server:

```sh
EVAC_TEST_HTTPS_URL=https://api.github.com/zen build/Debug/core_tests "[network]"
```

## Layout

```
src/core/   Everything that does not need a window (settings, paths, downloads, installs, mods, ...).
src/ui/     The gtkmm interface: one file pair per tab, and a worker thread helper.
tests/      Catch2 tests for src/core.
data/       Files that ship with the launcher: bundled fonts and license texts.
```

Slow operations (downloads, installs, backups) run on a worker thread, one at a time, and the tab that started
them is disabled until they finish.

## License

The code in this repository is released under the MIT license, see [`LICENSE`](LICENSE). To contribute, see
[`CONTRIBUTING.md`](CONTRIBUTING.md).

This program started as a port of Catapult, which is released under the MIT license, Copyright (c) 2021-present
qrrk. The original license text is kept in [`data/licenses/Catapult.txt`](data/licenses/Catapult.txt) and is
shown in the launcher under Settings, "View opensource licenses". The MIT license requires that notice to stay
with copies of the work, so please keep it.

The bundled fonts (Fantasque Sans Mono, Hack, Inconsolata LGC, PT Mono, Ubuntu Mono) come with their own
licenses, found in [`data/fonts/licenses/`](data/fonts/licenses/). The libraries this program uses have their
own licenses too; the launcher lists them in the same window.
