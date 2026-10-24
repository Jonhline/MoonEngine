# WEEKEND RESEARCH — the four dishes, measured first-hand

*2026-10-22, Caine. Measured against `/home/or/moonengine` @ `1ae9b71` (clean tree) plus
ARMSX2's own release notes and Discord's own docs. Nothing here is dispatched; this is the
inventory before any code moves. No game build was run for this pass — every claim below is
read from the source that is on disk right now, with the file:line to check it.*

---

## 1. BETTER SENSE OF SPEED — G6.7 "THE RUSH"

### What the camera actually is

The camera is **not a stored struct we have to find**. It is rebuilt from car state every frame:

- `src/app/race_game.c:2262-2300` — the chase branch: `race_view_camera_point()` + `gt4_race_run_camera()`
  → `race_view_set_camera(view, eye, rows, sx, sx * 1.30122, 0.1)`.
- `src/core/physics/race_run.c:1229` — `gt4_race_run_camera()` reads the car's position/yaw/pitch
  (`+0x8a4/+0x8c4/…`), and for `type == 1` (behind the car) already subtracts the **view lag**
  (`car+0x994`, `car+0x998`). The game's own camera-trailing is in the physics data.

### The FOV is ONE number for the chase views

```c
float sx = 1.73205f;                       /* race_game.c:2296 */
if (view != 0 && view != 1)
    sx = 1 / tanf(atanf(1 / point[6]) * 4 / 3);
```

`1.73205 = tan(60°)`. Views 0 and 1 (on/behind the car) get a **constant** projection; every other
view derives its field from the car model's camera point (`+0x18`, 52° on the Ka). So a wider chase
FOV is one constant / one call-site, and it does not touch the user's car data.

### The renderer seams

- **Post-process:** `src/platform/sdl/video_gl.c:763-775` — `plat_video_end_frame()` blits the finished
  scene FBO straight to the window with `glBlitFramebuffer`. That is the exact place a full-screen pass
  belongs. It needs a second FBO (you cannot sample the target you are drawing into) — that is the whole
  cost of radial blur / vignette / chromatic pinch.
- **Nothing of THE RUSH exists today:** `grep -E 'shake|vignette|radial|chromatic|wind'` across
  `src/app` and `src/platform/sdl` returns zero hits. Greenfield in a clean seam.
- **Shake** goes on the camera vector (`eye`/`rows`) before `race_view_set_camera` — no new pass.

### Frame rate — good news and one trap

- The simulation is a fixed 60 Hz accumulator: `race_game.c:63 #define STEP (1.0/60)`,
  `:2596 g->acc += dt`, `:2603 while (g->acc >= STEP)`. Presentation is throttled separately in
  `plat_video_begin_frame()` (`video_gl.c:733`) from `frame_limit`.
- The settings already offer **Unlimited / 60 / 90 / 120 / 144 / 165 / 240**
  (`video_settings.c:300-302`). High-refresh presentation is already there.
- **The trap:** the only interpolation is `drawn()` (`race_run.c:1202`), which blends the car's old and
  new 60 Hz states by the guest's own blend byte. At 120 Hz you get **the same pose twice**. High refresh
  alone will not smooth the motion — sub-tick interpolation is the missing piece, not the frame limit.

### Wind synthesis — the actual math (no file, generated per sample)

Wind is **turbulent pressure fluctuation**, so its level follows dynamic pressure, **∝ v²** (drag force is
∝ v², aero *power* ∝ v³ — we want the noise, not the power). Everything below runs in the mix callback.

```
v_ref = 80 m/s (288 km/h)
x     = clamp(v / v_ref, 0, 1.25)          // v = CAR_SPEED, car.h:75
gain  = 0.35 * x²                          // ≈0.11 at 100 km/h, ≈0.35 at 288
f_lo  = 180 + 900 * x^1.5                  // Hz, low "rush" band  (buffer/A-pillar)
f_hi  = 3000 + 6000 * x                    // Hz, high "hiss" band (mirrors, seams)
gust  = 1 + 0.18*sin(2π·0.37t) + 0.09*sin(2π·0.91t)
L,R   = two independent white-noise streams   // decorrelated ⇒ stereo width
rush  = bandpass(L, f_lo, Q≈0.7)  * gain * gust
hiss  = highpass(R, f_hi)         * gain * 0.6
out  += (rush + hiss) * view_gain          // cockpit 1.0, chase 0.55, TV 0.3
```

