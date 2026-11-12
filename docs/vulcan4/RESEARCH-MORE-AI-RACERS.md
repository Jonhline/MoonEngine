# RESEARCH — MORE AI RACERS THAN THE GAME'S SIX

Captain's question (2026-10-19): *"how possible it is to add more ai racers in a race?"* — then: *"research
it. Make it a pc only thing for now. I'll dispatch a playtester once we finish."*

Everything below is MEASURED in this tree or read out of the guest's own structures. Nothing here is a guess
dressed as a plan.

---

## The verdict in one line

**Possible, but it is the deepest change on the list: the six-car field is baked into the ORIGINAL GAME's
own data, not just into our C.** Our own layers are easy to widen; the guest's entry list, its grid-side
table and the replay validator are the real wall.

---

## Where six actually lives (measured, file:line)

### A. OURS — plain C tables sized 6, cheap to widen

| place | what is 6 |
|---|---|
| `src/core/race_bspec.h:16` | `RACE_BSPEC_CARS 6` — the B-Spec/Race Monitor car array |
| `src/core/race_bspec.h:39`, `race_bspec.c:40-42,99,161,203-209,284-326` | `car[6]`, `lap[6]`, `best[6]`, `best_mark[6]`, `shown[6]` |
| `src/app/race_game_bspec.inc:153-155` | hard clamp: `if (n > RACE_BSPEC_CARS) n = RACE_BSPEC_CARS;` and `rank[6]` |
| `src/core/race_result.c:32,35,36,347` | arcade board `rows[6]`, `done_car[6]`, `timed[6]`, `order[6]`, plus `at < 6` bounds checks |
| `src/core/race_hud.c:1156` | HUD grid-letter row `const int letter[6]` |
| `src/core/physics/race_run.c:76` | `float height[6], travel0[6][4]` (start-of-race per-car state) |
| `src/app/race_game.c:242,996` | `last_colors[5]`, `for (i < nothers && i < 5)` |

These are mechanical: widen the constant, the loops and the bounds checks, and update the tests that pin
them (`tests/gtsuite` slices touch the B-Spec monitor).

### B. THE HARD CLAMPS — our code choosing five opponents

| place | what it does |
|---|---|
| `src/app/main.c:4838` | `menu_race_entries(mn, others, other_colors, 5)` — asks the menu for five opponents |
| `src/app/race_game.c:754-755` | `if (nothers > 5) nothers = 5;` |
| `src/core/career_event.c:240` | `grid.cars = 6;` — the career race builds a six-car grid |
| `src/core/replay_file.c:281,316` | the replay format stores `nothers` and **rejects `nothers > 5`** on load |

The replay one matters: the on-disk replay format itself validates a six-car field. A 7+ car race either
needs a new replay kind or must refuse to record (a documented limitation is better than a corrupt file).

### C. THE GUEST'S OWN SIX — the actual wall

| place | what it is |
|---|---|
| `src/core/physics/race_run.c:97` | `static const uint32_t entry_at[6] = {0x01bd7710, 0x01bdacf0, 0x01cd9190, ...}` — **six fixed guest addresses: the game's race-entry table is six slots deep** |
| `src/core/physics/init.c:17` | `SIDES 0x00620AD8  /* [6] which side of the grid slot */` — the guest's own per-slot table has six entries |
| `src/core/career_original.c:180` | `00447828 (+0x91 for player, +0x8c..90 for five opponents)` — the contest record itself is player + five |
| `src/core/physics/stream/*`, `CONDUCTOR_FIELD_CARS 0x6698` + `CAR_SIZE` stride | the conductor's car array — **count-driven**, indexed `cond + 0x6698 + i * CAR_SIZE`, so iteration is honest |

So the good news is real: the **physics and AI loops are count-driven** (`NCARS(c)` = the guest's own car
count at `+0x10000-0x760`, `ai.c:1717`; the conductor array is a strided guest array, not a C array of 6).
The bad news is the **entry table is six pointers into the guest's own memory**, and its size is whatever the
original allocated. A seventh entry has to come from somewhere.

---

## What a PC-only first experiment looks like

1. **Arcade, one extra car.** Arcade owns its opponent pick (`src/core/arcade.c`,
   `MCarData.PickUpEnemyCars`, `menu.c:346`). Raise `race_game.c:754`'s clamp from 5 to 6, widen
   `last_colors[5]`, and give the extra entry a grid slot from the guest's own table if one exists. Success =
   the extra car starts, drives, is overtaken, finishes, and appears on the arcade board
   (`race_result.c`).
