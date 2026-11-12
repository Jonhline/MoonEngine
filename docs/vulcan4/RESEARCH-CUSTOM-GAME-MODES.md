# CUSTOM GAME MODES — research for the next development session

Measured 2026-10-26 against `experimental/more-than-6` (8f5f85f). This is a
feasibility map, not a plan that has been built.

## 1. What a "mode" is in this engine

A race is a `gt4_race_desc` handed to `gt4_race_run_open()`; the conductor then
runs the guest's own session. Everything a mode needs already exists in three
places:

| layer | what it owns | file |
|---|---|---|
| world | which guest race world runs (Arcade, GT Mode free run 11, practice 12, Power & Speed 15) | `race_game.c:1946` `race_game_set_world_mode` |
| rules | laps, tyre compound, gearbox, aids, B-Spec, pit menu, split battle, time trial, field size, phantom cars | `race_run.h:39-95` |
| classification | one merged list of N entrants: stable id, official/phantom, laps, distance, finish time, live rank, place | `race_run.c` (`gt4_race_entrant`, `order_classification`) |

**Consequence:** a custom mode is a rules layer over that list — we do not need
new physics, and we do not need to touch the guest. That is the whole opening.

What we already proved on this branch: N entrants can be raced, ranked, shown
on a results board, paid, and refused a replay. A mode only has to say *who
wins and who leaves*, and draw it.

## 2. Candidate modes, with the real hook

| # | mode | rule | hook | size | risk |
|---|---|---|---|---|---|
| A | **Elimination** | every N seconds the last running car is retired | classification order + a timer in the rules tick; `CAR_FIELD_POSITION` already written by us | S | retiring a car mid-race is unproven |
| B | **One-make** | every entrant drives the same model | `d.phantom_car[]` + the official five all set to the player's car | XS | none known |
| C | **Reverse grid** | grid reversed by points/last result | the grid formation code added for phantoms already computes poses from the official grid | S | needs a persistent last-result source |
| D | **Traffic time attack** | phantoms are slow obstacles, not rivals | phantom AI profile (`entry + 0x343c`) + exclude them from classification | S | AI profile range unmeasured |
| E | **Pursuit** | one phantom target must be caught within a lap | classification + a target id; HUD distance-to-target | M | HUD element is new |
| F | **Mandatory pit** | at least one stop, tyre wear on | `pit_menu`, `tire`, `laps` already in the desc | M | guest's wear model is per-race, needs a multiplier hook |
| G | **Rally / point-to-point** | start and finish on different courses | course loading + a checkpoint list | L | course pipeline is not ours |
| H | **Drift / score** | points for angle and speed | needs a new scoring pass over car state | L | no scoring system exists |
| I | **Drag** | 400 m, reaction start | a straight course + a launch gate | M | course availability unmeasured |
| J | **Event synthesizer** | generate rule sets (Spec II style) | composes A–F; a table of modes + parameters | M | value depends on A–F existing |

Size: XS < half a day, S ≈ a day, M ≈ a few days, L = a project.

## 3. The one architectural decision

**Build one rules tick, not ten special cases.**

```
mode_tick(run, dt)  ->  reads classification, writes: retire(car), target(car), score(car)
```

Every mode above is then a small function plus its HUD line. Without that, each
mode becomes a patch in `race_game.c` and the sixth one will fight the first.

## 4. Recommended for the next session (order matters)

1. **B — One-make.** Cheapest real mode, and it doubles as a test of phantom
   selection: every entrant must be the same model, which exercises the rated
   picker from dish 11.
2. **A — Elimination.** The first mode that *changes the race*, and the first
   that needs a car to leave mid-race. That capability (retire) is reused by
   almost every other mode, including damage-limited events.
3. **D — Traffic time attack.** Phantoms stop being rivals and become the
   course. It proves the AI-profile knob and gives an immediately fun result.

**J (the synthesizer) goes last** — it is only a table over whatever modes
exist. Shipping it first would generate events the engine cannot run.

## 5. Open questions that decide the sizes above

1. Can a car be retired mid-race cleanly? (set the finish flag with no time,
   or a DNF state the guest respects). Measure before promising A.
2. What is the range of the AI profile word (`entry + 0x343c`)? Traffic mode
   lives or dies on making a phantom genuinely slow.
3. Does a mode need to survive a save/replay? Current rule: >6-car races
   refuse to record. Modes should follow the same rule — refuse, with a reason.
4. Which modes are legal in GT Mode vs arcade only? Arcade is ours; GT Mode is
   the disc's scripts. Default: arcade first, as with the phantom field.

## 6. What NOT to build

- No mode that changes the six-car default. Every mode is opt-in, off by
  default (LAW 12).
- No mode that needs a new physics model (G, H). Those are projects, not modes.
- No mode that silently changes prize or points. If a mode pays differently it
  must say so in the UI.
