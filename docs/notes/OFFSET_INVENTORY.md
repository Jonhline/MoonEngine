# Remaining raw memory offsets

This inventory covers physics C files and race_game.c with its private include fragments. It counts literal additions in the first address argument of GT4 memory accessors, gt4_ptr, and read/write helpers, plus direct calls of local offset accessor macros. Each source literal is counted once. Address arithmetic is included; an entry does not establish a field meaning. Bases retain their source expressions and may refer to different structures in different functions.

Remaining occurrences: **4622**. Baseline: **5,956**. The broader initial simple-expression search found 6,334 candidates, including arithmetic outside memory accessors.

## Lap and position subsystem round

Scope: `src/core/physics/laps.c`. Position ordering lives in `session.c` and was read as supporting evidence. Shared constants were renamed across their users without changing expressions. No fidelity grade changes.

Raw memory-offset occurrences in laps.c: **41 before, 18 after**. Existing UNK identifiers used here: **23 before, 3 after**; **20** received evidence-backed names. Sixteen additional constants replace 23 raw literal uses. Some remaining raw offsets are accesses to other structures; they have not been assigned speculative field meanings. Virtual-method slots, absolute addresses and float bit patterns are outside the raw memory-field count and remain unchanged.

### Previously unknown fields

| Old name | New name | Evidence |
| --- | --- | --- |
| `CAR_BLOCK_UNK_4AE` | `CAR_BLOCK_NEXT_SECTOR_INDEX` | func_00358608 +0x4AE: advances on sector crossing, resets at start line |
| `CAR_BLOCK_UNK_4B0` | `CAR_BLOCK_REVERSE_LAP_DEBT` | func_00358608 +0x4B0: reverse crossings add debt; forward crossings repay it |
| `CAR_BLOCK_UNK_4B2` | `CAR_BLOCK_FINISH_DRIVE_STARTED` | func_00343D78 +0x4B2: set once when finishing or pulling off |
| `CAR_BLOCK_UNK_4B4` | `CAR_BLOCK_NEXT_TIMING_POINT_INDEX` | func_00358608 +0x4B4: advances at timed points, resets each lap |
| `CAR_BLOCK_UNK_467` | `CAR_BLOCK_PIT_LANE_STATE` | func_0035B060 +0x467: 2 entering, 3 at box, 4 leaving |
| `CAR_BLOCK_UNK_4B7` | `CAR_BLOCK_PIT_BOX_INDEX` | func_0035B060 +0x4B7: signed index passed to pit-box distance/heading |
| `CAR_BLOCK_UNK_4B8` | `CAR_BLOCK_PIT_SERVICE_FLAGS` | func_0035AF18 +0x4B8: tyre/fuel/player-selection service bits |
| `CAR_BLOCK_UNK_4BA` | `CAR_BLOCK_PIT_SERVICE_COUNTDOWN` | func_0035C0B8 +0x4BA: decremented during tyre/fuel service |
| `CAR_BLOCK_UNK_614` | `CAR_BLOCK_LAP_TIMING_REFERENCE` | func_00358608 +0x614: elapsed-lap subtraction; func_003659E8 finish ordering |
| `CAR_BLOCK_UNK_650` | `CAR_BLOCK_COURSE_PROGRESS_RATE` | func_00358608 +0x650: signed distance change per frame times 60 |
| `CONDUCTOR_FIELD_UNK_D9E4_U` | `CONDUCTOR_FIELD_PIT_STOP_COUNT_U` | func_0035AD10 +0xD9E4: per-car uint16 count incremented at pit entry |
| `CAR_BLOCK_UNK_466` | `CAR_BLOCK_DRIVING_CONTROL_MODE` | func_0035BF08 block+0x466: driving control mode (0 player, 1 B-Spec) |
| `CAR_BLOCK_UNK_64C` | `CAR_BLOCK_PIT_MENU_COUNTDOWN` | func_0035BCC8/0035BCE8 block+0x64C: timeout, zero or -1 after player touch |
| `CAR_BLOCK_UNK_570` | `CAR_BLOCK_HEADING` | func_0035B060 block+0x570: pit-box heading passed as third rotation angle |
| `CAR_BLOCK_UNK_65B` | `CAR_BLOCK_TIMED_START_STATE` | func_00358608 block+0x65B: 1 awaiting timed start crossing, then 2 |
| `CAR_BLOCK_UNK_62E` | `CAR_BLOCK_PIT_STOP_REQUEST_STATE` | func_0035B060 block+0x62E: race-requested stop, states 0/1/2; physics notes Laps |
| `CAR_BLOCK_UNK_633` | `CAR_BLOCK_FFB_RAMP_COUNTDOWN` | func_00353ED8 block+0x633: starts at 30; driver force-feedback output decrements/ramp-scales |
| `CAR_BLOCK_UNK_5C3` | `CAR_BLOCK_PIT_WRONG_WAY_SPEED_LEVEL` | func_0035B060 block+0x5C3: 0, 1 above 10 km/h, 2 above 40 km/h in wrong-way pit branch |
| `CAR_BLOCK_UNK_C` | `CAR_BLOCK_ORIENTATION_BASIS_0` | func_0035B060 block+0xC: first vector output of gt4_rotation_from_angles |
| `CAR_BLOCK_UNK_18` | `CAR_BLOCK_ORIENTATION_BASIS_1` | func_0035B060 block+0x18: second vector output of gt4_rotation_from_angles |

### Unresolved existing fields

| Name | Reason retained as UNK |
| --- | --- |
| `CAR_BLOCK_UNK_56C` | Second rotation angle; assigning pitch/roll requires rotation-convention evidence. |
| `CAR_BLOCK_UNK_5C1` | Reset on lap crossing and driving-mode changes; complete flag lifecycle is not established. |
| `CONDUCTOR_FIELD_UNK_DA78_U` | Per-car objects with stride 0x4F0; exact object role and +0x43C/+0x440 flags require controller analysis. |

### All original raw access sites in laps.c

Line numbers below refer to the source before this round. Each row is one distinct literal occurrence; nested accessors count once.

| Line | Base | Literal |
| ---: | --- | --- |
| 26 | `GT4_U32(cond + CONDUCTOR_FIELD_COURSE_U)` | `4` |
| 26 | `GT4_U32(GT4_U32(cond + CONDUCTOR_FIELD_COURSE_U) + 4)` | `0xdc` |
| 26 | `GT4_U32(GT4_U32(GT4_U32(cond + CONDUCTOR_FIELD_COURSE_U) + 4) + 0xdc)` | `0x14` |
| 46 | `b` | `0x5fb` |
| 47 | `b` | `0x5fb` |
| 48 | `b` | `0x5fa` |
| 58 | `car + CAR_POSITION` | `0x464` |
| 129 | `race` | `0xe0` |
| 130 | `race` | `0x80` |
| 139 | `race` | `0xe4` |
| 142 | `race` | `0xe4` |
| 172 | `race` | `0xe0` |
| 174 | `race` | `0xe4` |
| 209 | `race` | `0xe4` |
| 217 | `race` | `0xe0` |
| 219 | `race` | `0xe4` |
| 288 | `obj` | `0x1b` |
| 289 | `GT4_U32(car + CAR_SESSION)` | `0xcbe1u` |
| 344 | `base` | `0xd9f6u` |
| 344 | `base` | `0xd9f4u` |
| 345 | `base` | `0xda26u` |
| 345 | `base` | `0xda24u` |
| 346 | `base` | `0xda27u` |
| 346 | `base` | `0xda25u` |
| 355 | `base` | `0xd9f0u` |
| 356 | `base` | `0xda20u` |
| 357 | `base` | `0xda21u` |
| 363 | `car_at(cond, idx)` | `0x56a` |
| 381 | `GT4_U32(car + CAR_PARAMS)` | `0x32` |
| 387 | `GT4_U32(car)` | `0x84` |
| 434 | `cond` | `0xda5cu` |
| 476 | `ai` | `0x43c` |
| 476 | `ai` | `0x440` |
| 543 | `b` | `0x24` |
| 595 | `b` | `0x461` |
| 646 | `ai` | `0x43c` |
| 648 | `car + CAR_FIELD_DRIVER_MODEL` | `0x1b` |
| 669 | `GT4_U32(cond)` | `0xbc` |
| 677 | `ai` | `0x43c` |
| 677 | `ai` | `0x440` |
| 690 | `b` | `0x461` |

### New constant evidence

| Name | Value/type | Evidence |
| --- | --- | --- |
| `LAPS_TIMING_RECORDS_ENABLED` | `0xe0` | func_00358608 race+0xE0: gate for timed-point/lap recording |
| `LAPS_REPLAY_SPLITS_ENABLED` | `0xe4` | func_00358608 race+0xE4: gate for replay split recording |
| `CAR_BLOCK_SIGNAL_GUARD_TAG` | `0x5fb` | func_0035ACD8 block+0x5FB: table tag suppresses zero-tag signals |
| `CAR_BLOCK_SIGNAL_KIND` | `0x5fa` | func_0035ACD8 block+0x5FA: selected signal kind |
| `CONDUCTOR_FIELD_PIT_LINE_POINT_COUNT_U` | `0xda5cu` | func_0034C750 conductor+0xDA5C: signed pit-line point count; zero disables pits |
| `CONDUCTOR_FIELD_PIT_LAP_HISTORY_HEAD_U` | `0xd9f0u` | func_0035AD10 conductor+0xD9F0: latest pit-stop lap, car stride 8 |
| `CONDUCTOR_FIELD_PIT_LAP_SHIFT_SOURCE_U` | `0xd9f4u` | func_0035AD10 conductor+0xD9F4: third lap-history element before shift |
| `CONDUCTOR_FIELD_PIT_LAP_SHIFT_DEST_U` | `0xd9f6u` | func_0035AD10 conductor+0xD9F6: fourth lap-history element after shift |
| `CAR_BLOCK_ORIENTATION_BASIS_2` | `0x24` | func_0035B060 block+0x24: third vector output of gt4_rotation_from_angles |
| `CAR_FIELD_DRIVING_CONTROL_MODE` | `0x56a` | func_0035ADF0 car+0x56A: same control-mode byte as block+0x466 |
| `CONDUCTOR_FIELD_PIT_FRONT_TYRE_HISTORY_U` | `0xda20u` | func_0035AD10/0035C0B8 +0xDA20: newest front compound; car stride 8 |
| `CONDUCTOR_FIELD_PIT_REAR_TYRE_HISTORY_U` | `0xda21u` | func_0035AD10/0035C0B8 +0xDA21: newest rear compound; car stride 8 |
| `CONDUCTOR_FIELD_PIT_FRONT_SHIFT_SOURCE_U` | `0xda24u` | func_0035AD10 +0xDA24: third front-compound history element before shift |
| `CONDUCTOR_FIELD_PIT_REAR_SHIFT_SOURCE_U` | `0xda25u` | func_0035AD10 +0xDA25: third rear-compound history element before shift |
| `CONDUCTOR_FIELD_PIT_FRONT_SHIFT_DEST_U` | `0xda26u` | func_0035AD10 +0xDA26: fourth front-compound history element after shift |
| `CONDUCTOR_FIELD_PIT_REAR_SHIFT_DEST_U` | `0xda27u` | func_0035AD10 +0xDA27: fourth rear-compound history element after shift |

