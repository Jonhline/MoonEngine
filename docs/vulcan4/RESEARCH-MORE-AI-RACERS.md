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