2. **Widen ours in the same pass** — B-Spec monitor, arcade board, HUD letters.
3. **Then the entry table.** Find where the guest allocates the six entries (the addresses in
   `race_run.c:97` are the observable end of it) and whether room exists for a seventh, or whether a slot can
   be repointed at a larger allocation we make ourselves. This is the make-or-break step and it needs a
   measurement, not an opinion.
4. **Replays:** refuse to record a >6-car race, and say so on screen. Do not write a file the loader rejects.
5. **GT mode last.** Career event data names five opponents per event; the contest record is player + five.
   Nothing here should be attempted before arcade proves the mechanism.

## Risks to write down now

- **AI pace is authored for five.** More cars means more traffic, and GT4's AI does not know how to
  overtake cleanly in a crowd; expect pile-ups until the pace logic is touched.
- **Guest memory.** The entry pointers are into the original's own heap; a wrong stride corrupts the
  conductor and takes the race with it.
- **Saves.** Anything that stores a field size (replays today, save data possibly) must be versioned, not
  silently extended.
- **Scope discipline:** PC only for now (captain's instruction). No Android/Switch work on this line.

## What the playtester should be asked to prove (when the captain dispatches one)

1. An arcade race with the extra car: it starts on the grid, drives, and is listed in the standings.
2. Overtaking it changes the player's position correctly.
3. It finishes; the results board lists it with a time.
4. No crash across: race start, a full lap, a pit stop, race end, results exit, and re-entry to the menu.
5. A >6-car race refuses to record a replay, with a visible reason, and the menu still works afterwards.

## Open questions this research did NOT answer (measure these next)

- What is the real size of the guest's entry table, and what do the six addresses in `race_run.c:97` belong to?
- Does the conductor's car array have slack beyond six, or is it exactly six strides?
- How many grid slots does a course actually publish (`grid_slot`, `init.c:98-101`)? Some courses may not have a seventh slot — that alone could bound the feature per course.
- Does the SpecDB/menu opponent pick (`menu_race_entries`) have a natural cap below six?

---

## MEASURED 2026-10-24 (batch 5) — the entry blocks have scratch, the conductor does not

The audit correction named three six-slot structures in the guest conductor, and the open question was
whether the six fixed entry addresses have slack beyond six. `tests/ui/race_audio.c --field-audit`
(`ctest -R race_field_size_audit`) builds a real six-car Arcade race from the retail disc and prints the
addresses. Its output on the captain's v2.00 disc:

```
field audit: conductor 0x01cf8ed0, 6 cars
  entry blocks: 0x01bd7710..0x01ce6910, stride 0x35e0
  next slot after entry 1: 0x01bde2d0; after entry 5: 0x01ce6910 (= WORLD)
  per-car blocks: 0x01cf8f68..0x01cff568 (6 x 0x1100)
  car states:     0x01cff568..0x01d05928 (6 x 0x10a0)
  a 7th block would start at 0x01cff568 (= car state 0)
  a 7th state would start at 0x01d05928 (= driver/FFB +0xca58)
```

What this settles:

1. **The entry blocks are not the wall.** The CORE.GT4 image loads only `0x00100000..0x00617a14` and
   `0x00617a80..0x006d5dfc` (BSS to `0x008a215c`); all six entry blocks sit in scratch RAM above that.
   Block A has a free next stride slot at `0x01bde2d0`, so a seventh (and probably eighth) 0x35E0 block
   could be placed without touching the image. Block B's next slot is already `WORLD` (`0x01ce6910`).
2. **The conductor arrays are the wall.** The per-car block array starts at `cond+0x98`, six strides of
   `0x1100` later is exactly `cond+0x6698` — which is the first car state, not free space. The car-state
   array starts there and six strides of `0x10A0` later is exactly `cond+0xCA58` — the driver/FFB field.
   There are **zero spare bytes** between either array and the next conductor field. A seventh car's
   per-car block would overwrite car state 0; its car state would overwrite the FFB record.
3. **The count-driven loops cannot save this.** `NCARS(c)` and the physics/AI loops would happily iterate
   seven, but they index these two contiguous arrays. The seventh iteration is the corruption above. To
   support seven honestly, both arrays and every conductor field from `+0x6698` onward (course, rules,
   car count, position order, session vtable at `+0x10140`, …) would have to move and every hardcoded
   offset in the physics would have to follow. That is not a safe change behind a switch.

**Verdict: 5 opponents is the wall.** The `tweaks.field_size` switch was NOT added, the field clamps were
NOT raised, and no seventh entry was written. The research question is answered with the measurement
above; custom modes must be built on a six-car field unless the conductor layout is versioned/relocated as
its own project. The audit probe stays as a regression pin: if someone later gives the conductor real
slack, that test must be updated first.
