# THE LAUNCHER — how it is built, and how the executable is made

*Written 2026-10-16 for Jonhline's agent, so nobody has to reverse-engineer our front door. Every claim
carries the file it comes from. Read this with `docs/vulcan4/LAUNCHER-V2.md` (the design rules) and
`docs/vulcan4/MOD-MANIFEST.md` (the mod format) beside it.*

---

## 1. ONE BINARY, TWO DOORS

There is no separate launcher program. `moonengine` is the **same executable** as the game; which door it
opens is decided by the arguments (`src/app/main.c:1837`):

| how it is started | what opens |
|---|---|
| **no arguments** — `moonengine.exe` (what `launcher.cmd` and the Start Menu shortcut run) | **the studio**: our own room with a car and the dock (MODS / OPTIONS / START / EXIT) |
| `<disc.iso>` | the game, straight away (intro → menus) |
| `--launcher --launcher-v2` | the **older panel door**: a window with DISC / MODS / OPTIONS / START pages (`src/app/launcher_ui.c`) |
| `--studio <car>` | the studio with a chosen car code, no dock hand-over (`main.c:1934`) |

`prepared == 2` means "no disc was given", and that is what sets `front_end` (`main.c:1837`); `main.c:2004`
and `:2041` then run the studio instead of the game. **The captain's launcher is the studio** — that is the
one to keep working; the panel door is the older surface and only it carries the mod manager UI today.

## 2. THE FILES

| file | what it owns |
|---|---|
| `src/app/main.c` (3,290 lines) | the whole studio: room draw, dock, rig, wizard, hand-over, and the game |
| `src/app/launcher.c` (49) | `launcher_bootstrap`: argument handling, the preferences path, handing `argv` to the engine |
| `src/app/launcher.h` (45) | `launcher_startup` / `launcher_preferences`: disc, saves, car, colour, video file, `page`, `shot`, `install_zips` |
| `src/app/launcher_ui.c` (923) | the panel door: pages, the mod list/card, zip install, `mods.ini`, the hand-over |
| `src/app/launcher_ui.h` (11) | `launcher_ui_run(startup)` |
| `src/app/launcher_config.c` (217) | `launcher.ini`: read/write of the remembered disc, saves dir, car, colour |
| `src/app/launcher_icons.c` (70) | the dock's icon pictures (read from a folder of the user's own choosing) |
| `src/app/launcher_photos.c` (101) | the launcher's photo wall (menu photos, PNG) |
| `src/app/launcher_text.c` (154) | the launcher's own text face (reads a face out of the disc, falls back to a built-in) |
| `src/app/launcher_gallery.c` (42) | the car/photo gallery capture path |
| `src/platform/sdl/showroom.c` | the studio scene: the white room, the mirror floor, lights, the car, the rig |
| `src/core/mod_manifest.c` | which mods are on (`moon_mod_roots`), the manifest fields, `<prefs>/mods` |
| `src/core/mod_merge.c`, `src/core/vol.c` | the mod source: files shadow, lists merge |

## 3. THE STUDIO (the captain's launcher), STEP BY STEP

1. **Start** — no disc: `main.c:2004` prints the front-end banner and builds a `showroom` in **studio mode**
   (`showroom_open_studio`, `main.c:2037`). The scene is the disc's own studio room (`/menu/select/bg.mdl`,
   `light.mot`, `camera.mot`) drawn as a seamless white room with a mirrored floor
   (`docs/vulcan4/STUDIO-ROOM.md`).
2. **The car** — the room's car comes from the **rig file**, else `--studio <car>`, else
   `startup.preferences.car` (the `car=` line of `launcher.ini`), else the built-in default
   `FRONT_CAR` (`main.c:1469`, `:2041`).
3. **The rig** — `%APPDATA%\MoonEngine\MoonEngine\studio-rig.txt` (Linux: `~/.local/share/MoonEngine/
   MoonEngine/`): camera, target, car offset, car angle, `car_code`, `camera_lock`. Written by the studio
   whenever the captain moves something (`main.c:1635`), read at start (`main.c:1690-1697`). The camera can be
   locked with **L** (`main.c:1726`): locked = the dock is interactive, free = drag/WASD/QE/RF move the camera.
