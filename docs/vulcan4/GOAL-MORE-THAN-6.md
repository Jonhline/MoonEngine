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
- 2026-10-24: that scan is done. Every 16-bit immediate `0x6698` in the image
  was classified: 3 `addiu` base computations (above) and 3 `sw` that are
  **unrelated absolutes** — `sw v1,0x6698(v0)` with `lui v0,0x6d / 0x82 / 0x83`
  = writes to 0x006D6698, 0x00826698, 0x00836698. No load reads the state array
  relative to a bare pointer without a base computation. Nothing else claims
  the constant.
- Our own six-ceilings located: `main.c:5900` (menu_race_entries … 5),
  `race_game.c:970-971` (nothers > 5), `race_game.c:261 last_colors[5]`.

## 7. CAPTAIN'S DIRECTION (2026-10-24): VERSATILE, NOT SEVEN

*"Adjust it to be versatile. Even up to 50-100 opponents… if we do 7 opponents
instead of 6, we work more and more to give u the ability to do 10-12."*

That kills the fixed-stride patch as the long-term design, because of two hard
numbers:

1. **16-bit offsets.** Guest instructions reach a field with `addiu rt,base,imm`,
   and `imm` is 16 bits. Today `CAR_COUNT` sits at `0x6698 + 6*0x10A0`. At
   7 cars the tail shifts by `0x21A0` (fine), but at 12 cars it shifts by
   `0xE000`, at 16 by `0x12C00` — past what one instruction can address, so the
   compiled code cannot express it at all.
2. **Per-car cost.** block `0x1100` + state `0x10A0` = `0x21A0` = 8,608 B per
   car. 100 cars = ~860 KB, plus O(N²) contact work and 100 cars of AI, on a
   32 MB guest.

### The design that scales (three tiers, one field-size value)

- **Tier 0 (N ≤ 6):** today's layout, untouched. Byte-identical.
- **Tier 1 (7 ≤ N ≤ 12):** the shift, done as a *computed* constant:
  `tail_shift = (N-6) * 0x21A0`, applied by the verified site table. Fits in
  16 bits; no pointer indirection.
- **Tier 2 (N > 12):** cars move OUT of the conductor into a **car pool** in
  free guest RAM, and the conductor holds a pointer per car instead of a
  contiguous stride. The few readers that compute `base + i*stride` are
  redirected through the pool (a lookup function in C + a patched accessor in
  the guest). This is the only shape that can reach 50-100 without running out
  of addressable offsets.

So the deliverable is not "7 cars" — it is **one field-size variable** that
selects the tier, with 6, 8, 10, 12, then pool mode behind it. Every tier keeps
the same gates: 6 cars byte-identical, then N cars start/finish/score/show N
rows.

Honest ceiling: 10-12 will race properly. 50-100 is chaos mode — it will run,
it will thrash the physics and the HUD, and it is a stress test, not a
simulation.


## 6c. CORRECTION — THE FIRST PATCH PLAN WAS WRONG (measured same night)

The conductor is not "three arrays then free space". Everything from `0xCA58`
upwards is a dense tail of named fields (`CONDUCTOR_FIELD_COURSE 0xCBD8`,
`PAIR_LAP_GAPS 0xCCC8`, `LINES 0xDA50`, `RULES 0xF818`, `CAR_COUNT 0xF8A0`, the
vtable at `0x10140`, …). A 7th car needs one more block (`0x1100`) **and** one
more state (`0x10A0`) inserted mid-struct, so **the whole tail shifts by
`0x21A0`** — the guest reads those fields by fixed offsets too.

Measured patch surface (addiu/addi sites whose immediate equals a named field
above `0x6698`): **38 sites across 12 fields**, plus the 3 car-base sites.

| field | sites |
|-------|-------|
| `0xCA58` CA58 | 3 (but see below) |
| `0xCAB8` CAB8 | 1 |
| `0xCBD8` COURSE | 5 |
| `0xCBE0` COURSE_FLAG_14 | 1 |
| `0xCCC8` PAIR_LAP_GAPS | 2 |
| `0xDA50` LINES | 5 |
| `0xF818` RULES | 1 |
| `0xF830` UNK_F830 | 10 |
| `0xF858` ALL_CAR_FINISH | 1 |
| `0xF860` UNK_F860 | 4 |
| `0xF898` RACE_CLOCK | 1 |
| `0xF8A0` CAR_COUNT | 4 |

**The trap:** matching on the immediate alone is unsafe. The three `0xCA58`
sites are `lui 0x15/0x6a` + `addiu …,-13736` = absolute addresses `0x0015CA58`
and `0x006ACA58`, not conductor offsets. A blind `+0x21A0` rewrite would corrupt
unrelated code. Every one of the 38 sites must be traced to a conductor-based
register before it is patched, and the patcher must refuse if the expected
instruction word does not match.

Status: machinery not built yet; probe reverted, tree clean, 6-car behaviour
untouched.


1. **Guest constants (7 cars).** At load time, rewrite the three `addiu`
   immediates `0x6698 → 0x7798` (0x1572B4, 0x34BF54, 0x34C1AC) and the three
   `0xCA58` bases `→ 0xC158` (0x14FDF0, 0x2D0CB0, 0x2D43B8). Guard: if the
   image at those addresses does not match the expected instruction word,
   refuse to patch and say so — never write a different disc's code.
2. **Our structures.** `CONDUCTOR_FIELD_CARS` 0x6698 → 0x7798, the `0xCA58`
   definition → 0xC158, `RACE_BSPEC_CARS` 6 → 7, arrays sized 7, clamps in
   `race_game.c` / `main.c` raised to 6 opponents, results/HUD to 7 rows.
3. **Seventh entry.** Fill `entries[6]` with a block at `0x01CE6910` (measured
   free), profile/grid fields set the same way `make_race` does for slots 1-5.
4. **Gates.** (a) 6-car race must be byte-identical to today. (b) 7-car race:
   seven cars start, seven finish, results show seven rows, no conductor
   corruption after 5 minutes. (c) replay refuses >6 with a visible message.
5. **Only then** the auto-scrolling grid (`~/.hermes/recipes/moon-grid-autoscroll.txt`).



