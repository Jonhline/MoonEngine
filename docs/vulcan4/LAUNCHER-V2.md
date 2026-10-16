# LAUNCHER V2 — the revised design (captain's corrections, 2026-10-15)

## UPDATE 2026-10-16 — the prep window is gone

The engine's Win32 preparation window (`launcher.c`, GDI) has been **deleted**, not hidden. The flow
built today (see `LAUNCHER.md` §0): no-arg / `--launcher` → the in-engine **first-run wizard**
(once) then the **studio room**; `--launcher-v2` → this SDL screen; the disc/`--studio`/`--shot`
CLI untouched. `launcher_bootstrap` opens no window and returns 1/2/0/-1. The pad/video/audio
pages that lived only in the deleted window are now this screen's job — its placeholder at
`launcher_ui.c:442` says so; their saved data still loads and applies.


The captain has now seen **both** launchers and made the call. His own words, in order:

> *"its not really what i wanted. im changing my mind. the launcher will be a window or fullscreen based on the
> user. bc the same window as the launcher will show u the game. and idk its too pixellated. i want to go for a
> more aesthetic one. and im working on the logo rn."*

> *"ok so this old one. i like it. it has alot of features. it even has an entire settings thing. u can change
> everything. lets just build on top of this one"*

> *"first things first i wanna make it sizeable. u can resize. uh. focus on it like when u fill ur screen but u
> can still see the taskbar and u can fullscreen. secondly. settings dosnt need its own tab. and i think we will
> make our own launcher. i want to make one that will have the title on the left top. and under it the options
> and also in the background ill put some screenshots or. anything tbh"*

## THE DECISION (final, so we stop flip-flopping)

**We make our own launcher** (`src/app/launcher_ui.c`), keeping **the engine screen's functions** — video,
audio, controls/pads, disc, player data — but with our own layout and look. The engine's screen
(`src/app/launcher.c`) stays in the tree as the reference for *what* the settings do and as the fallback
(`--launcher-v1`); it is **not** what we are building on any more.

## THE NORTH STAR FOR THE BACKGROUND (captain, 2026-10-15)

> *"once we do make the launcher i want to actually render the ford gt in this same pose. and it will actually be
> rendered in the menu. so there can be color changes and different moods and the car will reflect those
> lights"*

Not a picture of a car — **the car**, rendered live behind the menu, in the GT4 title-screen pose, with a paint
colour and a light mood the user can change, and the paint responding to those lights.

**Why this is reachable and not a fantasy:** every piece already exists in this repo.

| need | already here |
|---|---|
| decode the car from the user's disc | `src/platform/sdl/car_model.c` (CAR4/MDLS + the 256-opcode model VM) |
| a place a car is already posed and lit | the showroom path (`src/platform/sdl/showroom.c`) and photo mode (`src/app/photo_travel.c`) |
| the camera/pose | `src/core/course_tvcam.c` and the menu presentation code |
| the material/paint the car carries | the model's material table (80-byte records, `docs/vulcan4/MOD-FORMATS.md`) |

**What it changes about the launcher:** this cannot be an SDL panel app drawing rectangles. The background
becomes the engine's **own GL renderer**, with the dock drawn on top. Which means the launcher and the game are
in **one window with one GL context** — the exact thing the captain asked for, and it also makes the fade
transition free.

**The honest order of work:**

