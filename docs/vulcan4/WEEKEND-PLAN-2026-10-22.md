# WEEKEND PLAN — every task, researched individually

*2026-10-22, Caine. Companion to `RESEARCH-WEEKEND-2026-10-22.md`. Measured against
`/home/or/moonengine` @ `1ae9b71`, clean tree. Nothing dispatched — each brief ends with the exact
first step and the proof that closes it. Ordering and the why are at the bottom.*

**Eleven tasks: 4 dev (his list) + 7 on the board (issues #20–#26). Two of the seven are already
built** — marked `DONE IN TREE`, so they cost zero and free two slots.

---

# DEV LIST

## D1 — SENSE OF SPEED (G6.7, issue #29)

- **State:** camera rebuilt per frame (`race_game.c:2262-2300` → `race_run.c:1229`); chase FOV is a
  constant `1.73205f` (`:2296`); post seam is the final blit (`video_gl.c:763`); frame limit already
  offers 120/144/240 but the sim is a 60 Hz accumulator (`race_game.c:63,2596`) so poses duplicate.
  No shake/blur/wind anywhere in the tree.
- **Plan (4 slices, each shippable alone):**
  1. FOV constant + eye point (arithmetic).
  2. Spring/damper camera over the guest target.
  3. Full-screen pass at the final blit — **needs a second FBO**.
  4. Shake (separate toggle, real zero) + wind synth (math in the research doc).
- **Cost:** 1 = hours; 2 = a day; 3 = a day + frame-time print; 4 = half day each.
- **First step:** slice 1 only — change `sx` and the camera point for views 0/1, capture a before/after
  pair at the same speed, and let him say which is faster without being told.
- **Verify:** same car, same corner, two PNGs; frame time unchanged within 0.2 ms.

## D2 — MORE THAN SIX CARS (G6.10, issue #30)

- **State:** physics is count-driven, but **three** six-slot walls: entry blocks `race_run.c:97` (stride
  `0x35E0`; block B's next slot is `WORLD`), per-car block `cond+0x98` (stride `0x1100` → ends exactly
  at `CARS` `0x6698`), car state `cond+0x6698` (stride `0x10A0` → ends exactly at `0xCA58`). Renderer
  already draws 8 (`OPPONENTS 7`). App tables and the gate at `race_game.c:1113` stop at six.
- **Plan:** slice 1 = widen our own tables behind the existing six clamp (no visual change, fully
  testable). Slice 2 = a 7th entry in block A's free slot `0x01bde2d0` **after proving it unused**.
  Slice 3 = side-car storage + accessor for the two conductor arrays. Slice 4 = grid/points/replay.
- **Cost:** slice 1 = a day, low risk. Slices 2–4 = a branch, not a weekend.
- **First step:** slice 1. Grep every `[6]` in the audit, make it `RACE_MAX_CARS` + a live count.
- **Verify:** 6-car race byte-identical (same replay hash), then one 7-car race on a scratch branch.

## D3 — DISCORD (new, no issue)

- **State:** ARMSX2 pattern measured — vendored MIT `discord-rpc` + **helper process** so the GPL binary
  never links the proprietary SDK. Discord's docs: presence needs **no auth** (Application ID +
  UpdateRichPresence); account linking/friends need the Social SDK + OAuth; Android is supported.
- **Blocked on him:** the Discord **Application ID** (Developer Portal). Nothing else.
- **Plan:** tier 1 = presence over local IPC (course, car, speed, mode) — one module, off by default,
  silent if Discord is absent. Tier 2 (his call) = account link + friends + cover art = helper process.
- **Cost:** tier 1 = a day; tier 2 = a sub-project.
- **First step:** he creates the app → I write the IPC client behind a flag.
- **Verify:** card shows *Moon Engine — Tsukuba* on desktop; with Discord closed, zero errors and no
  added frame time.

## D4 — ENGINE SWAPS (G6.5, issue #28)

- **State:** engine identity is one 8-byte SpecDB key at `setup+0x38` (read by `record.c:666` →
  `engine()`, which takes 24 rpm points, 24 torque values, redline, turbo flag). The same row's `+0x18`
  is the sound id (`race_audio.c:431`). So one key moves torque, redline, engine braking **and** sound.
- **Plan:** write the donor key after the setup block is built (`race_run.c:536-560`, before
  `gt4_race_init`); pass the donor label to `gt4_engine_bank_open_car` (`race_audio.c:444`); leave the
  gearbox row (`+0x48`) alone; optional TURBINE key (`+0xb0`) only if induction should follow.
- **Cost:** the smallest real feature on the list — a day for two same-family swaps in Arcade.
- **First step:** one swap (NA→NA) driven from our own config, undo = drop the key.
- **Verify:** tach reads the donor redline, the note is the donor's, dropping the config restores the
  original engine byte-for-byte, save untouched.

---

# BOARD (issues #20–#26)

## B1 — #20 Vulkan surface at handover + second window

- **State:** half fixed already. `launcher_fsr.c:216-228` now refuses the hidden GL helper on Android
  with a comment naming the exact SDL error — **the "second window kills the process" half is closed in
  code**. The surface half has machinery: `Context::recreate_surface()` (`vk_context.cpp:478-516`),
  `moon_vk_render_surface_lost()` (`vk_render.h:19`), and the `VK_ERROR_SURFACE_LOST_KHR` paths
  (`:544,566`). What is unproven is whether the **activity handover** reaches that machinery.
- **First step:** trace the handover — log `moon_vk_render_surface_lost()` entry and the
  `vulkan: surface lost, recreated` line, then relaunch and read the log.
- **Verify:** pick disc → game starts; no `-1000000000`, no "only supports one window"; background →
  foreground once, still drawing.

## B2 — #21 Launcher on every launch

