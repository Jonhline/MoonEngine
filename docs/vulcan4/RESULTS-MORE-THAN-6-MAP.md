# RESULTS / MORE THAN SIX — RECON MAP

Read-only recon. No source edit, build or commit. Paths are under
`/home/or/moonengine`. "Native fix" means no guest code or disc asset change.

## 1. FINISH DETECTION

| place | data | native fix | risk |
|---|---|---|---|
| `race_run.c:1300-1302` | `gt4_race_run_finished()` reads only car 0 `+0x5B6` | yes, already native | phantom winning first does not end the player's race; result starts only after the player finishes |
| `race_game.c:3024-3027` | `g->finished` starts at 0 when car 0 finishes | yes | no all-field end; if the product wants the flag to end the race on a phantom win, this gate is wrong |
| `session.c:935-958` `race_lap` | finish when leader is through, last lap, or time limit; writes `+0x614` | yes | each conductor decides its own finish; official and shadow do not share the decision |
| `session.c:1095-1111` `race_finish_line` | finish on line crossing; writes `+0x614`, calls `gt4_car_finish_drive` | yes | same path runs for a phantom conductor; its flag is in the shadow, not the official conductor |
| `ai.c:2617-2624` | `gt4_car_finish_drive` sets `CAR_BLOCK_FINISH_DRIVE_STARTED` = absolute `+0x5B6` | yes | phantom gets the flag by code path; not runtime-verified in the current probe |

A phantom crossing first sets `+0x5B6` only in `PHANTOM_COND`. The player keeps
driving and `g->finished` stays -1 until car 0 crosses. That is usable, but the
result must read the shadow's finish state, not the official six.

## 2. FINISH ORDER AND TIMES

| field | writer / reader | meaning | native fix | risk |
|---|---|---|---|---|
| `+0x5B6` | `ai.c:2623-2624`; `race_run.c:1381`; `race.h:21` | finished flag | yes | set per conductor; shadow flag is invisible to official code |
| `+0x614` | `session.c:958`, `:1110`; comparator `session.c:306,354` | lap timing reference / finish time argument | yes | written by the finish path for both conductors |
| `+0x718` | read at `race_run.c:1382`, `race.h:22`, `career_original.c:426-429`; native writes only in licence/battle/start (`session.c:1243,1687,1698,1707,1713`) | line time used by HUD/results | **unverified / likely missing in the normal race path** | `race_finish_line` writes `+0x614`, not `+0x718`; `race_result_state.time` reads `+0x718`; a phantom (and an official car) can report a stale line time |
| `race_game_finish_order` `race_game.c:1683-1702` | official six, sorted by `+0x5B3` | six-entry result order | partial | buffer is six; it cannot return phantoms; live rank is not final finish order |
| `race_result.c:360-369` | sorts by `s->rank` | board order | partial | uses live `+0x5B3`, not frozen finish time |
| `main_session.inc:182-184` | `uint8_t order[6]`; requires `n == 6` | championship order | no for >6 | 8/12 cars fail this guard or overflow if widened |

Merged order: keep one native entrant list. For each entrant store
`official/phantom`, owner conductor, car address, `finished`, native-captured
finish time, laps, distance, live rank. Sort finished cars by captured time;
then running cars by the existing cross-conductor comparator. Freeze the list
when car 0 finishes. Use it for HUD, result board, prize and points.

Finish-time capture must not trust `+0x718` until measured. Capture at the
`+0x5B6` transition (or from `+0x614` if the race clock semantics check out).

**Measurement needed:** run the 12-car `PROBE_PHANTOM_RUN`, then print
phantom/official `+0x5B6/+0x614/+0x718` at the finish. Expected: `+0x5B6` 1;
`+0x614` differs by car; `+0x718` tells whether the existing line-time field is
usable. No such probe exists today.

## 3. RESULTS BOARD

| place | data | native fix | risk |
|---|---|---|---|
| `race_game_results.inc:31-43` | `rs.cars = min(cars, 6)`; loops official 0..5; `rank` = `+0x5B3`, `finished` = `+0x5B6`, `time` = `+0x718`, `name` = `g->hud_names[i]` | partial | phantoms never enter `rs`; six official rows only |
| `race_result.h:18-30` | arrays sized `MOON_MAX_CARS` (12) | yes for data | data capacity is 12, not 6 |
| `race_result.c:32-40` | `row rows[12]`, `nrows`, `player_row` | yes | data layer can hold 12 |
| `race_result.c:360-406` | `nrows = min(cars,12)`; sort by `rank`; rows by classification; `player_row` is the row index | yes | `player_row` is not the true place; six-row movie may never query rows 6..11 |
| `race_result.c:93-165` | `cb_getname`, `cb_gettime`, `cb_getrank` (`player_row`), `cb_isvisible(at < nrows)` | partial | movie script is the unknown; `result.strb` is `/menu/common/result.strb` (`race_result.c:264`) |
| `race_game.c:1771-1790` | quick results; six-entry order and `menu_quick_race.grid[6]` | no for >6 | phantoms have no grid slot; rows dropped |
| `menu.h:218-239` | `grid[6]`, `total_points[6]`, `round_points[6]` | no for >6 | menu data is fixed six |

