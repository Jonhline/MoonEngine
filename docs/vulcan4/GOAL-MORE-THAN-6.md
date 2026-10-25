# GOAL — MORE THAN SIX OPPONENTS (measured 2026-10-24)

Captain: *"figure out how to do more than 6 opponents. I'll playtest later."*
This file is the durable goal. Measurements below were taken from the running
engine with a temporary probe (`MOON_RACE_DUMP`), then the probe was reverted —
the tree is clean and nothing here is a shipped change.

## 1. WHAT IS MEASURED (live race, Nürburgring, 6 cars)

```
cond    = 0x01CF8ED0        (the conductor)
race    = 0x01C3F620       (GT4_U32(cond))
entries = 0x00A47CB0       (GT4_U32(race+0x60))
count   = 6                (GT4_U32(entries))
```

Entry pointers, `GT4_U32(entries + 0x24 + i*4)`:

| slot | pointer | stride from previous |
|------|---------|----------------------|
| 0 | 0x01BD7710 | — |
| 1 | 0x01BDACF0 | 0x35E0 |
| 2 | 0x01CD9190 | 0x1004A0 (not contiguous — another structure sits between) |
| 3 | 0x01CDC770 | 0x35E0 |
| 4 | 0x01CDFD50 | 0x35E0 |
| 5 | 0x01CE3330 | 0x35E0 |
| 6 | **0x00000000** | table slot exists, empty |
| 7 | **0x00000000** | table slot exists, empty |

**Finding 1 — the entry table is NOT the wall.** It holds at least 8 pointer
slots; only 6 are filled.

**Finding 2 — a 7th entry block has somewhere to live.** Slots 2..5 are
contiguous at stride 0x35E0, so a 7th block is `0x01CE3330 + 0x35E0 =
0x01CE6910`. Probed at race start AND after 45 s of racing: **all zeros, nothing
writes it.** The conductor starts at 0x01CF8ED0, leaving ~0xF9E0 of gap above
0x01CE9EF0 — room for several more entry blocks.

**Finding 3 — the real wall is INSIDE the conductor.** Three packed arrays,
all exactly six deep:

| offset | stride | cars | ends at |
|--------|--------|------|---------|
| cond+0x98 | 0x1100 | 6 | 0x6698 |
| cond+0x6698 | 0x10A0 | 6 | 0xCA58 |
| cond+0xCA58 | 0x0010 | 6 | 0xCAB8 |

`cond+0xCA58` is **live during a race** (`00800000` at start → `00890000` after
45 s) and is followed immediately by floats (`0xCAB8` = 0x3F97D846 …). So a 7th
car's state block would start exactly on top of a live field, and everything
after 0xCA58 shifts by 0x10A0.

**Finding 4 — the simulation loops are count-driven.**
`ai.c:1717 NCARS(c) = GT4_U32(c + 0x10000 - 0x760)` and
`cond + 0x6698 + i * CAR_SIZE`; nothing in the physics hard-codes six. The
limit is layout and data, not logic.

## 2. WHY IT IS HARD

The recompiled guest code reads those arrays with **constant offsets from the
conductor base**. Widening the first array (`0x1100 × N`) pushes everything
down, and the guest's own instructions still say `+0x6698`. So there are only
three honest strategies:

### A. Widen the conductor and teach every reader the new offsets
Build the conductor in an arena we own, place the arrays at the new offsets
(`0x98 + N*0x1100`, then states, then the 0x10 array, then the floats), and
patch **every** reader of the old constants. Cheap only if the number of
readers is small — that is the next measurement.
*Risk: any unpatched reader silently reads the wrong car. Corruption, not a
crash.*

### B. Extra cars outside the conductor ("phantom opponents")
Keep the official field at six. Add N extra cars driven by our own AI loop and
rendered like the others, excluded from the official results and the conductor.
*Cheap, safe, and it delivers what a player sees — more cars on track.*
*Cost: they cannot affect official placings or prize logic, and traffic
behaviour near them is our AI's job.*

### C. A second conductor for the extra cars
Instantiate a second conductor with its own six slots, run it at the same
course, and merge the two fields for drawing and for traffic.
*Middle cost. Two conductors means two sets of timers/positions to reconcile.*