4. **The dock** — four slots `MODS, OPTIONS, START, EXIT` (`main.c:1257`), icons read from the disc
   (`main.c:1676`, "loaded from the disc"/"MISSING"), names drawn in a face taken from the disc
   (`dock_font_load`, `main.c:1338`). Left/Right move the pointer, hover follows the mouse
   (`dock_hover`, `main.c:1394`). **ENTER on MODS/OPTIONS/EXIT currently prints
   `studio: <name> pressed (nothing behind it yet)`** (`main.c:1730`) — the pages behind them are the next
   build; the design they must follow is section 7.
5. **START** — `start_fade = 1` (`main.c:1729`): the launcher fades the whole room to black, then the game's
   opening rises out of that black (one fade duration and one easing for every hand-over, `main.c:1654`).
   The disc image used for the hand-over is `startup.preferences.disc`, validated in the launcher
   (`launcher_validate_disc`, `launcher_config.c`).
6. **First run** — with no `launcher.ini` the **setup wizard** runs inside the window (`first_run_wizard`,
   `main.c:1532`): pick the disc, pick the graphics API; it writes the preferences and then the room opens.
7. **The room never talks to the game's screens**: when the game takes over, the showroom's rig is cleared
   (`showroom_clear_rig`, `showroom.c:107`) — **and today that clearing is incomplete** (it clears the car's
   offset, not the camera), which is why GT Mode's buy-car screen was drawn with the launcher's camera. The
   rule the captain gave: **the game's screens use their own cameras; the launcher's rig belongs to the
   launcher.**

## 4. THE PANEL DOOR (`--launcher --launcher-v2`)

`launcher_bootstrap` (`launcher.c:18-45`) parses `--launcher-v2`, the optional `--launcher-config`, and a
disc; it fills `launcher_startup` and, on START, rewrites `argv` to
`<exe> <disc> --save-dir <saves> --config-file <video.ini>` and returns 1 — that is the entire hand-over.

`launcher_ui_run` (`launcher_ui.c`) opens one SDL window and draws:

- a **background picture** if `background.png` (or a PNG in `backgrounds/`) sits beside the executable
  (`load_background`), else a gradient;
- the **dock** (DISC/MODS/OPTIONS/START + EXIT) and one open page as a **panel over the dock**;
- **DISC** — browse folders for `.iso` files, Enter picks and validates one;
- **MODS** — the manager: the left half lists installed mods with `[x]`/`[ ]`, the right half draws the
  selected mod's **card** (name, version, author, price, description, its `image` picture, what it adds, its
  folder). Keys: **SPACE** toggle, **D** open the folder, **DEL** delete (Yes/No first). A row that is a
  `.zip` is an **archive**: ENTER/SPACE unpacks it beside itself (`install_zip`) and the folder becomes the
  mod. Sources scanned: `<prefs>/mods` first, then a `mods` folder beside the exe (portable installs);