| Base expression | Offset | Uses | Files (uses) |
| --- | --- | ---: | --- |
| `OF(offset)` | `0x34` | 30 | `src/core/physics/ai.c` (30) |
| `OU8(offset)` | `0x1B` | 22 | `src/core/physics/ai.c` (22) |
| `OU8(offset)` | `0x1C` | 21 | `src/core/physics/ai.c` (19), `src/core/physics/driver.c` (2) |
| `race` | `0x60` | 20 | `src/app/race_game.c` (3), `src/app/race_game_bspec.inc` (1), `src/app/race_game_results.inc` (1), `src/core/physics/init.c` (3), `src/core/physics/race.c` (7), `src/core/physics/session.c` (5) |
| `s` | `0x8` | 20 | `src/core/physics/aids.c` (2), `src/core/physics/clutch.c` (13), `src/core/physics/tyre.c` (4), `src/core/physics/walls.c` (1) |
| `w` | `0x8` | 19 | `src/core/physics/driver.c` (1), `src/core/physics/ground.c` (5), `src/core/physics/init.c` (1), `src/core/physics/misc.c` (1), `src/core/physics/walls.c` (11) |
| `t` | `0x4` | 18 | `src/core/physics/ai.c` (1), `src/core/physics/driver.c` (6), `src/core/physics/ground.c` (1), `src/core/physics/race.c` (1), `src/core/physics/session.c` (6), `src/core/physics/setup.c` (3) |
| `OU8(offset)` | `0x1D` | 17 | `src/core/physics/ai.c` (17) |
| `course` | `0x4` | 17 | `src/app/race_game.c` (2), `src/app/race_game_license.inc` (2), `src/core/physics/collision.c` (1), `src/core/physics/course_objects.c` (2), `src/core/physics/driver.c` (1), `src/core/physics/init.c` (3), `src/core/physics/lines.c` (1), `src/core/physics/session.c` (2), `src/core/physics/walls.c` (3) |
| `d` | `0x2` | 17 | `src/core/physics/setup.c` (17) |
| `GT4_U32(course + 4)` | `0xDC` | 16 | `src/app/race_game.c` (1), `src/app/race_game_license.inc` (2), `src/core/physics/collision.c` (1), `src/core/physics/course_objects.c` (2), `src/core/physics/driver.c` (1), `src/core/physics/init.c` (3), `src/core/physics/lines.c` (1), `src/core/physics/session.c` (2), `src/core/physics/walls.c` (3) |
| `car + CAR_ANGLES` | `0x4` | 16 | `src/core/physics/chassis.c` (12), `src/core/physics/suspension.c` (1), `src/core/physics/tyre.c` (2), `src/core/physics/walls.c` (1) |
| `obj` | `0xE8` | 16 | `src/core/physics/ai.c` (16) |
| `t` | `0x8` | 16 | `src/core/physics/ai.c` (1), `src/core/physics/driver.c` (7), `src/core/physics/ground.c` (2), `src/core/physics/race.c` (1), `src/core/physics/session.c` (3), `src/core/physics/setup.c` (1), `src/core/physics/slip.c` (1) |
| `car + CAR_MATRIX` | `0x18` | 15 | `src/core/physics/aero.c` (1), `src/core/physics/chassis.c` (9), `src/core/physics/race_run.c` (1), `src/core/physics/suspension.c` (4) |
| `a` | `0x14` | 14 | `src/core/physics/ai.c` (8), `src/core/physics/setup.c` (3), `src/core/physics/suspension.c` (3) |
| `car + CAR_VELOCITY` | `0x4` | 14 | `src/core/physics/aero.c` (1), `src/core/physics/body.c` (4), `src/core/physics/chassis.c` (6), `src/core/physics/ground.c` (2), `src/core/physics/suspension.c` (1) |
| `d` | `0x40` | 14 | `src/core/physics/setup.c` (14) |
| `e` | `0xC` | 14 | `src/core/physics/lines.c` (1), `src/core/physics/race.c` (1), `src/core/physics/setup.c` (6), `src/core/physics/surface.c` (5), `src/core/physics/view.c` (1) |
| `t` | `0xC` | 14 | `src/core/physics/driver.c` (8), `src/core/physics/ground.c` (1), `src/core/physics/race.c` (3), `src/core/physics/session.c` (2) |
| `GT4_U32(race + 0x60)` | `0x8` | 13 | `src/app/race_game_bspec.inc` (1), `src/app/race_game_results.inc` (1), `src/core/physics/init.c` (3), `src/core/physics/race.c` (3), `src/core/physics/session.c` (5) |
| `d` | `0x12` | 13 | `src/core/physics/setup.c` (12), `src/core/physics/view.c` (1) |
| `e` | `0x4` | 13 | `src/app/race_game.c` (3), `src/core/physics/ai.c` (1), `src/core/physics/driver.c` (3), `src/core/physics/session.c` (1), `src/core/physics/step.c` (2), `src/core/physics/view.c` (1), `src/core/physics/walls.c` (2) |
| `w` | `0x64` | 13 | `src/core/physics/aids.c` (11), `src/core/physics/driver.c` (1), `src/core/physics/misc.c` (1) |
| `OF(offset)` | `0x28` | 12 | `src/core/physics/ai.c` (12) |
| `OF(offset)` | `0x2C` | 12 | `src/core/physics/ai.c` (12) |
| `OF(offset)` | `0x30` | 12 | `src/core/physics/ai.c` (12) |
| `OU8(offset)` | `0x1E` | 12 | `src/core/physics/ai.c` (12) |
| `OU8(offset)` | `0x43` | 12 | `src/core/physics/ai.c` (12) |
| `car + CAR_ANGLES` | `0x8` | 12 | `src/core/physics/chassis.c` (8), `src/core/physics/view.c` (2), `src/core/physics/walls.c` (2) |
| `obj` | `0x22` | 12 | `src/core/physics/ai.c` (12) |
| `p` | `0x4` | 12 | `src/core/physics/ai.c` (2), `src/core/physics/lines.c` (4), `src/core/physics/setup.c` (3), `src/core/physics/walls.c` (3) |
| `p + P_A` | `0x4` | 12 | `src/core/physics/walls.c` (12) |
| `params` | `0x32` | 12 | `src/core/physics/aids.c` (1), `src/core/physics/chassis.c` (1), `src/core/physics/driver.c` (3), `src/core/physics/drivetrain.c` (1), `src/core/physics/init.c` (4), `src/core/physics/suspension.c` (1), `src/core/physics/wear.c` (1) |
| `race` | `0x84` | 12 | `src/core/physics/race.c` (5), `src/core/physics/session.c` (6), `src/core/physics/step.c` (1) |
| `OF(offset)` | `0x38` | 11 | `src/core/physics/ai.c` (11) |
| `cmd` | `0x4` | 11 | `src/core/physics/driver.c` (11) |
| `p` | `0x8` | 11 | `src/core/physics/ai.c` (1), `src/core/physics/init.c` (1), `src/core/physics/lines.c` (4), `src/core/physics/session.c` (1), `src/core/physics/setup.c` (2), `src/core/physics/walls.c` (2) |
| `p` | `0x24` | 11 | `src/core/physics/lines.c` (4), `src/core/physics/session.c` (1), `src/core/physics/setup.c` (6) |
| `race` | `0xDC` | 11 | `src/app/race_game_pregrid.inc` (1), `src/app/race_game_replay.inc` (1), `src/core/physics/license_session.c` (1), `src/core/physics/race.c` (8) |
| `v` | `0x8` | 11 | `src/app/race_game_license.inc` (1), `src/core/physics/init.c` (1), `src/core/physics/race_run.c` (1), `src/core/physics/session.c` (2), `src/core/physics/walls.c` (6) |
| `GT4_U32(car + CAR_PARAMS)` | `0x32` | 10 | `src/core/physics/ai.c` (1), `src/core/physics/gearbox.c` (2), `src/core/physics/init.c` (1), `src/core/physics/laps.c` (1), `src/core/physics/misc.c` (1), `src/core/physics/step.c` (1), `src/core/physics/view.c` (1), `src/core/physics/walls.c` (2) |
| `OF(offset)` | `0x24` | 10 | `src/core/physics/ai.c` (10) |
| `a` | `0x8` | 10 | `src/core/physics/ai.c` (3), `src/core/physics/lines.c` (1), `src/core/physics/setup.c` (4), `src/core/physics/suspension.c` (1), `src/core/physics/walls.c` (1) |
| `a` | `0xC` | 10 | `src/core/physics/ai.c` (4), `src/core/physics/lines.c` (1), `src/core/physics/setup.c` (4), `src/core/physics/suspension.c` (1) |
| `obj` | `0x18` | 10 | `src/core/physics/ai.c` (10) |
| `p` | `0x18` | 10 | `src/core/physics/ai.c` (1), `src/core/physics/init.c` (1), `src/core/physics/lines.c` (2), `src/core/physics/session.c` (1), `src/core/physics/setup.c` (5) |
| `s` | `0x4` | 10 | `src/core/physics/aids.c` (1), `src/core/physics/clutch.c` (5), `src/core/physics/tyre.c` (1), `src/core/physics/walls.c` (3) |
| `OU8(offset)` | `0x51` | 9 | `src/core/physics/ai.c` (9) |
| `OU8(offset)` | `0x52` | 9 | `src/core/physics/ai.c` (9) |
| `a` | `0x4` | 9 | `src/core/physics/ai.c` (4), `src/core/physics/lines.c` (1), `src/core/physics/setup.c` (3), `src/core/physics/suspension.c` (1) |
| `course` | `0xB0` | 9 | `src/app/race_game.c` (3), `src/app/race_game_license.inc` (1), `src/core/physics/course_objects.c` (1), `src/core/physics/init.c` (2), `src/core/physics/session.c` (2) |
| `params` | `0x18` | 9 | `src/app/race_game_license.inc` (1), `src/core/physics/aids.c` (1), `src/core/physics/chassis.c` (6), `src/core/physics/clutch.c` (1) |
| `params` | `0x20` | 9 | `src/core/physics/chassis.c` (9) |
| `race` | `0xBC` | 9 | `src/app/race_game.c` (1), `src/app/race_game_logger.inc` (1), `src/core/physics/race.c` (5), `src/core/physics/session.c` (2) |
| `w` | `0xAC` | 9 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (4), `src/core/physics/race.c` (1), `src/core/physics/tyre.c` (3) |
| `w + WHEEL_MOUNT` | `0x4` | 9 | `src/app/race_game.c` (1), `src/core/physics/chassis.c` (7), `src/core/physics/race_run.c` (1) |
| `b` | `0x4` | 8 | `src/core/physics/ai.c` (2), `src/core/physics/lines.c` (1), `src/core/physics/view.c` (5) |
| `b` | `0xC` | 8 | `src/core/physics/ai.c` (2), `src/core/physics/lines.c` (1), `src/core/physics/view.c` (5) |
| `b` | `0x24` | 8 | `src/core/physics/init.c` (1), `src/core/physics/lines.c` (2), `src/core/physics/session.c` (1), `src/core/physics/walls.c` (4) |
| `eb` | `0x4` | 8 | `src/core/physics/walls.c` (8) |
| `layout` | `0x14` | 8 | `src/core/physics/ai.c` (1), `src/core/physics/aids.c` (3), `src/core/physics/misc.c` (1), `src/core/physics/steer.c` (1), `src/core/physics/wear.c` (2) |
| `lines` | `0x8` | 8 | `src/core/physics/ai.c` (4), `src/core/physics/lines.c` (4) |
| `obj` | `0xEC` | 8 | `src/core/physics/ai.c` (8) |
| `p` | `0x20` | 8 | `src/core/physics/ai.c` (1), `src/core/physics/init.c` (1), `src/core/physics/session.c` (1), `src/core/physics/setup.c` (4), `src/core/physics/walls.c` (1) |
| `p` | `0x28` | 8 | `src/core/physics/ai.c` (2), `src/core/physics/setup.c` (6) |
| `p + P_B` | `0x4` | 8 | `src/core/physics/walls.c` (8) |
| `race` | `0x80` | 8 | `src/app/race_game.c` (1), `src/core/physics/init.c` (6), `src/core/physics/laps.c` (1) |
| `race` | `0x88` | 8 | `src/core/physics/license_session.c` (1), `src/core/physics/race_run.c` (1), `src/core/physics/session.c` (6) |
| `t` | `0x14` | 8 | `src/core/physics/ground.c` (1), `src/core/physics/init.c` (3), `src/core/physics/race.c` (1), `src/core/physics/session.c` (2), `src/core/physics/setup.c` (1) |
| `w` | `0x50` | 8 | `src/core/physics/driver.c` (2), `src/core/physics/init.c` (5), `src/core/physics/race.c` (1) |
| `w + WHEEL_MOUNT` | `0x8` | 8 | `src/app/race_game.c` (1), `src/core/physics/chassis.c` (7) |
| `OU8(offset)` | `0x1F` | 7 | `src/core/physics/ai.c` (7) |
| `a` | `0x18` | 7 | `src/core/physics/ai.c` (2), `src/core/physics/lines.c` (1), `src/core/physics/setup.c` (3), `src/core/physics/suspension.c` (1) |
| `b` | `0x0` | 7 | `src/core/physics/view.c` (7) |
| `c` | `0x4` | 7 | `src/app/race_game.c` (1), `src/core/physics/ai.c` (1), `src/core/physics/aids.c` (2), `src/core/physics/init.c` (1), `src/core/physics/session.c` (1), `src/core/physics/setup.c` (1) |
| `car + CAR_MATRIX` | `0xC` | 7 | `src/core/physics/body.c` (1), `src/core/physics/chassis.c` (5), `src/core/physics/walls.c` (1) |
| `car + CAR_MATRIX` | `0x20` | 7 | `src/core/physics/aero.c` (1), `src/core/physics/chassis.c` (2), `src/core/physics/ground.c` (1), `src/core/physics/suspension.c` (3) |
| `car + CAR_MATRIX + 0x18` | `0x4` | 7 | `src/core/physics/chassis.c` (4), `src/core/physics/race_run.c` (1), `src/core/physics/suspension.c` (2) |
| `car + CAR_POSITION` | `0x4` | 7 | `src/core/physics/ai.c` (3), `src/core/physics/chassis.c` (2), `src/core/physics/race_run.c` (1), `src/core/physics/suspension.c` (1) |
| `d` | `0x90` | 7 | `src/core/physics/setup.c` (7) |
| `drive` | `0x3` | 7 | `src/core/physics/clutch.c` (5), `src/core/physics/ground.c` (1), `src/core/physics/wheels.c` (1) |
| `dst` | `0x5` | 7 | `src/core/physics/ai.c` (7) |
| `l` | `0xC` | 7 | `src/core/physics/race.c` (7) |
| `m` | `0x3` | 7 | `src/core/physics/misc.c` (7) |
| `o + H_POS` | `0x4` | 7 | `src/core/physics/walls.c` (7) |
| `obj` | `0x68` | 7 | `src/core/physics/ai.c` (7) |
| `p` | `0x1C` | 7 | `src/core/physics/ai.c` (2), `src/core/physics/setup.c` (5) |
| `p` | `0x2C` | 7 | `src/core/physics/session.c` (3), `src/core/physics/setup.c` (3), `src/core/physics/walls.c` (1) |
| `p` | `0xFD8` | 7 | `src/core/physics/init.c` (2), `src/core/physics/setup.c` (5) |
| `race` | `0xB8` | 7 | `src/core/physics/race.c` (7) |
| `s` | `0x9` | 7 | `src/core/physics/clutch.c` (7) |
| `t` | `0x10` | 7 | `src/core/physics/ai.c` (1), `src/core/physics/chassis.c` (1), `src/core/physics/ground.c` (1), `src/core/physics/race.c` (1), `src/core/physics/session.c` (3) |
| `w` | `0x4` | 7 | `src/core/physics/driver.c` (1), `src/core/physics/ground.c` (4), `src/core/physics/init.c` (1), `src/core/physics/misc.c` (1) |
| `world` | `0xCC8` | 7 | `src/app/race_game.c` (2), `src/core/physics/driver.c` (4), `src/core/physics/session.c` (1) |
| `GT4_U32(cond + CONDUCTOR_FIELD_COURSE_U)` | `0x4` | 6 | `src/app/race_game_bspec.inc` (1), `src/core/physics/init.c` (2), `src/core/physics/laps.c` (1), `src/core/physics/misc.c` (1), `src/core/physics/session.c` (1) |
| `OF(offset)` | `0x58` | 6 | `src/core/physics/ai.c` (6) |
| `OF(offset)` | `0x5C` | 6 | `src/core/physics/ai.c` (6) |
| `OU8(offset)` | `0x20` | 6 | `src/core/physics/ai.c` (6) |
| `WORLD` | `0x64` | 6 | `src/core/physics/race_run.c` (6) |
| `b` | `0x8` | 6 | `src/core/physics/ai.c` (2), `src/core/physics/lines.c` (1), `src/core/physics/view.c` (2), `src/core/physics/walls.c` (1) |
| `b` | `0x14` | 6 | `src/core/physics/ai.c` (3), `src/core/physics/chassis.c` (2), `src/core/physics/session.c` (1) |
| `c` | `0x8` | 6 | `src/core/physics/ai.c` (1), `src/core/physics/aids.c` (2), `src/core/physics/init.c` (1), `src/core/physics/session.c` (1), `src/core/physics/setup.c` (1) |
| `car + CAR_LOAD_TARGET` | `0x4` | 6 | `src/core/physics/chassis.c` (4), `src/core/physics/suspension.c` (2) |
| `car + CAR_POSITION` | `0x8` | 6 | `src/core/physics/ai.c` (1), `src/core/physics/chassis.c` (4), `src/core/physics/race_run.c` (1) |
| `car + CAR_VELOCITY` | `0x8` | 6 | `src/core/physics/aero.c` (1), `src/core/physics/chassis.c` (4), `src/core/physics/suspension.c` (1) |
| `d` | `0x10` | 6 | `src/core/physics/setup.c` (6) |
| `d` | `0x26` | 6 | `src/core/physics/driver.c` (6) |
| `e` | `0x1` | 6 | `src/core/physics/car.c` (2), `src/core/physics/driver.c` (3), `src/core/physics/setup.c` (1) |
| `e` | `0x8` | 6 | `src/app/race_game.c` (2), `src/core/physics/lines.c` (2), `src/core/physics/runway.c` (1), `src/core/physics/view.c` (1) |
| `e` | `0x14` | 6 | `src/app/race_game.c` (1), `src/core/physics/runway.c` (1), `src/core/physics/surface.c` (3), `src/core/physics/view.c` (1) |
| `e` | `0x18` | 6 | `src/app/race_game.c` (2), `src/core/physics/race.c` (3), `src/core/physics/view.c` (1) |
| `o + H_VEL` | `0x4` | 6 | `src/core/physics/walls.c` (6) |
| `obj` | `0xE4` | 6 | `src/core/physics/ai.c` (6) |
| `p` | `0x10` | 6 | `src/core/physics/ai.c` (1), `src/core/physics/session.c` (1), `src/core/physics/setup.c` (4) |
| `r` | `0xDC` | 6 | `src/core/physics/license_session.c` (2), `src/core/physics/session.c` (4) |
| `rec` | `0xBF` | 6 | `src/core/physics/setup.c` (6) |
| `s` | `0x50` | 6 | `src/core/physics/tyre.c` (2), `src/core/physics/wear.c` (4) |
| `src` | `0x2` | 6 | `src/core/physics/ai.c` (1), `src/core/physics/session.c` (1), `src/core/physics/setup.c` (4) |
| `t` | `0x18` | 6 | `src/core/physics/ai.c` (2), `src/core/physics/ground.c` (1), `src/core/physics/init.c` (1), `src/core/physics/session.c` (2) |
| `w` | `0x10` | 6 | `src/core/physics/ground.c` (4), `src/core/physics/init.c` (1), `src/core/physics/view.c` (1) |
| `w` | `0x70` | 6 | `src/core/physics/driver.c` (3), `src/core/physics/init.c` (1), `src/core/physics/view.c` (1), `src/core/physics/wear.c` (1) |
| `w` | `0x8C` | 6 | `src/core/physics/driver.c` (1), `src/core/physics/ground.c` (1), `src/core/physics/init.c` (2), `src/core/physics/misc.c` (1), `src/core/physics/view.c` (1) |
| `w` | `0xB1` | 6 | `src/core/physics/init.c` (4), `src/core/physics/tyre.c` (2) |
| `D` | `0x4` | 5 | `src/core/physics/session.c` (5) |
| `D` | `0x8` | 5 | `src/core/physics/session.c` (5) |
| `GT4_U32(GT4_U32(cond + CONDUCTOR_FIELD_COURSE_U) + 4)` | `0xDC` | 5 | `src/app/race_game_bspec.inc` (1), `src/core/physics/init.c` (2), `src/core/physics/laps.c` (1), `src/core/physics/misc.c` (1) |
| `HU(offset)` | `0x3F0` | 5 | `src/core/physics/walls.c` (5) |
| `OF(offset)` | `0x44` | 5 | `src/core/physics/ai.c` (5) |
| `OF(offset)` | `0x54` | 5 | `src/core/physics/ai.c` (5) |
| `OU8(offset)` | `0x6B` | 5 | `src/core/physics/ai.c` (5) |
| `a` | `0x1C` | 5 | `src/core/physics/setup.c` (4), `src/core/physics/suspension.c` (1) |
| `a` | `0x2C` | 5 | `src/core/physics/lines.c` (4), `src/core/physics/setup.c` (1) |
| `car + CAR_SUSP_REACH` | `0x4` | 5 | `src/core/physics/chassis.c` (1), `src/core/physics/suspension.c` (4) |
| `d` | `0x1` | 5 | `src/core/physics/init.c` (1), `src/core/physics/setup.c` (4) |
| `d` | `0x14` | 5 | `src/core/physics/session.c` (1), `src/core/physics/setup.c` (4) |
| `d` | `0x1C` | 5 | `src/core/physics/session.c` (1), `src/core/physics/setup.c` (4) |
| `d` | `0x2C` | 5 | `src/core/physics/setup.c` (5) |
| `d` | `0x2E` | 5 | `src/core/physics/setup.c` (5) |
| `drive_of(car)` | `0x13` | 5 | `src/core/physics/clutch.c` (5) |
| `e` | `0x2` | 5 | `src/app/race_game.c` (1), `src/core/physics/car.c` (1), `src/core/physics/driver.c` (3) |
| `e` | `0xFC` | 5 | `src/core/physics/setup.c` (5) |
| `entry` | `0x3444` | 5 | `src/app/race_game_bspec.inc` (1), `src/core/physics/init.c` (3), `src/core/physics/session.c` (1) |
| `lines` | `0xC` | 5 | `src/core/physics/ai.c` (3), `src/core/physics/lines.c` (2) |
| `m` | `0x2` | 5 | `src/core/physics/misc.c` (5) |
| `p` | `0x14` | 5 | `src/core/physics/init.c` (1), `src/core/physics/setup.c` (4) |
| `p` | `0x33` | 5 | `src/core/physics/session.c` (3), `src/core/physics/setup.c` (2) |
| `p` | `0x1040` | 5 | `src/core/physics/init.c` (1), `src/core/physics/setup.c` (4) |
| `p` | `0x1048` | 5 | `src/core/physics/setup.c` (5) |
| `p + P_WALL_DIR` | `0x4` | 5 | `src/core/physics/walls.c` (5) |
| `params` | `0x262` | 5 | `src/core/physics/ai.c` (4), `src/core/physics/race_run.c` (1) |
| `params` | `0x1040` | 5 | `src/core/physics/driver.c` (3), `src/core/physics/steer.c` (1), `src/core/physics/suspension.c` (1) |
| `prof` | `0x80` | 5 | `src/core/physics/ai.c` (5) |
| `r` | `0xC` | 5 | `src/core/physics/driver.c` (1), `src/core/physics/race.c` (1), `src/core/physics/setup.c` (3) |
| `rec` | `0x128` | 5 | `src/core/physics/setup.c` (5) |
| `s` | `0xC` | 5 | `src/core/physics/aids.c` (1), `src/core/physics/tyre.c` (3), `src/core/physics/walls.c` (1) |
| `s` | `0x10` | 5 | `src/core/physics/aids.c` (1), `src/core/physics/tyre.c` (4) |
| `s` | `0x18` | 5 | `src/core/physics/aids.c` (2), `src/core/physics/tyre.c` (3) |
| `src` | `0x1` | 5 | `src/core/physics/ai.c` (1), `src/core/physics/session.c` (1), `src/core/physics/setup.c` (3) |
| `src` | `0x3` | 5 | `src/core/physics/session.c` (1), `src/core/physics/setup.c` (4) |
| `t` | `0x4C` | 5 | `src/core/physics/session.c` (5) |
| `v` | `0xC` | 5 | `src/app/race_game_license.inc` (1), `src/core/physics/init.c` (1), `src/core/physics/race_run.c` (1), `src/core/physics/walls.c` (2) |
| `w` | `0x38` | 5 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (1), `src/core/physics/view.c` (3) |
| `w` | `0x68` | 5 | `src/core/physics/driver.c` (2), `src/core/physics/init.c` (1), `src/core/physics/steer.c` (1), `src/core/physics/wear.c` (1) |
| `w` | `0x74` | 5 | `src/core/physics/driver.c` (2), `src/core/physics/init.c` (1), `src/core/physics/view.c` (1), `src/core/physics/wear.c` (1) |
| `w` | `0x89` | 5 | `src/core/physics/init.c` (2), `src/core/physics/tyre.c` (1), `src/core/physics/view.c` (2) |
| `w` | `0x90` | 5 | `src/core/physics/driver.c` (1), `src/core/physics/ground.c` (3), `src/core/physics/init.c` (1) |
| `w` | `0xA4` | 5 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (1), `src/core/physics/misc.c` (3) |
| `w` | `0xA8` | 5 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (3), `src/core/physics/tyre.c` (1) |
| `AI_SETTINGS` | `0x5C` | 4 | `src/core/physics/ai.c` (4) |
| `D` | `0xC` | 4 | `src/core/physics/session.c` (4) |
| `D` | `0x10` | 4 | `src/core/physics/session.c` (4) |
| `GT4_U32(GT4_U32(GT4_U32(cond + CONDUCTOR_FIELD_COURSE_U) + 4) + 0xdc)` | `0x14` | 4 | `src/app/race_game_bspec.inc` (1), `src/core/physics/init.c` (1), `src/core/physics/laps.c` (1), `src/core/physics/misc.c` (1) |
| `GT4_U32(car)` | `0x84` | 4 | `src/core/physics/driver.c` (3), `src/core/physics/laps.c` (1) |
| `GT4_U32(race + 0x84)` | `0xCC8` | 4 | `src/core/physics/race.c` (1), `src/core/physics/session.c` (2), `src/core/physics/step.c` (1) |
| `HB(offset)` | `0x5A` | 4 | `src/core/physics/setup.c` (4) |
| `OU8(offset)` | `0x21` | 4 | `src/core/physics/ai.c` (4) |
| `OU8(offset)` | `0x3F` | 4 | `src/core/physics/ai.c` (4) |
| `OU8(offset)` | `0x40` | 4 | `src/core/physics/ai.c` (3), `src/core/physics/driver.c` (1) |
| `OU8(offset)` | `0x41` | 4 | `src/core/physics/ai.c` (3), `src/core/physics/driver.c` (1) |
| `WORLD` | `0xD68` | 4 | `src/core/physics/race_run.c` (4) |
| `a` | `0x10` | 4 | `src/core/physics/setup.c` (4) |
| `a` | `0x24` | 4 | `src/core/physics/ai.c` (1), `src/core/physics/lines.c` (2), `src/core/physics/setup.c` (1) |
| `a` | `0x30` | 4 | `src/core/physics/lines.c` (2), `src/core/physics/setup.c` (2) |
| `adln` | `0x4` | 4 | `src/core/physics/lines.c` (4) |
| `adln` | `0x18` | 4 | `src/core/physics/lines.c` (4) |
| `b` | `0x10` | 4 | `src/core/physics/session.c` (1), `src/core/physics/view.c` (3) |
| `c` | `0x2` | 4 | `src/core/physics/ai.c` (1), `src/core/physics/init.c` (1), `src/core/physics/session.c` (1), `src/core/physics/setup.c` (1) |
| `c` | `0x3` | 4 | `src/core/physics/ai.c` (1), `src/core/physics/init.c` (1), `src/core/physics/session.c` (1), `src/core/physics/setup.c` (1) |
| `car + CAR_140` | `0xC` | 4 | `src/core/physics/chassis.c` (3), `src/core/physics/ground.c` (1) |
| `car + CAR_DOWNFORCE` | `0x4` | 4 | `src/core/physics/aero.c` (2), `src/core/physics/chassis.c` (1), `src/core/physics/suspension.c` (1) |
| `car + CAR_MATRIX` | `0x4` | 4 | `src/core/physics/body.c` (1), `src/core/physics/chassis.c` (2), `src/core/physics/race_run.c` (1) |
| `car + CAR_MATRIX` | `0x1C` | 4 | `src/core/physics/aero.c` (1), `src/core/physics/chassis.c` (1), `src/core/physics/suspension.c` (2) |
| `car + CAR_POSITION` | `0x38` | 4 | `src/core/physics/chassis.c` (4) |
| `car + CAR_POSITION` | `0x440` | 4 | `src/app/race_game_bspec.inc` (1), `src/core/physics/suspension.c` (2), `src/core/physics/view.c` (1) |
| `car + CAR_POSITION` | `0x4B3` | 4 | `src/core/physics/session.c` (4) |
| `car + CAR_POSITION` | `0x4B6` | 4 | `src/core/physics/session.c` (4) |
| `car + CAR_POSITION` | `0x614` | 4 | `src/core/physics/session.c` (4) |
| `car + CAR_REF_A` | `0x4` | 4 | `src/core/physics/chassis.c` (4) |
| `car + CAR_STEER_RAW` | `0x2` | 4 | `src/core/physics/driver.c` (4) |
| `car + CAR_WHEEL_TURNS` | `0x4` | 4 | `src/core/physics/chassis.c` (4) |
| `cmd` | `0x1` | 4 | `src/core/physics/ai.c` (1), `src/core/physics/driver.c` (3) |
| `cmd` | `0x8` | 4 | `src/core/physics/driver.c` (4) |
| `cmd` | `0xC` | 4 | `src/core/physics/driver.c` (4) |
| `d` | `0x3` | 4 | `src/core/physics/init.c` (1), `src/core/physics/setup.c` (3) |
| `d` | `0x24` | 4 | `src/core/physics/setup.c` (4) |
| `drive` | `0x2` | 4 | `src/core/physics/ai.c` (1), `src/core/physics/clutch.c` (1), `src/core/physics/gearbox.c` (2) |
| `e` | `0x1C` | 4 | `src/app/race_game.c` (2), `src/core/physics/view.c` (1), `src/core/physics/walls.c` (1) |
| `e` | `0xCE` | 4 | `src/core/physics/setup.c` (4) |
| `e` | `0xE4` | 4 | `src/core/physics/setup.c` (4) |
| `e` | `0x2880` | 4 | `src/core/physics/race.c` (4) |
| `ea + E_NORMAL` | `0x4` | 4 | `src/core/physics/walls.c` (4) |
| `g` | `0x1C` | 4 | `src/core/physics/chassis.c` (1), `src/core/physics/suspension.c` (2), `src/core/physics/view.c` (1) |
| `g` | `0x33` | 4 | `src/core/physics/init.c` (2), `src/core/physics/view.c` (2) |
| `l` | `0x4` | 4 | `src/core/physics/lines.c` (1), `src/core/physics/race.c` (3) |
| `m` | `0x4` | 4 | `src/core/physics/misc.c` (4) |
| `obj` | `0x2C` | 4 | `src/core/physics/ai.c` (4) |
| `order + (uint32_t)(j` | `0x1` | 4 | `src/core/physics/session.c` (4) |
| `p` | `0x31` | 4 | `src/core/physics/session.c` (2), `src/core/physics/setup.c` (2) |
| `p` | `0x32` | 4 | `src/core/physics/init.c` (1), `src/core/physics/session.c` (2), `src/core/physics/setup.c` (1) |
| `p` | `0x34` | 4 | `src/core/physics/lines.c` (1), `src/core/physics/session.c` (3) |
| `p` | `0x3C` | 4 | `src/core/physics/init.c` (2), `src/core/physics/lines.c` (1), `src/core/physics/setup.c` (1) |
| `p + PARAMS_DOWNFORCE` | `0x4` | 4 | `src/core/physics/setup.c` (4) |
| `params` | `0x50` | 4 | `src/app/race_game.c` (2), `src/app/race_game_cameras.inc` (2) |
| `params` | `0x10C` | 4 | `src/core/physics/steer.c` (2), `src/core/physics/suspension.c` (2) |
| `params` | `0xFD8` | 4 | `src/core/physics/ai.c` (3), `src/core/physics/race_run.c` (1) |
| `params` | `0x107C` | 4 | `src/core/physics/init.c` (2), `src/core/physics/misc.c` (2) |
| `prof` | `0x8` | 4 | `src/core/physics/ai.c` (4) |
| `prof` | `0xC` | 4 | `src/core/physics/ai.c` (4) |
| `prof` | `0x1C` | 4 | `src/core/physics/ai.c` (4) |
| `prof` | `0x74` | 4 | `src/core/physics/ai.c` (2), `src/core/physics/init.c` (2) |
| `r` | `0xA` | 4 | `src/core/physics/driver.c` (1), `src/core/physics/setup.c` (3) |
| `r` | `0x928` | 4 | `src/core/physics/session.c` (4) |
| `r` | `0x968` | 4 | `src/core/physics/license_session.c` (3), `src/core/physics/session.c` (1) |
| `race` | `0xB4` | 4 | `src/core/physics/race.c` (4) |
| `race` | `0xC8` | 4 | `src/core/physics/race.c` (4) |
| `rec` | `0x5` | 4 | `src/core/physics/setup.c` (4) |
| `rec` | `0x12B` | 4 | `src/core/physics/setup.c` (4) |
| `rec` | `0x158` | 4 | `src/core/physics/setup.c` (4) |
| `rec` | `0x1B8` | 4 | `src/core/physics/init.c` (3), `src/core/physics/setup.c` (1) |
| `s` | `0x0` | 4 | `src/core/physics/clutch.c` (2), `src/core/physics/tyre.c` (2) |
| `s` | `0x2` | 4 | `src/core/physics/session.c` (1), `src/core/physics/walls.c` (3) |
| `s` | `0x14` | 4 | `src/core/physics/aids.c` (1), `src/core/physics/tyre.c` (3) |
| `s` | `0x1C` | 4 | `src/core/physics/aids.c` (1), `src/core/physics/session.c` (1), `src/core/physics/tyre.c` (2) |
| `s` | `0x28` | 4 | `src/core/physics/aids.c` (1), `src/core/physics/wear.c` (3) |
| `s` | `0x60` | 4 | `src/core/physics/tyre.c` (1), `src/core/physics/wear.c` (3) |
| `src` | `0x30` | 4 | `src/core/physics/ai.c` (2), `src/core/physics/setup.c` (2) |
| `t` | `0x1C` | 4 | `src/core/physics/ground.c` (1), `src/core/physics/init.c` (1), `src/core/physics/session.c` (2) |
| `t` | `0x20` | 4 | `src/core/physics/ai.c` (1), `src/core/physics/ground.c` (1), `src/core/physics/session.c` (2) |
| `t` | `0x68` | 4 | `src/core/physics/session.c` (2), `src/core/physics/setup.c` (2) |
| `w` | `0x1C` | 4 | `src/core/physics/driver.c` (2), `src/core/physics/init.c` (2) |
| `w` | `0x5C` | 4 | `src/core/physics/aids.c` (2), `src/core/physics/driver.c` (1), `src/core/physics/init.c` (1) |
| `w` | `0x94` | 4 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (3) |
| `w` | `0xC0` | 4 | `src/core/physics/init.c` (1), `src/core/physics/misc.c` (3) |
| `world` | `0x64` | 4 | `src/core/physics/race.c` (1), `src/core/physics/session.c` (3) |
| `(c)` | `0x10000` | 3 | `src/core/physics/ai.c` (3) |
| `AID_TABLE` | `0x117` | 3 | `src/core/physics/aids.c` (3) |
| `F(offset)` | `0x728` | 3 | `src/core/physics/driver.c` (3) |
| `GT4_U32(car + CAR_PARAMS)` | `0x1076` | 3 | `src/core/physics/init.c` (1), `src/core/physics/suspension.c` (2) |
| `GT4_U32(cond)` | `0x84` | 3 | `src/app/race_game.c` (1), `src/core/physics/session.c` (2) |
| `LOOSE_SURFACE` | `0x4` | 3 | `src/core/physics/surface.c` (3) |
| `LOOSE_SURFACE` | `0x8` | 3 | `src/core/physics/surface.c` (3) |
| `OF(offset)` | `0xB8` | 3 | `src/core/physics/ai.c` (3) |
| `OF(offset)` | `0xBC` | 3 | `src/core/physics/ai.c` (3) |
| `OU8(offset)` | `0x1A` | 3 | `src/core/physics/ai.c` (3) |
| `OU8(offset)` | `0x3E` | 3 | `src/core/physics/ai.c` (3) |
| `OU8(offset)` | `0x4A` | 3 | `src/core/physics/ai.c` (3) |
| `OU8(offset)` | `0x4C` | 3 | `src/core/physics/ai.c` (3) |
| `OU8(offset)` | `0x6E` | 3 | `src/core/physics/ai.c` (1), `src/core/physics/driver.c` (2) |
| `RACE` | `0x78` | 3 | `src/core/physics/race_run.c` (3) |
| `RACE` | `0x84` | 3 | `src/core/physics/race_run.c` (3) |
| `SB(offset)` | `0x9` | 3 | `src/core/physics/ai.c` (1), `src/core/physics/session.c` (2) |
| `SETTINGS` | `0xA0` | 3 | `src/core/physics/race_run.c` (3) |
| `U8(offset)` | `0x565` | 3 | `src/core/physics/driver.c` (3) |
| `a` | `0x28` | 3 | `src/core/physics/lines.c` (2), `src/core/physics/setup.c` (1) |
| `a` | `0x34` | 3 | `src/core/physics/lines.c` (2), `src/core/physics/setup.c` (1) |
| `a` | `0x74` | 3 | `src/core/physics/ai.c` (3) |
| `a1` | `0x718` | 3 | `src/core/physics/session.c` (3) |
| `ai` | `0x43C` | 3 | `src/core/physics/laps.c` (3) |
| `b` | `0x20` | 3 | `src/core/physics/session.c` (2), `src/core/physics/walls.c` (1) |
| `b` | `0x2C` | 3 | `src/core/physics/init.c` (1), `src/core/physics/session.c` (2) |
| `b` | `0x38` | 3 | `src/core/physics/chassis.c` (1), `src/core/physics/init.c` (2) |
| `b` | `0x444` | 3 | `src/core/physics/init.c` (3) |
| `b` | `0x456` | 3 | `src/core/physics/driver.c` (2), `src/core/physics/init.c` (1) |
| `b` | `0x461` | 3 | `src/core/physics/init.c` (1), `src/core/physics/laps.c` (2) |
| `b` | `0x4C0` | 3 | `src/core/physics/init.c` (3) |
| `b` | `0x4E8` | 3 | `src/core/physics/ai.c` (1), `src/core/physics/init.c` (1), `src/core/physics/view.c` (1) |
| `b` | `0x4F0` | 3 | `src/core/physics/init.c` (2), `src/core/physics/view.c` (1) |
| `b` | `0x51C` | 3 | `src/core/physics/clutch.c` (2), `src/core/physics/init.c` (1) |
| `b` | `0x544` | 3 | `src/core/physics/ai.c` (3) |
| `b` | `0x590` | 3 | `src/core/physics/init.c` (3) |
| `b` | `0x594` | 3 | `src/core/physics/init.c` (3) |
| `b` | `0x5C2` | 3 | `src/core/physics/ai.c` (2), `src/core/physics/init.c` (1) |
| `b` | `0x600` | 3 | `src/app/race_game_license.inc` (1), `src/core/physics/init.c` (1), `src/core/physics/race_run.c` (1) |
| `b` | `0x60E` | 3 | `src/core/physics/init.c` (1), `src/core/physics/session.c` (2) |
| `b` | `0x610` | 3 | `src/core/physics/init.c` (1), `src/core/physics/view.c` (2) |
| `b` | `0x630` | 3 | `src/core/physics/init.c` (1), `src/core/physics/walls.c` (2) |
| `blk` | `0x1` | 3 | `src/core/physics/driver.c` (3) |
| `car + CAR_140` | `0x4` | 3 | `src/core/physics/chassis.c` (2), `src/core/physics/ground.c` (1) |
| `car + CAR_140` | `0x18` | 3 | `src/core/physics/chassis.c` (3) |
| `car + CAR_CAMBER_FORCE` | `0x4` | 3 | `src/core/physics/body.c` (1), `src/core/physics/tyre.c` (2) |
| `car + CAR_ENGINE_WEAR` | `0xC` | 3 | `src/core/physics/surface.c` (3) |
| `car + CAR_ENGINE_WEAR` | `0x14` | 3 | `src/core/physics/ground.c` (1), `src/core/physics/init.c` (1), `src/core/physics/surface.c` (1) |
| `car + CAR_POSITION` | `0x441` | 3 | `src/app/race_game.c` (1), `src/core/physics/race_run.c` (1), `src/core/physics/session.c` (1) |
| `car + CAR_POSITION` | `0x466` | 3 | `src/core/physics/ai.c` (1), `src/core/physics/driver.c` (1), `src/core/physics/race_run.c` (1) |
| `car + CAR_REF_A` | `0x8` | 3 | `src/core/physics/chassis.c` (3) |
| `car + CAR_REF_B` | `0x4` | 3 | `src/core/physics/chassis.c` (2), `src/core/physics/race_run.c` (1) |
| `car + CAR_TCS_THROTTLE` | `0x4` | 3 | `src/core/physics/aids.c` (3) |
| `cmd` | `0xA` | 3 | `src/core/physics/driver.c` (3) |
| `cond` | `0xCBE3` | 3 | `src/core/physics/init.c` (1), `src/core/physics/session.c` (2) |
| `d` | `0xC` | 3 | `src/core/physics/session.c` (1), `src/core/physics/setup.c` (2) |
| `d` | `0x13` | 3 | `src/core/physics/setup.c` (3) |
| `d` | `0x22` | 3 | `src/core/physics/driver.c` (3) |
| `d` | `0x30` | 3 | `src/core/physics/setup.c` (3) |
| `d` | `0x48` | 3 | `src/core/physics/setup.c` (3) |
| `drive` | `0x1` | 3 | `src/core/physics/clutch.c` (1), `src/core/physics/ground.c` (1), `src/core/physics/wheels.c` (1) |
| `drive` | `0x4` | 3 | `src/core/physics/clutch.c` (3) |
| `drive` | `0x13` | 3 | `src/core/physics/clutch.c` (2), `src/core/physics/race_run.c` (1) |
| `drive` | `0x14` | 3 | `src/core/physics/wheels.c` (3) |
| `e` | `0x3` | 3 | `src/core/physics/car.c` (1), `src/core/physics/driver.c` (2) |
| `e` | `0x10` | 3 | `src/core/physics/race.c` (1), `src/core/physics/surface.c` (1), `src/core/physics/view.c` (1) |
| `e` | `0x6C` | 3 | `src/core/physics/setup.c` (3) |
| `e` | `0xD4` | 3 | `src/core/physics/setup.c` (3) |
| `e` | `0xD6` | 3 | `src/core/physics/setup.c` (3) |
| `e` | `0xD8` | 3 | `src/core/physics/setup.c` (3) |
| `e` | `0xD9` | 3 | `src/core/physics/setup.c` (3) |
| `e` | `0xE8` | 3 | `src/core/physics/setup.c` (3) |
| `e` | `0xF0` | 3 | `src/core/physics/setup.c` (3) |
| `e` | `0xF4` | 3 | `src/core/physics/setup.c` (3) |
| `e` | `0xF8` | 3 | `src/core/physics/setup.c` (3) |
| `e` | `0x11E0` | 3 | `src/core/physics/race.c` (3) |
| `e` | `0x11E8` | 3 | `src/core/physics/race.c` (3) |
| `e` | `0x11EC` | 3 | `src/core/physics/race.c` (3) |
| `e + 0x1c` | `0x4` | 3 | `src/app/race_game.c` (2), `src/core/physics/view.c` (1) |
| `entries` | `0x8` | 3 | `src/app/race_game.c` (2), `src/core/physics/race.c` (1) |
| `entry` | `0x343C` | 3 | `src/core/physics/init.c` (1), `src/core/physics/session.c` (2) |
| `entry_at[i]` | `0x198` | 3 | `src/core/physics/race_run.c` (3) |
| `entry_at[i]` | `0x3444` | 3 | `src/core/physics/race_run.c` (3) |
| `g` | `0x8` | 3 | `src/core/physics/init.c` (3) |
| `g` | `0x20` | 3 | `src/core/physics/chassis.c` (1), `src/core/physics/suspension.c` (2) |
| `g` | `0x24` | 3 | `src/core/physics/chassis.c` (1), `src/core/physics/suspension.c` (2) |
| `g` | `0x32` | 3 | `src/core/physics/collision.c` (1), `src/core/physics/ground.c` (1), `src/core/physics/init.c` (1) |
| `g + GROUND_NORMAL` | `0x4` | 3 | `src/core/physics/collision.c` (3) |
| `gar` | `0x18` | 3 | `src/core/physics/record.c` (3) |
| `gar + off` | `0x4` | 3 | `src/core/physics/garage.c` (2), `src/core/physics/record.c` (1) |
| `l` | `0x8` | 3 | `src/core/physics/race.c` (3) |
| `l` | `0x10` | 3 | `src/core/physics/race.c` (3) |
| `layout` | `0x2C` | 3 | `src/core/physics/clutch.c` (1), `src/core/physics/steer.c` (2) |
| `layout` | `0x3C` | 3 | `src/app/race_game_license.inc` (1), `src/core/physics/ai.c` (2) |
| `list` | `0x8` | 3 | `src/core/physics/race.c` (3) |
| `list` | `0x14` | 3 | `src/core/physics/race.c` (3) |
| `m` | `0x5` | 3 | `src/core/physics/misc.c` (3) |
| `m` | `0x1A` | 3 | `src/core/physics/init.c` (3) |
| `mesh` | `0x78` | 3 | `src/core/physics/course_objects.c` (1), `src/core/physics/walls.c` (2) |
| `mesh` | `0x84` | 3 | `src/core/physics/course_objects.c` (1), `src/core/physics/walls.c` (2) |
| `mesh` | `0x8C` | 3 | `src/core/physics/course_objects.c` (1), `src/core/physics/walls.c` (2) |
| `next` | `0x8` | 3 | `src/core/physics/course_objects.c` (3) |
| `obj` | `0x3C` | 3 | `src/core/physics/ai.c` (3) |
| `out` | `0x2` | 3 | `src/core/physics/driver.c` (2), `src/core/physics/setup.c` (1) |
| `out` | `0x18` | 3 | `src/core/physics/init.c` (3) |
| `p` | `0x0` | 3 | `src/core/physics/setup.c` (3) |
| `p` | `0xC` | 3 | `src/core/physics/setup.c` (2), `src/core/physics/walls.c` (1) |
| `p` | `0x12` | 3 | `src/core/physics/ai.c` (2), `src/core/physics/session.c` (1) |
| `p` | `0x21` | 3 | `src/core/physics/session.c` (3) |
| `p` | `0x2D` | 3 | `src/core/physics/session.c` (3) |
| `p` | `0x114` | 3 | `src/core/physics/setup.c` (2), `src/core/physics/walls.c` (1) |
| `p` | `0x200` | 3 | `src/core/physics/init.c` (1), `src/core/physics/setup.c` (2) |
| `p` | `0x103C` | 3 | `src/core/physics/setup.c` (3) |
| `p` | `0x10D1` | 3 | `src/core/physics/init.c` (3) |
| `p` | `0x10DC` | 3 | `src/core/physics/setup.c` (3) |
| `p + PARAMS_ROLL_HEIGHT` | `0x4` | 3 | `src/core/physics/setup.c` (3) |
| `params` | `0x1C` | 3 | `src/core/physics/chassis.c` (3) |
| `params` | `0x24` | 3 | `src/core/physics/chassis.c` (3) |
| `params` | `0x31` | 3 | `src/core/physics/drivetrain.c` (1), `src/core/physics/init.c` (2) |
| `params` | `0x202` | 3 | `src/core/physics/ai.c` (1), `src/core/physics/clutch.c` (1), `src/core/physics/view.c` (1) |
| `params` | `0x103C` | 3 | `src/core/physics/ai.c` (1), `src/core/physics/init.c` (2) |
| `params` | `0x1078` | 3 | `src/core/physics/init.c` (1), `src/core/physics/misc.c` (2) |
| `prof` | `0x20` | 3 | `src/core/physics/ai.c` (3) |
| `prof` | `0x64` | 3 | `src/core/physics/ai.c` (3) |
| `prof` | `0x6C` | 3 | `src/core/physics/ai.c` (3) |
| `prof` | `0x70` | 3 | `src/core/physics/ai.c` (2), `src/core/physics/init.c` (1) |
| `prof` | `0x78` | 3 | `src/core/physics/ai.c` (3) |
| `prof` | `0x7C` | 3 | `src/core/physics/ai.c` (3) |
| `pt` | `0x4` | 3 | `src/core/physics/lines.c` (3) |
| `pt` | `0x8` | 3 | `src/core/physics/lines.c` (3) |
| `pt` | `0x14` | 3 | `src/core/physics/lines.c` (3) |
| `race` | `0x8C` | 3 | `src/core/physics/license_session.c` (1), `src/core/physics/race_run.c` (1), `src/core/physics/session.c` (1) |
| `race` | `0xB0` | 3 | `src/core/physics/race.c` (3) |
| `race` | `0xE8` | 3 | `src/core/physics/race.c` (3) |
| `rec` | `0x1` | 3 | `src/core/physics/setup.c` (3) |
| `rec` | `0x2` | 3 | `src/core/physics/setup.c` (3) |
| `rec` | `0x20` | 3 | `src/core/physics/setup.c` (3) |
| `rec` | `0xBD` | 3 | `src/core/physics/setup.c` (3) |
| `rec` | `0x116` | 3 | `src/core/physics/setup.c` (3) |
| `rec` | `0x117` | 3 | `src/core/physics/setup.c` (3) |
| `rec` | `0x138` | 3 | `src/core/physics/setup.c` (3) |
| `rec` | `0x157` | 3 | `src/core/physics/setup.c` (3) |
| `rec` | `0x1C2` | 3 | `src/core/physics/init.c` (2), `src/core/physics/setup.c` (1) |
| `rec` | `0x1F5` | 3 | `src/core/physics/init.c` (3) |
| `rw` | `0x14` | 3 | `src/app/race_game.c` (1), `src/core/physics/lines.c` (2) |
| `rw` | `0x9C` | 3 | `src/app/race_game_license.inc` (1), `src/core/physics/init.c` (2) |
| `s` | `0x3` | 3 | `src/core/physics/session.c` (1), `src/core/physics/walls.c` (2) |
| `s` | `0x20` | 3 | `src/core/physics/aids.c` (2), `src/core/physics/race.c` (1) |
| `s` | `0x24` | 3 | `src/core/physics/aids.c` (1), `src/core/physics/wear.c` (2) |
| `s` | `0x30` | 3 | `src/core/physics/aids.c` (2), `src/core/physics/wear.c` (1) |
| `s` | `0x5C` | 3 | `src/core/physics/tyre.c` (3) |
| `s` | `0x68` | 3 | `src/core/physics/race.c` (1), `src/core/physics/tyre.c` (1), `src/core/physics/wear.c` (1) |
| `s0` | `0xC0` | 3 | `src/core/physics/session.c` (3) |
| `setup` | `0x18` | 3 | `src/core/physics/race_run.c` (3) |
| `solver` | `0x4` | 3 | `src/core/physics/walls.c` (3) |
| `src` | `0x4` | 3 | `src/core/physics/session.c` (2), `src/core/physics/setup.c` (1) |
| `src` | `0x10` | 3 | `src/core/physics/session.c` (1), `src/core/physics/setup.c` (2) |
| `src` | `0x18` | 3 | `src/core/physics/session.c` (1), `src/core/physics/setup.c` (2) |
| `src` | `0x20` | 3 | `src/core/physics/session.c` (1), `src/core/physics/setup.c` (2) |
| `src` | `0x28` | 3 | `src/core/physics/ai.c` (1), `src/core/physics/setup.c` (2) |
| `t` | `0x2C` | 3 | `src/core/physics/session.c` (3) |
| `t` | `0x30` | 3 | `src/core/physics/ai.c` (1), `src/core/physics/session.c` (2) |
| `t` | `0x69` | 3 | `src/core/physics/session.c` (3) |
| `v` | `0x4` | 3 | `src/app/race_game_license.inc` (1), `src/core/physics/init.c` (1), `src/core/physics/race_run.c` (1) |
| `w` | `0x14` | 3 | `src/core/physics/ground.c` (1), `src/core/physics/init.c` (2) |
| `w` | `0x18` | 3 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (2) |
| `w` | `0x20` | 3 | `src/core/physics/clutch.c` (1), `src/core/physics/init.c` (1), `src/core/physics/view.c` (1) |
| `w` | `0x30` | 3 | `src/core/physics/driver.c` (2), `src/core/physics/init.c` (1) |
| `w` | `0x48` | 3 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (1), `src/core/physics/view.c` (1) |
| `w` | `0x4C` | 3 | `src/core/physics/aids.c` (1), `src/core/physics/init.c` (1), `src/core/physics/steer.c` (1) |
| `w` | `0x8A` | 3 | `src/core/physics/ground.c` (1), `src/core/physics/init.c` (2) |
| `w` | `0x8B` | 3 | `src/core/physics/ground.c` (1), `src/core/physics/init.c` (2) |
| `w` | `0xA0` | 3 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (1), `src/core/physics/view.c` (1) |
| `w` | `0xB4` | 3 | `src/core/physics/init.c` (1), `src/core/physics/wear.c` (2) |
| `w` | `0xC4` | 3 | `src/core/physics/init.c` (1), `src/core/physics/misc.c` (2) |
| `A` | `0x0` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/session.c` (1) |
| `A` | `0x4` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/session.c` (1) |
| `A` | `0x8` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/session.c` (1) |
| `A` | `0xC` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/session.c` (1) |
| `A` | `0x10` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/session.c` (1) |
| `A` | `0x14` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/session.c` (1) |
| `A` | `0x20` | 2 | `src/core/physics/session.c` (2) |
| `AID_TABLE` | `0xE5` | 2 | `src/core/physics/aids.c` (2) |
| `AI_SETTINGS` | `0x2` | 2 | `src/core/physics/ai.c` (2) |
| `AI_SETTINGS` | `0x5` | 2 | `src/core/physics/ai.c` (2) |
| `AI_SETTINGS` | `0xA` | 2 | `src/core/physics/ai.c` (2) |
| `AI_SETTINGS` | `0xB` | 2 | `src/core/physics/ai.c` (2) |
| `C` | `0x20` | 2 | `src/core/physics/session.c` (2) |
| `CAR_AT(c, i)` | `0x7E` | 2 | `src/core/physics/ai.c` (2) |
| `COLUMN` | `0x8` | 2 | `src/core/physics/steer.c` (2) |
| `D` | `0x0` | 2 | `src/core/physics/session.c` (2) |
| `D` | `0x14` | 2 | `src/core/physics/session.c` (2) |
| `D` | `0x18` | 2 | `src/core/physics/session.c` (2) |
| `DRIVE_SETTINGS` | `0x11` | 2 | `src/core/physics/driver.c` (2) |
| `F(offset)` | `0x56C` | 2 | `src/core/physics/aids.c` (2) |
| `F(offset)` | `0x5A8` | 2 | `src/core/physics/aids.c` (1), `src/core/physics/driver.c` (1) |
| `GHOST_PROFILE` | `0xD0` | 2 | `src/core/physics/race_run.c` (2) |
| `GHOST_PROFILE` | `0x1CC` | 2 | `src/core/physics/race_run.c` (2) |
| `GT4_U32(GT4_U32(cond) + 0x60)` | `0x8` | 2 | `src/app/race_game_bspec.inc` (2) |
| `GT4_U32(car + CAR_PARAMS)` | `0x48` | 2 | `src/core/physics/license_session.c` (1), `src/core/physics/session.c` (1) |
| `GT4_U32(car + CAR_PARAMS)` | `0xFE0` | 2 | `src/core/physics/ai.c` (2) |
| `GT4_U32(car + CAR_SESSION) + GT4_U8(car + CAR_PLAYER) * 4u` | `0x10024` | 2 | `src/core/physics/aids.c` (2) |
| `GT4_U32(cond)` | `0x60` | 2 | `src/app/race_game_bspec.inc` (2) |
| `GT4_U32(cond)` | `0xE0` | 2 | `src/core/physics/race_run.c` (2) |
| `GT4_U32(cond)` | `0xE4` | 2 | `src/core/physics/session.c` (2) |
| `GT4_U32(entries + (uint32_t)i * 4)` | `0x3444` | 2 | `src/app/race_game_bspec.inc` (2) |
| `GT4_U32(lines)` | `0x9C` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/lines.c` (1) |
| `GT4_U32(mesh + 0x78) + ((uint32_t)id & 0xffff) * 12` | `0x8` | 2 | `src/core/physics/course_objects.c` (1), `src/core/physics/walls.c` (1) |
| `GT4_U32(r->cond)` | `0xBC` | 2 | `src/core/physics/race_run.c` (2) |
| `HB(offset)` | `0x5B` | 2 | `src/core/physics/setup.c` (2) |
| `HU(offset)` | `0x440` | 2 | `src/core/physics/walls.c` (2) |
| `LOOSE_SURFACE` | `0xC` | 2 | `src/core/physics/surface.c` (2) |
| `LOOSE_SURFACE` | `0x10` | 2 | `src/core/physics/surface.c` (2) |
| `OF(offset)` | `0x0` | 2 | `src/core/physics/ai.c` (2) |
| `OF(offset)` | `0x4` | 2 | `src/core/physics/ai.c` (2) |
| `OF(offset)` | `0x8` | 2 | `src/core/physics/ai.c` (2) |
| `OF(offset)` | `0xC` | 2 | `src/core/physics/ai.c` (2) |
| `OF(offset)` | `0x10` | 2 | `src/core/physics/ai.c` (2) |
| `OF(offset)` | `0x14` | 2 | `src/core/physics/ai.c` (2) |
| `OF(offset)` | `0x60` | 2 | `src/core/physics/ai.c` (2) |
| `OF(offset)` | `0x64` | 2 | `src/core/physics/ai.c` (2) |
| `OF(offset)` | `0x70` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/driver.c` (1) |
| `OF(offset)` | `0x74` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/driver.c` (1) |
| `OU8(offset)` | `0x49` | 2 | `src/core/physics/ai.c` (2) |
| `OU8(offset)` | `0x4B` | 2 | `src/core/physics/ai.c` (2) |
| `OU8(offset)` | `0x53` | 2 | `src/core/physics/ai.c` (2) |
| `OU8(offset)` | `0x6A` | 2 | `src/core/physics/ai.c` (2) |
| `OU8(offset)` | `0x6D` | 2 | `src/core/physics/driver.c` (2) |
| `Q_EVENT(q, i)` | `0x2` | 2 | `src/core/physics/race.c` (2) |
| `Q_EVENT(q, i)` | `0x4` | 2 | `src/core/physics/race.c` (2) |
| `RATE_REC` | `0x600` | 2 | `src/core/physics/race_run.c` (2) |
| `RATE_WEAR` | `0x14` | 2 | `src/core/physics/race_run.c` (2) |
| `S` | `0xF` | 2 | `src/core/physics/session.c` (2) |
| `S` | `0x10` | 2 | `src/core/physics/session.c` (2) |
| `S` | `0x1E2` | 2 | `src/core/physics/session.c` (2) |
| `SB(offset)` | `0x1` | 2 | `src/core/physics/session.c` (2) |
| `SB(offset)` | `0x2` | 2 | `src/core/physics/session.c` (2) |
| `SB(offset)` | `0x3` | 2 | `src/core/physics/session.c` (2) |
| `SB(offset)` | `0x8` | 2 | `src/core/physics/session.c` (2) |
| `SB(offset)` | `0xA` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/session.c` (1) |
| `SB(offset)` | `0xB` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/session.c` (1) |
| `SB(offset)` | `0xD` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/session.c` (1) |
| `SB(offset)` | `0xE` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/session.c` (1) |
| `SHAKE_PARAM` | `0x20` | 2 | `src/core/physics/walls.c` (2) |
| `SHIFT_SETTINGS` | `0x8` | 2 | `src/core/physics/gearbox.c` (2) |
| `U8(offset)` | `0x564` | 2 | `src/core/physics/driver.c` (2) |
| `WEAR_LIMITS` | `0xC` | 2 | `src/core/physics/ground.c` (2) |
| `WEAR_LIMITS` | `0x1C` | 2 | `src/core/physics/ground.c` (2) |
| `a` | `0x0` | 2 | `src/core/physics/setup.c` (2) |
| `a` | `0x20` | 2 | `src/core/physics/setup.c` (2) |
| `a` | `0x3C` | 2 | `src/core/physics/setup.c` (2) |
| `a` | `0x78` | 2 | `src/core/physics/ai.c` (2) |
| `a` | `0x10140` | 2 | `src/core/physics/race_run.c` (2) |
| `a0 + P_A` | `0x4` | 2 | `src/core/physics/walls.c` (2) |
| `a1` | `0x5B0` | 2 | `src/core/physics/session.c` (2) |
| `a1` | `0x5B6` | 2 | `src/core/physics/session.c` (2) |
| `a1` | `0x5B8` | 2 | `src/core/physics/session.c` (2) |
| `a1` | `0x71C` | 2 | `src/core/physics/session.c` (2) |
| `a1 + P_A` | `0x4` | 2 | `src/core/physics/walls.c` (2) |
| `a3` | `0x8` | 2 | `src/core/physics/ai.c` (2) |
| `a3` | `0xC` | 2 | `src/core/physics/ai.c` (2) |
| `a3` | `0x10` | 2 | `src/core/physics/ai.c` (2) |
| `a3` | `0x18` | 2 | `src/core/physics/ai.c` (2) |
| `a3` | `0x1C` | 2 | `src/core/physics/ai.c` (2) |
| `adln` | `0x14` | 2 | `src/core/physics/lines.c` (2) |
| `ai` | `0x440` | 2 | `src/core/physics/laps.c` (2) |
| `b` | `0x1C` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/walls.c` (1) |
| `b` | `0x48` | 2 | `src/core/physics/suspension.c` (2) |
| `b` | `0x4C` | 2 | `src/core/physics/suspension.c` (2) |
| `b` | `0x45A` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (1) |
| `b` | `0x462` | 2 | `src/core/physics/ai.c` (2) |
| `b` | `0x464` | 2 | `src/core/physics/ai.c` (2) |
| `b` | `0x46C` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/init.c` (1) |
| `b` | `0x470` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/init.c` (1) |
| `b` | `0x48C` | 2 | `src/core/physics/clutch.c` (1), `src/core/physics/init.c` (1) |
| `b` | `0x4E0` | 2 | `src/core/physics/init.c` (2) |
| `b` | `0x4F4` | 2 | `src/core/physics/init.c` (2) |
| `b` | `0x4F6` | 2 | `src/core/physics/init.c` (2) |
| `b` | `0x4F8` | 2 | `src/core/physics/init.c` (2) |
| `b` | `0x500` | 2 | `src/core/physics/init.c` (2) |
| `b` | `0x508` | 2 | `src/core/physics/init.c` (2) |
| `b` | `0x518` | 2 | `src/core/physics/init.c` (2) |
| `b` | `0x558` | 2 | `src/core/physics/chassis.c` (1), `src/core/physics/init.c` (1) |
| `b` | `0x58C` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/init.c` (1) |
| `b` | `0x592` | 2 | `src/core/physics/init.c` (2) |
| `b` | `0x5A0` | 2 | `src/core/physics/init.c` (1), `src/core/physics/misc.c` (1) |
| `b` | `0x5A8` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/init.c` (1) |
| `b` | `0x5BC` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/init.c` (1) |
| `b` | `0x607` | 2 | `src/core/physics/init.c` (2) |
| `b` | `0x608` | 2 | `src/core/physics/walls.c` (2) |
| `b` | `0x618` | 2 | `src/core/physics/race.c` (2) |
| `b` | `0x61C` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/init.c` (1) |
| `b` | `0x620` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/init.c` (1) |
| `b` | `0x62C` | 2 | `src/core/physics/init.c` (2) |
| `b` | `0x62D` | 2 | `src/core/physics/init.c` (2) |
| `b` | `0x631` | 2 | `src/core/physics/init.c` (1), `src/core/physics/walls.c` (1) |
| `best` | `0x10` | 2 | `src/core/physics/session.c` (2) |
| `buf` | `0x2` | 2 | `src/core/physics/driver.c` (2) |
| `c` | `0xF85C` | 2 | `src/core/physics/license_session.c` (2) |
| `car` | `0x10C` | 2 | `src/core/physics/init.c` (2) |
| `car` | `0x5B4` | 2 | `src/core/physics/race.c` (2) |
| `car` | `0x64C` | 2 | `src/core/physics/license_session.c` (1), `src/core/physics/view.c` (1) |
| `car` | `0x650` | 2 | `src/core/physics/license_session.c` (1), `src/core/physics/view.c` (1) |
| `car` | `0x697` | 2 | `src/core/physics/misc.c` (2) |
| `car` | `0x6FC` | 2 | `src/core/physics/race_run.c` (2) |
| `car` | `0x6FD` | 2 | `src/core/physics/race_run.c` (2) |
| `car` | `0x6FE` | 2 | `src/core/physics/misc.c` (1), `src/core/physics/race_run.c` (1) |
| `car` | `0x735` | 2 | `src/core/physics/step.c` (2) |
| `car` | `0x75F` | 2 | `src/core/physics/session.c` (2) |
| `car` | `0x76C` | 2 | `src/core/physics/steer.c` (2) |
| `car` | `0x789` | 2 | `src/core/physics/init.c` (1), `src/core/physics/walls.c` (1) |
| `car` | `0x7CC` | 2 | `src/core/physics/view.c` (2) |
| `car + (uint32_t)i * 0xec` | `0x1ED` | 2 | `src/core/physics/ai.c` (2) |
| `car + BATTERY` | `0x5` | 2 | `src/core/physics/clutch.c` (2) |
| `car + CAR_140` | `0x8` | 2 | `src/core/physics/chassis.c` (2) |
| `car + CAR_140 + 0xc` | `0x4` | 2 | `src/core/physics/chassis.c` (1), `src/core/physics/ground.c` (1) |
| `car + CAR_FIELD_DRIVER_MODEL` | `0x1C` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/walls.c` (1) |
| `car + CAR_FIELD_DRIVER_MODEL` | `0xE4` | 2 | `src/core/physics/init.c` (1), `src/core/physics/race_run.c` (1) |
| `car + CAR_FIELD_DRIVER_MODEL` | `0xE8` | 2 | `src/core/physics/init.c` (1), `src/core/physics/race_run.c` (1) |
| `car + CAR_FIELD_DRIVER_MODEL` | `0xEC` | 2 | `src/core/physics/init.c` (1), `src/core/physics/race_run.c` (1) |
| `car + CAR_INPUTS` | `0x4` | 2 | `src/core/physics/chassis.c` (2) |
| `car + CAR_MATRIX` | `0x8` | 2 | `src/core/physics/chassis.c` (2) |
| `car + CAR_POSITION` | `0x30` | 2 | `src/core/physics/chassis.c` (2) |
| `car + CAR_POSITION` | `0x34` | 2 | `src/core/physics/chassis.c` (2) |
| `car + CAR_POSITION` | `0x618` | 2 | `src/core/physics/session.c` (2) |
| `car + CAR_REF_B_PREV` | `0x4` | 2 | `src/core/physics/chassis.c` (1), `src/core/physics/race_run.c` (1) |
| `car_at(c, (uint32_t)lead)` | `0x5B3` | 2 | `src/core/physics/step.c` (2) |
| `car_at(cond, GT4_U32(order + k * 4))` | `0x5B3` | 2 | `src/core/physics/session.c` (2) |
| `car_at(cond, i)` | `0x5B3` | 2 | `src/core/physics/session.c` (2) |
| `car_at(self, car)` | `0x5B3` | 2 | `src/core/physics/session.c` (2) |
| `cfg` | `0x3` | 2 | `src/core/physics/setup.c` (2) |
| `cmd` | `0x2` | 2 | `src/core/physics/driver.c` (2) |
| `cmd` | `0xE` | 2 | `src/core/physics/driver.c` (2) |
| `cmd` | `0x14` | 2 | `src/core/physics/driver.c` (2) |
| `cond` | `0x80` | 2 | `src/core/physics/session.c` (2) |
| `cond` | `0x84` | 2 | `src/core/physics/session.c` (2) |
| `cond` | `0x88` | 2 | `src/core/physics/session.c` (2) |
| `cond` | `0x8C` | 2 | `src/core/physics/session.c` (2) |
| `cond` | `0x90` | 2 | `src/core/physics/session.c` (2) |
| `cond` | `0x94` | 2 | `src/core/physics/session.c` (2) |
| `cond` | `0xCBE2` | 2 | `src/core/physics/session.c` (2) |
| `cond` | `0xCBE5` | 2 | `src/core/physics/session.c` (2) |
| `cond` | `0xCC14` | 2 | `src/core/physics/misc.c` (1), `src/core/physics/session.c` (1) |
| `cond` | `0xCCA4` | 2 | `src/core/physics/misc.c` (1), `src/core/physics/session.c` (1) |
| `cond` | `0xD9F0` | 2 | `src/app/race_game_bspec.inc` (1), `src/core/physics/session.c` (1) |
| `cond` | `0xDA20` | 2 | `src/app/race_game_bspec.inc` (1), `src/core/physics/session.c` (1) |
| `cond` | `0xDA21` | 2 | `src/app/race_game_bspec.inc` (1), `src/core/physics/session.c` (1) |
| `cond` | `0xF83A` | 2 | `src/core/physics/session.c` (2) |
| `cond` | `0xF878` | 2 | `src/core/physics/init.c` (2) |
| `cond` | `0xF940` | 2 | `src/core/physics/car.c` (1), `src/core/physics/init.c` (1) |
| `cond` | `0xFB20` | 2 | `src/app/race_game.c` (1), `src/core/physics/init.c` (1) |
| `cond` | `0xFB21` | 2 | `src/app/race_game.c` (1), `src/core/physics/init.c` (1) |
| `cond` | `0x10148` | 2 | `src/core/physics/session.c` (2) |
| `d` | `0x4` | 2 | `src/core/physics/init.c` (1), `src/core/physics/session.c` (1) |
| `d` | `0x44` | 2 | `src/core/physics/setup.c` (2) |
| `d` | `0x50` | 2 | `src/core/physics/init.c` (2) |
| `d` | `0x68` | 2 | `src/core/physics/init.c` (1), `src/core/physics/setup.c` (1) |
| `d` | `0x69` | 2 | `src/core/physics/init.c` (2) |
| `d` | `0x98` | 2 | `src/core/physics/setup.c` (2) |
| `d` | `0xBC` | 2 | `src/core/physics/setup.c` (2) |
| `drive` | `0x1C` | 2 | `src/core/physics/wheels.c` (2) |
| `dst` | `0x78` | 2 | `src/core/physics/ai.c` (2) |
| `dst` | `0x7C` | 2 | `src/core/physics/ai.c` (2) |
| `dst` | `0x80` | 2 | `src/core/physics/ai.c` (2) |
| `dst` | `0x84` | 2 | `src/core/physics/ai.c` (2) |
| `e` | `0x28` | 2 | `src/core/physics/view.c` (2) |
| `e` | `0x2C` | 2 | `src/app/race_game.c` (1), `src/core/physics/view.c` (1) |
| `e` | `0x34` | 2 | `src/app/race_game.c` (1), `src/core/physics/view.c` (1) |
| `e` | `0x38` | 2 | `src/app/race_game.c` (1), `src/core/physics/view.c` (1) |
| `e` | `0xEC` | 2 | `src/core/physics/setup.c` (2) |
| `e` | `0x4DC` | 2 | `src/core/physics/session.c` (2) |
| `e + 8` | `0x4` | 2 | `src/app/race_game.c` (2) |
| `e0 + P_A` | `0x4` | 2 | `src/core/physics/walls.c` (2) |
| `entry` | `0x3448` | 2 | `src/core/physics/session.c` (2) |
| `entry_at[i]` | `0x20` | 2 | `src/core/physics/race_run.c` (2) |
| `ev` | `0x3` | 2 | `src/core/physics/driver.c` (2) |
| `from` | `0x40` | 2 | `src/core/physics/race.c` (2) |
| `g` | `0x4` | 2 | `src/core/physics/init.c` (2) |
| `g + GROUND_NORMAL` | `0x0` | 2 | `src/core/physics/collision.c` (2) |
| `g + GROUND_NORMAL` | `0x8` | 2 | `src/core/physics/collision.c` (2) |
| `gar` | `0x48` | 2 | `src/core/physics/init.c` (2) |
| `gar` | `0x4C` | 2 | `src/core/physics/init.c` (2) |
| `gar` | `0x146` | 2 | `src/core/physics/record.c` (2) |
| `gar` | `0x147` | 2 | `src/core/physics/record.c` (2) |
| `gar` | `0x156` | 2 | `src/core/physics/record.c` (2) |
| `gar` | `0x157` | 2 | `src/core/physics/record.c` (2) |
| `gar` | `0x159` | 2 | `src/core/physics/record.c` (2) |
| `gar` | `0x165` | 2 | `src/core/physics/garage.c` (2) |
| `gar` | `0x168` | 2 | `src/core/physics/record.c` (2) |
| `gt4_race_run_car(g->run, 0)` | `0x8` | 2 | `src/app/race_game.c` (2) |
| `h` | `0x388` | 2 | `src/core/physics/walls.c` (2) |
| `h` | `0x474` | 2 | `src/core/physics/course_objects.c` (2) |
| `h` | `0x48C` | 2 | `src/core/physics/course_objects.c` (1), `src/core/physics/walls.c` (1) |
| `h` | `0x490` | 2 | `src/core/physics/course_objects.c` (1), `src/core/physics/walls.c` (1) |
| `h` | `0x494` | 2 | `src/core/physics/course_objects.c` (1), `src/core/physics/walls.c` (1) |
| `l` | `0x14` | 2 | `src/core/physics/race.c` (2) |
| `lay` | `0x4` | 2 | `src/core/physics/init.c` (1), `src/core/physics/setup.c` (1) |
| `lay` | `0xC` | 2 | `src/core/physics/setup.c` (1), `src/core/physics/view.c` (1) |
| `layout` | `0xC` | 2 | `src/core/physics/ai.c` (2) |
| `line_point(&l, i)` | `0x14` | 2 | `src/core/physics/ai.c` (2) |
| `line_point(&l, n - 1)` | `0x14` | 2 | `src/core/physics/ai.c` (2) |
| `line_point(&l, s)` | `0x2` | 2 | `src/core/physics/ai.c` (2) |
| `line_point(&l, s2)` | `0x14` | 2 | `src/core/physics/ai.c` (2) |
| `line_point(&l, s3)` | `0x14` | 2 | `src/core/physics/ai.c` (2) |
| `line_point(l, s0)` | `0x14` | 2 | `src/core/physics/ai.c` (2) |
| `lines` | `0x4` | 2 | `src/core/physics/lines.c` (2) |
| `list` | `0x18` | 2 | `src/core/physics/race.c` (2) |
| `m` | `0x6` | 2 | `src/core/physics/misc.c` (2) |
| `m` | `0x6E` | 2 | `src/core/physics/init.c` (2) |
| `m` | `0x6F` | 2 | `src/core/physics/init.c` (2) |
| `o + H_POS` | `0x8` | 2 | `src/core/physics/walls.c` (2) |
| `obj` | `0x14` | 2 | `src/core/physics/ai.c` (2) |
| `obj` | `0x34` | 2 | `src/core/physics/ai.c` (2) |
| `obj` | `0x38` | 2 | `src/core/physics/ai.c` (2) |
| `obj` | `0x44` | 2 | `src/core/physics/ai.c` (2) |
| `obj` | `0x4C` | 2 | `src/core/physics/misc.c` (2) |
| `obj` | `0x6D` | 2 | `src/core/physics/driver.c` (2) |
| `obj` | `0x1CC` | 2 | `src/core/physics/driver.c` (2) |
| `out` | `0x4` | 2 | `src/core/physics/init.c` (1), `src/core/physics/setup.c` (1) |
| `out` | `0x8` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (1) |
| `out` | `0xD` | 2 | `src/core/physics/driver.c` (2) |
| `out` | `0xE` | 2 | `src/core/physics/driver.c` (2) |
| `out` | `0xF` | 2 | `src/core/physics/driver.c` (2) |
| `out` | `0x10` | 2 | `src/core/physics/init.c` (2) |
| `out` | `0x14` | 2 | `src/core/physics/init.c` (2) |
| `p` | `0x2` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/session.c` (1) |
| `p` | `0x2E` | 2 | `src/core/physics/session.c` (2) |
| `p` | `0x30` | 2 | `src/core/physics/session.c` (1), `src/core/physics/setup.c` (1) |
| `p` | `0x54` | 2 | `src/core/physics/init.c` (2) |
| `p` | `0x230` | 2 | `src/core/physics/init.c` (1), `src/core/physics/setup.c` (1) |
| `p` | `0x29C` | 2 | `src/core/physics/init.c` (1), `src/core/physics/setup.c` (1) |
| `p` | `0x340` | 2 | `src/core/physics/setup.c` (2) |
| `p` | `0x34C` | 2 | `src/core/physics/setup.c` (2) |
| `p` | `0x398` | 2 | `src/core/physics/setup.c` (2) |
| `p` | `0x3A4` | 2 | `src/core/physics/setup.c` (2) |
| `p` | `0x1034` | 2 | `src/core/physics/setup.c` (2) |
| `p` | `0x1044` | 2 | `src/core/physics/setup.c` (2) |
| `p` | `0x1076` | 2 | `src/core/physics/init.c` (1), `src/core/physics/setup.c` (1) |
| `p` | `0x1078` | 2 | `src/core/physics/setup.c` (2) |
| `p` | `0x107C` | 2 | `src/core/physics/setup.c` (2) |
| `p` | `0x1080` | 2 | `src/core/physics/setup.c` (2) |
| `p` | `0x10D2` | 2 | `src/core/physics/setup.c` (2) |
| `p` | `0x10D3` | 2 | `src/core/physics/setup.c` (2) |
| `pad_out(car)` | `0x1` | 2 | `src/core/physics/view.c` (2) |
| `params` | `0x4` | 2 | `src/app/race_game.c` (1), `src/app/race_game_cameras.inc` (1) |
| `params` | `0x8` | 2 | `src/app/race_game.c` (1), `src/app/race_game_cameras.inc` (1) |
| `params` | `0xC` | 2 | `src/app/race_game.c` (1), `src/app/race_game_cameras.inc` (1) |
| `params` | `0x28` | 2 | `src/core/physics/chassis.c` (2) |
| `params` | `0x2C` | 2 | `src/core/physics/chassis.c` (2) |
| `params` | `0x3C` | 2 | `src/core/physics/chassis.c` (2) |
| `params` | `0x110` | 2 | `src/core/physics/suspension.c` (2) |
| `params` | `0x236` | 2 | `src/core/physics/gearbox.c` (2) |
| `params` | `0x29C` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/aids.c` (1) |
| `params` | `0x1077` | 2 | `src/core/physics/init.c` (1), `src/core/physics/misc.c` (1) |
| `params` | `0x1084` | 2 | `src/core/physics/chassis.c` (1), `src/core/physics/init.c` (1) |
| `params` | `0x10D4` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (1) |
| `params + (uint32_t)surf` | `0xF98` | 2 | `src/core/physics/ai.c` (2) |
| `pcar` | `0x14` | 2 | `src/app/race_game_bspec.inc` (2) |
| `player_2p` | `0x5B6` | 2 | `src/app/race_game.c` (1), `src/app/race_game_hud.inc` (1) |
| `prof` | `0x4` | 2 | `src/core/physics/ai.c` (2) |
| `prof` | `0x10` | 2 | `src/core/physics/ai.c` (2) |
| `prof` | `0x14` | 2 | `src/core/physics/ai.c` (2) |
| `prof` | `0x18` | 2 | `src/core/physics/ai.c` (2) |
| `prof` | `0x5C` | 2 | `src/core/physics/ai.c` (2) |
| `prof` | `0x68` | 2 | `src/core/physics/ai.c` (2) |
| `prof` | `0xA8` | 2 | `src/core/physics/ai.c` (2) |
| `profile(obj)` | `0x60` | 2 | `src/core/physics/ai.c` (2) |
| `pt` | `0xC` | 2 | `src/core/physics/lines.c` (2) |
| `pt` | `0x12` | 2 | `src/core/physics/lines.c` (2) |
| `pt` | `0x13` | 2 | `src/core/physics/lines.c` (2) |
| `pt` | `0x1C` | 2 | `src/core/physics/lines.c` (2) |
| `pt` | `0x20` | 2 | `src/core/physics/lines.c` (2) |
| `pt` | `0x24` | 2 | `src/core/physics/lines.c` (2) |
| `q` | `0x4` | 2 | `src/core/physics/walls.c` (2) |
| `q` | `0xC` | 2 | `src/core/physics/lines.c` (2) |
| `q + P_B` | `0x4` | 2 | `src/core/physics/walls.c` (2) |
| `r` | `0x8` | 2 | `src/core/physics/race.c` (1), `src/core/physics/session.c` (1) |
| `r` | `0xE` | 2 | `src/core/physics/setup.c` (2) |
| `r` | `0x14` | 2 | `src/core/physics/race.c` (1), `src/core/physics/session.c` (1) |
| `r` | `0x88` | 2 | `src/core/physics/license_session.c` (2) |
| `race` | `0x70` | 2 | `src/core/physics/race.c` (2) |
| `race` | `0xC0` | 2 | `src/core/physics/race.c` (2) |
| `race` | `0xC4` | 2 | `src/core/physics/race.c` (2) |
| `race` | `0xF0` | 2 | `src/core/physics/license_session.c` (1), `src/core/physics/race.c` (1) |
| `rec` | `0x3` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x8` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x23` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x24` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x25` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x26` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x29` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x3A` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x40` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x4C` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x4F` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x50` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x9A` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x9C` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0xB8` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0xBE` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0xC0` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0xD6` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x108` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x10A` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x10C` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x110` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x111` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x114` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x115` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x118` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x119` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x11A` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x11B` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x129` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x12A` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x150` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x154` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x155` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x156` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x1E4` | 2 | `src/core/physics/setup.c` (2) |
| `rec` | `0x283` | 2 | `src/core/physics/init.c` (2) |
| `rnw` | `0x4` | 2 | `src/core/physics/runway.c` (2) |
| `row` | `0x2` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/session.c` (1) |
| `row` | `0x3` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/session.c` (1) |
| `row` | `0x6` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/session.c` (1) |
| `row` | `0x10` | 2 | `src/app/race_game_license.inc` (1), `src/core/physics/ai.c` (1) |
| `row` | `0x12` | 2 | `src/core/physics/ai.c` (2) |
| `row` | `0x18` | 2 | `src/core/physics/ai.c` (2) |
| `rw` | `0x38` | 2 | `src/core/physics/session.c` (2) |
| `rw` | `0x58` | 2 | `src/core/physics/session.c` (2) |
| `rw` | `0x5C` | 2 | `src/core/physics/init.c` (2) |
| `rw` | `0x90` | 2 | `src/core/physics/lines.c` (2) |
| `s` | `0x1` | 2 | `src/core/physics/session.c` (1), `src/core/physics/walls.c` (1) |
| `s` | `0x6` | 2 | `src/core/physics/session.c` (1), `src/core/physics/walls.c` (1) |
| `s` | `0x1D` | 2 | `src/core/physics/aids.c` (1), `src/core/physics/session.c` (1) |
| `s` | `0x1E` | 2 | `src/core/physics/aids.c` (1), `src/core/physics/session.c` (1) |
| `s` | `0x1F` | 2 | `src/core/physics/aids.c` (1), `src/core/physics/session.c` (1) |
| `s` | `0x2C` | 2 | `src/core/physics/aids.c` (1), `src/core/physics/wear.c` (1) |
| `s` | `0x32` | 2 | `src/core/physics/aids.c` (2) |
| `s` | `0x33` | 2 | `src/core/physics/aids.c` (2) |
| `s` | `0x40` | 2 | `src/core/physics/race.c` (1), `src/core/physics/wear.c` (1) |
| `s` | `0x48` | 2 | `src/core/physics/wear.c` (2) |
| `s` | `0x69` | 2 | `src/core/physics/tyre.c` (1), `src/core/physics/wear.c` (1) |
| `s` | `0xCC` | 2 | `src/core/physics/ground.c` (2) |
| `sa` | `0x4AC` | 2 | `src/app/race_game_bspec.inc` (2) |
| `session` | `0xD4` | 2 | `src/app/race_game.c` (1), `src/core/physics/race_run.c` (1) |
| `session` | `0xF89C` | 2 | `src/core/physics/ai.c` (2) |
| `setup` | `0x12A` | 2 | `src/core/physics/race_run.c` (2) |
| `src` | `0x8` | 2 | `src/core/physics/session.c` (1), `src/core/physics/setup.c` (1) |
| `src` | `0xC` | 2 | `src/core/physics/session.c` (1), `src/core/physics/setup.c` (1) |
| `src` | `0x38` | 2 | `src/core/physics/setup.c` (2) |
| `src` | `0x40` | 2 | `src/core/physics/setup.c` (2) |
| `src` | `0x48` | 2 | `src/core/physics/setup.c` (2) |
| `src` | `0x50` | 2 | `src/core/physics/setup.c` (2) |
| `src` | `0x56` | 2 | `src/core/physics/setup.c` (2) |
| `src` | `0x5C` | 2 | `src/core/physics/setup.c` (2) |
| `t` | `0x0` | 2 | `src/core/physics/ground.c` (1), `src/core/physics/setup.c` (1) |
| `t` | `0x38` | 2 | `src/core/physics/ai.c` (1), `src/core/physics/session.c` (1) |
| `t` | `0x50` | 2 | `src/core/physics/session.c` (2) |
| `t` | `0x64` | 2 | `src/core/physics/session.c` (2) |
| `t` | `0x78` | 2 | `src/core/physics/session.c` (1), `src/core/physics/setup.c` (1) |
| `t` | `0x164` | 2 | `src/core/physics/setup.c` (2) |
| `t` | `0x168` | 2 | `src/core/physics/setup.c` (2) |
| `t` | `0x16C` | 2 | `src/core/physics/setup.c` (2) |
| `t` | `0x178` | 2 | `src/core/physics/setup.c` (2) |
| `t` | `0x1D9` | 2 | `src/core/physics/setup.c` (2) |
| `to` | `0x40` | 2 | `src/core/physics/race.c` (2) |
| `tp` | `0x164` | 2 | `src/core/physics/aids.c` (1), `src/core/physics/gearbox.c` (1) |
| `tri` | `0x2` | 2 | `src/core/physics/collision.c` (2) |
| `tri` | `0x4` | 2 | `src/core/physics/collision.c` (2) |
| `tyre` | `0x68` | 2 | `src/core/physics/tyre.c` (2) |
| `v` | `0x14` | 2 | `src/core/physics/session.c` (2) |
| `vel` | `0x4` | 2 | `src/app/race_game.c` (1), `src/core/physics/view.c` (1) |
| `w` | `0x0` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/misc.c` (1) |
| `w` | `0xC` | 2 | `src/core/physics/init.c` (1), `src/core/physics/view.c` (1) |
| `w` | `0x28` | 2 | `src/core/physics/init.c` (1), `src/core/physics/view.c` (1) |
| `w` | `0x2C` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (1) |
| `w` | `0x34` | 2 | `src/core/physics/driver.c` (2) |
| `w` | `0x3C` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (1) |
| `w` | `0x58` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (1) |
| `w` | `0x60` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (1) |
| `w` | `0x6C` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (1) |
| `w` | `0x78` | 2 | `src/core/physics/driver.c` (2) |
| `w` | `0x80` | 2 | `src/core/physics/clutch.c` (1), `src/core/physics/driver.c` (1) |
| `w` | `0x88` | 2 | `src/core/physics/aids.c` (2) |
| `w` | `0x98` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (1) |
| `w` | `0x9C` | 2 | `src/core/physics/driver.c` (1), `src/core/physics/init.c` (1) |
| `w` | `0xBC` | 2 | `src/core/physics/init.c` (1), `src/core/physics/view.c` (1) |
| `wall` | `0x8` | 2 | `src/core/physics/course_objects.c` (2) |
| `wall` | `0xC` | 2 | `src/core/physics/course_objects.c` (2) |
| `world` | `0x6C` | 2 | `src/app/race_game.c` (1), `src/core/physics/session.c` (1) |
| `world` | `0xCCC` | 2 | `src/core/physics/driver.c` (2) |
| `x` | `0x30` | 2 | `src/core/physics/setup.c` (2) |
| `ys + (uint32_t)(i` | `0x1` | 2 | `src/core/physics/aids.c` (2) |
| `(b)` | `0x523` | 1 | `src/core/physics/clutch.c` (1) |
| `(b)` | `0x52C` | 1 | `src/core/physics/clutch.c` (1) |
| `(b)` | `0x534` | 1 | `src/core/physics/clutch.c` (1) |
| `0x006208b8u` | `0x0` | 1 | `src/core/physics/session.c` (1) |
| `0x006208b8u` | `0x4` | 1 | `src/core/physics/session.c` (1) |
| `0x006208b8u` | `0x8` | 1 | `src/core/physics/session.c` (1) |
| `0x006208b8u` | `0xC` | 1 | `src/core/physics/session.c` (1) |
| `0x006208b8u` | `0x10` | 1 | `src/core/physics/session.c` (1) |
| `0x006208b8u` | `0x14` | 1 | `src/core/physics/session.c` (1) |
| `0x00620944u` | `0x160` | 1 | `src/core/physics/ground.c` (1) |
| `0x00620b62u` | `0x2` | 1 | `src/core/physics/session.c` (1) |
| `0x00620cd8u` | `0x5` | 1 | `src/core/physics/session.c` (1) |
| `0x00620cd8u` | `0x6` | 1 | `src/core/physics/session.c` (1) |
| `0x00620cd8u` | `0xB` | 1 | `src/core/physics/session.c` (1) |
| `0x00620cd8u` | `0xC` | 1 | `src/core/physics/session.c` (1) |
| `0x00620cd8u` | `0xAC` | 1 | `src/core/physics/session.c` (1) |
| `0x00620cd8u` | `0xAD` | 1 | `src/core/physics/session.c` (1) |
| `0x00620cd8u` | `0xBE` | 1 | `src/core/physics/session.c` (1) |
| `0x00620cd8u` | `0xC2` | 1 | `src/core/physics/session.c` (1) |
| `0x00620cd8u` | `0xC3` | 1 | `src/core/physics/session.c` (1) |
| `0x00620cd8u` | `0xED` | 1 | `src/core/physics/session.c` (1) |
| `0x00620cd8u` | `0xEE` | 1 | `src/core/physics/session.c` (1) |
| `0x00620cd8u` | `0xEF` | 1 | `src/core/physics/session.c` (1) |
| `0x00622110u` | `0x10` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x2A` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x2B` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x2C` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x30` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x31` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x36` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x38` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x61` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x62` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x64` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x65` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x66` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x6C` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x6D` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x70` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x71` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x75` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x77` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x78` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x79` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u` | `0x7A` | 1 | `src/core/physics/driver.c` (1) |
| `0x00622110u + r` | `0xEE` | 1 | `src/core/physics/session.c` (1) |
| `0x00623e50u` | `0xA` | 1 | `src/core/physics/init.c` (1) |
| `0x00623e50u` | `0xB` | 1 | `src/core/physics/init.c` (1) |
| `0x00623fb8u` | `0x38` | 1 | `src/core/physics/init.c` (1) |
| `0x00623fb8u` | `0x3F` | 1 | `src/core/physics/init.c` (1) |
| `0x006244d8u` | `0x10` | 1 | `src/core/physics/clutch.c` (1) |
| `A` | `0x18` | 1 | `src/core/physics/session.c` (1) |
| `A` | `0x1C` | 1 | `src/core/physics/session.c` (1) |
| `A` | `0x24` | 1 | `src/core/physics/session.c` (1) |
| `A` | `0x28` | 1 | `src/core/physics/session.c` (1) |
| `A` | `0x2C` | 1 | `src/core/physics/session.c` (1) |
| `AID_TABLE` | `0xDD` | 1 | `src/core/physics/aids.c` (1) |
| `AID_TABLE` | `0xDE` | 1 | `src/core/physics/aids.c` (1) |
| `AID_TABLE` | `0xDF` | 1 | `src/core/physics/aids.c` (1) |
| `AID_TABLE` | `0xE0` | 1 | `src/core/physics/aids.c` (1) |
| `AID_TABLE` | `0xE6` | 1 | `src/core/physics/aids.c` (1) |
| `AID_TABLE` | `0x115` | 1 | `src/core/physics/aids.c` (1) |
| `AID_TABLE` | `0x134` | 1 | `src/core/physics/aids.c` (1) |
| `AID_TABLE` | `0x135` | 1 | `src/core/physics/aids.c` (1) |
| `AID_TABLE` | `0x136` | 1 | `src/core/physics/aids.c` (1) |
| `AID_TABLE` | `0x137` | 1 | `src/core/physics/aids.c` (1) |
| `AID_TABLE` | `0x146` | 1 | `src/core/physics/aids.c` (1) |
| `AID_TABLE` | `0x147` | 1 | `src/core/physics/aids.c` (1) |
| `AID_TABLE` | `0x148` | 1 | `src/core/physics/aids.c` (1) |
| `AID_TABLE` | `0x149` | 1 | `src/core/physics/aids.c` (1) |
| `AI_PROFILE` | `0xD0` | 1 | `src/core/physics/race_run.c` (1) |
| `AI_SETTINGS` | `0x4` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x8` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x9` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x10` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x18` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x1C` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x20` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x24` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x2C` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x30` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x34` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x38` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x3C` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x40` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x44` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x74` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x78` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x7C` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x80` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x84` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x88` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x8C` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0x90` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0xAC` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0xB8` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0xDC` | 1 | `src/core/physics/ai.c` (1) |
| `AI_SETTINGS` | `0xE8` | 1 | `src/core/physics/ai.c` (1) |
| `B` | `0xC` | 1 | `src/core/physics/session.c` (1) |
| `B` | `0x2C` | 1 | `src/core/physics/session.c` (1) |
| `B` | `0x4C` | 1 | `src/core/physics/session.c` (1) |
| `B` | `0xAC` | 1 | `src/core/physics/session.c` (1) |
| `B` | `0x10C` | 1 | `src/core/physics/session.c` (1) |
| `B` | `0x16C` | 1 | `src/core/physics/session.c` (1) |
| `B` | `0x184` | 1 | `src/core/physics/session.c` (1) |
| `BRAKE_HEAT` | `0x4` | 1 | `src/core/physics/misc.c` (1) |
| `BRAKE_HEAT` | `0x8` | 1 | `src/core/physics/misc.c` (1) |
| `BRAKE_HEAT` | `0xC` | 1 | `src/core/physics/misc.c` (1) |
| `C` | `0x0` | 1 | `src/core/physics/session.c` (1) |
| `C` | `0x4` | 1 | `src/core/physics/session.c` (1) |
| `C` | `0x8` | 1 | `src/core/physics/session.c` (1) |
| `C` | `0xC` | 1 | `src/core/physics/session.c` (1) |
| `C` | `0x10` | 1 | `src/core/physics/session.c` (1) |
| `C` | `0x14` | 1 | `src/core/physics/session.c` (1) |
| `C` | `0x18` | 1 | `src/core/physics/session.c` (1) |
| `C` | `0x1C` | 1 | `src/core/physics/session.c` (1) |
| `C` | `0x24` | 1 | `src/core/physics/session.c` (1) |
| `C` | `0x28` | 1 | `src/core/physics/session.c` (1) |
| `C` | `0x2C` | 1 | `src/core/physics/session.c` (1) |
| `CAR_AT(c, i)` | `0x6A0` | 1 | `src/core/physics/ai.c` (1) |
| `CAR_AT(c, k)` | `0x65` | 1 | `src/core/physics/ai.c` (1) |
| `COLUMN` | `0x4` | 1 | `src/core/physics/steer.c` (1) |
| `COLUMN` | `0x10` | 1 | `src/core/physics/steer.c` (1) |
| `COLUMN` | `0x18` | 1 | `src/core/physics/steer.c` (1) |
| `COMPLIANCE` | `0x14` | 1 | `src/core/physics/steer.c` (1) |
| `COMPLIANCE` | `0x18` | 1 | `src/core/physics/steer.c` (1) |
| `COMPLIANCE` | `0x1C` | 1 | `src/core/physics/steer.c` (1) |
| `COMPLIANCE` | `0x34` | 1 | `src/core/physics/steer.c` (1) |
| `COMPLIANCE` | `0x50` | 1 | `src/core/physics/steer.c` (1) |
| `COMPLIANCE` | `0x64` | 1 | `src/core/physics/steer.c` (1) |
| `COND` | `0xF839` | 1 | `src/core/physics/race_run.c` (1) |
| `COURSE` | `0x4` | 1 | `src/core/physics/race_run.c` (1) |
| `COURSE` | `0x9C` | 1 | `src/core/physics/race_run.c` (1) |
| `COURSE_DATA` | `0xDC` | 1 | `src/core/physics/race_run.c` (1) |
| `D` | `0x1C` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x20` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x24` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x28` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x2C` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x30` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x34` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x38` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x3C` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x40` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x44` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x48` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x4C` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x50` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x54` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x58` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x5C` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x60` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x64` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x68` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x6C` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x70` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x74` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x78` | 1 | `src/core/physics/session.c` (1) |
| `D` | `0x7C` | 1 | `src/core/physics/session.c` (1) |
| `ELECTRIC` | `0x1` | 1 | `src/core/physics/setup.c` (1) |
| `ENTRIES` | `0x0` | 1 | `src/core/physics/race_run.c` (1) |
| `ENTRIES` | `0x8` | 1 | `src/core/physics/race_run.c` (1) |
| `ENTRIES` | `0x24` | 1 | `src/core/physics/race_run.c` (1) |
| `F(offset)` | `0x550` | 1 | `src/core/physics/driver.c` (1) |
| `F(offset)` | `0x5D0` | 1 | `src/core/physics/aids.c` (1) |
| `F(offset)` | `0x5D4` | 1 | `src/core/physics/aids.c` (1) |
| `F(offset)` | `0x5E4` | 1 | `src/core/physics/driver.c` (1) |
| `F(offset)` | `0x674` | 1 | `src/core/physics/driver.c` (1) |
| `F(offset)` | `0x6C0` | 1 | `src/core/physics/aids.c` (1) |
| `F(offset)` | `0x720` | 1 | `src/core/physics/driver.c` (1) |
| `F(offset)` | `0x724` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x4` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x14` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x15` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x16` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x17` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x18` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x19` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x23` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x27` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x29` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x2D` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x2F` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x31` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x5C` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x5D` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x5E` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x74` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x75` | 1 | `src/core/physics/driver.c` (1) |
| `G(offset)` | `0x76` | 1 | `src/core/physics/driver.c` (1) |
| `GAME_SETTINGS` | `0x8` | 1 | `src/core/physics/setup.c` (1) |
| `GAME_SETTINGS` | `0x101` | 1 | `src/core/physics/setup.c` (1) |
| `GAME_SETTINGS` | `0x112` | 1 | `src/core/physics/setup.c` (1) |
| `GAME_SETTINGS` | `0x131` | 1 | `src/core/physics/init.c` (1) |
| `GAME_SETTINGS` | `0x133` | 1 | `src/core/physics/init.c` (1) |
| `GAME_SETTINGS` | `0x13C` | 1 | `src/core/physics/setup.c` (1) |
| `GARAGE_SCRATCH` | `0x10000` | 1 | `src/core/physics/race_run.c` (1) |
| `GARAGE_SCRATCH` | `0x14000` | 1 | `src/core/physics/race_run.c` (1) |
| `GS(offset)` | `0x2E` | 1 | `src/core/physics/driver.c` (1) |
| `GT4_U32(GT4_U32(GT4_U32(a + 4) + 0x10000u - 0x3428u) + 4)` | `0xDC` | 1 | `src/core/physics/ai.c` (1) |
| `GT4_U32(GT4_U32(GT4_U32(lines) + 4) + 0xdc)` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `GT4_U32(GT4_U32(GT4_U32(r + 0x60) + 8) + i * 4)` | `0x3464` | 1 | `src/core/physics/session.c` (1) |
| `GT4_U32(GT4_U32(GT4_U32(race + 0x60) + 8) + i * 4)` | `0x1198` | 1 | `src/core/physics/session.c` (1) |
| `GT4_U32(GT4_U32(GT4_U32(race + 0x60) + 8) + i * 4)` | `0x3444` | 1 | `src/core/physics/session.c` (1) |
| `GT4_U32(GT4_U32(GT4_U32(race + 0x84) + 0x6c) + 0x80)` | `0x4` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(GT4_U32(GT4_U32(session + 0xcbd8u) + 4) + 0xdc)` | `0x14` | 1 | `src/core/physics/aids.c` (1) |
| `GT4_U32(GT4_U32(GT4_U32(session + CONDUCTOR_FIELD_COURSE_U) + 4) + 0xdc)` | `0x14` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(GT4_U32(a + 4) + 0x10000u - 0x3428u)` | `0x4` | 1 | `src/core/physics/ai.c` (1) |
| `GT4_U32(GT4_U32(car) + 0x60)` | `0x8` | 1 | `src/core/physics/car.c` (1) |
| `GT4_U32(GT4_U32(cond + CONDUCTOR_FIELD_COURSE_U) + 4)` | `0x80` | 1 | `src/core/physics/session.c` (1) |
| `GT4_U32(GT4_U32(cond) + 0x84)` | `0xCC8` | 1 | `src/core/physics/session.c` (1) |
| `GT4_U32(GT4_U32(course + 4) + 0xdc)` | `0x54` | 1 | `src/core/physics/session.c` (1) |
| `GT4_U32(GT4_U32(e + 0x18) + 0x10)` | `0xFE0` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(GT4_U32(entry + 0x18) + 0x10)` | `0xFE0` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(GT4_U32(gt4_race_run_conductor(g->run)) + 0x84)` | `0xCC8` | 1 | `src/app/race_game.c` (1) |
| `GT4_U32(GT4_U32(lines) + 4)` | `0xDC` | 1 | `src/core/physics/ai.c` (1) |
| `GT4_U32(GT4_U32(race + 0x84) + 0x6c)` | `0x80` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(GT4_U32(session + 0xcbd8u) + 4)` | `0xDC` | 1 | `src/core/physics/aids.c` (1) |
| `GT4_U32(GT4_U32(session + CONDUCTOR_FIELD_COURSE_U) + 4)` | `0xDC` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(RACE + 0x84)` | `0xE408` | 1 | `src/core/physics/race_run.c` (1) |
| `GT4_U32(a + 4)` | `0x10000` | 1 | `src/core/physics/ai.c` (1) |
| `GT4_U32(at + 0x18)` | `0x5B3` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(base + 0xe4) + GT4_U8(GT4_U32(base + 0xe8) + 0x544) * 4` | `0x10024` | 1 | `src/core/physics/body.c` (1) |
| `GT4_U32(base + 0xe8)` | `0x544` | 1 | `src/core/physics/body.c` (1) |
| `GT4_U32(c + 0x84)` | `0x10000` | 1 | `src/core/physics/view.c` (1) |
| `GT4_U32(c)` | `0xBC` | 1 | `src/core/physics/step.c` (1) |
| `GT4_U32(car + CAR_PARAMS)` | `0x30` | 1 | `src/app/race_game_hud.inc` (1) |
| `GT4_U32(car + CAR_PARAMS)` | `0x31` | 1 | `src/core/physics/ai.c` (1) |
| `GT4_U32(car + CAR_PARAMS)` | `0x54` | 1 | `src/core/physics/ai.c` (1) |
| `GT4_U32(car + CAR_PARAMS)` | `0x202` | 1 | `src/core/physics/clutch.c` (1) |
| `GT4_U32(car + CAR_PARAMS)` | `0x260` | 1 | `src/core/physics/gearbox.c` (1) |
| `GT4_U32(car + CAR_PARAMS)` | `0x262` | 1 | `src/core/physics/ai.c` (1) |
| `GT4_U32(car + CAR_PARAMS)` | `0x2A0` | 1 | `src/core/physics/ai.c` (1) |
| `GT4_U32(car + CAR_PARAMS)` | `0xFD8` | 1 | `src/core/physics/ai.c` (1) |
| `GT4_U32(car + CAR_PARAMS)` | `0x1074` | 1 | `src/core/physics/race_run.c` (1) |
| `GT4_U32(car + CAR_SESSION)` | `0xCBE1` | 1 | `src/core/physics/laps.c` (1) |
| `GT4_U32(car + CAR_SESSION) + CONDUCTOR_FIELD_UNK_CAB8_U + GT4_U8(car + CAR_PLAYER) * 0x30u` | `0x18` | 1 | `src/core/physics/race_run.c` (1) |
| `GT4_U32(car)` | `0x60` | 1 | `src/core/physics/car.c` (1) |
| `GT4_U32(cond + CONDUCTOR_FIELD_COURSE_U)` | `0xA0` | 1 | `src/core/physics/init.c` (1) |
| `GT4_U32(cond + CONDUCTOR_FIELD_COURSE_U) + (n < 4 ? n : 0) * 4` | `0xA0` | 1 | `src/core/physics/ai.c` (1) |
| `GT4_U32(cond)` | `0xBC` | 1 | `src/core/physics/laps.c` (1) |
| `GT4_U32(e + 0x18)` | `0x10` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(e + 0x18)` | `0x5B3` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(e + 0x2880)` | `0x4` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(entry + 0x18)` | `0x10` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(entry + 0x343c)` | `0x1CC` | 1 | `src/core/physics/session.c` (1) |
| `GT4_U32(ents + 0x24 + i * 4)` | `0x20` | 1 | `src/app/race_game.c` (1) |
| `GT4_U32(gt4_race_run_conductor(g->run))` | `0x84` | 1 | `src/app/race_game.c` (1) |
| `GT4_U32(lines)` | `0x4` | 1 | `src/core/physics/ai.c` (1) |
| `GT4_U32(mesh + 0x78) + (id & 0xffff) * 12` | `0x8` | 1 | `src/core/physics/walls.c` (1) |
| `GT4_U32(obj + 0xe4) + GT4_U8(GT4_U32(obj + 0xe8) + CAR_PLAYER) * 4u` | `0x10024` | 1 | `src/core/physics/ai.c` (1) |
| `GT4_U32(r + 0x60)` | `0x8` | 1 | `src/core/physics/session.c` (1) |
| `GT4_U32(r->cond)` | `0xB8` | 1 | `src/core/physics/race_run.c` (1) |
| `GT4_U32(race + 0x70)` | `0xA0` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(race + 0x84)` | `0x6C` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(race + 0x84)` | `0x70` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(rw + 0x9c) + (uint32_t)set * 32` | `0x18` | 1 | `src/core/physics/init.c` (1) |
| `GT4_U32(rw + 0x9c) + (uint32_t)v * 32` | `0x18` | 1 | `src/core/physics/init.c` (1) |
| `GT4_U32(s6 + 0xe4) + GT4_U8(GT4_U32(s6 + 0xe8) + CAR_PLAYER) * 4u` | `0x10024` | 1 | `src/core/physics/init.c` (1) |
| `GT4_U32(session + 0xcbd8u)` | `0x4` | 1 | `src/core/physics/aids.c` (1) |
| `GT4_U32(session + CONDUCTOR_FIELD_COURSE_U)` | `0x4` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(session)` | `0xBC` | 1 | `src/core/physics/ai.c` (1) |
| `GT4_U32(t)` | `0xF8A0` | 1 | `src/core/physics/session.c` (1) |
| `GT4_U32(world + 0x64)` | `0x4` | 1 | `src/core/physics/session.c` (1) |
| `GT4_U32(world + 0x64)` | `0x2FC` | 1 | `src/core/physics/race.c` (1) |
| `GT4_U32(world + 0x6c)` | `0xE0` | 1 | `src/core/physics/session.c` (1) |
| `HB(offset)` | `0x60` | 1 | `src/core/physics/setup.c` (1) |
| `HB(offset)` | `0x61` | 1 | `src/core/physics/setup.c` (1) |
| `HB(offset)` | `0x64` | 1 | `src/core/physics/setup.c` (1) |
| `HB(offset)` | `0x65` | 1 | `src/core/physics/setup.c` (1) |
| `HB(offset)` | `0x66` | 1 | `src/core/physics/setup.c` (1) |
| `HB(offset)` | `0x67` | 1 | `src/core/physics/setup.c` (1) |
| `HB(offset)` | `0x68` | 1 | `src/core/physics/setup.c` (1) |
| `HB(offset)` | `0x69` | 1 | `src/core/physics/setup.c` (1) |
| `HB(offset)` | `0x6A` | 1 | `src/core/physics/setup.c` (1) |
| `HB(offset)` | `0x6E` | 1 | `src/core/physics/setup.c` (1) |
| `HB(offset)` | `0x6F` | 1 | `src/core/physics/setup.c` (1) |
| `HB(offset)` | `0x70` | 1 | `src/core/physics/setup.c` (1) |
| `HB(offset)` | `0x71` | 1 | `src/core/physics/setup.c` (1) |
| `HB(offset)` | `0x72` | 1 | `src/core/physics/setup.c` (1) |
| `HB(offset)` | `0x73` | 1 | `src/core/physics/setup.c` (1) |
| `HB(offset)` | `0x74` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xC0` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xC4` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xC8` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xCC` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xD0` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xD4` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xD8` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xDC` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xE0` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xE4` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xE8` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xEC` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xF0` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xF4` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xF8` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0xFC` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0x100` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0x104` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0x108` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0x10C` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0x110` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0x114` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0x118` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0x11C` | 1 | `src/core/physics/setup.c` (1) |
| `HF(offset)` | `0x120` | 1 | `src/core/physics/setup.c` (1) |
| `OF(offset)` | `0xC0` | 1 | `src/core/physics/ai.c` (1) |
| `OU8(offset)` | `0x3C` | 1 | `src/core/physics/ai.c` (1) |
| `OU8(offset)` | `0x48` | 1 | `src/core/physics/ai.c` (1) |
| `OU8(offset)` | `0x50` | 1 | `src/core/physics/ai.c` (1) |
| `OU8(offset)` | `0x6F` | 1 | `src/core/physics/driver.c` (1) |
| `P(offset)` | `0x0` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x1` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x2` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x3` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x4` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x6` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x7` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x8` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x9` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0xA` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0xB` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0xC` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0xD` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0xE` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0xF` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x10` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x11` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x14` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x16` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x17` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x18` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x19` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x1A` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x1B` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x1C` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x1D` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x1E` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x1F` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x26` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x28` | 1 | `src/core/physics/session.c` (1) |
| `P(offset)` | `0x29` | 1 | `src/core/physics/session.c` (1) |
| `PB(offset)` | `0x0` | 1 | `src/core/physics/session.c` (1) |
| `PB(offset)` | `0x1` | 1 | `src/core/physics/session.c` (1) |
| `PB(offset)` | `0x2` | 1 | `src/core/physics/session.c` (1) |
| `PB(offset)` | `0x5` | 1 | `src/core/physics/session.c` (1) |
| `PB(offset)` | `0x6` | 1 | `src/core/physics/session.c` (1) |
| `PB(offset)` | `0xAE` | 1 | `src/core/physics/session.c` (1) |
| `PB(offset)` | `0xAF` | 1 | `src/core/physics/session.c` (1) |
| `PROFILE` | `0xD0` | 1 | `src/core/physics/race_run.c` (1) |
| `PROFILE` | `0x1CC` | 1 | `src/core/physics/race_run.c` (1) |
| `Q` | `0x18` | 1 | `src/core/physics/session.c` (1) |
| `Q` | `0x30` | 1 | `src/core/physics/session.c` (1) |
| `Q` | `0x60` | 1 | `src/core/physics/session.c` (1) |
| `Q` | `0x78` | 1 | `src/core/physics/session.c` (1) |
| `Q` | `0x90` | 1 | `src/core/physics/session.c` (1) |
| `Q_EVENT(q, i` | `0x1` | 1 | `src/core/physics/race.c` (1) |
| `R(offset)` | `0x0` | 1 | `src/core/physics/driver.c` (1) |
| `R(offset)` | `0x3` | 1 | `src/core/physics/driver.c` (1) |
| `R(offset)` | `0x5` | 1 | `src/core/physics/driver.c` (1) |
| `R(offset)` | `0x6` | 1 | `src/core/physics/driver.c` (1) |
| `R(offset)` | `0x7` | 1 | `src/core/physics/driver.c` (1) |
| `R(offset)` | `0xA` | 1 | `src/core/physics/driver.c` (1) |
| `R(offset)` | `0xB` | 1 | `src/core/physics/driver.c` (1) |
| `R(offset)` | `0xC` | 1 | `src/core/physics/driver.c` (1) |
| `R(offset)` | `0xD` | 1 | `src/core/physics/driver.c` (1) |
| `R(offset)` | `0x10` | 1 | `src/core/physics/driver.c` (1) |
| `R(offset)` | `0x14` | 1 | `src/core/physics/driver.c` (1) |
| `R(offset)` | `0x1B` | 1 | `src/core/physics/driver.c` (1) |
| `R(offset)` | `0x1C` | 1 | `src/core/physics/driver.c` (1) |
| `RACE` | `0x14` | 1 | `src/core/physics/race_run.c` (1) |
| `RACE` | `0x18` | 1 | `src/core/physics/race_run.c` (1) |
| `RACE` | `0x1C` | 1 | `src/core/physics/race_run.c` (1) |
| `RACE` | `0x60` | 1 | `src/core/physics/race_run.c` (1) |
| `RACE` | `0x70` | 1 | `src/core/physics/race_run.c` (1) |
| `RACE` | `0x80` | 1 | `src/core/physics/race_run.c` (1) |
| `RACE` | `0x88` | 1 | `src/core/physics/race_run.c` (1) |
| `RACE` | `0x8C` | 1 | `src/core/physics/race_run.c` (1) |
| `RATE_REC + 0x600` | `0xFE0` | 1 | `src/core/physics/race_run.c` (1) |
| `RB(offset)` | `0x0` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0x1` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0x4` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0x5` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0x7` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0x8` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0x9` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0xA` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0xB` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0xC` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0xD` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0xE` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0xF` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0x15` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0x16` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0x17` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0x18` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0x1C` | 1 | `src/core/physics/ai.c` (1) |
| `RB(offset)` | `0x1D` | 1 | `src/core/physics/ai.c` (1) |
| `ROLLING_CURVE` | `0x4C` | 1 | `src/core/physics/setup.c` (1) |
| `S` | `0xB` | 1 | `src/core/physics/session.c` (1) |
| `S` | `0xC` | 1 | `src/core/physics/session.c` (1) |
| `SB(offset)` | `0x4` | 1 | `src/core/physics/session.c` (1) |
| `SB(offset)` | `0x6` | 1 | `src/core/physics/session.c` (1) |
| `SB(offset)` | `0x7` | 1 | `src/core/physics/session.c` (1) |
| `SB(offset)` | `0xC` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x10` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x11` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x12` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x13` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x15` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x16` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x1C` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x25` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x2B` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x2C` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x2D` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x2E` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x31` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x32` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x35` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x39` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x3A` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x3B` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x3C` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x3D` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x3E` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x40` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x41` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x42` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x43` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x44` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x45` | 1 | `src/core/physics/ai.c` (1) |
| `SB(offset)` | `0x46` | 1 | `src/core/physics/ai.c` (1) |
| `SETTINGS` | `0x40` | 1 | `src/core/physics/race_run.c` (1) |
| `SETTINGS` | `0xB6` | 1 | `src/core/physics/drivetrain.c` (1) |
| `SETTINGS` | `0xB7` | 1 | `src/core/physics/drivetrain.c` (1) |
| `SETTINGS` | `0x139` | 1 | `src/core/physics/suspension.c` (1) |
| `SET_B(offset)` | `0xDC` | 1 | `src/core/physics/session.c` (1) |
| `SET_B(offset)` | `0xDD` | 1 | `src/core/physics/session.c` (1) |
| `SET_B(offset)` | `0xDE` | 1 | `src/core/physics/session.c` (1) |
| `SET_B(offset)` | `0xDF` | 1 | `src/core/physics/session.c` (1) |
| `SET_B(offset)` | `0xE4` | 1 | `src/core/physics/session.c` (1) |
| `SET_B(offset)` | `0xE5` | 1 | `src/core/physics/session.c` (1) |
| `SET_B(offset)` | `0xE6` | 1 | `src/core/physics/session.c` (1) |
| `SET_B(offset)` | `0xE7` | 1 | `src/core/physics/session.c` (1) |
| `SET_B(offset)` | `0xE8` | 1 | `src/core/physics/session.c` (1) |
| `SET_B(offset)` | `0xE9` | 1 | `src/core/physics/session.c` (1) |
| `SHAKE_PARAM` | `0x24` | 1 | `src/core/physics/walls.c` (1) |
| `SHAKE_PARAM` | `0x28` | 1 | `src/core/physics/walls.c` (1) |
| `SHAKE_PARAM` | `0x30` | 1 | `src/core/physics/walls.c` (1) |
| `SHIFT_SETTINGS` | `0x9` | 1 | `src/core/physics/gearbox.c` (1) |
| `SHIFT_SETTINGS` | `0xA` | 1 | `src/core/physics/gearbox.c` (1) |
| `SHIFT_SETTINGS` | `0xB` | 1 | `src/core/physics/gearbox.c` (1) |
| `STEER_TABLES` | `0x50` | 1 | `src/core/physics/driver.c` (1) |
| `SURFACE_BUMPS` | `0xAC` | 1 | `src/core/physics/ground.c` (1) |
| `SURFACE_BUMPS` | `0x10C` | 1 | `src/core/physics/ground.c` (1) |
| `SURFACE_BUMPS` | `0x184` | 1 | `src/core/physics/ground.c` (1) |
| `T` | `0x0` | 1 | `src/core/physics/session.c` (1) |
| `T` | `0x4` | 1 | `src/core/physics/session.c` (1) |
| `T` | `0x8` | 1 | `src/core/physics/session.c` (1) |
| `T` | `0xC` | 1 | `src/core/physics/session.c` (1) |
| `T` | `0x10` | 1 | `src/core/physics/session.c` (1) |
| `T` | `0x14` | 1 | `src/core/physics/session.c` (1) |
| `T` | `0x18` | 1 | `src/core/physics/session.c` (1) |
| `T` | `0x1C` | 1 | `src/core/physics/session.c` (1) |
| `T` | `0x20` | 1 | `src/core/physics/session.c` (1) |
| `T` | `0x24` | 1 | `src/core/physics/session.c` (1) |
| `T` | `0x28` | 1 | `src/core/physics/session.c` (1) |
| `T` | `0x2C` | 1 | `src/core/physics/session.c` (1) |
| `T` | `0x30` | 1 | `src/core/physics/session.c` (1) |
| `U8(offset)` | `0x56B` | 1 | `src/core/physics/driver.c` (1) |
| `U8(offset)` | `0x5C0` | 1 | `src/core/physics/driver.c` (1) |
| `U8(offset)` | `0x5E8` | 1 | `src/core/physics/driver.c` (1) |
| `WEAR_LIMITS` | `0x14` | 1 | `src/core/physics/ground.c` (1) |
| `WORLD` | `0x70` | 1 | `src/core/physics/race_run.c` (1) |
| `a` | `0xD` | 1 | `src/core/physics/setup.c` (1) |
| `a` | `0xE` | 1 | `src/core/physics/setup.c` (1) |
| `a` | `0xF` | 1 | `src/core/physics/setup.c` (1) |
| `a` | `0x1D` | 1 | `src/core/physics/setup.c` (1) |
| `a` | `0x1E` | 1 | `src/core/physics/setup.c` (1) |
| `a` | `0x1F` | 1 | `src/core/physics/setup.c` (1) |
| `a` | `0x31` | 1 | `src/core/physics/setup.c` (1) |
| `a` | `0x32` | 1 | `src/core/physics/setup.c` (1) |
| `a` | `0x33` | 1 | `src/core/physics/setup.c` (1) |
| `a` | `0x38` | 1 | `src/core/physics/setup.c` (1) |
| `a` | `0x40` | 1 | `src/core/physics/setup.c` (1) |
| `a` | `0x48` | 1 | `src/core/physics/setup.c` (1) |
| `a` | `0x67` | 1 | `src/core/physics/ai.c` (1) |
| `a` | `0x440` | 1 | `src/core/physics/session.c` (1) |
| `a` | `0x441` | 1 | `src/core/physics/session.c` (1) |
| `a` | `0x466` | 1 | `src/core/physics/session.c` (1) |
| `a` | `0x467` | 1 | `src/core/physics/session.c` (1) |
| `a` | `0x4A4` | 1 | `src/core/physics/session.c` (1) |
| `a` | `0x4B2` | 1 | `src/core/physics/session.c` (1) |
| `a` | `0x5A8` | 1 | `src/core/physics/ai.c` (1) |
| `a` | `0x614` | 1 | `src/core/physics/session.c` (1) |
| `a` | `0x754` | 1 | `src/core/physics/ai.c` (1) |
| `a0` | `0xE408` | 1 | `src/core/physics/race_run.c` (1) |
| `a1` | `0x22` | 1 | `src/core/physics/session.c` (1) |
| `a1` | `0x28` | 1 | `src/core/physics/session.c` (1) |
| `a1` | `0x29` | 1 | `src/core/physics/session.c` (1) |
| `a1` | `0x2A` | 1 | `src/core/physics/session.c` (1) |
| `a1` | `0x75F` | 1 | `src/core/physics/session.c` (1) |
| `a3` | `0x4` | 1 | `src/core/physics/ai.c` (1) |
| `a3` | `0xD` | 1 | `src/core/physics/ai.c` (1) |
| `a3` | `0xE` | 1 | `src/core/physics/ai.c` (1) |
| `a3` | `0xF` | 1 | `src/core/physics/ai.c` (1) |
| `a3` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `a3` | `0x1D` | 1 | `src/core/physics/ai.c` (1) |
| `a3` | `0x1E` | 1 | `src/core/physics/ai.c` (1) |
| `a3` | `0x1F` | 1 | `src/core/physics/ai.c` (1) |
| `a3` | `0x20` | 1 | `src/core/physics/ai.c` (1) |
| `a3` | `0x24` | 1 | `src/core/physics/ai.c` (1) |
| `a3` | `0x28` | 1 | `src/core/physics/ai.c` (1) |
| `a3` | `0x2C` | 1 | `src/core/physics/ai.c` (1) |
| `a3` | `0x30` | 1 | `src/core/physics/ai.c` (1) |
| `a3` | `0x31` | 1 | `src/core/physics/ai.c` (1) |
| `a3` | `0x32` | 1 | `src/core/physics/ai.c` (1) |
| `a3` | `0x33` | 1 | `src/core/physics/ai.c` (1) |
| `at` | `0x2` | 1 | `src/core/physics/race.c` (1) |
| `at` | `0x4` | 1 | `src/core/physics/race.c` (1) |
| `at` | `0xC` | 1 | `src/core/physics/race.c` (1) |
| `at` | `0x10` | 1 | `src/core/physics/race.c` (1) |
| `at` | `0x18` | 1 | `src/core/physics/race.c` (1) |
| `b` | `0x18` | 1 | `src/core/physics/ai.c` (1) |
| `b` | `0x25` | 1 | `src/core/physics/session.c` (1) |
| `b` | `0x26` | 1 | `src/core/physics/session.c` (1) |
| `b` | `0x28` | 1 | `src/core/physics/session.c` (1) |
| `b` | `0x29` | 1 | `src/core/physics/session.c` (1) |
| `b` | `0x2A` | 1 | `src/core/physics/session.c` (1) |
| `b` | `0x30` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x34` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x448` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x458` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x45C` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x460` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x463` | 1 | `src/core/physics/ai.c` (1) |
| `b` | `0x474` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x478` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x47C` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x480` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x498` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x49C` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x4A0` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x4B1` | 1 | `src/core/physics/race.c` (1) |
| `b` | `0x4B5` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x4BD` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x4BE` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x4C4` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x4C8` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x4D4` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x4D8` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x4DC` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x4E4` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x4E5` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x521` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x523` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x527` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x528` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x529` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x52A` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x52C` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x534` | 1 | `src/core/physics/wheels.c` (1) |
| `b` | `0x550` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x554` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x564` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x574` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x578` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x588` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x593` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x598` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x59C` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x5AC` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x5F8` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x5F9` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x5FA` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x5FB` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x60C` | 1 | `src/core/physics/walls.c` (1) |
| `b` | `0x60D` | 1 | `src/core/physics/walls.c` (1) |
| `b` | `0x624` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x628` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x634` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x638` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x644` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x648` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x654` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x658` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x65A` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x65C` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x660` | 1 | `src/core/physics/init.c` (1) |
| `b` | `0x754` | 1 | `src/core/physics/ai.c` (1) |
| `b + CAR_BLOCK_ORIENTATION_BASIS_0` | `0x4` | 1 | `src/core/physics/driver.c` (1) |
| `b + CAR_BLOCK_ORIENTATION_BASIS_1` | `0x4` | 1 | `src/core/physics/driver.c` (1) |
| `b + P_A` | `0x4` | 1 | `src/core/physics/walls.c` (1) |
| `ba` | `0x4A4` | 1 | `src/core/physics/race.c` (1) |
| `base` | `0xE4` | 1 | `src/core/physics/body.c` (1) |
| `base` | `0xE8` | 1 | `src/core/physics/body.c` (1) |
| `base` | `0xCDE0` | 1 | `src/app/race_game_bspec.inc` (1) |
| `base` | `0xD400` | 1 | `src/app/race_game_bspec.inc` (1) |
| `base` | `0xD480` | 1 | `src/app/race_game_bspec.inc` (1) |
| `bb` | `0x444` | 1 | `src/core/physics/race.c` (1) |
| `bb` | `0x4A4` | 1 | `src/core/physics/race.c` (1) |
| `bb` | `0x4B2` | 1 | `src/core/physics/race.c` (1) |
| `block` | `0x4B` | 1 | `src/core/physics/aids.c` (1) |
| `block` | `0x4C` | 1 | `src/core/physics/aids.c` (1) |
| `block` | `0x4E` | 1 | `src/core/physics/aids.c` (1) |
| `block` | `0x6E` | 1 | `src/core/physics/aids.c` (1) |
| `buf` | `0xD` | 1 | `src/core/physics/setup.c` (1) |
| `buf` | `0x30` | 1 | `src/core/physics/init.c` (1) |
| `buf` | `0x48` | 1 | `src/core/physics/init.c` (1) |
| `c` | `0x1` | 1 | `src/core/physics/init.c` (1) |
| `c` | `0x9` | 1 | `src/core/physics/init.c` (1) |
| `c` | `0x84` | 1 | `src/core/physics/view.c` (1) |
| `c` | `0xE0` | 1 | `src/core/physics/view.c` (1) |
| `c` | `0xCBF8` | 1 | `src/core/physics/step.c` (1) |
| `c` | `0xF819` | 1 | `src/core/physics/session.c` (1) |
| `c` | `0xF859` | 1 | `src/core/physics/license_session.c` (1) |
| `c` | `0xF85D` | 1 | `src/core/physics/license_session.c` (1) |
| `ca` | `0x5A8` | 1 | `src/core/physics/race.c` (1) |
| `car` | `0x30` | 1 | `src/core/physics/ai.c` (1) |
| `car` | `0x32` | 1 | `src/core/physics/driver.c` (1) |
| `car` | `0x38` | 1 | `src/app/race_game.c` (1) |
| `car` | `0x3C` | 1 | `src/app/race_game.c` (1) |
| `car` | `0x5C` | 1 | `src/core/physics/ai.c` (1) |
| `car` | `0x81` | 1 | `src/core/physics/aids.c` (1) |
| `car` | `0x11C` | 1 | `src/core/physics/init.c` (1) |
| `car` | `0x128` | 1 | `src/core/physics/init.c` (1) |
| `car` | `0x16C` | 1 | `src/app/race_game_license.inc` (1) |
| `car` | `0x1B4` | 1 | `src/core/physics/ai.c` (1) |
| `car` | `0x1C8` | 1 | `src/core/physics/init.c` (1) |
| `car` | `0x1ED` | 1 | `src/core/physics/init.c` (1) |
| `car` | `0x547` | 1 | `src/core/physics/init.c` (1) |
| `car` | `0x560` | 1 | `src/core/physics/walls.c` (1) |
| `car` | `0x564` | 1 | `src/core/physics/gearbox.c` (1) |
| `car` | `0x5BA` | 1 | `src/core/physics/session.c` (1) |
| `car` | `0x5E9` | 1 | `src/core/physics/aids.c` (1) |
| `car` | `0x6C5` | 1 | `src/core/physics/race.c` (1) |
| `car` | `0x6EC` | 1 | `src/core/physics/steer.c` (1) |
| `car` | `0x70C` | 1 | `src/core/physics/step.c` (1) |
| `car` | `0x710` | 1 | `src/core/physics/step.c` (1) |
| `car` | `0x730` | 1 | `src/core/physics/ground.c` (1) |
| `car` | `0x731` | 1 | `src/core/physics/ground.c` (1) |
| `car` | `0x770` | 1 | `src/core/physics/steer.c` (1) |
| `car` | `0x78A` | 1 | `src/core/physics/init.c` (1) |
| `car` | `0x78B` | 1 | `src/core/physics/init.c` (1) |
| `car` | `0x78E` | 1 | `src/core/physics/init.c` (1) |
| `car` | `0x7EB` | 1 | `src/core/physics/license_session.c` (1) |
| `car` | `0x890` | 1 | `src/core/physics/race_run.c` (1) |
| `car` | `0x8AC` | 1 | `src/core/physics/race_run.c` (1) |
| `car` | `0x8C8` | 1 | `src/core/physics/misc.c` (1) |
| `car` | `0x8E0` | 1 | `src/core/physics/steer.c` (1) |
| `car` | `0x900` | 1 | `src/app/race_game_license.inc` (1) |
| `car` | `0x994` | 1 | `src/core/physics/misc.c` (1) |
| `car` | `0x998` | 1 | `src/core/physics/misc.c` (1) |
| `car` | `0x1084` | 1 | `src/core/physics/init.c` (1) |
| `car` | `0x1088` | 1 | `src/core/physics/init.c` (1) |
| `car` | `0x108C` | 1 | `src/core/physics/init.c` (1) |
| `car + (uint32_t)i * 0xec` | `0x194` | 1 | `src/core/physics/ai.c` (1) |
| `car + (uint32_t)k * 0xec` | `0x215` | 1 | `src/app/race_game_hud.inc` (1) |
| `car + CAR_140` | `0x14` | 1 | `src/core/physics/chassis.c` (1) |
| `car + CAR_140 + 0x18` | `0x4` | 1 | `src/core/physics/chassis.c` (1) |
| `car + CAR_158` | `0x4` | 1 | `src/core/physics/suspension.c` (1) |
| `car + CAR_ANGLES` | `0x0` | 1 | `src/core/physics/walls.c` (1) |
| `car + CAR_FIELD_DRIVER_MODEL` | `0x1B` | 1 | `src/core/physics/laps.c` (1) |
| `car + CAR_FIELD_DRIVER_MODEL` | `0x1E` | 1 | `src/core/physics/init.c` (1) |
| `car + CAR_FIELD_DRIVER_MODEL` | `0xB8` | 1 | `src/core/physics/ai.c` (1) |
| `car + CAR_FIELD_DRIVER_MODEL` | `0xBC` | 1 | `src/core/physics/ai.c` (1) |
| `car + CAR_FIELD_DRIVER_MODEL` | `0xC0` | 1 | `src/core/physics/ai.c` (1) |
| `car + CAR_FRONT_SPEED` | `0x4` | 1 | `src/core/physics/ground.c` (1) |
| `car + CAR_MATRIX` | `0x10` | 1 | `src/core/physics/chassis.c` (1) |
| `car + CAR_MATRIX` | `0x14` | 1 | `src/core/physics/chassis.c` (1) |
| `car + CAR_MATRIX + 0xc` | `0x4` | 1 | `src/core/physics/body.c` (1) |
| `car + CAR_POSITION` | `0x464` | 1 | `src/core/physics/laps.c` (1) |
| `car + CAR_POSITION` | `0x467` | 1 | `src/core/physics/ai.c` (1) |
| `car + CAR_POSITION` | `0x4A4` | 1 | `src/core/physics/ai.c` (1) |
| `car + CAR_POSITION` | `0x4AC` | 1 | `src/core/physics/race_run.c` (1) |
| `car + CAR_POSITION` | `0x4B1` | 1 | `src/core/physics/tyre.c` (1) |
| `car + CAR_POSITION` | `0x4B2` | 1 | `src/core/physics/race_run.c` (1) |
| `car + CAR_POSITION` | `0x570` | 1 | `src/core/physics/view.c` (1) |
| `car + CAR_POSITION` | `0x632` | 1 | `src/core/physics/race.c` (1) |
| `car + CAR_REF_A_PREV` | `0x4` | 1 | `src/core/physics/chassis.c` (1) |
| `car + CAR_REF_B` | `0x8` | 1 | `src/core/physics/chassis.c` (1) |
| `car + CAR_STEER_RAW_PREV` | `0x2` | 1 | `src/core/physics/driver.c` (1) |
| `car + CAR_VIBRATION` | `0x1` | 1 | `src/core/physics/view.c` (1) |
| `car + CAR_VIBRATION` | `0x2` | 1 | `src/core/physics/view.c` (1) |
| `car + CAR_WHEELS + i * 0xec` | `0x20` | 1 | `src/core/physics/init.c` (1) |
| `car + CAR_WHEELS + i * 0xec` | `0x28` | 1 | `src/core/physics/init.c` (1) |
| `car + MSG` | `0x1` | 1 | `src/core/physics/misc.c` (1) |
| `car_at(c, GT4_U32(c + CONDUCTOR_FIELD_POSITION_ORDER))` | `0x5B6` | 1 | `src/core/physics/session.c` (1) |
| `car_at(c, a1)` | `0x104` | 1 | `src/core/physics/session.c` (1) |
| `car_at(c, a1) + 0x104` | `0x614` | 1 | `src/core/physics/session.c` (1) |
| `car_at(c, i)` | `0x56A` | 1 | `src/core/physics/license_session.c` (1) |
| `car_at(cond, GT4_U32(cond + CONDUCTOR_FIELD_POSITION_ORDER_U))` | `0x5B6` | 1 | `src/core/physics/session.c` (1) |
| `car_at(cond, a1)` | `0x718` | 1 | `src/core/physics/session.c` (1) |
| `car_at(cond, cur)` | `0x71C` | 1 | `src/core/physics/session.c` (1) |
| `car_at(cond, i)` | `0x5B6` | 1 | `src/core/physics/session.c` (1) |
| `car_at(cond, other)` | `0x71C` | 1 | `src/core/physics/session.c` (1) |
| `car_at(self, a1)` | `0x5B7` | 1 | `src/core/physics/session.c` (1) |
| `car_of(cond, i)` | `0x736` | 1 | `src/core/physics/race.c` (1) |
| `cb` | `0x5A8` | 1 | `src/core/physics/race.c` (1) |
| `cd` | `0xDC` | 1 | `src/app/race_game.c` (1) |
| `cfg` | `0x4` | 1 | `src/core/physics/setup.c` (1) |
| `cfg` | `0x7` | 1 | `src/core/physics/setup.c` (1) |
| `cfg` | `0xA` | 1 | `src/core/physics/setup.c` (1) |
| `cfg` | `0xBC` | 1 | `src/core/physics/setup.c` (1) |
| `cfg` | `0xBD` | 1 | `src/core/physics/setup.c` (1) |
| `cfg` | `0x102` | 1 | `src/core/physics/setup.c` (1) |
| `cfg` | `0x105` | 1 | `src/core/physics/setup.c` (1) |
| `cfg` | `0x107` | 1 | `src/core/physics/setup.c` (1) |
| `cfg` | `0x109` | 1 | `src/core/physics/setup.c` (1) |
| `cfg` | `0x10B` | 1 | `src/core/physics/setup.c` (1) |
| `cfg` | `0x118` | 1 | `src/core/physics/setup.c` (1) |
| `cfg` | `0x119` | 1 | `src/core/physics/setup.c` (1) |
| `cmd` | `0x7` | 1 | `src/core/physics/driver.c` (1) |
| `cmd` | `0x10` | 1 | `src/core/physics/driver.c` (1) |
| `cmd` | `0x15` | 1 | `src/core/physics/driver.c` (1) |
| `cond` | `0xCBDF` | 1 | `src/core/physics/init.c` (1) |
| `cond` | `0xCBE1` | 1 | `src/core/physics/init.c` (1) |
| `cond` | `0xCBE9` | 1 | `src/core/physics/init.c` (1) |
| `cond` | `0xCBF0` | 1 | `src/core/physics/init.c` (1) |
| `cond` | `0xCBF8` | 1 | `src/core/physics/session.c` (1) |
| `cond` | `0xD3EC` | 1 | `src/core/physics/session.c` (1) |
| `cond` | `0xD40C` | 1 | `src/core/physics/session.c` (1) |
| `cond` | `0xD44C` | 1 | `src/core/physics/session.c` (1) |
| `cond` | `0xD60C` | 1 | `src/core/physics/session.c` (1) |
| `cond` | `0xD624` | 1 | `src/core/physics/session.c` (1) |
| `cond` | `0xD924` | 1 | `src/core/physics/session.c` (1) |
| `cond` | `0xF839` | 1 | `src/core/physics/session.c` (1) |
| `cond` | `0xF8BC` | 1 | `src/core/physics/session.c` (1) |
| `cond` | `0xF8C0` | 1 | `src/core/physics/session.c` (1) |
| `cond` | `0xF8F0` | 1 | `src/core/physics/session.c` (1) |
| `cond` | `0xF8F4` | 1 | `src/core/physics/session.c` (1) |
| `cond` | `0xF928` | 1 | `src/core/physics/init.c` (1) |
| `cond` | `0xF929` | 1 | `src/core/physics/init.c` (1) |
| `cond` | `0xF92A` | 1 | `src/core/physics/init.c` (1) |
| `cond` | `0xF92B` | 1 | `src/core/physics/init.c` (1) |
| `cond` | `0xF968` | 1 | `src/core/physics/init.c` (1) |
| `cond` | `0x10024` | 1 | `src/core/physics/init.c` (1) |
| `cond + CONDUCTOR_FIELD_LINES_U` | `0xC` | 1 | `src/core/physics/init.c` (1) |
| `cond + CONDUCTOR_FIELD_RULES_U` | `0x4C` | 1 | `src/core/physics/init.c` (1) |
| `cond + CONDUCTOR_FIELD_RULES_U` | `0x4E` | 1 | `src/core/physics/session.c` (1) |
| `cond + CONDUCTOR_FIELD_TYRE_STATE` | `0x68` | 1 | `src/app/race_game_hud.inc` (1) |
| `cond + CONDUCTOR_FIELD_TYRE_STATE` | `0x69` | 1 | `src/app/race_game_hud.inc` (1) |
| `cond + a1` | `0xF866` | 1 | `src/core/physics/session.c` (1) |
| `cond + i * 4` | `0xF8F8` | 1 | `src/core/physics/init.c` (1) |
| `cond + i * 4` | `0xF910` | 1 | `src/core/physics/init.c` (1) |
| `course` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `course` | `0x9C` | 1 | `src/core/physics/lines.c` (1) |
| `course_of(cond)` | `0xA0` | 1 | `src/core/physics/session.c` (1) |
| `d` | `0x8` | 1 | `src/core/physics/session.c` (1) |
| `d` | `0x18` | 1 | `src/core/physics/session.c` (1) |
| `d` | `0x54` | 1 | `src/core/physics/init.c` (1) |
| `d` | `0x70` | 1 | `src/core/physics/setup.c` (1) |
| `d` | `0x94` | 1 | `src/core/physics/setup.c` (1) |
| `d` | `0xB0` | 1 | `src/core/physics/setup.c` (1) |
| `d` | `0xB4` | 1 | `src/core/physics/setup.c` (1) |
| `d` | `0xC0` | 1 | `src/core/physics/setup.c` (1) |
| `d` | `0xC4` | 1 | `src/core/physics/setup.c` (1) |
| `d` | `0xC8` | 1 | `src/core/physics/setup.c` (1) |
| `d` | `0xCC` | 1 | `src/core/physics/setup.c` (1) |
| `data` | `0xDC` | 1 | `src/app/race_game.c` (1) |
| `drive` | `0x8` | 1 | `src/core/physics/clutch.c` (1) |
| `drive` | `0x24` | 1 | `src/core/physics/wheels.c` (1) |
| `drive` | `0x30` | 1 | `src/core/physics/clutch.c` (1) |
| `drive` | `0x68` | 1 | `src/core/physics/gearbox.c` (1) |
| `drive` | `0xC4` | 1 | `src/core/physics/clutch.c` (1) |
| `drive_of(car)` | `0x2` | 1 | `src/core/physics/gearbox.c` (1) |
| `dst` | `0x1` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x2` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x4` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x8` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x9` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0xA` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0xB` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x10` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x18` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x1C` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x20` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x24` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x2C` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x30` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x34` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x38` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x3C` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x40` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x44` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x5C` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x74` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x88` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x8C` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x90` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x94` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x98` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x9C` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0xA0` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0xA4` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0xA8` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0xC4` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0xC8` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0xCC` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0xD0` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0xD4` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0xD8` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0xF4` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0xF8` | 1 | `src/core/physics/ai.c` (1) |
| `dst` | `0x128` | 1 | `src/core/physics/ai.c` (1) |
| `e` | `0x20` | 1 | `src/core/physics/view.c` (1) |
| `e` | `0x30` | 1 | `src/core/physics/view.c` (1) |
| `e` | `0xD0` | 1 | `src/core/physics/setup.c` (1) |
| `e` | `0x4E0` | 1 | `src/core/physics/session.c` (1) |
| `e` | `0x4E4` | 1 | `src/core/physics/session.c` (1) |
| `e` | `0x4E8` | 1 | `src/core/physics/session.c` (1) |
| `e` | `0x4EC` | 1 | `src/core/physics/session.c` (1) |
| `e` | `0x4F0` | 1 | `src/core/physics/session.c` (1) |
| `e` | `0x4FC` | 1 | `src/core/physics/session.c` (1) |
| `e` | `0x500` | 1 | `src/core/physics/session.c` (1) |
| `e` | `0x510` | 1 | `src/core/physics/session.c` (1) |
| `e` | `0x119C` | 1 | `src/core/physics/race.c` (1) |
| `e` | `0x2894` | 1 | `src/core/physics/race.c` (1) |
| `e` | `0x3488` | 1 | `src/core/physics/race.c` (1) |
| `e + 0x119c` | `0x40` | 1 | `src/core/physics/race.c` (1) |
| `e + E_NORMAL` | `0x4` | 1 | `src/core/physics/walls.c` (1) |
| `e1 + P_A` | `0x4` | 1 | `src/core/physics/walls.c` (1) |
| `ea` | `0x4` | 1 | `src/core/physics/walls.c` (1) |
| `ea` | `0x8` | 1 | `src/core/physics/walls.c` (1) |
| `ea` | `0xC` | 1 | `src/core/physics/walls.c` (1) |
| `eh` | `0x4` | 1 | `src/core/physics/walls.c` (1) |
| `engine` | `0xE4` | 1 | `src/core/physics/clutch.c` (1) |
| `engine` | `0xEC` | 1 | `src/core/physics/clutch.c` (1) |
| `entry` | `0x18` | 1 | `src/core/physics/race.c` (1) |
| `entry` | `0x20` | 1 | `src/app/race_game.c` (1) |
| `entry` | `0x38` | 1 | `src/app/race_game.c` (1) |
| `entry` | `0x1B0` | 1 | `src/app/race_game_results.inc` (1) |
| `entry` | `0x11E8` | 1 | `src/app/race_game_bspec.inc` (1) |
| `entry` | `0x11EC` | 1 | `src/app/race_game_bspec.inc` (1) |
| `entry` | `0x344C` | 1 | `src/core/physics/init.c` (1) |
| `entry` | `0x3450` | 1 | `src/core/physics/init.c` (1) |
| `entry` | `0x3454` | 1 | `src/core/physics/init.c` (1) |
| `entry` | `0x3458` | 1 | `src/core/physics/init.c` (1) |
| `entry_at[0]` | `0x3448` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_at[0]` | `0x3450` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_at[0]` | `0x3454` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_at[1]` | `0x343C` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_at[1]` | `0x3448` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_at[1]` | `0x3450` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_at[1]` | `0x3454` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_at[i]` | `0x24` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_at[i]` | `0x38` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_at[i]` | `0x11DC` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_at[i]` | `0x343C` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_at[i]` | `0x3450` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_at[i]` | `0x3454` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_at[i]` | `0x3458` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_at[i]` | `0x3468` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_at[i]` | `0x347C` | 1 | `src/core/physics/race_run.c` (1) |
| `entry_of(race, i)` | `0x119C` | 1 | `src/core/physics/race.c` (1) |
| `ents` | `0x24` | 1 | `src/app/race_game.c` (1) |
| `ev` | `0x10` | 1 | `src/core/physics/driver.c` (1) |
| `ev` | `0x14` | 1 | `src/core/physics/driver.c` (1) |
| `ev` | `0x15` | 1 | `src/core/physics/driver.c` (1) |
| `f` | `0x4` | 1 | `src/app/race_game.c` (1) |
| `flags` | `0x1F` | 1 | `src/core/physics/surface.c` (1) |
| `flags` | `0x68` | 1 | `src/core/physics/ai.c` (1) |
| `flags` | `0x69` | 1 | `src/core/physics/ai.c` (1) |
| `front` | `0x8` | 1 | `src/core/physics/ai.c` (1) |
| `g` | `0xC` | 1 | `src/core/physics/init.c` (1) |
| `g` | `0x14` | 1 | `src/core/physics/view.c` (1) |
| `g + 0x1c` | `0x4` | 1 | `src/core/physics/view.c` (1) |
| `g0` | `0x8` | 1 | `src/core/physics/init.c` (1) |
| `g0` | `0xC` | 1 | `src/core/physics/init.c` (1) |
| `g1` | `0x8` | 1 | `src/core/physics/init.c` (1) |
| `gar` | `0x4` | 1 | `src/core/physics/init.c` (1) |
| `gar` | `0x40` | 1 | `src/core/physics/race.c` (1) |
| `gar` | `0x44` | 1 | `src/core/physics/race.c` (1) |
| `gar` | `0x60` | 1 | `src/core/physics/car.c` (1) |
| `gar` | `0x68` | 1 | `src/core/physics/car.c` (1) |
| `gar` | `0x12F` | 1 | `src/core/physics/record.c` (1) |
| `gar` | `0x130` | 1 | `src/core/physics/record.c` (1) |
| `gar` | `0x14A` | 1 | `src/core/physics/record.c` (1) |
| `gar` | `0x14B` | 1 | `src/core/physics/record.c` (1) |
| `gar` | `0x14C` | 1 | `src/core/physics/record.c` (1) |
| `gar` | `0x14D` | 1 | `src/core/physics/record.c` (1) |
| `gar` | `0x14E` | 1 | `src/core/physics/record.c` (1) |
| `gar` | `0x14F` | 1 | `src/core/physics/record.c` (1) |
| `gar` | `0x158` | 1 | `src/core/physics/record.c` (1) |
| `gar` | `0x15A` | 1 | `src/core/physics/record.c` (1) |
| `gar` | `0x160` | 1 | `src/core/physics/garage.c` (1) |
| `gar` | `0x161` | 1 | `src/core/physics/garage.c` (1) |
| `gr` | `0x1C` | 1 | `src/core/physics/driver.c` (1) |
| `gr` | `0x20` | 1 | `src/core/physics/driver.c` (1) |
| `gr` | `0x24` | 1 | `src/core/physics/driver.c` (1) |
| `gt4_car_wheel(car, a)` | `0x20` | 1 | `src/core/physics/gearbox.c` (1) |
| `gt4_car_wheel(car, b)` | `0x20` | 1 | `src/core/physics/gearbox.c` (1) |
| `gt4_car_wheel(car, i)` | `0x47` | 1 | `src/core/physics/race.c` (1) |
| `gt4_car_wheel(car, i)` | `0x50` | 1 | `src/core/physics/ai.c` (1) |
| `gt4_car_wheel(car, i)` | `0x89` | 1 | `src/core/physics/suspension.c` (1) |
| `gt4_car_wheel(car, i) + WHEEL_MOUNT` | `0x4` | 1 | `src/core/physics/suspension.c` (1) |
| `gt4_car_wheel(car, side)` | `0x89` | 1 | `src/core/physics/clutch.c` (1) |
| `gt4_race_run_car(g->run, 0)` | `0x554` | 1 | `src/app/race_game.c` (1) |
| `gt4_race_run_car(g->run, 0)` | `0x5A8` | 1 | `src/app/race_game.c` (1) |
| `gt4_race_run_car(g->run, 0)` | `0x5B3` | 1 | `src/app/race_game.c` (1) |
| `gt4_race_run_car(g->run, 1)` | `0x554` | 1 | `src/app/race_game.c` (1) |
| `gt4_race_run_car(g->run, c)` | `0x5B3` | 1 | `src/app/race_game.c` (1) |
| `gt4_race_run_car(g->run, car)` | `0x5B2` | 1 | `src/app/race_game_bspec.inc` (1) |
| `gt4_race_run_car(g->run, i)` | `0x8` | 1 | `src/app/race_game.c` (1) |
| `gt4_race_run_car(r, 0)` | `0x5B6` | 1 | `src/core/physics/race_run.c` (1) |
| `gt4_tyre_params(params, i, GT4_U8(car + (uint32_t)i * 0xec + 0x1ed))` | `0xB8` | 1 | `src/core/physics/ai.c` (1) |
| `gt4_tyre_params(params, i, GT4_U8(gt4_car_wheel(car, i) + WHEEL_SURFACE))` | `0x168` | 1 | `src/core/physics/aids.c` (1) |
| `h` | `0x0` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x4` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x8` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0xC` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x4C` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x50` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x90` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x94` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0xD4` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0xD8` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x118` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x11C` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x15C` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x160` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x1A0` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x1A4` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x1E4` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x1E8` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x3EC` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x430` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x434` | 1 | `src/core/physics/ai.c` (1) |
| `h` | `0x438` | 1 | `src/core/physics/ai.c` (1) |
| `h` | `0x44C` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x450` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x454` | 1 | `src/core/physics/course_objects.c` (1) |
| `h` | `0x458` | 1 | `src/core/physics/course_objects.c` (1) |
| `h` | `0x45C` | 1 | `src/core/physics/course_objects.c` (1) |
| `h` | `0x460` | 1 | `src/core/physics/course_objects.c` (1) |
| `h` | `0x464` | 1 | `src/core/physics/course_objects.c` (1) |
| `h` | `0x468` | 1 | `src/core/physics/course_objects.c` (1) |
| `h` | `0x488` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x498` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x49C` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x4A0` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x4A4` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x4A8` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x4AC` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x4C4` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x4E0` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x4E4` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x4E8` | 1 | `src/core/physics/walls.c` (1) |
| `h` | `0x4EC` | 1 | `src/core/physics/walls.c` (1) |
| `hull[i]` | `0x450` | 1 | `src/core/physics/course_objects.c` (1) |
| `hull[i]` | `0x480` | 1 | `src/core/physics/course_objects.c` (1) |
| `l` | `0x2` | 1 | `src/core/physics/lines.c` (1) |
| `l` | `0x18` | 1 | `src/core/physics/race.c` (1) |
| `lay` | `0x14` | 1 | `src/core/physics/init.c` (1) |
| `lay` | `0x24` | 1 | `src/core/physics/view.c` (1) |
| `lay` | `0x2C` | 1 | `src/core/physics/driver.c` (1) |
| `layout` | `0x1` | 1 | `src/core/physics/ai.c` (1) |
| `layout` | `0x4` | 1 | `src/core/physics/ai.c` (1) |
| `layout` | `0x8` | 1 | `src/core/physics/steer.c` (1) |
| `layout` | `0xD` | 1 | `src/core/physics/ai.c` (1) |
| `layout` | `0x10` | 1 | `src/core/physics/ai.c` (1) |
| `layout` | `0x1C` | 1 | `src/core/physics/ai.c` (1) |
| `line_point(&l, (int)l.count - 3)` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `line_point(&l, 0)` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `line_point(&l, 1)` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `line_point(&l, 2)` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `line_point(&l, from)` | `0x10` | 1 | `src/core/physics/ai.c` (1) |
| `line_point(&l, k)` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `line_point(&l, s5)` | `0x1` | 1 | `src/core/physics/ai.c` (1) |
| `line_point(&l, s5)` | `0x2` | 1 | `src/core/physics/ai.c` (1) |
| `line_point(&l, sec - 1)` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `line_point(&l, sec)` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `line_point(l, 0)` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `line_point(l, s1)` | `0x14` | 1 | `src/core/physics/ai.c` (1) |
| `list + slot * 16` | `0xC` | 1 | `src/core/physics/init.c` (1) |
| `lp` | `0x8` | 1 | `src/core/physics/collision.c` (1) |
| `m` | `0x1` | 1 | `src/core/physics/misc.c` (1) |
| `m` | `0x6C` | 1 | `src/core/physics/init.c` (1) |
| `m` | `0x6D` | 1 | `src/core/physics/init.c` (1) |
| `m` | `0x70` | 1 | `src/core/physics/init.c` (1) |
| `m` | `0x74` | 1 | `src/core/physics/init.c` (1) |
| `o` | `0x440` | 1 | `src/core/physics/walls.c` (1) |
| `o + H_BOX` | `0x8` | 1 | `src/core/physics/walls.c` (1) |
| `o + H_BOX` | `0xC` | 1 | `src/core/physics/walls.c` (1) |
| `o + H_BOX_SIZE` | `0x4` | 1 | `src/core/physics/walls.c` (1) |
| `o + H_POS_B` | `0x4` | 1 | `src/core/physics/walls.c` (1) |
| `o + H_STEP` | `0x4` | 1 | `src/core/physics/walls.c` (1) |
| `obj` | `0x1B` | 1 | `src/core/physics/laps.c` (1) |
| `obj` | `0x1C` | 1 | `src/core/physics/driver.c` (1) |
| `obj` | `0x1D` | 1 | `src/core/physics/driver.c` (1) |
| `obj` | `0x48` | 1 | `src/core/physics/ai.c` (1) |
| `obj` | `0x54` | 1 | `src/core/physics/ai.c` (1) |
| `obj` | `0x6E` | 1 | `src/core/physics/driver.c` (1) |
| `obj` | `0x78` | 1 | `src/core/physics/ai.c` (1) |
| `obj` | `0xB8` | 1 | `src/core/physics/ai.c` (1) |
| `obj` | `0xBC` | 1 | `src/core/physics/ai.c` (1) |
| `obj` | `0xC0` | 1 | `src/core/physics/ai.c` (1) |
| `obj` | `0xD0` | 1 | `src/core/physics/driver.c` (1) |
| `obj` | `0x1C8` | 1 | `src/core/physics/driver.c` (1) |
| `options` | `0x970` | 1 | `src/app/race_game.c` (1) |
| `other` | `0x58` | 1 | `src/core/physics/ai.c` (1) |
| `out` | `0xA` | 1 | `src/core/physics/driver.c` (1) |
| `out` | `0xB` | 1 | `src/core/physics/driver.c` (1) |
| `own` | `0x20` | 1 | `src/core/physics/aids.c` (1) |
| `p` | `0x1` | 1 | `src/core/physics/session.c` (1) |
| `p` | `0x3` | 1 | `src/core/physics/session.c` (1) |
| `p` | `0x13` | 1 | `src/core/physics/session.c` (1) |
| `p` | `0x22` | 1 | `src/core/physics/session.c` (1) |
| `p` | `0x23` | 1 | `src/core/physics/session.c` (1) |
| `p` | `0x2F` | 1 | `src/core/physics/session.c` (1) |
| `p` | `0x38` | 1 | `src/core/physics/lines.c` (1) |
| `p` | `0x40` | 1 | `src/core/physics/init.c` (1) |
| `p` | `0x44` | 1 | `src/core/physics/init.c` (1) |
| `p` | `0x48` | 1 | `src/core/physics/walls.c` (1) |
| `p` | `0x10C` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x110` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x12C` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x130` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x134` | 1 | `src/core/physics/init.c` (1) |
| `p` | `0x140` | 1 | `src/core/physics/init.c` (1) |
| `p` | `0x1A0` | 1 | `src/core/physics/init.c` (1) |
| `p` | `0x1CC` | 1 | `src/core/physics/session.c` (1) |
| `p` | `0x1D0` | 1 | `src/core/physics/session.c` (1) |
| `p` | `0x202` | 1 | `src/core/physics/init.c` (1) |
| `p` | `0x21C` | 1 | `src/core/physics/init.c` (1) |
| `p` | `0x236` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x23C` | 1 | `src/core/physics/init.c` (1) |
| `p` | `0x244` | 1 | `src/core/physics/init.c` (1) |
| `p` | `0x247` | 1 | `src/core/physics/init.c` (1) |
| `p` | `0x262` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x2CC` | 1 | `src/core/physics/init.c` (1) |
| `p` | `0x310` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x314` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x344` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x368` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x36C` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0xFC8` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0xFE0` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0xFF4` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x1008` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x101C` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x1020` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x1024` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x102C` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x104A` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x104B` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x1050` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x1054` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x1058` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x105C` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x1064` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x1074` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x1075` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x1088` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x1090` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x10CC` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x10D0` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x10D4` | 1 | `src/core/physics/setup.c` (1) |
| `p` | `0x10D8` | 1 | `src/core/physics/setup.c` (1) |
| `p + 0x14` | `0x330` | 1 | `src/core/physics/init.c` (1) |
| `p + 0x23c` | `0x90` | 1 | `src/core/physics/init.c` (1) |
| `p + 8` | `0x310` | 1 | `src/core/physics/init.c` (1) |
| `p + PARAMS_AXLE_GRIP` | `0x4` | 1 | `src/core/physics/setup.c` (1) |
| `p + PARAMS_DRIVE` | `0x12` | 1 | `src/core/physics/init.c` (1) |
| `p + PARAMS_PITCH_ARM` | `0x4` | 1 | `src/core/physics/setup.c` (1) |
| `p + PARAMS_ROLL_ARM` | `0x4` | 1 | `src/core/physics/setup.c` (1) |
| `p + PARAMS_TYRES` | `0x3` | 1 | `src/core/physics/setup.c` (1) |
| `p + PARAMS_TYRE_INDEX` | `0x18` | 1 | `src/core/physics/setup.c` (1) |
| `p + P_LOCAL` | `0x4` | 1 | `src/core/physics/walls.c` (1) |
| `p + at` | `0x4` | 1 | `src/core/physics/walls.c` (1) |
| `pa` | `0xC` | 1 | `src/core/physics/ai.c` (1) |
| `pa` | `0x18` | 1 | `src/core/physics/ai.c` (1) |
| `pa` | `0x4B1` | 1 | `src/core/physics/ai.c` (1) |
| `params` | `0x0` | 1 | `src/app/race_game_cameras.inc` (1) |
| `params` | `0x10` | 1 | `src/core/physics/chassis.c` (1) |
| `params` | `0x14` | 1 | `src/core/physics/chassis.c` (1) |
| `params` | `0x33` | 1 | `src/core/physics/chassis.c` (1) |
| `params` | `0x48` | 1 | `src/core/physics/init.c` (1) |
| `params` | `0x4C` | 1 | `src/core/physics/init.c` (1) |
| `params` | `0x200` | 1 | `src/core/physics/ai.c` (1) |
| `params` | `0x234` | 1 | `src/core/physics/ai.c` (1) |
| `params` | `0x246` | 1 | `src/core/physics/ai.c` (1) |
| `params` | `0x247` | 1 | `src/core/physics/clutch.c` (1) |
| `params` | `0x260` | 1 | `src/app/race_game.c` (1) |
| `params` | `0x2A0` | 1 | `src/core/physics/gearbox.c` (1) |
| `params` | `0x2C4` | 1 | `src/core/physics/misc.c` (1) |
| `params` | `0x2C8` | 1 | `src/core/physics/chassis.c` (1) |
| `params` | `0x2D4` | 1 | `src/core/physics/ai.c` (1) |
| `params` | `0x31C` | 1 | `src/core/physics/driver.c` (1) |
| `params` | `0x418` | 1 | `src/core/physics/ai.c` (1) |
| `params` | `0xFC8` | 1 | `src/core/physics/slip.c` (1) |
| `params` | `0xFF4` | 1 | `src/core/physics/ai.c` (1) |
| `params` | `0x1008` | 1 | `src/core/physics/steer.c` (1) |
| `params` | `0x101C` | 1 | `src/core/physics/ai.c` (1) |
| `params` | `0x1024` | 1 | `src/core/physics/aids.c` (1) |
| `params` | `0x102C` | 1 | `src/core/physics/aids.c` (1) |
| `params` | `0x1034` | 1 | `src/core/physics/aids.c` (1) |
| `params` | `0x104A` | 1 | `src/core/physics/ai.c` (1) |
| `params` | `0x104B` | 1 | `src/core/physics/init.c` (1) |
| `params` | `0x1050` | 1 | `src/core/physics/steer.c` (1) |
| `params` | `0x1058` | 1 | `src/core/physics/steer.c` (1) |
| `params` | `0x106C` | 1 | `src/core/physics/init.c` (1) |
| `params` | `0x1070` | 1 | `src/core/physics/init.c` (1) |
| `params` | `0x1075` | 1 | `src/core/physics/race_run.c` (1) |
| `params` | `0x1076` | 1 | `src/core/physics/suspension.c` (1) |
| `params` | `0x1080` | 1 | `src/core/physics/misc.c` (1) |
| `params` | `0x1088` | 1 | `src/core/physics/wear.c` (1) |
| `params` | `0x1090` | 1 | `src/core/physics/wear.c` (1) |
| `params` | `0x10CC` | 1 | `src/core/physics/steer.c` (1) |
| `params` | `0x10D0` | 1 | `src/core/physics/steer.c` (1) |
| `params` | `0x10D3` | 1 | `src/core/physics/driver.c` (1) |
| `params` | `0x10D8` | 1 | `src/core/physics/init.c` (1) |
| `params + 0x234` | `0x12` | 1 | `src/core/physics/ai.c` (1) |
| `params + PARAMS_DOWNFORCE` | `0x4` | 1 | `src/core/physics/aero.c` (1) |
| `params + PARAMS_DRIVE` | `0x2` | 1 | `src/app/race_game.c` (1) |
| `params + PARAMS_DRIVE` | `0x3` | 1 | `src/core/physics/driver.c` (1) |
| `params + PARAMS_DRIVE` | `0x4` | 1 | `src/core/physics/clutch.c` (1) |
| `params + PARAMS_DRIVE` | `0x8` | 1 | `src/core/physics/clutch.c` (1) |
| `params + PARAMS_PITCH_ARM` | `0x4` | 1 | `src/core/physics/chassis.c` (1) |
| `params + PARAMS_ROLL_ARM` | `0x4` | 1 | `src/core/physics/chassis.c` (1) |
| `pb` | `0xC` | 1 | `src/core/physics/ai.c` (1) |
| `pb` | `0x18` | 1 | `src/core/physics/ai.c` (1) |
| `pb` | `0x4B1` | 1 | `src/core/physics/ai.c` (1) |
| `pcar` | `0x104` | 1 | `src/app/race_game_bspec.inc` (1) |
| `pcar + 0x104` | `0x4B1` | 1 | `src/app/race_game_bspec.inc` (1) |
| `pcar + 0x14` | `0x6D` | 1 | `src/app/race_game_bspec.inc` (1) |
| `pcar + 0x14` | `0x6E` | 1 | `src/app/race_game_bspec.inc` (1) |
| `player` | `0x5A8` | 1 | `src/app/race_game_pregrid.inc` (1) |
| `prev` | `0x10` | 1 | `src/core/physics/race.c` (1) |
| `prof` | `0x58` | 1 | `src/core/physics/ai.c` (1) |
| `prof` | `0x60` | 1 | `src/core/physics/ai.c` (1) |
| `profile(obj)` | `0x58` | 1 | `src/core/physics/ai.c` (1) |
| `profile(obj)` | `0xA8` | 1 | `src/core/physics/ai.c` (1) |
| `pt` | `0x10` | 1 | `src/core/physics/lines.c` (1) |
| `pt` | `0x18` | 1 | `src/core/physics/lines.c` (1) |
| `q` | `0x11` | 1 | `src/core/physics/race.c` (1) |
| `q` | `0x12` | 1 | `src/core/physics/race.c` (1) |
| `q` | `0x824` | 1 | `src/app/race_game.c` (1) |
| `q + P_A` | `0x4` | 1 | `src/core/physics/walls.c` (1) |
| `q + P_LOCAL` | `0x4` | 1 | `src/core/physics/walls.c` (1) |
| `r` | `0x1` | 1 | `src/core/physics/driver.c` (1) |
| `r` | `0x2` | 1 | `src/core/physics/driver.c` (1) |
| `r` | `0x4` | 1 | `src/core/physics/race.c` (1) |
| `r` | `0x6` | 1 | `src/core/physics/driver.c` (1) |
| `r` | `0xB` | 1 | `src/core/physics/driver.c` (1) |
| `r` | `0x10` | 1 | `src/core/physics/race.c` (1) |
| `r` | `0x11` | 1 | `src/core/physics/driver.c` (1) |
| `r` | `0x12` | 1 | `src/core/physics/driver.c` (1) |
| `r` | `0x13` | 1 | `src/core/physics/driver.c` (1) |
| `r` | `0x15` | 1 | `src/core/physics/driver.c` (1) |
| `r` | `0x16` | 1 | `src/core/physics/driver.c` (1) |
| `r` | `0x17` | 1 | `src/core/physics/driver.c` (1) |
| `r` | `0x18` | 1 | `src/core/physics/session.c` (1) |
| `r` | `0x1A` | 1 | `src/core/physics/driver.c` (1) |
| `r` | `0x1B` | 1 | `src/core/physics/driver.c` (1) |
| `r` | `0x1C` | 1 | `src/core/physics/session.c` (1) |
| `r` | `0x1D` | 1 | `src/core/physics/driver.c` (1) |
| `r` | `0x1E` | 1 | `src/core/physics/driver.c` (1) |
| `r` | `0x24` | 1 | `src/core/physics/race.c` (1) |
| `r` | `0x60` | 1 | `src/core/physics/session.c` (1) |
| `r` | `0x70` | 1 | `src/core/physics/license_session.c` (1) |
| `r` | `0x80` | 1 | `src/app/race_game.c` (1) |
| `r` | `0x8C` | 1 | `src/core/physics/license_session.c` (1) |
| `r` | `0x98` | 1 | `src/core/physics/license_session.c` (1) |
| `r` | `0xE0` | 1 | `src/core/physics/license_session.c` (1) |
| `r` | `0xEC` | 1 | `src/core/physics/session.c` (1) |
| `r` | `0xF0` | 1 | `src/core/physics/license_session.c` (1) |
| `race` | `0x3` | 1 | `src/core/physics/setup.c` (1) |
| `race` | `0x4` | 1 | `src/core/physics/session.c` (1) |
| `race` | `0x20` | 1 | `src/core/physics/ai.c` (1) |
| `race` | `0x78` | 1 | `src/core/physics/init.c` (1) |
| `race` | `0xCC` | 1 | `src/core/physics/race.c` (1) |
| `race` | `0xD4` | 1 | `src/core/physics/race.c` (1) |
| `race` | `0xD8` | 1 | `src/core/physics/race.c` (1) |
| `race` | `0xE0` | 1 | `src/core/physics/session.c` (1) |
| `race` | `0xE4` | 1 | `src/core/physics/session.c` (1) |
| `race` | `0xEC` | 1 | `src/core/physics/race.c` (1) |
| `race` | `0xF4` | 1 | `src/app/race_game.c` (1) |
| `race` | `0x920` | 1 | `src/core/physics/race.c` (1) |
| `race` | `0x924` | 1 | `src/core/physics/race.c` (1) |
| `race` | `0x96C` | 1 | `src/core/physics/race.c` (1) |
| `race` | `0xF824` | 1 | `src/core/physics/ai.c` (1) |
| `race` | `0xF828` | 1 | `src/core/physics/ai.c` (1) |
| `race` | `0xF82C` | 1 | `src/core/physics/ai.c` (1) |
| `race + 0x88` | `0x8` | 1 | `src/core/physics/session.c` (1) |
| `race + 0x88` | `0xC` | 1 | `src/core/physics/session.c` (1) |
| `race + 0x88` | `0x10` | 1 | `src/core/physics/session.c` (1) |
| `race + 0x88` | `0x14` | 1 | `src/core/physics/session.c` (1) |
| `race + 0x88` | `0x24` | 1 | `src/core/physics/session.c` (1) |
| `rear` | `0x8` | 1 | `src/core/physics/ai.c` (1) |
| `rec` | `0x4` | 1 | `src/core/physics/session.c` (1) |
| `rec` | `0x6` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x7` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x12` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x14` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x16` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x18` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1A` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1C` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1E` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x22` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x27` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x28` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x2A` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x2B` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x2C` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x2D` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x30` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x38` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x39` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x42` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x43` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x44` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x4D` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x4E` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x60` | 1 | `src/core/physics/session.c` (1) |
| `rec` | `0x66` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x88` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x8A` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x8B` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x8C` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x8D` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x8E` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x90` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x92` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x94` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x96` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x97` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x98` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0xA8` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0xBA` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0xBC` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0xD4` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0xD8` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x112` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x113` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x11C` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x11D` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x11E` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x120` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x144` | 1 | `src/core/physics/session.c` (1) |
| `rec` | `0x152` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x160` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x162` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x164` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x168` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x174` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x178` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x17A` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x180` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x181` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x182` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x190` | 1 | `src/core/physics/record.c` (1) |
| `rec` | `0x196` | 1 | `src/core/physics/record.c` (1) |
| `rec` | `0x19C` | 1 | `src/core/physics/record.c` (1) |
| `rec` | `0x1A2` | 1 | `src/core/physics/record.c` (1) |
| `rec` | `0x1A8` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1AA` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1BA` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1BB` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1BC` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1BD` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1BE` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1BF` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1C0` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1C1` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1C3` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1C8` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1CA` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1CC` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1CE` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1E0` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1E2` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1F0` | 1 | `src/core/physics/setup.c` (1) |
| `rec` | `0x1FF` | 1 | `src/core/physics/init.c` (1) |
| `rec` | `0x210` | 1 | `src/core/physics/setup.c` (1) |
| `rec + 0x283 + (k` | `0x3` | 1 | `src/core/physics/init.c` (1) |
| `rnw` | `0x38` | 1 | `src/core/physics/runway.c` (1) |
| `rnw` | `0x4A` | 1 | `src/core/physics/runway.c` (1) |
| `rnw` | `0x54` | 1 | `src/core/physics/runway.c` (1) |
| `rnw` | `0x58` | 1 | `src/core/physics/runway.c` (1) |
| `rnw` | `0x7C` | 1 | `src/core/physics/runway.c` (1) |
| `rnw` | `0x9C` | 1 | `src/core/physics/runway.c` (1) |
| `row` | `0x1` | 1 | `src/core/physics/session.c` (1) |
| `row` | `0x4` | 1 | `src/core/physics/session.c` (1) |
| `row` | `0x5` | 1 | `src/core/physics/session.c` (1) |
| `row` | `0x7` | 1 | `src/core/physics/session.c` (1) |
| `row` | `0xE` | 1 | `src/app/race_game_license.inc` (1) |
| `row` | `0x11` | 1 | `src/core/physics/ai.c` (1) |
| `row` | `0x13` | 1 | `src/core/physics/ai.c` (1) |
| `row` | `0x1B` | 1 | `src/core/physics/ai.c` (1) |
| `row` | `0x1C` | 1 | `src/core/physics/ai.c` (1) |
| `row` | `0x1D` | 1 | `src/core/physics/ai.c` (1) |
| `row` | `0x20` | 1 | `src/core/physics/ai.c` (1) |
| `row` | `0x2C` | 1 | `src/core/physics/driver.c` (1) |
| `row` | `0x30` | 1 | `src/core/physics/driver.c` (1) |
| `row` | `0x34` | 1 | `src/core/physics/driver.c` (1) |
| `row` | `0x38` | 1 | `src/core/physics/driver.c` (1) |
| `row` | `0x3C` | 1 | `src/core/physics/driver.c` (1) |
| `row` | `0x50` | 1 | `src/core/physics/driver.c` (1) |
| `row` | `0x54` | 1 | `src/core/physics/driver.c` (1) |
| `row` | `0x58` | 1 | `src/core/physics/driver.c` (1) |
| `row` | `0x5C` | 1 | `src/core/physics/driver.c` (1) |
| `row` | `0x60` | 1 | `src/core/physics/driver.c` (1) |
| `row` | `0x79` | 1 | `src/core/physics/driver.c` (1) |
| `row` | `0x7A` | 1 | `src/core/physics/driver.c` (1) |
| `row` | `0x7B` | 1 | `src/core/physics/driver.c` (1) |
| `row` | `0x7C` | 1 | `src/core/physics/driver.c` (1) |
| `row` | `0x80` | 1 | `src/core/physics/driver.c` (1) |
| `row` | `0xA4` | 1 | `src/core/physics/driver.c` (1) |
| `runway` | `0x14` | 1 | `src/app/race_game.c` (1) |
| `runway_of(course)` | `0x18` | 1 | `src/core/physics/session.c` (1) |
| `runway_of(course)` | `0x9C` | 1 | `src/core/physics/session.c` (1) |
| `runway_of(course_of(cond))` | `0x14` | 1 | `src/core/physics/session.c` (1) |
| `rw` | `0x3C` | 1 | `src/core/physics/lines.c` (1) |
| `rw` | `0x52` | 1 | `src/core/physics/lines.c` (1) |
| `rw` | `0x54` | 1 | `src/app/race_game_license.inc` (1) |
| `rw` | `0x60` | 1 | `src/core/physics/lines.c` (1) |
| `rw` | `0x64` | 1 | `src/core/physics/lines.c` (1) |
| `rw` | `0x7C` | 1 | `src/core/physics/lines.c` (1) |
| `rw` | `0x94` | 1 | `src/core/physics/init.c` (1) |
| `rw` | `0x98` | 1 | `src/core/physics/init.c` (1) |
| `s` | `0x5` | 1 | `src/core/physics/session.c` (1) |
| `s` | `0x7` | 1 | `src/core/physics/session.c` (1) |
| `s` | `0xD` | 1 | `src/core/physics/aids.c` (1) |
| `s` | `0xE` | 1 | `src/core/physics/aids.c` (1) |
| `s` | `0xF` | 1 | `src/core/physics/aids.c` (1) |
| `s` | `0x31` | 1 | `src/core/physics/aids.c` (1) |
| `s` | `0x34` | 1 | `src/core/physics/wear.c` (1) |
| `s` | `0x38` | 1 | `src/core/physics/wear.c` (1) |
| `s` | `0x3C` | 1 | `src/core/physics/wear.c` (1) |
| `s` | `0x41` | 1 | `src/core/physics/race.c` (1) |
| `s` | `0x44` | 1 | `src/core/physics/wear.c` (1) |
| `s` | `0x4C` | 1 | `src/core/physics/wear.c` (1) |
| `s` | `0x54` | 1 | `src/core/physics/tyre.c` (1) |
| `s` | `0x58` | 1 | `src/core/physics/tyre.c` (1) |
| `s` | `0x64` | 1 | `src/core/physics/tyre.c` (1) |
| `s` | `0xD0` | 1 | `src/core/physics/ground.c` (1) |
| `s` | `0xD1` | 1 | `src/core/physics/ground.c` (1) |
| `s` | `0xD2` | 1 | `src/core/physics/ground.c` (1) |
| `s` | `0xD3` | 1 | `src/core/physics/ground.c` (1) |
| `s` | `0xD4` | 1 | `src/core/physics/ground.c` (1) |
| `s` | `0xD5` | 1 | `src/core/physics/ground.c` (1) |
| `s` | `0xD6` | 1 | `src/core/physics/ground.c` (1) |
| `s` | `0xD7` | 1 | `src/core/physics/ground.c` (1) |
| `s` | `0xD8` | 1 | `src/core/physics/ground.c` (1) |
| `s` | `0x228` | 1 | `src/core/physics/walls.c` (1) |
| `s` | `0x22C` | 1 | `src/core/physics/walls.c` (1) |
| `s` | `0x230` | 1 | `src/core/physics/walls.c` (1) |
| `s` | `0x234` | 1 | `src/core/physics/walls.c` (1) |
| `s` | `0x238` | 1 | `src/core/physics/walls.c` (1) |
| `s` | `0x23C` | 1 | `src/core/physics/walls.c` (1) |
| `s` | `0x240` | 1 | `src/core/physics/walls.c` (1) |
| `s` | `0x244` | 1 | `src/core/physics/walls.c` (1) |
| `s + 0xc + (uint32_t)b * HULL_SIZE` | `0x388` | 1 | `src/core/physics/walls.c` (1) |
| `s6` | `0x28` | 1 | `src/core/physics/init.c` (1) |
| `s6` | `0xE4` | 1 | `src/core/physics/init.c` (1) |
| `s6` | `0xE8` | 1 | `src/core/physics/init.c` (1) |
| `sa` | `0x4AE` | 1 | `src/app/race_game_bspec.inc` (1) |
| `scratch` | `0x25` | 1 | `src/core/physics/garage.c` (1) |
| `self` | `0x88` | 1 | `src/core/physics/race.c` (1) |
| `self` | `0x94` | 1 | `src/core/physics/race.c` (1) |
| `session` | `0xD8` | 1 | `src/app/race_game.c` (1) |
| `session` | `0xE0` | 1 | `src/app/race_game.c` (1) |
| `session` | `0x6698` | 1 | `src/core/physics/ai.c` (1) |
| `session` | `0xCBD8` | 1 | `src/core/physics/aids.c` (1) |
| `session` | `0xCBDD` | 1 | `src/core/physics/ai.c` (1) |
| `session` | `0xCBE1` | 1 | `src/core/physics/ai.c` (1) |
| `session` | `0xCBFC` | 1 | `src/core/physics/ai.c` (1) |
| `session` | `0xF8A0` | 1 | `src/core/physics/ai.c` (1) |
| `session + 0x6698u + (uint32_t)rival * CAR_SIZE` | `0x640` | 1 | `src/core/physics/ai.c` (1) |
| `session + GT4_U8(b + CAR_BLOCK_PLAYER) * 2u` | `0xF878` | 1 | `src/core/physics/ai.c` (1) |
| `session + GT4_U8(car + CAR_PLAYER)` | `0xF866` | 1 | `src/core/physics/aids.c` (1) |
| `setup` | `0x48` | 1 | `src/core/physics/race_run.c` (1) |
| `setup` | `0x4C` | 1 | `src/core/physics/race_run.c` (1) |
| `setup` | `0x110` | 1 | `src/core/physics/race_run.c` (1) |
| `setup` | `0x160` | 1 | `src/core/physics/race_run.c` (1) |
| `setup` | `0x161` | 1 | `src/core/physics/race_run.c` (1) |
| `setup` | `0x162` | 1 | `src/core/physics/race_run.c` (1) |
| `setup` | `0x163` | 1 | `src/core/physics/race_run.c` (1) |
| `setup` | `0x165` | 1 | `src/core/physics/race_run.c` (1) |
| `setup` | `0x166` | 1 | `src/core/physics/race_run.c` (1) |
| `special_table(p)` | `0x30` | 1 | `src/core/physics/setup.c` (1) |
| `special_table(p)` | `0x74` | 1 | `src/core/physics/setup.c` (1) |
| `spring` | `0x4` | 1 | `src/core/physics/view.c` (1) |
| `spring` | `0x8` | 1 | `src/core/physics/view.c` (1) |
| `spring` | `0xC` | 1 | `src/core/physics/view.c` (1) |
| `src` | `0x0` | 1 | `src/core/physics/setup.c` (1) |
| `src` | `0x5` | 1 | `src/core/physics/setup.c` (1) |
| `src` | `0x6` | 1 | `src/core/physics/session.c` (1) |
| `src` | `0x9` | 1 | `src/core/physics/setup.c` (1) |
| `src` | `0xA` | 1 | `src/core/physics/setup.c` (1) |
| `src` | `0xB` | 1 | `src/core/physics/setup.c` (1) |
| `src` | `0xD` | 1 | `src/core/physics/setup.c` (1) |
| `src` | `0x14` | 1 | `src/core/physics/session.c` (1) |
| `src` | `0x21` | 1 | `src/core/physics/session.c` (1) |
| `src` | `0x22` | 1 | `src/core/physics/session.c` (1) |
| `src` | `0x24` | 1 | `src/core/physics/ai.c` (1) |
| `src` | `0x29` | 1 | `src/core/physics/ai.c` (1) |
| `src` | `0x2A` | 1 | `src/core/physics/ai.c` (1) |
| `src` | `0x34` | 1 | `src/core/physics/ai.c` (1) |
| `src` | `0x47` | 1 | `src/core/physics/ai.c` (1) |
| `src` | `0x60` | 1 | `src/core/physics/setup.c` (1) |
| `src` | `0x64` | 1 | `src/core/physics/setup.c` (1) |
| `t` | `0x1` | 1 | `src/core/physics/setup.c` (1) |
| `t` | `0x2` | 1 | `src/core/physics/setup.c` (1) |
| `t` | `0x3` | 1 | `src/core/physics/setup.c` (1) |
| `t` | `0x5` | 1 | `src/core/physics/setup.c` (1) |
| `t` | `0x6` | 1 | `src/core/physics/slip.c` (1) |
| `t` | `0xA` | 1 | `src/core/physics/slip.c` (1) |
| `t` | `0x24` | 1 | `src/core/physics/session.c` (1) |
| `t` | `0x28` | 1 | `src/core/physics/session.c` (1) |
| `t` | `0x34` | 1 | `src/core/physics/session.c` (1) |
| `t` | `0x3C` | 1 | `src/core/physics/session.c` (1) |
| `t` | `0x54` | 1 | `src/core/physics/session.c` (1) |
| `t` | `0x60` | 1 | `src/core/physics/session.c` (1) |
| `t` | `0x70` | 1 | `src/core/physics/setup.c` (1) |
| `t` | `0x7C` | 1 | `src/core/physics/session.c` (1) |
| `t` | `0x80` | 1 | `src/core/physics/session.c` (1) |
| `t` | `0x98` | 1 | `src/core/physics/setup.c` (1) |
| `t` | `0xB8` | 1 | `src/core/physics/setup.c` (1) |
| `t` | `0xC8` | 1 | `src/core/physics/setup.c` (1) |
| `t` | `0xE0` | 1 | `src/core/physics/setup.c` (1) |
| `t` | `0x1B8` | 1 | `src/core/physics/setup.c` (1) |
| `t` | `0x1C8` | 1 | `src/core/physics/setup.c` (1) |
| `t` | `0x1D8` | 1 | `src/core/physics/setup.c` (1) |
| `t` | `0x1DA` | 1 | `src/core/physics/setup.c` (1) |
| `t + 0x30` | `0x4` | 1 | `src/core/physics/session.c` (1) |
| `t + 0x54` | `0x4` | 1 | `src/core/physics/session.c` (1) |
| `t + j * 8` | `0x4` | 1 | `src/core/physics/session.c` (1) |
| `table` | `0xA8` | 1 | `src/core/physics/body.c` (1) |
| `tp` | `0x4` | 1 | `src/core/physics/wear.c` (1) |
| `tp` | `0x8` | 1 | `src/core/physics/ai.c` (1) |
| `tp` | `0x16C` | 1 | `src/core/physics/ai.c` (1) |
| `tp` | `0x1AC` | 1 | `src/core/physics/ai.c` (1) |
| `tp` | `0x1D8` | 1 | `src/core/physics/aids.c` (1) |
| `tri` | `0x6` | 1 | `src/core/physics/collision.c` (1) |
| `tri` | `0x7` | 1 | `src/core/physics/collision.c` (1) |
| `tyre` | `0x6C` | 1 | `src/core/physics/clutch.c` (1) |
| `tyre` | `0x1D8` | 1 | `src/core/physics/slip.c` (1) |
| `v` | `0x19` | 1 | `src/core/physics/session.c` (1) |
| `v` | `0x1A` | 1 | `src/core/physics/session.c` (1) |
| `v` | `0x23` | 1 | `src/core/physics/init.c` (1) |
| `v` | `0x24` | 1 | `src/core/physics/init.c` (1) |
| `v` | `0x25` | 1 | `src/core/physics/init.c` (1) |
| `v` | `0x2C` | 1 | `src/core/physics/init.c` (1) |
| `vel` | `0x8` | 1 | `src/core/physics/view.c` (1) |
| `w` | `0xE` | 1 | `src/core/physics/walls.c` (1) |
| `w` | `0x24` | 1 | `src/core/physics/init.c` (1) |
| `w` | `0x40` | 1 | `src/core/physics/init.c` (1) |
| `w` | `0x46` | 1 | `src/core/physics/init.c` (1) |
| `w` | `0x54` | 1 | `src/core/physics/driver.c` (1) |
| `w` | `0x79` | 1 | `src/core/physics/driver.c` (1) |
| `w` | `0x7A` | 1 | `src/core/physics/driver.c` (1) |
| `w` | `0x7B` | 1 | `src/core/physics/driver.c` (1) |
| `w` | `0x7C` | 1 | `src/core/physics/driver.c` (1) |
| `w` | `0xA6` | 1 | `src/core/physics/driver.c` (1) |
| `w` | `0xA7` | 1 | `src/core/physics/driver.c` (1) |
| `w` | `0xB0` | 1 | `src/core/physics/init.c` (1) |
| `w` | `0xB2` | 1 | `src/core/physics/init.c` (1) |
| `w` | `0xB3` | 1 | `src/core/physics/init.c` (1) |
| `w` | `0xB8` | 1 | `src/core/physics/init.c` (1) |
| `w` | `0xCC` | 1 | `src/core/physics/init.c` (1) |
| `w` | `0xD0` | 1 | `src/core/physics/init.c` (1) |
| `w` | `0xD4` | 1 | `src/core/physics/init.c` (1) |
| `wa` | `0xC` | 1 | `src/core/physics/view.c` (1) |
| `wa` | `0x10` | 1 | `src/core/physics/view.c` (1) |
| `wa` | `0x8C` | 1 | `src/core/physics/view.c` (1) |
| `wall` | `0xE` | 1 | `src/core/physics/course_objects.c` (1) |
| `wc` | `0xC` | 1 | `src/core/physics/view.c` (1) |
| `wc` | `0x10` | 1 | `src/core/physics/view.c` (1) |
| `wc` | `0x8C` | 1 | `src/core/physics/view.c` (1) |
| `wh` | `0x80` | 1 | `src/core/physics/clutch.c` (1) |
| `world` | `0xCC0` | 1 | `src/core/physics/driver.c` (1) |
| `world` | `0xD54` | 1 | `src/core/physics/session.c` (1) |
| `world` | `0xD68` | 1 | `src/core/physics/race.c` (1) |
| `world` | `0x10000` | 1 | `src/core/physics/session.c` (1) |
| `xs + (uint32_t)(i` | `0x1` | 1 | `src/core/physics/aids.c` (1) |
