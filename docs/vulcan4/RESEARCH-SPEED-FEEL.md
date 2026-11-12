# SENSE OF SPEED — the measured state of OUR engine (2026-10-23)

Issue: #29 `G6.7 — The Rush`. This file is research only: it records what the code does **today**,
with file:line, so a recipe can be written against facts instead of hope. The external reference
pass (what other racing games do, with sources) lives beside it in `speed-feel-references.md`.

## 1. The lens is a constant, and it is not the guest's

`src/app/race_game.c:2287`

```c
float sx = 1.73205f;
if (view != 0 && view != 1)
    sx = 1 / tanf(atanf(1 / point[6]) * 4 / 3);
race_view_set_camera(g->view, eye, rows, sx, sx * 1.30122f, 0.1f);
```

- Views 0 (bumper/on-car) and 1 (chase) use **sx = 1.73205 = 1/tan(30°) → 60° horizontal**.
- Every other view (TV, dive, license) uses the camera point's own half-width `point[6]` spread by 4/3.
- Vertical is `sx * 1.30122`, so the base chase lens is **60° h / 47.9° v**.
- The same constant appears at `race_game_cameras.inc:154` (dive cameras) and `race_game.c:2206`
  (split-screen path), and `race_cinematic.inc:146` for the cinematic projection.
  **Four copies of one number. A FOV knob must own all four or it will disagree with itself.**

## 2. The camera is already guest data, including the lag

`src/core/physics/race_run.c:1229` — `gt4_race_run_camera(r, car, type, point, eye, rows)`:

| type | what it is | where the numbers come from |
|---|---|---|
| 0 | on the car | car `+0x8a4/+0x8a8/+0x8ac` eye, `+0x8c4` yaw, `+0x8bc` pitch, `+0x8c0` roll, plus the player's view bob `+0x18` |
| 1 | **chase** | same, and **`yaw -= drawn(car, 0x994, 0x700)`, `pitch -= drawn(car, 0x998, 0x704)`** |
| ≥2 | TV / point cameras | the point's own turns `+0xC/+0x10/+0x14` (degrees) and lens `+0x18` |

- **GT4 already trails the car**: 0x994/0x998 are the game's own camera lag, subtracted from the car's
  yaw and pitch every frame. Any spring we add on top is a *second* spring unless we separate the two.
- Roll for type 1 is deliberately **0** (the chase camera does not roll with the car).
- The pose is rebuilt per frame from car state; nothing is smoothed in our layer.

## 3. Frame timing

- `src/app/main_race_flow.inc:213` — `race_game_update(flow->game, elapsed)` where `elapsed` is
  **wall-clock seconds for that frame**; `race_game.c:2528` passes it straight to the sim.
- Nothing interpolates sub-tick poses: at 120 Hz the same 60 Hz pose is drawn twice. High refresh
  (Unlimited/60…240) already exists in the video options, so the *cadence* is there; the *motion*
  between ticks is not.
- Profiling already prints a real frame time: `video profile: %.3f ms/frame, %.1f draws/frame,
  %.1f quads/frame, %.1f fps` (`video_gl.c:783`). **A speed effect can be measured on/off with a
  number, not an opinion.**

## 4. Where a post-process pass can live

`video_begin_scene` → MSAA FBO at `w×h` (Android render scale applies here) → `video_end_scene`:

```
blit resolve → daylight_post HDR (PC only) → edge AA → FSR upscale → ...
```

`video_end_frame` (`video_gl.c:771`) then blits the result to the default framebuffer.

- The honest seam is **after the resolve, before FSR/UI**: the scene texture exists, the 2D UI has not
  been drawn yet, and the pass can be skipped entirely when the knob is off.
- PC shaders are `#version 330 core` / `430 core` (`render3d.c`, `fsr.c`). Android renders through a
  separate GLES path, so **a post pass is two shader sources or it is PC-only.** Say which, in the recipe.
- HDR is excluded on Android by `#if` (`video_gl.c:1057`), i.e. there is precedent for "PC gets the
  expensive pass, Android gets the geometry/knob-only version".

## 5. Knob plumbing that already exists

`moon_options_int(&s->store, "graphics.<key>", default, min, max)` (`video_settings.c:135+`) →
`pc_video_options` → the launcher's GRAPHICS page (which now has a live A/B comparison film, his
`459eda6`). A new opt-in knob costs one field, one store key, one row — and lands in a screen the
captain already uses.

## 6. What is NOT there (verified by grep, 2026-10-23)

- No speed blur, radial blur, vignette, chromatic pinch, camera shake, FOV scale, or "rush" symbol
  anywhere in `src/`.
- No interpolation of camera or car poses between sim ticks.
- No per-frame camera trace (the only camera log is `showroom.c:147` for the launcher room).

## 7. The questions a recipe must answer before it writes a line

1. **Which camera types does the knob touch?** Chase (1) is the feature; on-car (0) and TV (≥2) must
   keep the guest's lens or replay/TV framing changes meaning.
2. **How is the guest's own lag measured?** print `0x994/0x998` over a lap before adding anything, or
   the spring is tuned against an unknown.
3. **What is the frame cost on the A34?** The Android target is pixel-bandwidth bound (#22), and a
   full-screen pass is exactly pixels.
4. **What is the proof?** A side-by-side pair at the same car, speed and frame — plus the profiler line
   with the pass on and off.
