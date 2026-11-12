# Research — Custom Race Modes (G6.26)

Date: 2026-10-24. Research only: no code, no switch, no behaviour change.
Scope: the modes named in `docs/vulcan4/WISHLIST.md:1006-1021` — **elimination**,
**reverse grid**, **one-make**, **no-contact**.

The recommendation is one data-driven **rule table per event**, not four hard-coded
modes. All four are the same shape: a small set of start-time and per-frame rules over
data the race already owns.

## What the engine already has

### Field and position

- `race_hud_state` already carries `rank` and `cars` (`src/core/race_hud.h:24`), the
  per-car map positions (`:44-45`), and `finished` / lap fields further down.
- The run exposes them per car: `out->rank` from `car+0x5B3`
  (`src/core/physics/race_run.c:1106`), `out->finished` from `car+0x5B6`
  (`src/core/physics/race_run.c:1128`), `out->grid_slot` from entry `+0x3444`
  (`src/core/physics/race_run.c:1136`).
- The race order is recomputed every step by `race_positions()`
  (`src/core/physics/session.c:318-335`), which sorts the conductor's `+0xF8A4`
  index table and writes each car's 1-based `+0x5B3`.
- The field is capped at six: `nothers > 5` is clamped in
  `src/app/race_game.c:845`, and prepared races reject `d->cars < 1 || d->cars > 6`
  in `src/app/race_game.c:1194`.

### Per-frame loop

- `race_game_update()` is the frame entry (`src/app/race_game.c:2612`).
- The camera + audio publish is `race_camera_audio_update(g, camera_dt)` at
  `src/app/race_game.c:2829`.
- The scene draw is `race_view_draw(g->view)` at `src/app/race_game.c:2873`
  (called from the app frame at `src/app/main.c:6528`).

### Retire and pit path

- `gt4_car_race_end()` (`src/core/physics/race.c:94`) is the original per-car end:
  it sets the retire bits `car+0x5B5` bit 4 / bit 8 at `src/core/physics/race.c:138-142`
  and `car+0x6C6` (`src/core/physics/race.c:104`, `:117`).
- The finish hand-over at frame 179 is `gt4_race_finish_handover()`
  (`src/core/physics/race.c:189`).
- The pit lane is driven from `pit_frame()` (`src/app/race_game.c:2445`) and
  `race_game_set_pit_mode_change()` (`src/app/race_game.c:2438`); the pit crew reads
  the same `+0x5B5` flags (`src/core/race_pit_crew.c:51`).
- **Not found:** a native "retire this car now, keep the race running" API. The
  existing retire path is the original end-of-race calculation, so elimination needs
  a deliberate, tested rule writer rather than calling `gt4_car_race_end()` directly.

### HUD announcements

- The rolling message queue is `add_line()` (`src/core/race_hud.c:1276-1301`), used
  by `race_hud_section_time()` (`:1303`), `race_hud_lap_time()` (`:1316`),
  `race_hud_top_speed()` (`:1328`), `race_hud_gap()` (`:1344`) and
  `race_hud_laps_behind()` (`:1368`).
- The big centre message is `draw_intro()` / `race_hud.c:1206-1260`; the START
  message is `race_hud_start()` (`src/core/race_hud.c:2181`).
- Text keys come from the disc's `RaceDisplay` rtext table
  (`race_text()`, `src/core/race_hud.c:1209-1213`). **This is the main gap for a new
  mode:** an elimination announcement that does not exist on the disc needs either a
  reused authored string or a new native draw. That must be measured before quoting
  the feature as "HUD only".

### Results board

- `race_result_state` already carries the board order `rank[6]`, `finished[6]`,
  race `time[6]`, `laps[6]` (`src/core/race_result.h:19-23`).
- The board is updated by `race_result_update()` (`src/core/race_result.c:345`),
  which sorts rows by `rank` at `src/core/race_result.c:365`.
