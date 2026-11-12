# AUDIT — every place Moon Engine assumes a fixed car count

*Written 2026-10-19 by Caine. Goal: G6.42 (a race field bigger than six). This is the inventory before any code
moves: what is **ours** and free to change, what is **the original game's memory layout** and therefore the wall.*

## Verdict

The physics is already **count-driven**. The wall is not a loop — it is **six pre-allocated entry blocks in guest
RAM**. Everything else on the list is our own table and can be widened today.

## The guest ABI (measured, `src/core/physics/race_run.c`)

| guest address | what it holds |
|---|---|
| `0x00a47cb0` (`ENTRIES`) | conductor's entry table header |
| `ENTRIES + 0x00` | **car count** (same value as `COND + 0xF8A0`, see below) |
| `ENTRIES + 0x08` | pointer to the per-car pointer table (`ENTRIES + 0x24`) |
| `ENTRIES + 0x24 + i*4` | **entry pointer for car `i`** |
| `0x00bcc010` (`COND`) + `0xF8A0` | car count again — `CONDUCTOR_FIELD_CAR_COUNT`, the field every physics/AI loop reads |

The field at `+0xF8A0` is read all over the physics: `ai.c`, `driver.c`, `init.c`, `session.c`, `misc.c`,
`license_session.c`, `race.c`, `step.c` (`NCARS`), `ai.c` (`NCARS(c)`). **All of those are already
`for (i = 0; i < count; i++)`** — none of them hardcode six.

## The wall: `entry_at[6]` (`race_run.c:97`)

```c
static const uint32_t entry_at[6] = {0x01bd7710u, 0x01bdacf0u, 0x01cd9190u,
                                     0x01cdc770u, 0x01cdfd50u, 0x01ce3330u};
```

Six hand-located structures inside the guest's own RAM. Measured spacing:

- **Block A** — `0x01bd7710` and `0x01bdacf0` (entries 0, 1), stride **0x35E0**. The next slot in this block would
  be `0x01bde2d0`.
- **Block B** — `0x01cd9190`, `0x01cdc770`, `0x01cdfd50`, `0x01ce3330` (entries 2–5), same stride **0x35E0**. The
  next slot would be `0x01ce6910`.

The two blocks sit `0xFE4A0` apart; every entry is 13,792 bytes (`0x35E0`).

**Consequence for car #7:** there is no seventh block. We must either

1. take the next stride slot in either block (`0x01bde2d0` after entry 1, or `0x01ce6910` after entry 5),
   **after proving that memory is unused by anything else in the loaded image**, or
2. allocate a fresh 0x35E0 block in free guest RAM and point the table at it.

Because the code writes the table from `entries` at `ENTRIES + 0x24`, and the count separately at
`ENTRIES + 0x00` / `COND + 0xF8A0`, **the engine's own plumbing may accept a seventh pointer** — that is the
experiment to run, and it is small.

## Our own assumptions (all free to widen)

| where | what assumes six | class |
|---|---|---|
| `src/core/race_bspec.h:16` | `RACE_BSPEC_CARS 6` — the position board's rows | ours |
| `src/core/race_bspec.c:40-42, 99, 161, 203-209, 284, 320-326` | per-row lap/best/mark arrays sized by it | ours |
| `src/core/race_result.h:19-28` | `rank[6]`, `finished[6]`, `time[6]`, `laps[6]`, `name[6]`, `top_speed[6]` | ours |
| `src/app/race_game.c:95,101,102` | `ids[6]`, `pit_motion[6]`, `pit_crew[6]` | ours |
| `src/app/race_game.c:138,741` | `names[6][96]`, `prepared_labels[6][96]` | ours |
| `src/app/race_game.c:245,247` | `disc_entries[6][0x228]`, `last_owned[6][0x4b8]` | ours (mirrors guest data) |
| `src/app/race_game.c:938,939` | `snap[6][48]`, `snap_cars[6]` | ours |
| `src/app/race_game.c:1379,1397,1401` | grid bodies / finish order / `ranks[6]` | ours |
| `src/app/race_game.c:1469,1488` | championship points `[6]`, `order[6]` | ours |
| `src/app/race_game.c:754` | clamps the field to **5 opponents** | ours — the actual entry gate |
| `src/app/main.c:4838` | asks for 5 opponents | ours |
| `src/core/career_event.c:240` | `grid.cars = 6` per event | ours (data) |
| `src/core/career_original.h:138-141` | championship points/order `[6]` | ours |
| `src/app/race_audio.c:45,62,63` | `sequence[6]`, `backfire[6]`, `exhaust_count[6]` | ours |
| `src/core/replay_file.c:316` | rejects a replay with more than 5 opponents | ours (format) |

Not every `[6]` in the tree is a car count — the grep also catches GLSL matrices and unrelated floats. The table
above is the reviewed set.

## What raising the field to seven costs, in order

1. **Widen our own tables** — `RACE_BSPEC_CARS` becomes a runtime count (or a max with a live count beside it),
   then `race_result.h`, `race_game.c`, `race_audio.c`, `career_original.h`.
2. **Open the gate** — `race_game.c:754` and `main.c:4838` currently stop at 5 opponents.
3. **Give the guest a seventh entry** — add the pointer at `ENTRIES + 0x24 + 6*4`, write the block, set
   `ENTRIES + 0x00` and `COND + 0xF8A0` to 7. Choose between the next stride slot and a fresh allocation, and
   **prove the chosen memory is unused** before writing.
4. **Make the screens fit** — the position board and results list draw six rows; seven needs a row count that is
   data-driven (G6.32's readability pass is a natural companion).
5. **Replay format** — either accept N or record the field size in the header and refuse only on mismatch.

## Risks (honest)

- The guest code may index the entry table by a fixed stride elsewhere; if so, a seventh pointer is accepted
  silently and then read past the array. That is the first thing to test.
- The start grid (course's own grid points) may only define six slots. A seventh car would share or miss a slot.
- Points tables and championship scoring assume six finishers in places we cannot see (data-driven tables).
- Anything that assumes the **player is the last entry** (`i ? i - 1 : cars - 1` at `race_run.c:202`) needs a read
  before the count changes.

## Recommendation

Do steps 1–2 first (ours alone, no guest memory touched), with the field still clamped at six so nothing changes
visually. That refactor is required either way, it is testable on desktop, and it turns "how many cars" into one
number instead of fifteen tables. Step 3 is the experiment on a branch, never on `main`.