The data layer already has 12 slots; the authored `result.strb` layout is
**unverified** for more than six rows. Do not claim a 7th/8th visible result
until the movie's row count is measured or a native result list is used.

## 4. PRIZE MONEY

| place | data | native fix | risk |
|---|---|---|---|
| `career_original.c:410-423` | `gt4_career_race_prize(race, position)`: six `u16` at race row `+0x20`, `position < 6`, x100 | yes | positions 7/8 return 0; no phantom identity/payout |
| `main_session.inc:141-144` | `position = race_game_event_position()`; prize indexed by it | yes | `race_game_event_position` counts only official cars (`race_game.c:1792-1808`), so a phantom ahead is invisible and the player gets the wrong prize |
| `career_original.c:50-70` | `gt4_career_process_event_result`: `counter = position + 1`, counters only `< 7`; `calendar_grade = position` | partial | 7th/8th place stats are silently dropped; grades above the six-position table are not validated |
| `gtmode_game.c:2111-2165` | career result applies `position`, prize, stats | partial | same index assumption |

Fix: prize is the **player's true classification index**, not the official-only
count. The table remains six positions; player 2nd of 8 selects index 1 and gets
the 2nd payout. Player 7th/8th has no table row and gets 0, explicitly.

## 5. CHAMPIONSHIP POINTS

| place | data | native fix | risk |
|---|---|---|---|
| `career_race_lookup.h:9` | `driver_points[6]` | no for >6 | awards table is six positions |
| `career_race_lookup.c:87-95` | `gt4_career_driver_points`, position 0..5 only | no for >6 | position 6/7 returns 0 |
| `career_original.c:737-751` | `gt4_career_championship_points(points[6], order[6], count, ...)`: skips `driver >= 6` but advances `awards[rank]` only for official drivers | partial | if a phantom is 1st, the first official gets `awards[0]` (1st points); player 2nd gets 1st points, not 2nd |
| `career_original.c:752-758` | `gt4_career_championship_rank(points[6])` | no for >6 | series rank is six rows |
| `main_session.inc:181-213` | `order[6]`, `n != 6` guard, `points[6]`, `q.total_points[6]`, `round_points[6]`, `series_rank` | partial | six-only; merging >6 needs a new classification-to-official mapping |
| `gtmode_game.c:2214-2275` | `points[6]` to championship result | partial | six rows |
| `career_original.c:806-828` | context: six 16-byte rows at `+0x30..+0x8F` | no | 7th row starts at `+0x90`; live buffer is 0x90 |
| `main_support.inc:38,42,913-930` | `points[6]`, `bodies[6]`, `keys[6]`, `paints[6]` | no | six entrants |
| `save_ps2.c:727` | saved `context_ps2` blob is 0x88 bytes | no | save has exactly six rows |
| `gtmode_game.c:2538-2543` | reads/writes `live+8`, 0x88 | no | six rows; the 7th cannot persist |

Merged points rule that honours "phantom winner takes the win": iterate the
classification by place `i`; if the entrant is official and `i < 6`, add
`awards[i]`; if phantom, add nothing and consume the place. The player 2nd gets
`awards[1]`. Phantom 1st leaves `awards[0]` unused. No phantom row is written.

## 6. IDENTITY

| place | data | native fix | risk |
|---|---|---|---|
| `race_game.c:164-165, 1087-1108` | `g->names[6]`, `g->hud_names[6]` from the player's six entries | yes for native new names | phantoms have no name slots |
| `race_game.c:267-269` | `last_others[5]`, `last_colors[5]` | yes | only official opponents saved |
| `race_game.c:1318-1332` | phantom renderer labels/colors are copied from official `others[src]`, `colors[src]` | yes | duplicate label/color; points would merge if indexed by that row |
| `race_run.c:288-302` | phantom entry is a clone of an official entry (`entry + 0x35e0`) | yes | the clone carries the official garage/name identity |
| `race_game_results.inc:42-43` | result name is `g->hud_names[i]` | partial | no phantom name |
| `race_view_draw_icon_front_alpha` usage `race_game.c:2355-2370` | icon by rank; phantom index = `g->others + j + 1` | done for live rank | result board still needs a phantom icon index |