- It plays `/menu/common/result.strb` (`src/core/race_result.c:264`) and is drawn and
  advanced by `race_result_press()` / `race_result_draw()` / `race_result_done()`
  (`src/core/race_result.c:412`, `:421`, `:426`).

### Hooks the rule table can use

- **Reverse grid:** the start order is authored once by `race_order_reset()`
  (`src/core/physics/session.c:435-445`, car `i` in place `i+1`) and
  `practice_order_reset()` (`src/core/physics/session.c:532-543`, place from the grid
  slot). A reverse-grid rule is a permutation at this point, before the first
  `race_positions()` step.
- **One-make:** the opponent list is already a parameter of the race start
  (`race_game_start_impl(..., const char *const *others, ...)`,
  `src/app/race_game.c:819-852`); prepared events carry their own cars
  (`src/app/race_game.c:852-905`). A one-make rule only has to fill the same list with
  one model label.
- **No-contact:** car-car contact is selected by a session "kind" in
  `gt4_step()` (`src/core/physics/step.c:193-215`); `cars_contact()`
  (`src/core/physics/step.c:47`) is only called for kind 0, 1 and 2. A no-contact rule
  can return a fourth kind (skip the block) without touching physics or AI.
- **Elimination:** positions are already read every step, the field is capped at six,
  and the retire flags exist. What is missing is a rule writer that retires the last
  car at an interval and a measured path for the HUD/results to show it.

## Rule-table shape

One table per event, read at race start and ticked per frame. Default all-zero keeps
every shipped race byte-identical (LAW 12).

```c
typedef struct {
    unsigned reverse_grid       : 1;  /* start order = reverse of qualifying/grid */
    unsigned one_make           : 1;  /* every entry uses the event's car model */
    unsigned no_contact         : 1;  /* session contact kind = none */
    unsigned elimination        : 1;  /* last place retires on an interval */
    int  elimination_laps;            /* 0 = every N seconds */
    float elimination_seconds;
    int  elimination_min_cars;        /* stop when this many remain */
} moon_race_rule;
```

Where it lives:

- start-time rules (reverse grid, one-make) are applied in `race_game_start_impl()`
  before the first physics step, using the existing order/opponent lists;
- the no-contact rule is a session kind consumed by `gt4_step()`;
- the elimination rule runs in `race_game_update()` after `race_positions()` has
  written `+0x5B3`, and must not touch AI, physics or authored data.

## The four modes

### 1. No-contact — cheapest

- **Already there:** `gt4_step()`'s contact-kind branch (`step.c:193-215`) and
  `cars_contact()` (`step.c:47`). The contact kind comes from
  `session_call(c, 0x140, ...)`.
- **Rule:** return a kind with no branch, so cars can overlap.
- **Player sees:** clean side-by-side racing; no bumping, no rubber-band shoves.
- **Effort (estimate):** 3–6 h, mostly measuring which sessions return which kind and
  guarding replays/2P.
- **Risk:** low. No physics, no AI, no authored data.
- **Breaks if careless:** replay parity (the recorded race was driven with contact) and
  2P contact expectations; keep it an explicit per-event rule.

### 2. Reverse grid — cheap, visible

- **Already there:** `race_order_reset()` (`session.c:435-445`) and
  `practice_order_reset()` (`session.c:532-543`); qualifying/grid slots already exist
  (`race_run.c:1136`).
- **Rule:** permute the start order (reverse the field) before the first position
  sort. No physics change.
- **Player sees:** the slowest qualifier starts at the front; a different opening lap.
- **Effort (estimate):** 6–10 h, mostly UI/event wiring and making sure the HUD and
  replay capture the new order.
- **Risk:** low–medium. Wrong permutation can put a car on another car's grid box;
  must use the runway's own grid entries, not free placement.
- **Breaks if careless:** Time Trial / license / mission sessions (single-car or
  scripted starts) and replays that assume the authored order.