- **State:** `launcher_bootstrap` returns "run the game" for any `argv[1]` that is not `--launcher`
  (`launcher.c:21`); `DiscActivity` persists the disc grant and jumps straight to `GameActivity`
  (`DiscActivity.java:105`, `:112`).
- **Plan:** Java always starts the launcher activity; the remembered disc becomes a **prefill argument**
  (`--launcher --disc <uri>`), never a bypass. One native branch.
- **Cost:** small — Java + one branch. Batch with B1 in the same APK: same launch path.
- **Verify:** force-stop → cold start with a saved disc → the studio room appears, disc preselected, no
  game window; then press Play → game starts.

## B3 — #22 A34 performance

- **State:** the render-scale knob **already exists** (`video_gl.c:51-54,1024`, commit `13082d6`) and
  defaults to **0 = full** — so a screenshot without the knob moved proves nothing. Still true: every
  Vulkan image runs in `VK_IMAGE_LAYOUT_GENERAL` (blit at `vk_context.cpp:465-467`; pipeline/context
  cited in the queue) and the swapchain is TRANSFER_DST-only with a full-extent blit each frame.
- **Plan:** (a) add the three overlay fields he authorised — render-target size, blit ms, present mode —
  so a screenshot answers "did the knob take"; (b) prove bandwidth: fps must scale with pixel count
  between 1.0 and 0.5; (c) then layouts, then draw-into-swapchain, then merge passes.
- **Cost:** (a) hours; (b) one APK; (c) the real work, medium risk.
- **Verify:** at 0.5 scale the frame time roughly halves on his A34; if it does not, the diagnosis is
  wrong and we stop before touching layouts.

## B4 — #23 Settings EXIT returns to its opener

- **State:** the menu already keeps a page stack (`menu->pages[]`, `menu_page_depth`/`menu_page_at`,
  `menu.c:1073-1081`). The shared Options screen closes to a fixed destination instead of popping.
- **Plan:** record `menu_page_depth` when options opens and pop to that depth on EXIT — one rule for
  every platform, not Android-only.
- **Open read:** the authored EXIT handler is in the UI data, not in a C string I can grep — find it in
  `menu.c`'s event path (`event_call`) for the options root before editing.
- **Verify:** GT home → settings → exit → **GT home**; title → settings → exit → **title**; focus
  restored on the screen that opened it.

## B5 — #24 Round touch controls + two digital steering buttons — **DONE IN TREE**

- **Evidence:** `mobile_controls.c:10-11` defines `STEER_LEFT`/`STEER_RIGHT` for digital mode;
  `android_host.c:482-550` draws every control as a disc + ring. Commits `77e8ce2` (round + two-button
  steering) and `3645cd9` (thumb targets).
- **Do not re-dispatch.** What is left is the **gap**, and it is real: `android_host.c:409-420` hides
  touch when a pad is *connected*, not when it was *recently used* — a paired-but-idle Bluetooth pad
  would remove a phone's only way to drive. That is S10, a small honest fix.

## B6 — #25 Dead presses / two presses to select — **DONE IN TREE (Android)**

- **Evidence:** `menu_touch()` now commits on a single tap under `__ANDROID__`
  (`menu.c:1563-1583`, commit `3645cd9`), and `pointer_hit()` accepts the whole authored row box, not
  just the label (`:1247-1260`). The hint text was corrected to "tap a row to select it".
- **Do not re-dispatch.** **First step is a device check, not code:** install the current APK and
  confirm. If double presses persist, the suspect is the installed APK being older than `3645cd9`.
- **Verify:** one tap on course select opens the next screen; the log prints
  `touch: settings tap commits (single)`.

## B7 — #26 Physics vs PCSX2 telemetry

- **State:** the tooling is **already there** — `tools/pcsx2_capture.py` carries a DebugServer client
  with `read_ram` / `read32` / `status` (`:56-118`). The car struct is `COND + 0x6698 + i·0x10A0`;
  car 0 = **`0x01cff568`** (computed: `0x01cf8ed0 + 0x6698`). Fields: speed `+0x640`, yaw `+0x8c4`,
  x/z `+0x8a4/+0x8ac`.
- **Plan:** one 60 Hz pad script → both sides; sample once per frame from the green light; two CSVs;
  report max/RMS deviation in metres and km/h. No engine change.
- **Cost:** a script + one PCSX2 session. **Zero product risk**, which makes it the best parallel task.
- **Caveat to state up front:** `COND` comes from Arcade captures — the same race and car must be used
  or the address is meaningless.

---

# EFFICIENCY PLAN — two lanes, one APK per batch

**Lane A — Android (only lane that needs his phone):**
Batch 1 APK = **B1 + B2** (same launch path, both verifiable in one cold start).
Batch 2 APK = **B3(a)** overlay fields (+ the knob already there).
Batch 3 = **B4** (global fix, ships in the same build).
Then: **B5/B6 device check**, no code.

**Lane B — Engine/PC (no phone, no build gate for him):**
**D4 swap → D1 slice 1 → D2 slice 1**, with **B7** running in parallel because it touches no product
code. D3 waits on the Application ID.

**Why this order:**
- Stale tasks first: B5 and B6 are built, so checking them costs one install and buys two slots back.
- B1 and B2 share a root cause (launch path) → one APK, one test session.
- D4 is the smallest real feature with a measurable proof; D1 slice 1 is arithmetic; D2 slice 1 is
  table-widening with a byte-identical regression test. All three are "lands on main" work.
- Nothing on the guest-memory branch (D2 slices 2–4) is promised for the weekend.

**Standing rules for every dish:** scope stated in one line (global vs Android-only), commits authored
`Or Golan`, never push, build every target + `ctest -R` on what it touched, and the deliverable is an
APK on the share plus the log he can attach.
