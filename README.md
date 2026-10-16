<div align="center">

# Moon Engine

**A native PC port of Gran Turismo 4. Bring your own disc.**

Previously named OpenGT4. The executable is now `MoonEngine.exe`; on first
start, player data from the old OpenGT4 folder is copied to the new one.
Tourist Trophy support is planned on the same base; it is not playable yet.

![Platform](https://img.shields.io/badge/platform-Windows-0078D6)
![Disc](https://img.shields.io/badge/disc-NTSC--U%20SCUS--97328-555)
![License](https://img.shields.io/badge/code-GPLv3-blue)

[Play](#play) · [Known issues](#known-issues) · [Roadmap](#roadmap) · [Controls](#controls) · [Building](#building) · [Credits](#credits)

</div>

> [!IMPORTANT]
> This repository contains **no Gran Turismo 4 data**: no disc image, executables, BIOS, music, models, textures or save files. You need your own copy of the game and your own dump of the disc. Issues asking for game files will be closed.

> [!NOTE]
> Moon Engine is an unofficial fan project, not affiliated with or endorsed by Sony Interactive Entertainment or Polyphony Digital. Gran Turismo is a trademark of Sony Interactive Entertainment; all trademarks belong to their owners. The project is developed with heavy AI assistance.

---

## Play

1. Download the latest `MoonEngine-<version>-setup.exe` from the Releases page and run it, or take `MoonEngine-<version>-win64.zip` and unzip it anywhere.
2. Run Moon Engine. Choose **Game disc** and select your own Gran Turismo 4 disc image (NTSC-U, SCUS-97328).
3. Adjust Video, Audio and Controls if you like, then choose **Play**.

Your disc image stays where it is. To update, run the new installer over the old one; your saves and settings are kept.

Versions 1.01 and 2.00 of the disc have been tested and run the same. To check a v2.00 dump: `CORE.GT4` on the disc has the SHA-1 `30031137ce494323816bb707b90d3ec0d07937d2`.

---

## Known issues

Moon Engine is in alpha. Before reporting a problem, check this list and the open [Issues](../../issues).

- LAN play is not available (the menu shows "Coming soon").
- Some sounds and visual details still differ from the PS2.
- The attract-mode demo may not play correctly.

When reporting a bug, say what you were doing, which disc version you have, and attach a screenshot if you can.

---

## What it is

MoonEngine is a native PC port of Gran Turismo 4, built through reimplementation in C. No emulation or static recompilation, with a strong focus on preserving the original experience. It reads the game's data from your own disc image.

> [!WARNING]
> Moon Engine is in **alpha**. Most of the game is playable, but some modes and features are still missing or incomplete, and some sounds and visuals still differ from the PS2. Expect bugs.

The plan is to reproduce the game as it was on the PS2 first, then improve it: modern rendering, new options and mods, on by default, with the original look and behaviour always one option away (see [Roadmap](#roadmap)).

## Roadmap

**1. The original** (now). Gran Turismo 4 as it was on the PS2: the same behaviour, look and controls, tested against the original. Widescreen uses the game's own 16:9 mode. Optional filters can recreate the PS2 image (640x448, dithering, interlacing).

**2. Improvements** (next). The same game, as if the studio had kept it alive on PC. Each improvement has its own option and is on by default, and an **Original** preset turns everything back to the PS2 game:

- a modern renderer: real shadows and reflections, new lighting, softer car shadows, higher resolutions and ultrawide screens;
- native keyboard and mouse, and new interface items in the game's own style;
- **mods**, in an official package format (name, version, author, dependencies), able to replace game files and add new cars, tracks and content; existing PS2 mods can have their cars, tracks and sounds imported from your own files;
- improved versions of some systems, such as the rival AI, offered next to the original, which stays available.

Car physics and race rules always stay the original ones, so lap times, ghosts, replays and saves stay valid whatever the options. Mods are optional; without them the game uses only your disc.

**3. Tourist Trophy** after that, built on the same base.

## Controls

A controller is recommended; any controller SDL recognises works, with the PlayStation layout. Buttons can be changed in the launcher or in the game's own Options.

| | Controller | Keyboard |
|---|---|---|
| Steer | left stick | arrows |
| Accelerate / brake | Cross / Square | up / down |
| Shift up / down | R2 / L2 | A / Z |
| Handbrake | Circle | Space |
| Reverse | Triangle | |
| Look back / change view | L1 / Select | V |
| Pause | Start | Esc |

In menus: the left stick or D-pad and Cross / Circle, or the arrows, Enter and Esc. F10 quits.

---

## Building

Windows, Visual Studio 2022 Build Tools and CMake. SDL2 and miniz are fetched by CMake; FFmpeg is used for the game's movies.

```
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release --target moonengine
ctest --test-dir build -C Release
```

Unit tests use synthetic data only; the disc-based tests need your own disc image. `tools/package_release.py` builds the release package.

Run without the launcher: `moonengine <your disc image>`. `--save-dir <folder>` keeps profiles, options and replays in another folder; `--fullscreen` starts in full screen.

## Layout

| Path | Purpose |
|------|---------|
| `include/platform/` | platform interfaces (video, audio, input, files, time) |
| `src/core/` | platform-independent game code: disc, UI, script interpreter, menus, career, saves, replays |
| `src/core/physics/` | car physics, AI and race session |
| `src/platform/` | SDL2 and OpenGL backends |
| `src/app/` | the `moonengine` program and its secondary launcher ([details](docs/LAUNCHER.md)) |
| `tests/` | unit tests and disc-based tests |
| `tools/`, `splat/` | analysis tools; the recompiler there only builds test oracles that compare the port with the original code, and is never part of the game |
| `docs/` | file formats and notes |

Porting to another system means implementing the interfaces in `include/platform/`.

## Contributing

Pull requests are not being accepted at the moment. Bug reports and suggestions are welcome in [Issues](../../issues).

Never post game assets, disc dumps or copyrighted binaries.

## Credits

- The Gran Turismo modding community, whose research made the game's file formats and scripts understandable; in particular Nenkai's [PDTools](https://github.com/Nenkai/PDTools) (MIT), against which the model and course formats were cross-checked.
- [SDL2](https://www.libsdl.org/) (zlib) and [miniz](https://github.com/richgel999/miniz) (MIT), fetched by CMake.
- AMD FidelityFX Super Resolution 1 headers in `third_party/fsr1` (MIT).
- [stb_image](https://github.com/nothings/stb) in `third_party/stb` (MIT).
- `third_party/ps2float` from PCSX2 (LGPL-3.0-or-later), PS2 floating-point arithmetic, with its licence files.
- The launcher's icons are from the Tango Icon Library (public domain).

## License

MoonEngine is free software under the **GNU General Public License, version 3** ([LICENSE](LICENSE)), with additional terms under its section 7 ([NOTICE](NOTICE)):

- **Attribution**: any work that contains or is derived from Moon Engine must say "Based on Moon Engine" in its documentation and, if it has a user interface, somewhere users can see it (an About, credits or launcher screen).
- **Modified versions** must be marked as different from the original and must not be presented as the official MoonEngine.

As the GPL requires, anyone who distributes Moon Engine or a work based on it must also make its source code available under the same terms. The license covers the code in this repository only, not the game.