### 3. One-make — medium

- **Already there:** the opponent list is already data (`race_game.c:819-852`), and
  every entry is a specdb label resolved from `GENERIC_CAR.idi`
  (`race_game.c:853-905`). Career events already fill the list from an event row.
- **Rule:** replace every opponent label with the player's (or the event's) model,
  keeping the field size and colors; optionally restrict tuning.
- **Player sees:** an evenly matched field of the same car.
- **Effort (estimate):** 8–14 h: choose the model, validate that five valid opponents
  exist, and surface the choice in the event UI.
- **Risk:** medium. A model may have no valid AI-spec opponents or colors; must fall
  back cleanly.
- **Breaks if careless:** career events whose opponent list is authored (one-make must
  be opt-in), and save/replay identity if the field changes after the race starts.

### 4. Elimination — most work, most new behaviour

- **Already there:** live positions (`session.c:318-335`), field cap six
  (`race_game.c:845`, `:1194`), retire flags (`race.c:138-142`), the HUD queue
  (`race_hud.c:1276`) and the results board (`race_result.h:19-23`).
- **Rule:** every N laps/seconds, read the last position and retire that car; stop at
  a minimum field; the last car running wins.
- **Player sees:** a countdown/interval, a car dropping out, a shrinking field, a
  different finish condition.
- **Effort (estimate):** 18–30 h. The hard parts are a safe retire writer, the
  announcement text, and the results/replay paths.
- **Risk:** high. Retirement touches the original finish flags, the results board, and
  replay; the board currently expects finishers (`race_result.c:365-380`).
- **Breaks if careless:** `gt4_car_race_end()`'s assumptions, fuel/tyre retirement,
  the results movie, and the "all cars finished" gates in
  `license_session.c:184,257`.

## Ranked cheapest-first

1. **No-contact** — 3–6 h, low risk, clean hook in `gt4_step()`.
2. **Reverse grid** — 6–10 h, low–medium risk, start-order permutation only.
3. **One-make** — 8–14 h, medium risk, opponent-list data only.
4. **Elimination** — 18–30 h, high risk, needs a safe retire writer and HUD text.

All figures are estimates, not measurements. The only measured numbers here are the
file:line references.

## What must be measured next

1. **Contact kind per session.** What `session_call(c, 0x140, ...)` returns in arcade,
   career, free run, time trial and 2P, and whether a fourth kind is safe for replays.
   (`src/core/physics/step.c:193-215`.)
2. **Start-order writer.** Confirm the exact conductor state `race_order_reset()`
   leaves (`session.c:435-445`) and whether changing it after entry setup affects
   `grid_slot` (`race_run.c:1136`) or the replay capture.
3. **Retire without ending the race.** Find a measured way to set `car+0x5B5` bit 4/8
   for one AI car and have the HUD/positions/results treat it as out, without running
   the original end-of-race fuel/tyre logic. (`race.c:94-145`.)
4. **HUD text.** Confirm whether an "OUT"/"ELIMINATED" line can reuse an authored
   `RaceDisplay` rtext key; otherwise a native line is new UI work.
   (`race_hud.c:1209-1213`, `:1276`.)
5. **Results board.** Confirm `race_result_update()` handles a car that never crosses
   the line; its sort reads `rank` and `finished` (`race_result.c:345-380`).
6. **Field identity.** For one-make, measure how many valid AI opponents/colors exist
   for a given model from `GENERIC_CAR.idi` (`race_game.c:853-905`).
7. **Frame cost.** Field sorting is already per step (`session.c:318`); the rule tick
   must add no per-car allocation and stay under the existing race frame budget.

## Fence

- Default rule table = all zero: shipped races behave exactly as today.
- No AI, physics or authored-data edits. No disc driving-line data is read; any
  assist line is a separate feature (see G6.15).
- Every rule is opt-in per event and visible in the event UI before the start.
