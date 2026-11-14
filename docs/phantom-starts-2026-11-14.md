# Phantom opponents — start behaviour, fixed 2026-11-14

Branch `experimental/more-than-6`, base commit `b50bcad`. **Working tree only, not committed.**
All changes are in `src/core/physics/race_run.c` (+267 / −111).

Boss's report at the start: on a rolling start the six phantoms were statically spawned
while the officials drove off; on a standing start fewer than six appeared and one was
off the track. Boss's report at the end: rolling works; standing phantoms line up behind
the player and launch without jumping onto the officials ("fine for now").

---

## 1. Rolling start — fixed

### What was wrong
`init_car()` (init.c:1160) already calls the guest's own start routine, session method
`0xF0` → `race_start_at()` (session.c:1031), and feeds kind/dist/speed into
`gt4_car_init()` (init.c:1181-1183). `gt4_car_init` applies the game's rolling placement
(`rolling_start`, init.c:687-701, `gt4_car_place`) and the field's start speed
(init.c:894-898, `car_rolling`). So the authentic path was already running for phantoms
through `gt4_race_init_extra_car()`.

Two things then erased it:
1. `make_phantoms()` hand-placed every phantom from the officials' rows AFTER init.
2. `gt4_race_run_frame()` called `place_phantoms()` and, on rolling starts,
   `phantom_hold_start()`, which zeroes `CAR_SPEED` / `CAR_ABS_SPEED` / `CAR_VELOCITY`
   and restores the hand pose (race_run.c:2684-2692 in the old numbering).

### Measured before (Cortex and Baki identical)
```
f=0    officials dist 1715.4..1815.0   phantoms 1695.9..1596.2
f=360  officials       1858.4..1881.0   phantoms 1715.3..1606.7   (creeping ~3 m/s)
ai thr f=0: phantom cars req=1.00, officials 1..5 req=0.01        (AI was not refusing)
```

### The fix
- `rolling` is latched at spawn into `r->phantom_rolling_start`; the frame loop uses the
  latch, not the live clock. This matters because `CONDUCTOR_FIELD_CLOCK_STOPPED_U`
  (0xCBE4) is CLEARED when the field crosses the start line (session.c:1085), and the old
  code then re-ran the standing path mid-race (re-place + speed zero) — that was the
  "they jump forward and stop" bug.
- The phantom conductor's per-car start tables are cleared so `0xF0` reaches its own grid
  formula instead of returning the officials' starts copied from `COND`:
  `memset(PHANTOM_COND + 0xF866, 0, 6)` (speeds, session.c:788) and
  `memset(PHANTOM_COND + 0xF86C, 0, 12)` (distances, session.c:785).
- `CAR_COUNT` stays `p` (6). It is NOT set to `official + p`: with slots `official+i`
  (6..11) and count 6 the formula gives `-420..-520`, i.e. 20..120 m behind the player.
  With count 12 it gives `-300..-400`, which is exactly where the officials already are
  (the player starts last), so the phantoms overlapped them.
- `place_phantoms()` and `phantom_hold_start()` are not called for rolling. The hand
  placement block in `make_phantoms()` was deleted.

### Verified (Tsukuba, field 12)
```
race start: rolling=1 start_speed=80 start_distance=-400
phantom start: i=0 slot=6 kind=2 dist=-420 speed=80
phantom start: i=5 slot=11 kind=2 dist=-520 speed=80
phantom pace: f=0 i=6  speed=22.21 thr=1.00 dist=1695.3 lap=0 off=0 marked=0
phantom pace: f=0 i=11 speed=22.21 thr=1.00 dist=1594.8 lap=0 off=0 marked=0
phantom pace: f=120 i=6 speed=22.79 thr=1.00 dist=1740.2
phantom clock: cleared at f=745 (latched rolling stays on)
post-line progress keeps growing for all six: f=900 2049..1991 -> f=1200 2170..2108 -> f=1500 2292..2236
```
Nürburgring (rolling=1, start_speed=60) gives the same shape: phantoms -520..-620,
f=0 speed 16.65, post-line growth for all six.

Evidence logs: `/tmp/gt4work/roll10.log`, `/tmp/gt4work/roll11.log`,
`/tmp/gt4work/roll5.log`, Baki `C:\MoonEngine\phantom-play.log`.

---

## 2. Standing start — fixed (single file behind the player)

### What was wrong
The course publishes only six grid slots (`phantom grid: course=grandvalley_east slots=6`,
measured with `MOON_PHANTOM_NATIVE=1`). The guest's `0xF0` therefore returns kind 0 (grid)
and `gt4_car_init` places on `grid_slot(6..11)`, which reads past the six-entry table and
aliases slots 0..5 — the officials' positions.

The old hand placement hid that with a line-6 pose, but only in `CAR_POSITION`. The car's
wheels, ground and travel still held the init (aliased) pose. While the countdown held the
cars, `phantom_hold_start()` rewrote `CAR_POSITION` every frame; at release the step
resolved to the init pose and the car snapped onto the officials' grid. That is the jump
Boss saw: "before the race starts they are off the grid on a different part of the track
and when the race starts they spawn where the normal opponents are."

### The fix — give the guest a 12-slot grid
In `make_phantoms()`, before the phantom init loop:
- read the course grid list: `rw = GT4_U32(GT4_U32(COURSE + 4) + 0xdc)`, `v = COURSE+0xB0`,
  `ptrloc = v < 0 ? rw + 0x5c : GT4_U32(rw + 0x9c) + v*32 + 0x18`, `list = GT4_U32(ptrloc)`;
- copy the `n` real entries (16 bytes each) to `PHANTOM_SCRATCH + 0x900`;
- author entries `n..official+p-1` along the course's own line 6: find the line-6 param
  nearest the last real slot, use the world gap between the last two slots as the pitch
  (clamped 5..12 m), and step back one pitch per slot;