## 3. NEXT MEASUREMENTS (in order, each one decides the next)

1. **Count the readers** of `+0x6698` and `+0x98` in the recompiled guest: how
   many call sites use the constants? If it is tens, strategy A is out; if it
   is a handful we control in C, A is viable.
2. **Map the 0xCA58 array.** Six 0x10 entries — what are they (positions?
   timers?) and who writes them. That decides how much has to shift.
3. **Free space above the conductor.** Scan from 0x01CF8ED0 forward for the
   first other live structure; sizes the arena for strategy A.
4. **Grid slots per course** (`init.c:98-101`): does every course publish a 7th
   grid slot, or is the feature course-limited?
5. **Replay format** (`replay_file.c:281,316`): a >6-car race must refuse to
   record with a visible reason rather than write an unloadable file.

## 4. OUR OWN CEILINGS TO WIDEN IN THE SAME PASS

- `race_bspec.h:16 RACE_BSPEC_CARS 6`, monitor arrays `race_bspec.c`
- `race_game.c:754-755 if (nothers > 5) nothers = 5`, `last_colors[5]`,
  `for (i < nothers && i < 5)`
- `main.c:4838 menu_race_entries(..., 5)`
- `career_event.c:240 grid.cars = 6`
- `race_result.c` `rows[6] / order[6] / done_car[6] / timed[6]`
- `race_hud.c:1156` grid letters
- renderer already allows 8 (`race_view.c:46 OPPONENTS 7`)

## 5. DEFINITION OF DONE

A race with **7 cars (6 opponents)** runs on PC: seven cars start, seven are
drawn, seven finish, results and HUD show seven rows (the grid auto-scrolls —
see `~/.hermes/recipes/moon-grid-autoscroll.txt`), and a replay of that race
refuses to record with a clear message instead of writing a broken file.
Captain playtests after that.

## 6. STATUS

- 2026-10-24: layout measured, four walls mapped, probe reverted, tree clean.
  Nothing shipped.
- 2026-10-24 (measurement 1 done): the guest code is the shipped MIPS binary
  loaded from CORE.GT4 (no C source to grep), so the readers were counted by
  scanning the image for `addiu/addi` with those immediates:
  - `0x6698` (cars base): **3 sites** — 0x1572B4, 0x34BF54, 0x34C1AC
  - `0x1100` (car stride): **9 sites** (some unrelated)
  - `0x10A0` (state stride): **5 sites** — 0x343AD8, 0x34BF7C, 0x3F854C,
    0x3F8620, 0x3F8680
  - `0xCA58` (after states): **3 sites** — 0x14FDF0, 0x2D0CB0, 0x2D43B8
  Only a handful of places COMPUTE those bases (the rest inherit a register
  already offset), which makes strategy A (widen + patch) far cheaper than the
  earlier estimate.
- 2026-10-24 (measurement 1 answered): all three `0x6698` sites are genuine
  base computations, disassembled from the image:
  - `0x1572B4  addiu t2,t2,0x6698`
  - `0x34BF54  addiu s0,s2,0x6698` (then indexes `+0x6DC`, `+0x9A0`, …)
  - `0x34C1AC  addiu v0,v0,0x6698` (then `lbu -1946(v0)` = the `+0x6698-0x79A` byte)
  Nothing exotic; each one makes the array base once and everything else is
  register + smaller offset. **Strategy A is viable.**

### The concrete patch for 7 cars

Keep both strides exactly as they are and only move the two bases:

| array | now | 7 cars |
|-------|-----|--------|
| per-car blocks (6 × 0x1100 from 0x98) | states at 0x6698 | blocks end 0x98+0x7700 → states at **0x7798** |
| car states (6 × 0x10A0) | 0xCA58 array | states end 0x7798+0x49C0 → **0xC158** |

So 7 cars = rewrite `0x6698 → 0x7798` at 3 sites and `0xCA58 → 0xC158` at
3 sites, plus our own `CONDUCTOR_FIELD_CARS` and friends. Strides 0x1100/0x10A0
stay untouched. Room to 0xC158+0x10*7 is well inside the conductor.
Remaining risk: code that assumes the states follow the blocks contiguously
without computing 0x6698 at all — the next scan.


