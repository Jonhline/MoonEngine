# RENAME — OpenGT4 → VULCAN 4 → MOON ENGINE (the surface, measured 2026-10-15, renamed again 2026-10-16)

**STATUS 2026-10-16 — THE JOINT PROJECT IS NOW "MOON ENGINE"** (the captain with Jonhline: *"Me and the creator
of opengt4 has decided to also rename our entire project. We called it moon engine."*). Layers 1, 2 and 3 below
are **applied** in this tree with the literals `MoonEngine` / `MOON ENGINE` / `moonengine`: `CMakeLists.txt:239`
(`OUTPUT_NAME moonengine`), the three `SDL_GetPrefPath` call sites, the window titles in `launcher.c` /
`launcher_ui.c` / `video_gl.c`, the launcher strings, and the log lines. Old user data is carried across by
`adopt_saves_from_old_names()` in `src/app/main.c`, which runs before anything reads a pref path and moves
`…/Vulcan4/Vulcan4/saves` or `…/OpenGT4/OpenGT4/saves` into `…/MoonEngine/MoonEngine/saves` once — `rename()`
refuses an existing target, so new saves always win over old ones (verified: the real run on 2026-10-16 printed
`moonengine: adopted saves from /home/or/.local/share/OpenGT4/OpenGT4/saves`).

What deliberately keeps its old name: the folder `docs/vulcan4/`, the internal CMake targets (`opengt4_core`,
`opengt4_platform_*`) and the `OPENGT4_*` / `VULCAN_*` environment variables (layer 4), the GPL fence (layer 5).
Still open: **`branding/` is a whole VULCAN identity** (wordmark, letterforms, marks — `branding/tools/`) and
needs a Moon Engine identity from the Creative department, and the prose sweep of `docs/` (about 100 mentions).

The captain: *"change the name on the saving stuff. everything related to opengt4. rename to vulcan4"*.
Measured in the tree with grep, not estimated. **Caine writes this map; the captain writes the code.**

## LAYER 1 — THE USER-DATA ROOT (exactly what was asked, 3 lines)

Every user-data path comes from three `SDL_GetPrefPath` calls. Change the two strings in each:

| file | line | what it roots |
|---|---|---|
| `src/app/main.c` | 1248 | **saves**: `%s`/saves → `slotNN.ogt4`, `active.ogt4` |
| `src/app/video_settings.c` | 27 | video settings |
| `src/app/launcher.c` | 725 | launcher preferences (`disc=`, `saves=`, `show=`) |

`SDL_GetPrefPath("MoonEngine","MoonEngine")` gives exactly `%APPDATA%\MoonEngine\MoonEngine\saves\` on
Windows (`~/.local/share/MoonEngine/MoonEngine/saves/` on Linux) — the name has moved twice, so the oldest
folders (`OpenGT4`, `Vulcan4`) are adopted on first run, see the status block at the top.

**The one consequence to decide:** saves and preferences now living under `...\OpenGT4\OpenGT4\` become
invisible. Cheap correct answer: on first run, if the new folder does not exist but the old one does, move
(or copy) it once and log a line saying so. Otherwise the captain's existing saves look "lost".

## LAYER 2 — WHAT THE USER READS (small, and worth it)

- The **log prefix**: every run prints `OpenGT4 - boot / MovieRoot`, `OpenGT4 - arcade / TopRoot`… — that is
  the name in the captain's own console output. Should read `MOON ENGINE`.
- The **window title**.
- Any string in the launcher/prep screen. (`LAW 16`: our surfaces carry MOON ENGINE and nothing Gran Turismo.)

## LAYER 3 — THE BINARY (the executable name) — **the captain wants this renamed**

`CMakeLists.txt:225` is the one game target: `add_executable(opengt4 src/app/main.c …)`.
**One line gets `moonengine.exe` without touching the 800 references to the internal library targets:**

```cmake
set_target_properties(opengt4 PROPERTIES OUTPUT_NAME moonengine)
```

(Placed beside the existing `set_target_properties(opengt4 PROPERTIES WIN32_EXECUTABLE TRUE)` at
`CMakeLists.txt:235`.) The internal targets (`opengt4_core`, `opengt4_platform_*`) stay as they are — internal
names nobody sees.

## LAYER 3b — NO `.bat`: WHAT THE WRAPPER WAS ACTUALLY DOING (delete it only after this lands)

The captain: *"i dont want the entire .bat thing. its just gonna be 1 exe!"* — correct goal, but the `.cmd` on
Baki is doing two jobs that **must move into the exe** before it can be deleted:

1. **It passes the disc image** on the command line. With no wrapper, the disc must come from the launcher and
   be remembered in the config (`%APPDATA%\Vulcan4\Vulcan4\` → `disc=…`) — i.e. **the front door has to exist
   for the exe to be double-clickable**, which is exactly the captain's plan.
2. **It captures the console output** (that is how the captain's run log was read for the window-title/frame
   analysis). The console is **optional**: `option(OPENGT4_WINDOWS_GUI … OFF)` at `CMakeLists.txt:233` sets
   `WIN32_EXECUTABLE TRUE` only when it is switched on — so today's `.exe` *does* print to a console, and a
   **release build with `-DOPENGT4_WINDOWS_GUI=ON` prints nowhere at all**.

**So: the engine should own its log.** One file, written by the program itself next to its user data
(`…\Vulcan4\Vulcan4\vulcan4.log`), mirroring what it prints, plus a `--log FILE` override. Add that *before*
deleting the `.bat`, or a real failure on his machine becomes unreadable.

Capture runs are unaffected: arguments can be passed straight to the `.exe`, no wrapper required.

## LAYER 4 — THE TEST HOOKS (recommend: leave them)

`OPENGT4_RACE_*`, `OPENGT4_LICENSE_*`, `OPENGT4_RACE_SNAPSHOT`, `OPENGT4_RACE_AUTOPILOT`… — ~90 environment
variables in `race_game.c`/`main.c` used by the test suite and by our capture scripts. They are invisible to
players. Renaming them breaks every script that sets them and buys nothing.
`shortcut: env-var prefix stays OPENGT4; upgrade only if the suite is renamed in the same pass.`

## LAYER 5 — THE FENCE THAT MUST **NOT** BE RENAMED

`NOTICE` and `LICENSE` are **GPL-3.0 with the OpenGT4 section-7 terms**: `LAW 14` requires the notice
**"Based on OpenGT4"** to stay in our docs and in the program's UI, and forbids presenting a modified build as
official OpenGT4. *That attribution is a legal obligation and the honest record of where the engine came from —
renaming the product is not the same as erasing the source.* Keep it, and keep upstream's copyright lines in
files we did not write.

## SO, IN ONE LINE

Layer 1 is the ask and costs three lines; layers 2–3 are cosmetic and safe; layer 4 is not worth the churn;
layer 5 is not ours to delete.
