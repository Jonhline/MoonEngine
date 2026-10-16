# LAUNCHER — the seam map (research, for whoever codes it)

**Written by Caine (R) 2026-10-15. Facts measured in the tree, not designed here.** Wishlist: `G6.37`.
**The captain writes the code** (`AGENTS.md`, *Who writes the code*).

## 0. CURRENT FLOW (2026-10-16 — the built one)

The standalone Win32 "prepare your disc" window (`launcher.c`, GDI) has been **deleted**. What runs today:

| launch | path |
|---|---|
| **no arguments** (double-click) | `launcher_bootstrap` returns **2** — the studio front-end. `main` runs the **first-run wizard** when no disc+API are saved, else goes **straight to the room**. |
| **`--launcher`** | same as no-arg (aliased to the wizard/room); it no longer opens a prep window. |
| **`--launcher-v2`** | our own SDL options screen (`launcher_ui.c`), then the game. |
| **disc / `--studio` / `--shot`** | the game CLI — `launcher_bootstrap` returns **1** immediately, untouched. |
| **`--launcher-gallery <disc> <folder>`** | `launcher_capture_gallery` (stays reachable here). |

`launcher_bootstrap` creates **no window** and opens **no disc**; it only validates, loads/saves
`launcher.ini`, and rewrites `argv`. Returns **1 = run the game, 2 = front-end, 0 = cancel, -1 = failure**
(header doc updated). The first-run wizard saves `disc`, `api`, `car`, `color` into `launcher.ini`
(`launcher_config.c`); a saved disc that no longer exists falls back to the wizard.

**What lost its editor in the deletion (reported, not silently dropped):** the prep window's own
**pad-bindings**, **video** and **audio** pages. Their *data* still loads and applies
(`launcher_options_load/save`); only the GDI editing UI is gone — its home is the v2 screen
(`launcher_ui.c`, currently a "MOON ENGINE LIMITATION" placeholder for those pages).

## 1. IT IS ALREADY ONE EXECUTABLE, IN-PROCESS

There is **no spawning** and no second binary. `main()` calls the launcher **first**:

```c
/* src/app/main.c */
launcher_startup startup={0};
int prepared=launcher_bootstrap(&argc,&argv,&startup);
```

`launcher_bootstrap` (`src/app/launcher.c`, declared `src/app/launcher.h`) **rewrites `argc`/`argv`** and
returns as above. So the launcher's whole job is *to decide, then hand the engine its arguments*.

## 2. WHAT THE SEAM CARRIES TODAY (`launcher.h:8-21`)

| struct | fields | note |
|---|---|---|
| `launcher_preferences` | `disc[1024]`, `saves[1024]`, `file[1024]`, `show` | persisted by `launcher_config.c` as `disc=`, `saves=`, `show=` |
| `launcher_options` | `width,height,fullscreen,vsync,upscale,filter,text,aspect`, `volume[7]`, `pad[2]` | video/audio/pad; saved in groups (1 video, 2 audio, 4 pad1P, 8 pad2P) |
| `launcher_startup` | `preferences`, `video_file[1024]`, **`arguments[7]`** | the argument-injection slot the engine receives |

The disc picker already validates: `launcher_validate_disc()` reads `SYSTEM.CNF` and reports a non-PS2 file.

## 3. THE ENGINE'S OWN ARGUMENTS (already parsed, `src/app/main.c:1239-1320`)

`--window WxH` · `--fullscreen` · `--upscale off|ultra-quality|fsr|quality` · `--ui-filter` ·
`--text-sharpness` · `--vsync` · `--no-config` · `--first-run` · `--intro` / `--skip-intro` ·
`--loading-preview` · `--loading-shot FILE` · `--preview-courses` · `--race-preview` · `--drive` ·
`--time-trial` · `--pre-race` · `--race-course LABEL` · `--race-car CAR` · `--split-screen` ·
`--shot FILE` · `--video-settings` · `--save-dir DIR` · `--player-name NAME` · positional: **the disc image**.

## 4. WHAT THE PROTOTYPE ACTUALLY NEEDS TO ADD

1. **A Mods page** next to `HOME/VIDEO/AUDIO/CONTROLS` (the page enum lives at the top of `launcher.c`).
   It reads a `mods/` directory (one subfolder per mod, optional `manifest.json` with name/author/version),
   shows tick boxes and an order, and stores the enabled list next to `disc=`/`saves=` in the config
   (`launcher_config.c` already owns that file).
2. **The hand-over:** the enabled list becomes the engine's mod source. Two clean options — pick ONE:
   - **argv (preferred):** add `--overlay <dir>` to the engine's parser and push it through `arguments[7]`.
     Explicit, visible in a log, and the launcher already owns this slot.
   - **environment:** set `OPENGT4_OVERLAY` before returning (Windows: `_putenv`). Less visible in logs.
3. **No mods ticked must mean the identical invocation** — that is the safety property (and the test).
4. **The transition** (captain's ask): the fade belongs either just before `launcher_bootstrap` returns 1
   (launcher fades out) or at the first engine frame (engine fades in from black). Music/video for it comes
   from the user's own disc (`LAW 1`), or is our own drawn sting (`LAW 16`).

## 5. FACTS THAT CONSTRAIN IT

- `launcher.c` is **Windows-only GDI today** (its header: no disc font, image, menu script or logo is loaded;
  shapes are newly drawn GDI primitives) and 50 KB. Moving the window/drawing to **SDL** (already a dependency,
  `src/platform/sdl/video_gl.c`: `plat_video_set_view:378`, `plat_draw_quad:662`, `plat_draw_glyph`) is what
  makes it portable — Linux now, Android later.
- The look is **ours to draw**: the palette the file already quotes is the game's own
  (`/projects/option/option.mproject` ScreenRoot: bg `171,171,178`, popup focus `184,132,46`).
- `--shot FILE` exists, so a prototype's **gate can be a picture of the window** with a mod listed.
