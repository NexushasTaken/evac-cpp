# catapult-cpp

A C++17 / GTK4 rewrite of [Catapult](https://github.com/qrrk/Catapult), the launcher and content manager for
[Cataclysm: Dark Days Ahead](https://github.com/CleverRaven/Cataclysm-DDA) and
[Cataclysm: Bright Nights](https://github.com/cataclysmbn/Cataclysm-BN) (and the forks Era of Decay,
There Is Still Hope and The Last Generation).

> **Work in progress, Linux only for now.** Not affiliated with or endorsed by the original Catapult or any of
> the games.

## Credits

This project is a (nearly) one-to-one port of **Catapult by qrrk** from Godot/GDScript to C++. The features, the
settings file format, the directory layout and the lists of stable releases and soundpacks all come from the
original, so the two should be able to share a folder of games (this has not been tested yet).
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
- Restoring a backup unpacks it to a staging folder first and asks for confirmation, so a broken backup can't
  destroy your current saves.

## Building

You need:

- a C++17 compiler (GCC or Clang), CMake 3.23 or newer, `pkg-config`
- [Conan](https://conan.io) 2
- the gtkmm 4 and fontconfig development files (fontconfig comes with GTK on most systems), for example `gtkmm-4.0` on Arch, `libgtkmm-4.0-dev` on Debian/Ubuntu or
  `gtkmm4.0-devel` on Fedora. (gtkmm is not on Conan Center, so it comes from your system.)

The other libraries (libcurl, libarchive, nlohmann_json, spdlog, Catch2) are fetched and built by Conan.

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
build/Debug/catapult
```

The launcher follows the XDG base directory spec:

| What                                                        | Where                                              |
|-------------------------------------------------------------|----------------------------------------------------|
| `catapult_settings.json`                                    | `$XDG_CONFIG_HOME/catapult` (`~/.config/catapult`) |
| Games: a folder per game (`dda/`, `bn/`, ...) with the installs, user data, mod repository and save backups | `$XDG_DATA_HOME/catapult` (`~/.local/share/catapult`) |
| Downloaded archives                                         | `$XDG_CACHE_HOME/catapult` (`~/.cache/catapult`)   |

**Portable mode:** if a `catapult_settings.json` sits next to the executable, the launcher keeps everything in
that folder instead, like the original (`cache/` and the game folders next to it). To go back to the XDG
folders, move or delete that file. The `data/` folder (bundled fonts, licenses) always has to be next to the
executable for now.

If you used an earlier build that kept its files next to the executable, move them by hand, for example:

```sh
mkdir -p ~/.config/catapult ~/.local/share/catapult ~/.cache/catapult
cd build/Debug
mv catapult_settings.json ~/.config/catapult/
mv dda bn eod tish tlg ~/.local/share/catapult/   # only the ones that exist
mv cache/* ~/.cache/catapult/                     # optional, they can be downloaded again
```

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
CATAPULT_TEST_HTTPS_URL=https://api.github.com/zen build/Debug/core_tests "[network]"
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

This program is a port of Catapult, which is released under the MIT license, Copyright (c) 2021-present qrrk. The
original license text is kept in [`data/licenses/Catapult.txt`](data/licenses/Catapult.txt) and is shown in the
launcher under Settings, "View opensource licenses". The MIT license requires that notice to stay with copies of
the work, so please keep it.

The bundled fonts (Fantasque Sans Mono, Hack, Inconsolata LGC, PT Mono, Ubuntu Mono) come with their own
licenses, found in [`data/fonts/licenses/`](data/fonts/licenses/). The libraries this program uses have their
own licenses too; the launcher lists them in the same window.
