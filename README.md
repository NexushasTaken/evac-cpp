# Evac: Cataclysm Launcher

This is the evac-cpp project.

A C++17 / Qt 6 launcher and content manager for
[Cataclysm: Dark Days Ahead](https://github.com/CleverRaven/Cataclysm-DDA) and
[Cataclysm: Bright Nights](https://github.com/cataclysmbn/Cataclysm-BN) (and the forks Era of Decay,
There Is Still Hope and The Last Generation), based on [Catapult](https://github.com/qrrk/Catapult).

> **Work in progress, Linux only for now.** Not affiliated with or endorsed by the original Catapult or any of the games.

> **Written with AI.** The code in this project was written by Claude, an AI model by Anthropic. See [How this was made](#how-this-was-made).

## Features

| Tab         | What it does                                                                        |
|-------------|-------------------------------------------------------------------------------------|
| Game        | Install, update, switch between and launch stable and experimental builds; switch between user data folders |
| Mods        | Install and delete mods, get the Kenan modpack into a local repository              |
| Soundpacks  | Install, reinstall and delete soundpacks, install from a downloaded file            |
| Fonts       | Set bundled or your own fonts, edit the fallback lists, sizes and rendering options |
| Snapshots   | Git snapshots of each world: take, restore or discard them (needs git, see Running) |
| Backups     | Back up, restore and delete save game backups                                       |
| Settings    | Launcher options, proxy, caching                                                    |

Not done yet:

- The changelog window and the font size help
- Translations (only English) and the language setting
- Desktop integration (`.desktop` file, icon) and packaging
- Windows and macOS

Differences from the original:

- The launcher always uses your system theme (Qt style) and display scaling. There are no launcher themes and no scale
  setting; the old `launcher_theme` and `ui_scale_*` keys in the settings file are kept but ignored.
- It is not a single self-contained executable. The `data/` folder (bundled fonts, licenses) has to be next to it.
- Files go to the XDG directories by default instead of next to the executable (see Running).
- Its own files have their own names: `evac_settings.json` (same keys as Catapult's `catapult_settings.json`),
  `evac_install_info.json` in each game install, and an `evac/` folder in each userdata folder.
- Restoring a backup unpacks it to a staging folder first and asks for confirmation, so a broken backup can't
  destroy your current saves.

# History

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

# Building and installing

See the [BUILDING](BUILDING.md) document.

# Contributing

See the [CONTRIBUTING](CONTRIBUTING.md) document.

# Credits

This project started as a (nearly) one-to-one port of **Catapult by qrrk** from Godot/GDScript to C++. The
features, the settings file format, the directory layout and the lists of stable releases and soundpacks all come
from the original. Evac is its own app now and uses its own file names (see Differences), so it does not share a
folder with Catapult.
All credit for the design goes to the original author and its contributors. The code here was written fresh in
C++, following the original's behavior.

The original project is MIT licensed, see [License](#license).

# Licensing

The code in this repository is released under the MIT license, see [`LICENSE`](LICENSE).

This program started as a port of Catapult, which is released under the MIT license, Copyright (c) 2021-present
qrrk. The original license text is kept in [`data/licenses/Catapult.txt`](data/licenses/Catapult.txt) and is
shown in the launcher under Settings, "View opensource licenses". The MIT license requires that notice to stay
with copies of the work, so please keep it.

The bundled fonts (Fantasque Sans Mono, Hack, Inconsolata LGC, PT Mono, Ubuntu Mono) come with their own
licenses, found in [`data/fonts/licenses/`](data/fonts/licenses/). The libraries this program uses have their
own licenses too; the launcher lists them in the same window.