1. The window and GL context belong to the launcher (the same refactor already on the list, now mandatory).
2. The launcher opens the disc it is about to run and pulls the chosen car's model.
3. Pose the camera to match the title screen; draw the dock over it.
4. One paint colour swap (the material tint the game's own repaint uses).
5. Light moods — and the reflections are the last slice, because they need an environment map the engine does
   not build yet. `shortcut: mood = light colour + intensity first, real reflections after.`

**The first milestone to show him:** the Ford GT, in that pose, one light mood, dock over it, in one window.


## THE CHANGE LIST (from the captain, in his order)

| # | change | state |
|---|---|---|
| 1 | **Resizable window**, plus fill-the-screen-with-the-taskbar-visible, plus fullscreen | **this brick** |
| 2 | **"Settings doesn't need its own tab"** — no `Game` / `Settings` split; one list of options | next |
| 3 | **Title (logo) top-left, the options under it** | after 2 |
| 4 | **Background carries screenshots** (or whatever he drops in) behind the UI | **this brick** (image slot + scrim) |
| 5 | The engine screen's settings functions move into ours | after 3 |
| 6 | The logo he is drawing replaces the text title | when he sends it |

## WHAT MUST NOT REGRESS

- Video/audio/pad settings keep working through `launcher_options_save`/`load` (the same files the engine reads).
- Disc validation stays `launcher_validate_disc`.
- The ticked mods still reach the engine as `OPENGT4_OVERLAY`.
- `--launcher --launcher-v2` keeps working for captures, and `OPENGT4_LAUNCHER_PAGE` selects a page.

## A CONFLICT TO SETTLE LATER (written down so it is not forgotten)

The captain wants **the launcher and the game in one window** (so the mode he picks applies to both, and the
transition is a same-surface fade). That is free **only if the launcher draws into the engine's window**; our
own launcher can do it (SDL), the engine's GDI screen cannot. This gets easier now that we are building ours.


## 1. ONE WINDOW — the launcher and the game share it (architecture)

**Today:** the launcher creates its own window, destroys it, and the engine creates a *second* one
(`video_gl.c:211`), each with its own mode. So the launcher can be a small window while the game is fullscreen,
and the mode is decided twice.

**Wanted:** one window for the whole session, in the user's chosen mode (windowed **or** fullscreen, at the
user's resolution). The launcher draws into it; the game continues in it.

**What that costs, in order:**

1. The window must exist **before** the launcher UI runs. Today `main()` calls the launcher at `main.c:1225`
   and `plat_video_init` at `1388` — the order has to invert for the settings that matter: load the saved video
   settings → open the window → run the launcher → run the game.
2. The launcher then draws `MODS`/`DISC`/`START` **into the engine's window** rather than making its own. If the
   captain changes mode inside the launcher, the window is recreated there and then.
3. **The payoff:** the transition becomes a same-surface fade — no window swap, no black gap. This is the thing
   the captain asked for earlier and it falls out of the one-window design for free.

## 2. THE TEXT — real anti-aliased type, not an 8x12 bitmap face (cosmetic, high impact)

The prototype's face is a chunky 8x12 bitmap (generated from DejaVu Mono by `tools/mk_font_launcher.py`)
because the engine has no font of its own — its glyphs are decoded from FT01 **on the user's disc**
(`src/core/font.h`), and the launcher runs before a disc is chosen.

Wanted: something that looks designed. The cheapest correct route, using what is already here:

- **`third_party/stb/stb_image.h` is already vendored** → loading a logo PNG costs no new dependency.
- Add **`stb_truetype.h`** (same family, public domain, single header) and rasterise glyphs at the real pixel
  size with anti-aliasing instead of blitting pixels. Scalable text also means the launcher looks right at
  1440p and 4K, which a fixed 8x12 atlas cannot.
- Ship one free typeface (OFL) **inside the binary** as bytes, so it is still one file on disk.

`shortcut: one typeface, no font picker, no fallback chain — upgrade when a second language needs a second face.`

## 3. THE LOGO (the captain is drawing it)

- Format: **PNG with transparency**. `stb_image.h` already reads it; no new code for decoding.
- Placement: loaded from beside the exe (`logo.png`) so he can iterate **without a rebuild**, with an embedded
  copy as the fallback and the text title as the last resort.
- Alpha blending + bilinear scaling must be on, or an anti-aliased logo will look like the bitmap font did.
- **Ask him for the intended display size** — draw the logo at the size it will be shown (or exactly 2x/4x for
  crispness), not at some arbitrary size we then rescale badly.

## WHAT SURVIVES FROM THE PROTOTYPE (`7cbfaf5`)

The seam and the behaviour are right and stay: `--launcher --launcher-v2` off by default; DISC browses and
validates with `launcher_validate_disc`; MODS ticks the subfolders of `mods/` and hands them to the engine as
`OPENGT4_OVERLAY`; START returns 1 through `launcher_bootstrap` so `main.c` builds the argv as before;
`OPENGT4_LAUNCHER_PAGE` selects a page for captures.

## ORDER OF WORK

1. **Text** (independent, immediately visible): stb_truetype + one typeface, replacing the bitmap face.
2. **One window** (touches `main.c`'s init order): window from the saved settings, launcher draws into it.
3. **Logo** plug-in point, so the captain's file drops in and shows up.
4. **Fade transition** — now nearly free, because it is one surface.

## THE CAPTAIN'S RULES FOR THE FRONT DOOR (2026-10-16)

Written down the moment he said them, so they cannot drift:

1. **A page is a transparent menu INSIDE the launcher, not a screen of its own.** The room stays visible behind it — the dock and the car are never covered by an opaque panel. (The old DISC/MODS/OPTIONS/START panel was the wrong shape; the studio's own dock is the right one.)
2. **A choice may move the camera.** Options are allowed to change where the room looks; camera placement is part of the menu, not a separate screen.
3. **A choice may change the colour** (of the room, the car, the UI).
4. The pages are reached from the dock, and START is the only one that hands over to the game.
5. The current state of that work: pressing MODS/OPTIONS/EXIT in the studio dock prints
   `studio: <name> pressed (nothing behind it yet)` — the pages behind them are the next build, and the mod manager from the older front door is to be brought in behind this dock, not kept as a separate window.