- **High-pass the wind at ~1.5 kHz on top of the band choice**, because the disc's own `roadnoiz.es`
  (3 layers, already mixed as engine voice #2) owns the low end — otherwise wind just reads as more rumble.
- **One biquad per channel, Direct Form II** (RBJ cookbook coefficients). ~10 flops/sample — nothing.
- **Test, not taste:** render 1 s at v = 0 / 27.8 / 83.3 m/s. Assert (a) v=0 is bit-exact silence,
  (b) RMS(83.3)/RMS(27.8) ≈ **9.0 ± 10 %** (19 dB, the v² law), (c) spectral centroid rises monotonically.
  Clear the gain above 1.25·v_ref so the A34 and a 300 km/h car don't clip the same.

### How Driveclub actually did it (the reference Boss named)

Driveclub is the proof that **sense of speed is mostly camera-space effects, not frame rate**. Evolution
dropped 60 fps for a locked 1080p30 and spent the freed budget on things that only exist in motion:

- **Full-screen camera blur + per-object blur, many samples.** Digital Foundry: *"Camera and object blur is
  used extensively… high precision and plenty of samples"*; the *"full-screen implementation [helps] to
  smooth over a locked 30fps refresh"* and keeps panning from feeling lacklustre. This is the single
  biggest contributor — it manufactures continuous motion between discrete frames.
- **Deliberate lens imperfections.** Tech art director Alex Perkins: they mimic *"slightly cheaper film
  lenses"* — chromatic aberration, dynamic lens flare, depth of field — plus subtle film grain/dithering
  and digital noise. DualShockers' verdict: effects that *"have nearly no meaning unless you see the game
  in motion: advanced motion blur and deliberate focus imperfections… flying dirt and camera shaking."*
- **Camera shake, tuned not random.** Named alongside the blur as one of the things that separates stills
  from motion.
- **Cockpit first.** Same review: *"you really want to play Driveclub from the cockpit view… the sensation
  of speed perceived from inside the car simply cannot be traded off."* Cars have modelled interiors and
  hands, so the camera has a frame of reference moving with the car.
- **Particles with the blur applied to them.** Leaves, dust, confetti; rain particles are motion-blurred
  into long vertical streaks rather than drawn as lines; windscreen droplets are deliberately
  out-of-focus and react to wipers and motion.
- **Latency below the refresh.** Perkins: the tyre/suspension update rate is *"much quicker than the actual
  refresh rate"*, and controller latency was minimised with Sony's pad designers. Speed reads as fast when
  the car answers before the frame does.
- **Audio is a separate recording, not a filter.** Paul Rustchynsky: interior and exterior engine audio are
  distinct captures; inside you get *"air rushing over the cockpit"* and muffled engine. A large part of
  "fast" is heard.

Mapping to our seams: the blur/shake/DOF/chroma pass is the `plat_video_end_frame` hook; the shake rides
the `eye`/`rows` vector; cockpit is already camera view 0; **we already have per-wheel motion blur**
(`race_view.c:1677,1942 wheel_blur`), so object blur has a precedent in-tree; our leaves/smoke systems can
inherit the same blur. The honest gap: Driveclub's per-object blur needs velocity, and our sim gives us
exact per-car pose deltas at 60 Hz — cheap to derive, no motion-vector buffer needed for cars.

### Cheapest-first order for the weekend

1. Chase FOV constant + eye offset (pure arithmetic, provable with a side-by-side capture pair).
2. Spring/damper camera over the guest's target, renderer-side (the guest state stays untouched).
3. Post pass in `plat_video_end_frame` (measure the frame time on/off and print it).
4. Shake, capped, separate toggle with a real zero.
5. Wind layer — blocked on the audio path, do not start it before step 1 ships.

---

## 2. MORE THAN SIX CARS — G6.10

### Correction to our own audit (important)

`docs/vulcan4/AUDIT-FIXED-CAR-COUNT.md` says *"the physics is already count-driven… the wall is six
pre-allocated entry blocks"*. **That is wrong, and this pass measured why.** The conductor has
**two more six-slot arrays, and they are exact**:

```c
/* init.c:1213 */  car_clear(car, cond, race, cond + 0x98 + i * 0x1100u);
/* init.c:1205 */  car = cond + CONDUCTOR_FIELD_CARS_U + i * CAR_SIZE;
```

| array | base | stride | six slots end at | what sits there |
|---|---|---|---|---|
| per-car block | `cond+0x98` | `0x1100` | `0x6698` | **`CONDUCTOR_FIELD_CARS`** |
| car state | `cond+0x6698` | `0x10A0` (`car.h:12`) | `0xCA58` | **`CONDUCTOR_FIELD_UNK_CA58`** (driver.c: force feedback) |

Both ends land **exactly** on the next field. A seventh car's state or per-car block writes straight
into the conductor's following data. So there are **three** walls, not one, and the biggest two are
not entry blocks — they are fixed offsets inside the original's session struct.

### The entry blocks (as the audit had them — re-verified)

`race_run.c:97` — six guest addresses, stride `0x35E0` (13,792 B):

- block A `0x01bd7710`, `0x01bdacf0` → next slot **`0x01bde2d0`** (headroom to prove unused).
- block B `0x01cd9190` … `0x01ce3330` → next slot `0x01ce6910`, which **is `WORLD`**. No headroom there.

Everything that counts cars is already a loop over `CONDUCTOR_FIELD_CAR_COUNT` (`0xF8A0`) — `ai.c`,
`driver.c`, `session.c`, `misc.c`, `race.c`, `step.c`. The count is not the problem.

### Our side

- The applicant gate is `race_game.c:1113` (`d->cars > 6`) and `prepared_others[5]` /
  `prepared_colors[5]` at `race_game.c:751-752` — five opponents by declaration.
- ~18 tables sized `[6]` (full list in the audit): `race_bspec.h:16` `RACE_BSPEC_CARS 6`,
  `race_result.h:19-28`, `race_game.c` grid/names/points, `career_original.h:138-141`,
  `race_audio.c:45,62,63` (`sequence[6]`, `backfire[6]`, `exhaust_count[6]`),
  `replay_file.c:316`.
- **The renderer is already big enough:** `race_view.c:46 #define OPPONENTS 7` → player + 7 opponents
  = 8 cars can be drawn today. That is the carrot and it costs nothing.

### Honest shape of the work

1. Widen our tables to a runtime count (no visual change, clamped at six — testable now).
2. Car #7 needs an entry block **and** a conductor side-car for the two 6-slot arrays, addressed through
   an accessor (`car_slot(cond, i)`) instead of `cond + 0x6698 + i*0x10A0` at every call site.
3. Prove the grid has a seventh slot, and that points/results/replay can carry it.
4. Only then raise the gate.

Point 2 is the real work and it is **not a weekend claim**. Point 1 is.

---

## 3. DISCORD INTEGRATION — what ARMSX2 actually did

### Their feature set (first-hand, from their own release notes)

- **2.6.5.8 (2026-07-28):** "Discord RPC is now built into ARMSX2. Link your Discord account from the
  Friends tab… including each game's cover art while in-game… The Friends tab shows which of your
  Discord friends are currently using ARMSX2, along with their avatar and the game they're playing…
  console-style notifications… completely opt-in… ARMSX2 does not create a separate account or operate
  an account or presence server… sends Discord the title and disc serial of the game you're running plus
  its cover-art address… authorization token is stored locally."
- **2.6.6:** RPC also shows RetroAchievements progress, the game's status line, unlocked count, Hardcore/Casual.
- **2.8 / nightly (Oct 2026):** remote play shows as *"Playing remote play (2 of 2)"*, and the person
  joining also gets RPC with the game's name and cover; "hosting no longer needs Discord"; Discord
  "checks in once a second instead of twenty" while backgrounded.
- **Their GPL answer, verbatim:** *"The Discord integration runs in a separate helper process and
  communicates with the emulator through a simple message interface. The GPL-covered emulator process
  never links against or loads Discord's proprietary SDK."* They vendor `3rdparty/discord-rpc`
  (the MIT open-source RPC library) and keep the SDK out of the GPL binary.

### What Discord's own docs say (this decides the design)

- **Presence alone needs no authentication**: set the Application ID, call `UpdateRichPresence`. That is
  the cheap path.
- **Account linking + Friends list requires the Social SDK** — a C++ library Discord ships *inside the
  game* — with OAuth2 scopes `openid` + `sdk.social_layer_presence`.
- **DMs/voice** are a separate, approval-gated tier. Presence + friends is the default scope set.
- RPC is supported on **desktop and Android (Social SDK 1.10+)**, not iOS/console/web.

> **Stale fact corrected:** `docs/DISPATCH-QUEUE.md:173` says Discord integration is "PC-only forever…
> Android gets nothing". That was true of the old IPC plan and is **not true of the Social SDK**.

### What "full on" costs, honestly

| tier | what the player sees | needs from Boss | needs code |
|---|---|---|---|
| Presence only | *Moon Engine — Tsukuba, 5/6* | **Application ID** | IPC client, ~300 lines, one file |
| + cover art | the game's art on the card | art uploaded to the app | asset keys |
| + account link / Friends | friends who are in Moon Engine, avatars, notifications | Application ID + OAuth redirect + (Discord review if friend-list UI) | Social SDK in a **helper process** (the ARMSX2 pattern) |

Two decisions are the captain's, not mine:

1. **Presence-only vs ARMSX2 parity.** Parity is a real sub-project: helper process, OAuth, covers,
   friends UI. Presence-only is a dish.
2. **Name on the card.** Law 16 says our surfaces say *Moon Engine*; GT4's identity appears only when the
   user's disc boots. So the app name is Moon Engine, and the course/car detail comes from the disc —
   that reading is consistent, but it is worth saying out loud before it ships.

Cannot be done without him: the Discord **Application ID** (Developer Portal). Everything else is code.

---

## 4. EXPERIMENTAL ENGINE SWAPS — G6.5

### The discovery: engine identity is ONE 8-byte key

Every table the record builder reads is a **key** in the garage setup block (`entry+0x20`, 0x178 bytes).
The engine's key is at **`setup + 0x38`**:

- `record.c:666` — `ROW(0x38, 0x50); engine(rec, s + 0x50);`
- `record.c:158-216` — `engine()` consumes that one row: **24 rpm points** (`0x1e..0x4c`), **24 torque
  values** (`0x55..0x6c`), redline scale (`0x6f`), turbo flag (`0x52`), engine-braking bytes.
- These become the `gt4_engine` block at **`params + 0x134`** (`car.h:216`): curve + `speed[24]` +
  `torque_nm[24]` + braking, 0x100 bytes.
- **The same row carries the sound id:** `race_audio.c:431` — `ENGINE row +0x18 soundNum`, read from the
  very same key. So the wish-list sentence *"engine = parameter block + sound identity"* is literally the
  data model. A row swap gives both.

### So the swap is (1) a write, (2) an argument

1. **Write** the donor car's 8-byte ENGINE key over `setup + 0x38` **after** the setup block is built
   (`race_run.c:536-560`, before `gt4_race_init` calls `gt4_car_record`) — torque curve, redline, engine
   braking and sound id all follow.
2. **Pass** the donor's car label to the audio path. `race_audio.c:444` opens the bank with
   `label = the car's own` + `id = the row's soundNum`; the donor's soundNum lives in the donor's folder,
   so the label has to travel with it. One extra argument, one call site.
3. **Optional, for induction:** the TURBINE row is a *separate* key (`setup+0xb0`, `record.c:527 turbo()`).
   A turbo↔NA swap needs that key too, or the swap must be documented as "engine only, induction stays".
4. **Leave the gearbox row (`+0x48`) alone** — GT7 keeps the gearbox when it swaps. Same here, for free.

### Fences and known mismatches

- The swap mapping lives in **our own config / extended content**, never written into the guest save —
  it is reversible by dropping the key, which is exactly `G6.5`'s "save-safe by construction" rule.
- Engine sounds come from the user's own disc (the donor's folder is on his disc). Nothing shipped.
- **Risk to measure, not assume:** the params *layout* index (`PARAMS_LAYOUT` `+0x030`) and the drivetrain
  block (`PARAMS_DRIVE` `+0x234`, incl. `DRIVE_ENGINE_INERTIA 0xc8` and `DRIVE_CLUTCH_RPM 0x2c`) come
  from the chassis/drivetrain, not the engine. Same-family swaps are safe; a rotary into a kei will hit
  those numbers first. That is why `G6.5` says same-family first — now there is a file:line for why.

### What a weekend version looks like

Start with **two same-family swaps** (e.g. NA→NA, similar displacement), in Arcade only, driven from our
own config. Prove: both cars drive, the tach reads the donor redline, the sound is the donor's, and
dropping the config returns the original engine byte-for-byte. That is `G6.5`'s DONE WHEN, minus the
menu.

---

## ORDER I WOULD DISPATCH (not dispatched — waiting on him)

1. **Engine swap** — smallest real feature, one key + one argument, no guest layout risk.
2. **Sense of speed step 1** — FOV + eye offset, arithmetic only, capture-pair proof.
3. **More than six step 1** — widen our own tables behind the six-car clamp; no guest memory touched.
4. **Discord** — blocked on the Application ID and on his presence-only vs parity call.

## What this pass did NOT do

- Did not run the game (no boot, no capture pair).
- Did not test a live Discord client.
- Did not attempt a seventh car or a swap in a running race.
- Did not read `race_view`'s 8-car draw path line by line beyond `OPPONENTS 7` and the
  `other_request[]` sizing — that is the next read if step 3 is approved.
