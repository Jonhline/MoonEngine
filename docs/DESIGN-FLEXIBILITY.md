# DESIGN LAW — NOTHING HARDCODED (captain, 2026-10-24)

*"I want nothing related to this to be hardcoded. Honestly nothing in this
project should be 100 percent hardcoded unless necessary. We are allowing
versatility in everything… U want 50 opponents? Sure! U want to add 1000 million
modded cars? Bet!"*

## THE LAW

1. **Every count, limit and capacity is a NAMED PARAMETER**, declared once, and
   read from one place. No bare `6`, `5`, `[6]`, `> 5`, `< 6` anywhere else.
2. **Default = the original game's value.** A missing or zero parameter must
   reproduce stock behaviour exactly. Versatility never changes what the player
   gets unless they ask for it.
3. **Never silently clamp.** If a requested value is refused, say so with the
   value, the allowed range and the reason. A silent `nothers = 5` is a bug.
4. **Data comes from files, not from code.** Field size, car lists, grid size,
   seat counts, event entries: config/`.ini`/data tables. Code reads them.
5. **Hardcoding is allowed only where physics or the guest binary forces it**
   (a 16-bit offset, a fixed guest struct). When that happens, it must be
   written down next to the constant, with the reason and the ceiling it
   implies.
6. **Every flexible parameter is documented** — name, type, range, default,
   where it is read, what it changes. Documentation is part of the feature.

## WHERE IT APPLIES FIRST (audited ceilings, 2026-10-24)

| what | today | becomes |
|------|-------|---------|
| `RACE_BSPEC_CARS` | `6` (`race_bspec.h:16`) | `race.field_size`, default 6, validated 1…`MOON_MAX_CARS` |
| opponent selection | `menu_race_entries(..., 5)` (`main.c:5900`) | `field_size - 1` |
| clamp | `if (nothers > 5) nothers = 5` (`race_game.c:970`) | compare to `field_size - 1`, report refusals |
| colour table | `last_colors[5]` (`race_game.c:261`) | sized by capacity, not by 5 |
| results | `rows[6] / order[6] / done_car[6] / timed[6]` | sized by capacity |
| HUD | grid letters (`race_hud.c:1156`) | loop to `field_size` |
| renderer | `OPPONENTS 7` (`race_view.c:46`) | capacity constant, raised once, documented |
| guest entry table | `entry_at[6]` (`race_run.c:97`) | generated from `field_size`, with the measured free block |
| career events | `grid.cars = 6` (`career_event.c:240`) | read from the event definition |

## CAPACITY vs REQUEST

Two different numbers, never confused:

- **CAPACITY** — what the memory layout and the guest can physically hold. One
  constant per tier (see `GOAL-MORE-THAN-6.md`): 6 → 12 → pool mode.
- **field_size** — what this race asked for. Must be `1 ≤ field_size ≤ capacity`.
  Exceed it and the game says why, in the UI, in plain words.

## THE TEST FOR "IS IT FLEXIBLE?"

Change only the parameter file. If the code must be edited, it is still
hardcoded. That is the whole check.
