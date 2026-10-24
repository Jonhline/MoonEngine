# RECIPE — the TWEAKS page, and Sense of Speed as its own group

**Written 2026-10-23 from the captain's own words.** Everything he asked for is in §0 verbatim in
substance; everything after it is the mechanism measured at HEAD `dbcd5bf` so a chef does not have to
guess. This is a recipe, not a plan: nothing here is implemented.

---

## 0. What the captain asked for (his words, in order)

1. **A category in the settings menu called TWEAKS** — "thats where all the gameplay related changes
   will be changable".
2. **Sense of Speed is its own sub-category** inside TWEAKS — "when u press on it it drops down every
   settings related to the menu".
3. **Every option is individually switchable** — his example: "if people for example want shake and
   wind audio but no dynamic fov. they can change. **everything can be changed**."
4. **The Enhanced preset enables every sense-of-speed option.**
5. (Reported in the same breath) **Discord Rich Presence is not working** — root cause measured and
   fixed separately; the toggle + id belong on this page, see §5.

**Standing law that constrains all of it:** LAW 12 — every option we add ships **OFF**; `0` means the
untouched original game. A preset may turn them on; the *default* may not.

---

## 1. What exists today (measured, not assumed)

| thing | where | what it does |
|---|---|---|
| PC Settings tile row | `src/core/menu_save.inc:392-440` | finds the authored row `Race/Replay/Screen/Picture Quality/Sound`, clones the **Screen** tile 5× into `PcGraphics / PcLighting / PcControls / PcMusic / PcPresets` |
| tile → page | `src/app/video_settings.c:1217-1226` | reads the tile's own name (`page_tiles[]`) and opens that tab; `MOON_SETTINGS_PAGE` overrides for tests |
| the tabs | `video_settings.c:986-1001` | `VIDEO_TAB_COUNT 6`, `page_titles[] = Screen/Graphics/Light & Shadow/Controls/Music/Presets`, `tab_rows[][]` = which authored rows each tab shows |
| the rows | `video_settings.c:682-689` | rows are **authored widgets** `choices[0..40]`; a page is a list of their indices |
| tab widgets | `video_settings.c:1581-1615` | tabs are created at runtime with `ui_widget_append`; on PC only one blank title is used |
| row visibility | `src/core/ui.h:40` | `ui_widget.visible` exists — a collapsible group can hide/show rows |
| presets | `video_settings.c:395` | `video_settings_preset_values(v, enhanced)` — one function, both presets |
| option store | `options_store.c` + `moon_options_int/get` | `"<group>.<key>"` strings; unknown keys are read but **not** written back as defaults |

**Consequence for the chef:** TWEAKS is a **7th tile**, cloned exactly like the other five, and a 7th
`page_tiles/page_titles` entry. Its rows cannot come from `tab_rows[]` (those are authored indices) —
they are cloned from an authored toggle row the way the tiles are cloned from Screen, or built as new
widgets. Pick one and say which.

The 5-tile row has no spare slot: a 6th tile needs `row[4]->x + pitch`, and `space->h`/the group must
grow by that much (`menu_save.inc:405-410` already shifts everything below `top`). **A second row
under the group is the safer layout** — say so in the diff if you choose it.

---

## 2. The page

```
PC SETTINGS
  [Graphics] [Light & Shadow] [Controls] [Music] [Presets] [TWEAKS]

TWEAKS
  ┌──────────────────────────────────────────────────────────────┐
  │ ▸ SENSE OF SPEED                              [ ON / OFF ]   │  ← group row, expands
  │     Dynamic FOV ............................... [ OFF ]      │
  │     FOV amount ................................ [  8° ]      │  ← only when FOV on
  │     Camera dynamics (trail / lead) ............ [ OFF ]      │
  │     Camera shake .............................. [ OFF ]      │
  │     Wind audio ................................ [ OFF ]      │
  │     Road roar boost ........................... [ OFF ]      │
  │     Radial blur ............................... [ OFF ]      │
  │     Blur strength ............................. [ 0.35 ]     │
  │     Vignette .................................. [ OFF ]      │
  ├──────────────────────────────────────────────────────────────┤
  │ Discord Rich Presence ........................ [ OFF ]       │
  │ Discord Application ID ............... 1562846084085194752   │  ← read-only text
  └──────────────────────────────────────────────────────────────┘
```

- **The group row is the switch the captain described:** press it → children appear (`visible`), press
  again → they collapse. Collapsed = the same as its children being off, or the group row keeps its
  own master toggle — **decide and write it down**; the captain's sentence implies a *disclosure*, not a
  master switch.
- While a child is off, its **value row is disabled and dimmed** (a FOV amount with FOV off is a lie).
- Every row writes its own store key immediately (the Presets page already does this) and marks the
  preset `modified` exactly like the existing rows do (`preset_modified`).