Distinct entrant: add a native identity table (stable id, display name, color,
car label) for every field slot. Use it for renderer labels, HUD name if ever
watched, result `rs.name`, and point/prize rows. Do not index a phantom with an
official key or reuse an official garage row.

## 7. DOUBLE-COUNT / CRASH / SILENT DROP

| place | failure | guard |
|---|---|---|
| `race_game_event_position` `race_game.c:1792-1808` | phantom ahead not counted; prize/points index too low | replace with classification place |
| `gt4_career_championship_points` `career_original.c:742-749` | phantom skipped but award rank still consecutive; official drivers get shifted awards | award by classification place, not official-only rank |
| `race_game_finish_order` `race_game.c:1683-1702` | six-entry order buffer; 8/12 overflow if callers widen | return count; caller buffer `MOON_MAX_CARS` |
| `draw_result` `race_game_results.inc:32` | caps at six; phantoms silently dropped | result rows = classification count, or documented limitation |
| `race_result_update` `race_result.c:360-390` | `nrows` can be 12 but movie may query six | measure `result.strb`; native board if fixed |
| `race_game_quick_results` `race_game.c:1771-1790` | six grid slots; phantom has no slot | keep official quick grid; separate classification for results |
| `race_game_quick_points` `race_game.c:1755-1768` | `points[driver]` assumes `entries <= 6` | guard; never pass phantom index |
| `career_race.points` and save | 7th row writes beyond 0x90/0x88 | six-row wall; see §5 |
| `race_field_size_request` `main_support.inc:57-80` | refused value falls back to six; UI sees no reason | return requested+honored; show/log, no silent fallback |
| `race_game_start_impl` `race_game.c:1052-1084` | prepared races ignore the request; only menu races use it | decide scope; show "prepared field fixed at 6" |
| `race_run.c:246` | logs field size only to stderr | UI must show honored count too |

## CONTROL SURFACE PROPOSAL

| piece | change | evidence |
|---|---|---|
| two controls | `race.phantoms` toggle + `race.field_size` count | `race.field_size` already read at `main_support.inc:66`; `moon_options` is the same store |
| one table | add rows to `pc_video_options`, `vs_info[]` (`video_settings.c:449-513`), `video_setting_get/set`, `save()` keys | launcher `main.c:628-639` and in-game Tweaks both call `video_settings_tweak`; parity test `main.c:654-690` |
| off default | toggle 0 -> field 6, no shadow conductor, default CRC unchanged | `race_limits.h`; current default `fa2b3669`/`b28c9af8` |
| 100 slider | UI value 1..100; above 20 red warning state in both pages; real engine limit shown next to it | current real limit 12; no silent clamp |
| no clamp | request >12 shows "requested N; engine max 12" and refuses or explicitly starts 12; log it | current fallback `main_support.inc:76-80` is silent |
| persistence | save `race.phantoms` and `race.field_size` with `moon_options_set`; `moon_options_save` at `video_settings.c:370`; read at next race start | survives restart |

The in-game page and launcher must use the same ordered table. The warning is
a real state (red text/border in the settings body), not a help tooltip.

## CAPACITY — REAL NUMBERS

Current hard ceilings: `MOON_CONDUCTOR_CARS=6`, `MOON_MAX_PHANTOMS=6`,
`MOON_MAX_CARS=12`; renderer `OPPONENTS=11`; result/prize/points six rows.
**Reachable today: 12 cars total (11 opponents).**

Cost: measured 6 -> 12 total adds 3.870 ms/frame = **0.645 ms/car**. Base 6-car
total 5.506 ms. Projection (not measured beyond 12, p99 unknown):

| total cars | opponents | phantoms | shadow conductors | total ms/frame | reachable |
|---:|---:|---:|---:|---:|---|
| 6 | 5 | 0 | 0 | 5.51 (measured) | yes |
| 12 | 11 | 6 | 1 | 9.38 (measured, p99 24.69) | yes |
| 20 | 19 | 14 | 3 | ~14.5 | no (arrays/results/memory) |
| 50 | 49 | 44 | 8 | ~33.9 | no |
| 100 | 99 | 94 | 16 | ~66.1 | no |

