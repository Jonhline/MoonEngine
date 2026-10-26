# PHANTOM OPPONENTS — BRICK 1

Strategy B from `GOAL-MORE-THAN-6.md` §2: extra cars outside the conductor,
driven by the game's AI and drawn by the game's renderer. The official six
stay the conductor's six.

## STEP 0 — CAN A PHANTOM BE DRAWN?

**Yes.** `race_view.c` never reads the guest `entries` table or
`prepared_others[]`.

- `race_view_set_opponents(v, cars, colors, n)` copies labels into
  `other_request[OPPONENTS]` and sets `other_count`.
- `load()` builds `a->other[i]` for `i < other_count && i < OPPONENTS`.
- `race_view_set_opponent(v, i, pos, rot, wheels)` publishes a pose at index
  `i`; the draw loop walks `i < OPPONENTS` and draws
  `now->other[i]` when `other_pose[i].on`.
- No conductor entry is consulted. The only limit was `OPPONENTS = 7`.
  It is now `MOON_MAX_OPPONENTS` (11), so a phantom is appended by passing
  one more label and publishing one more `race_view_set_opponent` index.

## STEP 1 — THE PARAMETER

`src/core/race_limits.h`:

| name | value | reason |
|---|---:|---|
| `MOON_CONDUCTOR_CARS` | 6 | guest blocks + states + `0x10` array are six deep; widening is the rewrite in `MEASURED-TAIL-SITES.md` |
| `MOON_MAX_CARS` | 12 | six official + six phantoms; renderer capacity, our arrays, replay official-six limit |
| `MOON_MAX_OPPONENTS` | 11 | `race_view.c` opponent arrays |
| `MOON_MAX_PHANTOMS` | 6 | phantom slots in the second conductor |

Request: `race.field_size` (config) or `MOON_RACE_FIELD_SIZE` (test override),
validated `1 … MOON_MAX_CARS`. Refusals print value, range and reason. A
missing key is six. The menu asks for `field_size - 1` opponents, capped at
five official opponents; above six the rest are phantoms.

## STEP 2 — THE PHANTOM CONDUCTOR

Addresses in guest RAM, above the measured zero tail:

| object | address |
|---|---|
| phantom conductor | `0x01D10000` |
| phantom race object | `0x01D21000` |
| phantom entries table | `0x01D22000` |
| phantom entry blocks | `0x01D23000` |
| phantom world copy | `0x01D40000` |
| phantom scratch | `0x01E30000` |

The real conductor (0x10144 bytes), race object and world are copied once.
The shadow gets its own car blocks/states, its own `CAR_COUNT`, position
order, per-player queues and pair tables. It is stepped every frame by the
same `gt4_step()` path as the official conductor. Its entries point at cloned
phantom entry blocks; its cars' `CAR_SESSION` points at the shadow, never the
official conductor.

Phantoms start as a second row behind the last official grid slot, offset
2.5 m left/right, so they do not claim a guest grid slot. They use the same
AI driver/physics functions and the same `race_view` model/pose path.

Exclusions:

- `CAR_COUNT` stays 6; results, prize and points read the official entries.
  The live rank `+0x5B3` is the exception: after both steps a merge pass
  orders all official and phantom cars and writes each car its own merged
  place. `CAR_COUNT`, entries, results and prize are not touched.
- The phantom race object has its own entries table, so its finish data stays
  in the shadow.
- A replay refuses to record or play a race with phantoms; the replay format
  records the official six only.
- Official-vs-phantom contact is off by construction (separate conductors).
  Phantom-vs-phantom contact is on inside the shadow. This is the simplest
  safe behaviour.

## G1 — DEFAULT SIX CARS IS BYTE-IDENTICAL

`race_inspect_probe`, Nürburgring, `ka_01`, 6-car field, 120 and 600 frames.
The state value is `mz_crc32` of the whole 32 MiB guest RAM.

| build | frames | state CRC | draws | triangles |
|---|---:|---|---:|---:|
| parent (`build-native`) | 120 | `fa2b3669` | 74693 | 3709243 |
| this branch | 120 | `fa2b3669` | 74693 | 3709243 |
| parent (`build-native`) | 600 | `b28c9af8` | 550981 | 29064029 |
| this branch | 600 | `b28c9af8` | 550981 | 29064029 |

Same CRC, same draw count, same triangle count.

## G2 — EIGHT CARS ON NÜRBURGRING

Run: 6 official + 2 phantoms, 18,000 frames (5 minutes at 60 Hz), no errors.

```
race: field_size=8 official=6 phantoms=2
phantom run: start cars=6 count=6 phantoms=2 crc=81cdc56f/fbac3e44/2f3976e6
phantom run: end   cars=6 count=6 phantoms=2 crc=81cdc56f/949ab964/632f5541 good=1
```