---

## 3. The options, with their real engine seams

All keys live under `tweaks.speed_*` so the store, the preset export and the file all agree.

| option | store key | default | engine seam (measured at HEAD) |
|---|---|---|---|
| Dynamic FOV | `tweaks.speed_fov` | 0 | the four hard-coded lens sites: `race_game.c:2206`, `race_game.c:2287`, `race_game_cameras.inc:154`, `race_cinematic.inc:146` — **one owner for the value, applied at all four** |
| FOV amount | `tweaks.speed_fov_degrees` | 8 | added to the *chase* lens only (view 1); views 0 and ≥2 keep the guest's lens |
| Camera dynamics | `tweaks.speed_cam_lag` | 0 | `gt4_race_run_camera()` `race_run.c:1229` gives the guest's own lag (car `+0x994/+0x998`); ours is a **second** spring layered on the eye, dt-normalised (`τ ≈ 0.12 s`), acceleration-driven — never a per-frame lerp (Nesky's weights are quoted at 60 fps) |
| Camera shake | `tweaks.speed_shake` | 0 | camera rows after they are built; Perlin noise on rotation only, amplitude ≤0.15°, trauma² decay |
| Wind audio | `tweaks.speed_wind` | 0 | `race_audio.c` `render()` callback — synthesised (noise → band-pass), speed-driven, never a shipped mp3 |
| Road roar boost | `tweaks.speed_road_roar` | 0 | `/sound/roadnoiz.es` is **already** decoded and mixed (`race_audio.c:414-445`, `race_car_sound.c:551-562`); this is a gain, not a new source |
| Radial blur | `tweaks.speed_blur` | 0 | `video_gl.c` after the scene resolve, before FSR/UI (`video_end_scene`, ~`:1158`) |
| Blur strength | `tweaks.speed_blur_amount` | 35 % | same pass |
| Vignette | `tweaks.speed_vignette` | 0 | same pass |

**Honest note to keep in the option text:** the one controlled study of a shipped racing game
(Split/Second, Sharan et al. MIG'13) found motion blur is *detectable* but changes neither perceived
speed, lap time, nor enjoyment. FOV and engine/wind audio are the parts with measured effect. Ship the
blur for the look; do not market it as speed.

---

## 4. Enhanced preset

`video_settings_preset_values(v, enhanced)` (`video_settings.c:395`) gains:

```c
v->speed_fov = v->speed_cam_lag = v->speed_shake = v->speed_wind =
    v->speed_road_roar = v->speed_blur = v->speed_vignette = enhanced;
v->speed_fov_degrees = enhanced ? 8 : 0;
v->speed_blur_amount  = enhanced ? 35 : 0;
```

**Classic leaves every one of them 0** — that is the authenticity guarantee, and it must be tested, not
assumed (§6, test 1).

---

## 5. Discord presence belongs on this page (it had no UI at all)

Measured root cause of "rich presence isnt working": the module was complete but **nothing could turn
it on** — no row anywhere read `integrations.discord_presence`, and `discord_presence_configure()`
refuses an empty app id (`discord_presence.c:578`). Fixed on 2026-10-23 by baking the captain's app id
(`1562846084085194752`, public client id) as the default in `main.c:4020`; the toggle itself still has
no home. **Put it here:** one switch + the id shown read-only.

---

## 6. Acceptance tests (a chef's dish is not done without these)

1. **Classic proves authenticity:** with Classic selected, every `tweaks.speed_*` key is `0` and the
   engine's camera projection is bit-identical to HEAD (log `sx` and compare).
2. **Enhanced turns them all on:** select Enhanced → all nine keys non-zero, then a race starts and the
   frame-time line prints with the pass on.
3. **Independent switches:** FOV off + shake on + wind on → the car's lens stays 60° while the shake and
   wind are audible/visible. This is the captain's own example; make it a test.
4. **Collapse:** press the group row → child rows are not drawn and cannot be focused; press again →
   they return with their values intact.
5. **No cost when off:** `video profile` ms/frame with every tweak off equals HEAD's within noise.
6. **Android:** the blur/vignette pass is PC-only unless a GLES shader is written (the PC path is
   `#version 330 core`). The FOV/audio/shake parts must still work on Android; print which parts are
   unavailable rather than silently no-op'ing.

---

## 7. What is still undecided (decide in the diff, do not guess silently)

- Second tile row vs a 6th tile squeezed into the authored row.
- Whether the group row is a disclosure or also a master switch.
- Where the values live in `pc_video_options` (9 ints) and whether they belong in the exported preset
  file (the preset interchange is a shipped format — adding fields is fine, dropping them is not).