Per shadow today: conductor 0x10144 + race 0x200 + entries 0x100 + 6*0x35e0 +
world 0xf000 + scratch 0x1000 = **214,916 bytes** (~210 KiB). 3/8/16 shadows =
0.64/1.72/3.44 MiB. The measured zero tail ends at `cond+0x139130`
(`0x1e32000`); current phantom arena runs `0x1d10000..0x1e31000`. Free tail
above it is about 1.8 MiB, less the physics/init scratch. 16 shadows do **not**
fit; 8 is borderline; 20 (3 shadows) fits RAM but not the renderer/results/
points arrays. 100 is **not reachable** without a wider guest conductor or a
new native car/renderer path. If 100 is shown, the UI must state "engine max 12"
and the real honored count.

## ONE MERGED CLASSIFICATION

```
field_entrant {
  uint8_t id;            // 0..5 official, 6.. phantom
  uint8_t official;
  uint32_t car, cond;    // owning conductor
  const char *name;      // native identity, never a cloned row
  uint32_t color;
  int finished;          // +0x5B6
  uint32_t finish_time;  // captured at transition, not raw +0x718
  int laps; float distance;
  int live_rank;         // +0x5B3
  int place;             // final 1..N
}
```

One producer: `gt4_race_run_frame` builds/updates the list; freeze on player
finish. One consumer surface:

- live HUD: `live_rank`/`place` -> `+0x5B3`
- finish: frozen order, captured times
- results: `race_result_state` rows 0..N-1 from the list
- prize: player's place -> `gt4_career_race_prize`
- points: for each place `i`, official entrant adds `awards[i]` if `i<6`
- identity: one name/color per entrant

This removes the five separate official-only paths (`race_game_event_position`,
`race_game_finish_order`, `draw_result`, `race_game_quick_results`,
`gt4_career_championship_points`).

## MINIMUM HONEST FIRST BUILD

In scope:

1. Two settings controls, off by default, shared by launcher and in-game Tweaks.
2. Runs up to the real 12-car limit; request above 12 is refused with the real
   number shown and logged.
3. One merged classification list for official + phantom.
4. Live `+0x5B3`, finish order, player prize, official points by true place.
5. Phantom identity distinct from the cloned official row (label/color/name).
6. Result board rows for every classified entrant, if `result.strb` permits;
   otherwise a native result list. Do not claim the win story with six rows.

Out:

- 20/50/100 cars (memory and six-row result/points walls).
- Phantom championship points persisting across rounds or a phantom champion.
- Phantom rows in the career save.
- Phantom prize identities/garages and phantom replay.
- Career/prepared grids beyond their fixed entrants.
- Multiple shadow conductors and a 100-car renderer.

The result board is the first dependency to measure: read
`/menu/common/result.strb` or run the 8-car result and count visible rows.
If it is fixed at six, the first build is a native result list, not a data-only
patch.

## THE WALL — CAREER SAVE

The career save holds **exactly six** championship rows, and the evidence is
structural:

- `gt4_career_championship_context` writes `context[0x30 + i*16]` for `i < 6`;
  the last row ends at `0x8F`; the live buffer is `0x90`.
- `save_ps2.c:727` saves `context_ps2` as `0x88` bytes; `gtmode_game.c:2538`
  reads it into `live + 8`, so the saved bytes are `live+8..live+0x8F` exactly.
- `career_race.points[6]`, `bodies[6]`, `keys[6]`, `paints[6]`; `menu_quick_race`
  six; `driver_points[6]`; `gt4_career_championship_rank` six.

Two honest options:

1. **Extend the save** — grow the context block and `context_ps2`, add N keys/
   paints/points, version the format, migrate old saves. Risk: original PS2 save
   compatibility, import/export, grand-prize logic, every six-row menu.
2. **Do not persist phantom points** — phantom consumes classification places;
   official drivers keep their true-place awards; no phantom row in the save;
   phantom cannot win the championship across rounds. Risk: a phantom race win
   is real only in that race result, not in the series table.

**Recommendation:** option 2 for the first build, with the limitation stated in
the UI/save notes. Option 1 only if "phantom champion" is required; it is a
save-format project, not a race-result patch.

## V1 — WHAT CHANGED

- `race_run.c` now owns one `field_entrant[]` classification: stable id,
  official/phantom flag, owner conductor, car, name, colour, finished flag,
  finish time, laps, distance, live rank and place. It is built after both
  steps, feeds `+0x5B3`, and freezes when the player finishes.
- Phantoms use distinct car labels and colours from the generic opponent pool;
  a request that cannot fill the count is refused with the real available
  number.
- Prize and championship points use the player's true place. For classification
  place `i`, an official entrant takes `awards[i]` only when `i < 6`; a phantom
  consumes the place and scores nothing.
- **V1 rule:** phantoms consume places and can take the race win. They do not
  persist across championship rounds; the career save keeps six official rows.