- entry mapping (measured): `+0 = world x`, `+4 = line height`, `+8 = render z`,
  `+0xC = heading − π` (`grid_heading()` adds π);
- repoint `ptrloc` at the extended table. Entries `0..n-1` are byte-identical, so
  officials and every other consumer see the same data. Refuses safely when
  `list == 0`, `n < 2`, `n > 64`, or the table would exceed 0x200 bytes.

The hand placement block and the `place_phantoms()` call were removed. The pose now comes
from `gt4_car_init` alone, so the hold pose, the wheel/ground state and the launch pose are
the same object.

### Verified (Grand Valley East, field 12)
```
phantom grid table: slots=6 -> 12 at 01e30900 (v=-1) pitch=8.00 base=2960.0
f=0    phantoms x = 15.2  6.9  -0.8  -8.8 -17.1 -25.7   (player x=23.66)
f=480  identical (held)
f=660  x = 15.9  7.8   0.2  -8.4 -15.8 -24.2            (launch, continuous)
f=690  x = 16.7  8.9   1.7  -7.9 -14.0 -22.2
f=720  x = 17.9 10.5   3.7  -7.0 -11.6 -19.3
f=750  x = 19.4 12.6   6.2  -5.7  -8.6 -15.7
delta 690->750 = 2.7 3.7 4.6 2.1 5.4 6.4 m   (acceleration, no teleport)
```
`drawn=6`, ids 6..11 distinct, `off=0 marked=0` at f=0, post-launch advances all > 0.

Evidence: `/tmp/gt4work/stand17.log`, Baki `C:\MoonEngine\phantom-stand.log`.

---

## 3. Known limitations (not hidden)

- **Standing phantoms are single file behind the player**, not a 2-wide grid. Boss:
  "they're lined up behind the player. that's fine for now." The extended slots use a fixed
  8 m pitch along line 6 with no lateral offset; a proper 2-wide row needs the real grid's
  lateral step, which was not measured.
- The extended grid pointer now points into `PHANTOM_SCRATCH` for the rest of the race.
  Entries 0..5 are identical, and `course_grid_count` is unchanged, so nothing that reads
  the six real slots is affected. Not exercised: any code that reloads or validates the
  course grid pointer mid-race.
- `place_phantoms()` is now dead code (kept in the file, no caller). Compiler does not warn
  at the current flags. Remove it in the next pass.
- The init-time measurement prints (`grid entry:`, `car 0 pos:`, `line6 nearest slot5:`,
  `phantom grid slot:`) are still unconditional. They are useful while tuning; gate them
  behind `MOON_PHANTOM_POS_TRACE` before a release build.
- No commit was made. Working tree only.
- Windows build used for the Baki test:
  `/mnt/ssd/moon-win-gui-roll/MoonEngine.exe` (GUI shape), stripped to
  `/tmp/MoonEngine-deploy4.exe` (14,575,616 B), deployed to `C:\MoonEngine\MoonEngine.exe`.
  Previous exe backed up as `C:\MoonEngine\MoonEngine.exe.bak-b50bcad`.

---

## 4. How to reproduce the tests

Cortex (headless, 1280x960, DISPLAY=:0):
```sh
XDG=/tmp/gt4work/test; mkdir -p "$XDG/MoonEngine/MoonEngine"
printf 'preset=Performance\nrace.phantoms=1\nrace.field_size=12\ngraphics.vsync=0\n' \
  > "$XDG/MoonEngine/MoonEngine/options.ini"
DISPLAY=:0 XDG_DATA_HOME="$XDG" SDL_AUDIODRIVER=dummy MOON_WINDOW_HIDDEN=1 \
MOON_PHANTOM_POS_TRACE=1 MOON_AI_THROTTLE_TRACE=1 timeout 30 \
./build-linux/MoonEngine "/mnt/ssd/gt4/Gran Turismo 4 (USA) (v2.00).iso" \
  --renderer vulkan --drive --race-course grandvalley_east \
  --config-file "$XDG/MoonEngine/MoonEngine/options.ini" --window 1280x960
```
Drop `--race-course` (or use `tsukuba`) for the rolling start.

Traces printed by `MOON_PHANTOM_POS_TRACE=1`:
- `race start: rolling=… start_speed=… start_distance=…`
- `phantom start: i=… slot=… kind=… dist=… speed=…` (the guest's own 0xF0 result)
- `phantom clock: cleared at f=… (latched rolling stays on)`
- `phantom pace: f=… i=… kind=… speed=… thr=… dist=… lap=… off=… marked=…`
- `phantom xyz: f=… i=… kind=… x=… z=… dist=…`
- `phantom grid table: slots=… -> … at … (v=…) pitch=… base=…`
- `pos table: f=… id=… …` (merged classification)
- `phantom draw: submitted=… drawn=… frame=…` and `hud map: field=… dots=…`

Baki: scheduled tasks `moon_phantoms2` (rolling, Tsukuba, `start-phantoms.bat` →
`C:\MoonEngine\phantom-play.log`) and `moon_phantoms_stand` (standing, grandvalley_east,
`start-phantoms-stand.bat` → `C:\MoonEngine\phantom-stand.log`). Neither bat uses `--shot`
any more: `--shot` exits the game after the capture, which killed Boss's live session.

---

## 5. What was not verified

- A real screenshot of the standing grid with all six phantoms in frame (the intro camera
  shows only two cars at Grand Valley; the on-road evidence is the position trace and
  `off=0`).
- A 2-wide standing formation (single file was accepted for now).
- Any course other than Tsukuba/Nürburgring (rolling) and Grand Valley East (standing).
- Long races / pit stops / finish classification with the new grid pointer.