- **OPTIONS** — a named limitation (video/audio/pad still live in the engine's own screen);
- **START** — `hand_over()`: builds the `;`-joined list of ticked folders, exports it as
  **`OPENGT4_OVERLAY`**, writes **`<prefs>/mods.ini`** (the tick list the engine reads when it is started
  without the launcher), validates the disc, and returns 1 to `main` with the rewritten `argv`.

Dev flags for this door: **`--launcher-page <disc|mods|options|start>`** opens a page immediately and
**`--launcher-shot <file.png>`** writes the frame (SDL_RenderReadPixels + miniz's PNG encoder) and quits —
that is how the launcher is photographed on a machine nobody can sit at. `OPENGT4_LAUNCHER_PAGE=<n>` does the
same by index.

## 5. WHERE THE LAUNCHER'S STATE LIVES

`plat_pref_dir()` (`src/platform/common/file_stdio.c`) is the single answer, and it matches what the app's
own `SDL_GetPrefPath("MoonEngine","MoonEngine")` calls return, so nothing moved when it was added:

| path | what |
|---|---|
| `%APPDATA%\MoonEngine\MoonEngine\` (Linux `~/.local/share/MoonEngine/MoonEngine/`) | the user's folder: everything below |
| `launcher.ini` | remembered disc, saves dir, car code, colour |
| `video.ini` | the engine's video settings |
| `saves/` | the save slots (`OGT4SAVE`, 8 × 4 MiB) |
| `mods/` | installed mods, one folder each; a `.zip` here is installable |
| `mods.ini` | the tick list (one folder name per line) the engine reads when started alone |
| `studio-rig.txt` | the studio camera/car (written by the studio, read at start) |

**Environment variables** the launcher/studio honour (all optional, all off by default):
`OPENGT4_OVERLAY` (the ticked mods — set by the launcher, else read from `mods.ini`),
`OPENGT4_OVERLAY_TRACE` (log every overlay hit and the resolved mod list),
`OPENGT4_LAUNCHER_PAGE`, and the studio dev hooks `MOONSTUDIO_ISO`, `MOONSTUDIO_START_LAUNCH`,
`MOONSTUDIO_LAUNCH_SHOT`, `MOONSTUDIO_START_SHOT`, `MOONSTUDIO_SETUP_SHOT`, `MOONSTUDIO_FRONT_SHOT`,
`MOONSTUDIO_LAUNCH_DELAY`, `MOONSTUDIO_DOCK_TRACE`.

## 6. HOW THE EXECUTABLE IS MADE

One CMake project (`CMakeLists.txt`), one game target, no runtime dependency on SDL. The names below are
Jonhline's current ones (he renamed the build to `moon_*` and the binary to `MoonEngine`):

| piece | how |
|---|---|
| `miniz` | `FetchContent`; inflate (the disc's deflate streams, PNG upload) and the PNG encoder behind `--shot` |
| `SDL2` | `FetchContent`; **linked statically** — `SDL2::SDL2-static` + `SDL2::SDL2main` |
| libraries | `moon_core` (game logic, formats, **our mods layer**), `moon_platform_common` (files, pref dir, renderer backend), `moon_platform_sdl` (window, GL, input, audio), `moon_physics`, `moon_race_sound`, `moon_career_vehicle`, `moon_ee`; on Windows also `moon_vk_render` + `moon_vulkan` (his Vulkan path) |
| source groups | `MOON_LAUNCHER_SOURCES` (the front door: `launcher.c`, `launcher_ui.c`, `launcher_text.c`, `launcher_config.c`, `launcher_photos.c`, `launcher_icons.c`, `launcher_gallery.c`) and `MOON_GRAPHICS_SOURCES` — the Switch build swaps both |
| the game target | `add_executable(moonengine …)` with **`OUTPUT_NAME MoonEngine`** (`CMakeLists.txt:275-276`) → `MoonEngine` on Linux, **`MoonEngine.exe` on Windows** |
| release shape | `MOON_WINDOWS_GUI=ON` (default) builds it as a **window program, no console** (`:255`, `:285`) |
| Windows cross-build | mingw-w64 toolchain `/mnt/ssd/mingw-sdl2/toolchain-mingw64.cmake`; SDL2 comes from the official mingw dev bundle, linked statically, so **the exe imports only Windows system DLLs** — no `SDL2.dll`, no mingw runtime. **One file.** |
| packaging | `tools/package_release.py` and the Inno Setup script `tools/installer/moonengine.iss` |

Build (Linux, from the repo root):

```bash
cmake -S . -B /mnt/ssd/moon-build -DCMAKE_BUILD_TYPE=Release
cmake --build /mnt/ssd/moon-build -j4 --target moonengine     # → MoonEngine
```

Windows cross-build (what ships to a Windows machine):

```bash
cmake -S . -B /mnt/ssd/moon-win -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=/mnt/ssd/mingw-sdl2/toolchain-mingw64.cmake
cmake --build /mnt/ssd/moon-win -j4 --target moonengine        # → MoonEngine.exe
```

House rules that bite: `-j4` with `nice/ionice` (the box runs a live Minecraft server), `TMPDIR` on the SSD,
never build inside the source tree (a build tree is ~176 MB), and on Windows build **only the game target** —
the `launcher_tests` target wants `SDL_main` and fails to link there.

## 7. RULES FOR ANYTHING ADDED TO THE FRONT DOOR (the captain's own, 2026-10-16)

1. A page is a **transparent menu INSIDE the launcher** — the room and the car stay visible behind it. Never
   an opaque full screen, never a second window.
2. A choice **may move the camera**, and **may change colours**. Camera placement is part of the menu.
3. The **launcher owns its rig**; a game screen must never inherit the launcher's camera, and the launcher must
   never be changed to suit a game screen.
4. Everything added is **off by default** (LAW 12): with no mods ticked and no overlay set, the game is
   byte-for-byte the disc's.
5. The dock's icons and the dock's font come **from the user's disc** — do not bundle game art.

## 8. SEEING IT WITHOUT A MOUSE (how to verify any launcher change)

```bash
Xvfb :99 -screen 0 1280x960x24 &                     # keep alive
DISPLAY=:99 stdbuf -oL -eL /mnt/ssd/vulcan4-build/moonengine --launcher --launcher-v2 \
    --launcher-page mods --launcher-shot /tmp/mods.png     # writes the frame and quits
```

- **`stdbuf -oL`** matters: a backgrounded run buffers stdout, so an empty log is not evidence of failure.
- The **studio animates** (mirror, car, cursor), so two captures are *not* byte-identical — compare a static
  region, or gate on the log lines and a camera print, never on a whole-frame md5. The game's own save screen
  *is* stable (`68d8eb83…`), which is why the mod gates used it.
- Keys do not arrive through `xdotool --window` (SDL ignores synthetic X events); use the page/shot flags
  above instead of typing at a window over ssh.

## 9. KNOWN GOTCHAS

- `showroom_clear_rig` now clears the camera too: a game screen can never inherit the launcher's view (this
  was the GT Mode shop drawn off-centre — see `docs/vulcan4/NEXT-SESSION.md`).
- The panel door and the studio are **two different front ends**; a screen added to one does not appear in the
  other. The captain uses the studio; the manager lives in the panel door for now.
- `%APPDATA%\MoonEngine\MoonEngine\` is the only place the launcher writes, so the program folder can stay
  read-only (`docs/vulcan4/MOD-MANIFEST.md`, "where mods live").
- Missing dock icons print `MISSING` rather than failing: the room is still usable and says so.

## 10. PUTTING IT ON A WINDOWS MACHINE (how the Baki build was done)

This is the whole ship: one file, one folder, no installer needed for a test.

1. **Cross-build on Linux** with the mingw toolchain (section 6) → `MoonEngine.exe`. Check the one-file rule:

   ```bash
   x86_64-w64-mingw32-objdump -p MoonEngine.exe | grep 'DLL Name'
   # every line must be a Windows system dll (KERNEL32, USER32, GDI32, OPENGL32, SHELL32, ole32, winhttp…)
   # If SDL2.dll or libgcc/libstdc++-6.dll appears, the static link did not happen.
   ```

2. **Copy it over** — `scp`, not `rsync` (Windows' OpenSSH has no rsync):

   ```bash
   scp -o ConnectTimeout=10 MoonEngine.exe "baki:C:/MoonEngine/MoonEngine.exe"
   ```

3. **The disc is the user's own file** and is never shipped with the build: copy their ISO (or point the
   launcher at an existing one). On Baki it lives at
   `C:\.Emulation\ps2\roms\Gran Turismo 4 (USA) (v2.00).iso`.

4. **Run it.** Double-clicking `MoonEngine.exe` with no arguments opens **the studio**; the first-run wizard
   asks where the disc is and remembers it in `launcher.ini`. With a disc path as the argument it goes straight
   into the game.

5. **Its user folder** (created on first run): `%APPDATA%\MoonEngine\MoonEngine\` — `saves/`, `video.ini`,
   `launcher.ini`, `mods/`, `mods.ini`, `studio-rig.txt`. Deleting that folder resets everything.

6. **Driving it over ssh (headless verification on that machine)** — the dev flags work there too:

   ```powershell
   .\MoonEngine.exe "<disc>" --studio ford_gt_05 --shot shot.png --no-config --shot-delay 8
   .\MoonEngine.exe --launcher --launcher-v2 --launcher-page mods --launcher-shot mods.png
   ```

   The `MOONSTUDIO_*` environment variables script the studio (auto-START, capture the wizard, capture the
   fade) when no keyboard can reach the window.

**Windows gotchas worth remembering:** a process started inside an ssh session dies when the session ends (use
a scheduled task if it must outlive you); quote the Windows user name — it contains a space
(`"Or Golan@baki"`); and the exe is unsigned, so SmartScreen will ask once.