The three CRCs are the official blocks (`cond+0x98`, 6×0x1100), states
(`cond+0x6698`, 6×0x10A0) and `0x10` array (`cond+0xCA58`, 6×0x10).

- `CAR_COUNT` is 6 before and after.
- All six official state blocks still have `CAR_SESSION == cond`.
- Both phantom cars have a session that is not `cond`.
- The `0x10` words stay in the measured range: `0x00750000` for entry 0,
  `0x00800000` for entries 1–5.
- Dumps: `/mnt/ssd/tmp/rank-8-final-phantom-before.bin` and
  `/mnt/ssd/tmp/rank-8-final-phantom-after.bin` (51,744 bytes each).

Screenshot: `/mnt/ssd/studio/phantom-8-nurburgring.png`.
The start camera frames six of the eight; the extra pair is in the pack
(duplicate labels chosen from the official opponents). A wider frame was not
obtained from the start camera; the 8-car run is proven by the pose count
(8), the phantom count (2), and the 600-frame draw delta below, not by all
eight fitting one start-camera frame. This is the honest limit of this brick.

## LIVE RANK — THE HUD FOLLOWS THE WHOLE FIELD

`merge_positions()` in `race_run.c` runs after the official and phantom
`gt4_step()` calls when `phantom_count > 0`. It tags each official car with
`cond` and each phantom with its own `CAR_SESSION`, sorts with
`gt4_position_order_cross()` in `session.c` (insertion sort, same shape as
`race_positions`), and writes only each car's `+0x5B3`. The official-only
`position_order()` and `race_positions()` are unchanged.

Gates:

- 8 cars: ranks `1..8`, player 8th; first frame above 6 is 1758
  (`player_rank=7`).
- 12 cars: ranks `1..12`, player 12th; phantoms hold 1st and 2nd.
- HUD two-digit path: `draw_rank()` now builds `"10".."12"` with
  `snprintf("%d")` and scales the pair uniformly inside the 42x45 box.
  Ranks 1..9 keep the original one-figure path, font, position, clip and
  roll. No rank is clamped.
- Live screenshots (path only):
  `/mnt/ssd/studio/rank-live-8-rank-8.png`;
  `/mnt/ssd/studio/rank-live-12-rank-10.png`;
  `/mnt/ssd/studio/rank-live-12-rank-12.png`.
- Isolated HUD render at rank 10:
  `/mnt/ssd/studio/rank-hud-unit-hud-rank-10.png`.

**Results contradiction:** the finish board still builds six official rows
(`rs.cars = 6`). If the player finishes 7th of 8, the HUD says 7/8, but the
board has no phantom rows and the player is the last official row (6th of
six). That contradicts the HUD. It is left for the results/score brick.

## G3 — LINUX BUILD

`cmake --build /home/or/moonengine/build-linux -j4` completes.

One-line 8-car Nürburgring launch:

```
MOON_RACE_FIELD_SIZE=8 /home/or/moonengine/build-linux/MoonEngine "/mnt/ssd/gt4/Gran Turismo 4 (USA) (v2.00).iso" --race-course nurburgring --race-car ka_01 --drive
```

The config form is `race.field_size=8` in the options store; the environment
variable is the test override.

## G4 — MEASURED CEILING

`race_inspect_probe`, Nürburgring, `ka_01`, 600 frames, 1280×960 OpenGL,
hidden window. `update` is the physics/AI step, `render` the scene, `total`
the frame.

| field | phantoms | update ms | render ms | total ms | draws/frame | tris/frame | p99 ms |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 6 | 0 | 0.825 | 4.649 | 5.506 | 918.3 | 48,440 | 15.07 |
| 8 | 2 | 1.109 | 5.786 | 6.924 | 1,067.8 | 56,309 | 18.15 |
| 12 | 6 | 1.620 | 7.709 | 9.376 | 1,313.7 | 67,515 | 24.69 |

Per phantom, measured:

- AI/physics: **+0.13–0.14 ms/frame**.
- Render: **+0.51–0.57 ms/frame**.
- Draws: **+66–75/frame**.
- Triangles: **+3.2–3.9k/frame**.

Tested ceiling: **12 cars (6 phantoms)**. It averages 9.38 ms/frame on this
OpenGL path, but p99 is 24.7 ms, so 12 cars is not a locked 60 fps; 8 cars
averages 6.92 ms with p99 18.1 ms. Six phantoms is the current capacity, not
a CPU wall found by further scaling.

## COMMITS

- `255e76e` — `race.field_size` and the 12-car capacity.
- `5955e33` — phantom conductor, AI step and renderer poses.
- `7aa5e54` — phantom gates and measurements.
- The live-rank commit follows this document update.
