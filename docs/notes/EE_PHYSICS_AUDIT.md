# EE arithmetic regression and resimulation audit

## Historical comparison

Windows Release copies of `27e2397^` (`b3dbf72`) and `27e2397` were built
independently. Only the three requested tests were run in each copy; local
working changes were not included. The original checks were retained.

| Test | Parent | 27e2397 | Cause and resolution |
|---|---|---|---|
| `bspec_driver` | Pass | Fail | The expectation came from an emulator using host-nearest division. Original `003F1790 +0xB8..0xE4` with EE arithmetic produces level-4 ratios `3F8CCCCC`, not `3F8CCCCD`. `physics_bspec_choices` compares the entire initialized state and its fifteen ratios with original instructions; a changed-byte negative fails. The test expectation now follows that result. |
| `gtmode_mission_cycle` | Pass | Fail | The external pad fixture no longer wins mission 9. After correcting the session differences below, the old commands finish second in both original/native EE execution for all 1,690 steps. The fixture gain is now 0.8. Victory, result board, original prize, saved record/credits and the separate losing mission remain mandatory. |
| `gtmode_mission_cycle_22` | Pass | Fail | The old external drafting driver finishes second in both implementations for all 11,443 steps. Its pad policy now uses gain 1.2 and passing gap 12. No vehicle, rival, wake, prize or success rule was adjusted. |

The refreshed winning pad fixtures are also compared with original instructions:
mission 9 matches for 1,282 steps and mission 22 for 11,400 steps, with rank 1
required. The negative changes a player byte. These comparisons cover all RAM
outside the oracle stack `01E18000..01EF0000`, starting from a shared native
constructor. They do not establish a fresh console constructor or silicon parity.
Fixture gains are controller inputs for tests, not game tuning parameters.

## Actual session defects found by the comparisons

Both defects existed before `27e2397`; changing arithmetic did not introduce
the incorrect dispatch or eligibility condition.

- `003F6760 +0x48..0x64` clears the timed-start state and calls virtual method
  `+0xA0`. The native licence handler called its base implementation directly,
  bypassing RaceMission's `003F6E50` override and retaining the wrong start
  reference. Native dispatch now follows the virtual call.
- `00358000 +0x78..0xB8` checks virtual `+0x30` before setting a new timing
  record. The native helper omitted that check and could mark a mission record
  incorrectly. The original condition is restored.

Historical native GT4R v1 execution retains the former behavior solely for
recording compatibility. Live play, disc recordings and new native recordings
always use corrected EE arithmetic.

## Oracle arithmetic inventory

The following 27 adapters previously reached `src/ee/fpu.c` host results rather
than the EE results used by the native solver. They now receive the exact-value
object directly, so static-library search order cannot silently select host math:

```
disc_setup_oracle
gtmode_logger_oracle gtmode_logger_drive gtmode_aspiration_oracle
gtmode_training_oracle gtmode_mission_session_oracle
gtmode_license_session_oracle gtmode_license_lines_oracle
gtmode_active_setup_oracle gtmode_enemy_indices_oracle
gtmode_paint_oracle gtmode_grid_paint_oracle gtmode_event_tyre_oracle
gtmode_start_oracle gtmode_start_select_oracle gtmode_event_cycle_oracle
gtmode_endurance_queries gtmode_event_wear_oracle gtmode_event_distance_oracle
gtmode_purchase_oracle gtmode_tuned_oracle gtmode_tune_power_probe
gtmode_setting_curve gtmode_setting_auto gtmode_setting_units
split_battle_oracle course_objects_oracle
```

`moon_recomp_physics`, `disc_checkpoint_oracle`, `disc_arithmetic_oracle` and
`disc_lap_oracle` already selected the EE result model. Executables inheriting
`moon_recomp_physics` now also receive the object directly, including
`physics_run`, `setup_run`, `moon_drive`, `surface_bumps_reference` and
`physics_controller_event`. Later targets inheriting that library receive the
same linkage automatically.

Presentation references (pit poses, image LUTs, Setting panel animation, camera
and audio tests) and the historical host-FPU unit tests are separate from vehicle
simulation. They were not silently converted or claimed as EE physics proofs.
Exception/status bookkeeping in the EE test adapter remains a host approximation;
the compared finite values/RAM use the integer model. Sharing a numeric model
can hide a model error; it is not independent hardware certification.

The corrected references exposed further native boundary differences:

- Logger sampling/interpolation now lowers its own C scalar operations through
  the same build-time transformation as the solver. Its original sampler and
  lookup comparisons remain byte/float comparisons, including negatives.
- Career engine/gear summaries retain EE operations and operand order when
  building shift points, normalized charts and power/torque values. Both the
  standard-car and upgraded-car original chart checks retain their gates.
- `0035DDB0 +0x14 -> 0010FB44` subtracts B-Spec mileage before unsigned distance
  conversion. The application and its career-cycle tests now use that EE
  subtraction rather than subtracting with the PC FPU before the checked helper.

## End-to-end command consumers

