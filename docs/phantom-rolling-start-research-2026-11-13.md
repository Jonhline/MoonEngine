# How GT4 itself starts a field (research, 2026-11-13)

Source of truth: `src/core/physics/session.c` `race_start_at()` (guest func_00347618,
session method 0xF0) and its caller at `session.c:1177`.

## The rule the game uses

```
back  = (int16_t)cond[0xF884]     // START_DISTANCE  = -400 on Tsukuba
speed =        cond[0xF886]       // START_SPEED     = 0x50 (80)
gap   =        cond[0xF887]       // START_GAP       = 0x14 (20)

back += (car_count - entry[0x3444] - 1) * gap   // entry+0x3444 = grid slot
kind  = back >= 0 ? 1 : 2                      // 2 = behind the line
dist  = back
v0    = speed
```

Then the car is placed at `func_0034C518` = **line 6 at `dist`**, heading from the
same line. Not the AI racing line (line 6 *is* the course's own line), not the
player's rotation matrix.

So a 12-car field on Tsukuba starts at `-400 + (12 - slot - 1) * 20`:
slot 0 = -180 m, slot 11 = -400 m. Officials are slots 0..5; a phantom
appending behind them takes slots 6..11 and lands at -300..-400.

Measured in our build (2026-11-13):

| thing | value |
|---|---|
| START_DISTANCE | -400 |
| START_SPEED | 80 |
| officials' measured spacing | 19.9 m (≈ START_GAP 20) |
| line-6 parameter nearest the rearmost official | 1722 vs its course distance 1715 |
| player speed held in headless `--drive` | 22.28 m/s, constant (= 80 km/h) |

## What we get wrong today

1. **Every phantom shares one grid slot.** `race_run.c:418` writes
   `w32(entry + 0x3444, official - 1)` for all of them. The game's placement
   formula is driven by that slot, so six phantoms with one slot is not the
   game's formation.
2. **We place by hand instead of calling the game's method 0xF0.** Hand
   placement can match the geometry but never sets the state the spawn routine
   sets (start kind, timed-start state, pace).
3. **Pace is unmeasured.** In headless `--drive` the whole field is pinned at
   exactly START_SPEED (22.28 m/s for the entire run), so phantom speeds from
   that harness say nothing about a real race. Phantom throttle read 0.10-0.22
   against an official's 1.19 there, but the official's own speed never moved
   either.

## The authentic path

Create the phantom entries with their own slots (`official + i`), then call the
guest's own start routine per phantom:

```c
uint32_t kind, dist, speed;   /* guest scratch */
gt4_session(PHANTOM_COND, 0xf0, i, ADDR(&kind), ADDR(&dist), ADDR(&speed), 0, 0, 0);
```

and let it place them on line 6 at the formula's distance. That is the game
doing it, not us.

## Also confirmed

- AI driver profile for non-player entries = `0x00622b60` (`AI_PROFILE`), which
  we already set — same profile the officials use.
- `CAR_BLOCK_TIMED_START_STATE` (car+0x65B) is 1 for kind-2 starts and only
  affects lap timing (`laps.c:152`), not throttle.
