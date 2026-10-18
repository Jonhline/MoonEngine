# Roadmap research — the captain's three asks (2026-10-18)

Read-the-code pass over the three entries the captain listed for today. **Nothing here is built yet.** Each item
ends with the route I would take, the cost, and the gate that decides it. Ledger entries: `WISHLIST.md` `G6.40`
(the debug overlay), `G6.41` (the movies), `A1` (touch that drives).

---

## A1 — TOUCH THAT DRIVES — *it already exists; the gap is elsewhere*

**What the code already has** (`src/platform/android/mobile_controls.h`, `mobile_controls.c`):

- **Nine controls**, laid out in normalised, safe-area coordinates: `STEER`, `ACCEL`, `BRAKE`, `UP`, `DOWN`,
  `HAND` (handbrake), `CAMERA`, `PAUSE`, `REVERSE`.
- **Ten simultaneous contacts**, each tracked by `int64_t` id, so two thumbs plus a stray palm do not steal each
  other's control (`moon_mobile_contact(phase, id, x, y)`).
- **Options** (`src/app/mobile_options.c`): size (3), opacity (5), layout (2), steering (digital/analog), and two
  separate switches — one for menus, one for races — under `controls.touch_*`. The `Classic` preset keeps
  pad-only behaviour.
- **It publishes into the engine's own pad state** (`moon_mobile_pad`), i.e. the same structure a physical pad
  fills. It does **not** emulate a DualShock and does not touch physics — which is exactly what the captain asked
  for ("not just emulating a controllers button").
- The visible buttons are drawn as **UI widgets**, not a second renderer: `moon_android_overlay(ui_project *ui)`
  (`android_host.c:103+`) adds them to the same project the game's menus use, and hides them entirely when a
  physical controller is connected (`s->controller`).
- `moon_android_event()` maps a `START` press to Escape, so the overlay's pause button is the game's pause.

**So the real gaps are:** (1) **the launcher takes no touch** — measured today: `android_host.c:76-77` disables
synthesis, `plat_sdl.c:140-145` carries touch as `plat_key_event.touch`, `plat_sdl.c:168-170` drops the
synthesised mouse, and the launcher polls raw `SDL_MOUSEMOTION`/`SDL_MOUSEBUTTONDOWN` (`main.c:1565`); (2) the
overlay has never been judged on real hardware.

**Route:** launcher-phase synthesis on (recipe already written, `/home/or/.hermes/recipes/moon-android-launcher-touch.txt`),
then a device pass on the Odin 2 with the overlay enabled. **Cost:** hours. **Gate:** a finger alone picks the
disc, presses a dock button and starts a race — and `Classic` still behaves exactly as it does now.

---

## G6.40 — THE DEBUG OVERLAY — *small, because the parts exist*

**What already exists:** `moon_frame_pacer` (`frame_pacing.h` — the one clock the frames are paced from),
`gt4_logger` + `logger_view` (the per-tick telemetry of `G6.20`, already drawn as graphs in
`src/core/menu_logger.inc`), `race_hud` (the game's own HUD drawn from disc assets), and the UI project system
that both the menus and the Android touch overlay draw through.

**What is missing:** a compact always-on-top readout — frame rate and frame time (worst frame, not just the
mean), the tick's own cost, draw submissions/triangles, the renderer in use and the device — behind one toggle,
off by default, drawn by the existing UI path rather than a new one.

**Route:** a `src/core/dev_overlay.c` that the front end and the race both call once per frame, fed by
`moon_frame_pacer` and the renderer's own counters; a key plus a stored option for the toggle. **Cost:** a day.
**Gate:** hidden → reference frame byte-identical (`68d8eb83dc75b6fe6e03cc2372ca186d`); shown → the readout's own
cost is printed so it can be subtracted. It is also the instrument that makes every later Android/Switch
performance argument checkable, so it earns its place before them.

---

## G6.41 — THE MOVIES WITHOUT THE SPARE PROGRAM — *no decoder needs writing*

**What the two paths do today:**

| | Windows today | Switch today |
|---|---|---|
| mechanism | spawn `ffmpeg.exe` (`movie.c:310`): `-v error -nostdin -threads 1 -i <file> -an -sn -vf <filter> -pix_fmt rgba -f rawvideo pipe:1` | link **libav** (`cmake/SwitchTargets.cmake` → `src/platform/switch/movie_decode.c`, `movie_ffmpeg.c`) |
| code | the search-order machinery (beside exe → user folder → build path → PATH) plus a temp-file pipe | `avformat` → `avcodec` → an **`avfilter` graph** → RGBA frames |
| weight | **168,964,608 B** beside a **13,319,343 B** engine | a few libraries, nothing spawned |
| Android | impossible — Android cannot spawn a program | not wired |

So: **the decoder already exists in this repository** (≈580 lines, and it uses `avfilter` for the scale/format
step rather than hand-rolled conversion). The work is packaging and wiring, not video decoding:

1. **Build a minimal FFmpeg** — `--disable-everything` then enable only what the movie path uses
   (`demuxer=mpegps`, `decoder=mpeg2video`, `protocol=file`, the scale/format filters, `--disable-programs`),
   optionally as **shared** libraries. A `--disable-everything` build is the documented way to shrink FFmpeg
   (StackOverflow, and every distro does it); the practical result for this feature set is **single-digit
   megabytes**, not 161.
2. **Cross-build it for the three targets** the project already builds for — win64 (the mingw toolchain in
   `cmake/`), Linux, Android arm64 (the NDK 27 we now have). One script, three outputs.
3. **Point every platform at the Switch decoder**: keep `movie_decode.c` as the shared implementation, retire the
   `CreateProcessW` path and the ffmpeg search order from `movie.c`, and delete `prepare_ffmpeg.c` and the
   launcher's download step. That also removes the "no ffmpeg beside the exe" class of bug from the captain's
   list for good — and gives Android menu movies at all.

**Licensing:** the project is GPL-3.0, so linking a GPL or LGPL FFmpeg is compatible; the notice file needs the
FFmpeg attribution, same as the switch port already does.

**Route:** build the minimal libav → wire it → delete the spawner. **Cost:** the build is the day's work; the code
change is small and mostly deletion. **Gate:** the same menu frame decoded by the new path is **byte-identical**
to the spawned ffmpeg's output (the comparison harness from the earlier ffmpeg work already exists), the log
loses `movie: using ffmpeg from <path>`, and a fresh install is **one artifact** with no second download.

---

## Order I would run them

1. **A1's launcher half** — smallest, and it unblocks testing on the A34 (which has no pad) and on the Odin.
2. **G6.40 overlay** — it turns every later "it feels slow" into a number, on the device, immediately.
3. **G6.41 movies** — the biggest, because it adds two cross-builds; it is also the one that makes the released
   product one file instead of two.