| Consumer | Coverage | Limit |
|---|---|---|
| Idle opening demo | The real Arcade TopRoot runs its scripts/animations until it requests a disc demo; the frontend plays all 9,996 commands of the first tape and cancels back through the attract controller. | Complete boot/movie display and every presentation transition are not a PS2 image comparison. |
| Replay Theater | All 13 original recordings play to their command-stream ends, including checkpoints and cancellation. | Finite poses/consumption do not independently prove their console starting state. The separate original-code step comparisons are narrower proofs. |
| Licence demonstrations | All 80 frontend demonstrations consume their original commands, finish without awarding a result and return to a fresh exam. The 12 RaceTraining cases also require finite moving entry-1 guide cars. | This tests demonstration Scene behavior, not player gold medals or exact safety-car rasterization. |
| Imported PS2 recordings | Synthetic PSU envelopes contain original Rdemo0 and checkpoint-based Rdemo9. Import, persistence, frontend reload and full playback reproduce the source disc recordings' final poses bit for bit. | These are not independently recorded PS2/PCSX2 memory cards. |
| Logger | Original sampling is checked separately; frontend B-1 commands produce 165 packed points identically after repeated analysis, native file save/load and Load Track B, preserving track A. | No independent PS2 Logger-screen capture or PS2 best-lap file is supplied. |

Each new frontend case has a deliberate dropped-command or changed-point
negative. PS2 best-lap ghost conversion remains unverified/unimplemented as a
separate import path; a full GT4M replay must not be counted as proof of a
FileReplayBestLap import. Native v1 ghosts mixed into current live physics also
remain outside this certification. The previously recorded B-6 34 ms timing
difference remains open; see [the initial arithmetic record](DISC_REPLAY_ARITHMETIC.md).

## Cost and exact fast paths

The same standalone `tools/physics_step_cost.c` was built against the parent,
`27e2397` and the optimized implementation. Windows x64 Release, MSVC 2022,
Ryzen 7 5700X (8 cores/16 logical processors); median of five runs of 3,000
steps, after 180 warm-up steps. The workload uses B-Spec with one or six
identical Rdemo0 donor cars. Loading is outside the measured interval;
rendering and audio are absent. Frequency was not locked. Different arithmetic
can change a trajectory's branch workload, so this is a race-step benchmark,
not an isolated per-instruction latency or a whole-game FPS measurement.

| Course/cars | Parent host math, us/step | 27e2397 EE, us/step | Optimized EE, us/step | EE cost reduction |
|---|---:|---:|---:|---:|
| Tsukuba / 1 | 7.209 | 144.319 | 132.033 | 8.5% |
| Tsukuba / 6 | 32.819 | 779.159 | 715.465 | 8.2% |
| Nurburgring / 6 | 36.868 | 867.902 | 806.548 | 7.1% |

EE arithmetic remains mandatory. Two integer-only shortcuts preserve the existing
model: a multiplier whose recoded low sixteen bits are zero has no Booth bit-15
correction; a power-of-two divisor or equal normalized significands has an exact
quotient, so exponent scaling replaces the SRT recurrence. Zero/denormal and
exception handling remain intact. No ordinary hardware-float replacement was used.

`physics_float_fast` compares 14,583,683 full 64-bit result-and-flag words against
the unchanged full recurrences, including all fast multiplier groups, normal
exponent pairs, randomized words and a changed-result negative. The reference
is compiled from the same dependency with only the new shortcuts disabled;
there is no second hand-maintained arithmetic implementation.

Example measurement, without a timing pass/fail threshold:

```
build/Release/physics_step_cost.exe <own-disc.iso> tsukuba 6 3000 5
```

The 77 generated physics/conversion objects plus seven boundary/reference/tool
objects compile for devkitA64 Cortex-A57 (84 objects, no compilation errors).
This does not execute on a Switch or measure ARM cost. No Switch hardware
measurement or NRO installation was performed in this audit.

## Vulkan negative probe

Windows records confirm an earlier `ui_blend_probe` heap-corruption exception
`0xC0000374` in `ntdll`. It could not be reproduced in 20 serial runs of the
unchanged negative. The probe returned directly from a failed assertion without
shutting video down; it now drains/shuts down video on both exit paths, preserving
the deliberate failing assertion. Forty subsequent runs, four at a time,
returned exactly the intended assertion failure instead of a crash.

The probe performs no vehicle simulation steps; this is investigated separately from
the arithmetic change. The missing teardown is corrected, but the available
records do not establish it as the heap corruption's root cause. No renderer
or driver fix, or general Vulkan stability guarantee, is claimed.

Final focused CTest groups pass: 48/48 original-code adapters, 33/33 regression
checks, 8/8 frontend resimulation cases and 4/4 complete mission step comparisons
(93 distinct test names). The 40-run Vulkan stress test is separate. Windows
Release and the 84 ARM64 objects compile. The original/new historical three-test
runs are retained separately from these final results.

Only focused builds/tests were run. No complete-suite run or push was performed.
