# Shared physics arithmetic and original recordings

## Cause and scope

Original recordings store commands, not a pose for every frame. Tiny numerical
differences accumulate when those commands are simulated again. Host float with
toward-zero rounding does not reproduce all EE ADD.S, SUB.S, MUL.S, DIV.S or
SQRT.S results. In particular, the multiplier's low bits depend on operand order.
The former original-code comparison also used host float, so agreement with that
oracle alone could miss this cause.

The existing hardware-derived integer model is documented in
[third_party/ps2float](../../third_party/ps2float/README.md). Its upstream source is
[libretro/ps2, revision cac1c32](https://github.com/libretro/ps2/tree/cac1c32c7d4c2ad00edd6f4ba756340cefb39dd3).
PCSX2 also documents limitations of host arithmetic in its
[floating-point research](https://wiki.pcsx2.net/PCSX2_Documentation/Nightmare_on_Floating-Point_Street).

This is a fidelity correction shared by all live modes: Arcade, GT Mode,
licences, missions, rival driving, Time Trial and new native recordings. It is
not a player-selectable physics improvement. Vehicle parameters, command bytes,
simulation cadence and formulas were not tuned to make the tapes drive correctly.

## Native implementation

`tools/ps2_replay_float.py` parses our C using `pycparser`. At build time it
replaces typed scalar float arithmetic with calls through `replay_float.h`:
addition, subtraction, multiplication, division, their compound assignments and
`sqrtf`. Integer operations remain C. Operand order is retained; a compound
assignment evaluates its lvalue only once. Host rounding/flush mode remains
necessary for conversions. This does not claim to model every FCR31 flag,
overflow-sensitive conversion, library operation or possible EE instruction.

`cmake/ReplayMath.cmake` builds 38 native source files, covering 259 externally
visible functions, into `moon_physics`. Generated project C stays under
`build/physics_math`. No original executable, original-code output, instruction
interpreter or disc-derived C is linked into the game. The transformation has
synthetic type/scope/order tests, including an intentionally swapped operand.

The original curve functions must remain distinct:

- `00359568` bisects, returns exact midpoint hits directly and multiplies the
  y difference before the x difference.
- `00359830 +0xA0..0xC4` scans and multiplies the x difference first.
- `003598F8 +0xBC..0x100` shares x coordinates for both outputs, x difference first.

Previously these paths shared one evaluator. Call sites in steering, clutch,
rolling resistance, tyre setup, grip and forces now use the corresponding
function. `00353800` also completes slip folding before reading its output sign;
the former C expression could read that output before the call wrote it.

## Existing native recordings

New native files are **GT4R v2**, with the same payload layout and corrected
arithmetic. Loading v1 marks it as historical host arithmetic; re-saving retains
v1. Future versions are rejected. Original GT4M imports always use EE arithmetic.

The build also produces prefixed historical functions from the same C source,
with host operators. Only full native v1 playback selects this path; leaving
playback returns to corrected live physics. This is a file compatibility rule,
not a physics option. No save/profile format or player data is rewritten.
Native v1 best-lap ghosts mixed into a corrected live Time Trial are not
certified by this work; new v2 saved ghosts have separate round-trip coverage: 5,614 steps at 0 m measured
position error, with a 0.01 m gate; the shifted-command negative reaches 16.263 m.

`physics_math_modes` compares all 32 MiB after 600 commands: public v1 dispatch
must match direct historical execution and live execution must differ. Its
negative changes a reference byte. Serialization tests check v1/v2 and rejection
of an unknown version. The formulas remain shared rather than copied into a
separate maintained physics implementation.

## Original-code comparisons

The physics oracle links `tests/physics/replay_arithmetic.c`, which provides EE
instruction values through the same integer model. Original instructions retain
their own control flow, operand ordering and memory accesses. Numeric-model bugs
could affect both sides: these comparisons establish native/original sequencing
and state agreement under that model, not independent silicon certification.
Exception/status bookkeeping remains a host approximation and is not the output
compared by the trajectory test.

`disc_replay_arithmetic` compares all RAM outside the oracle stack
`0x01e18000..0x01ef0000` after every step on all 13 title recordings. Its registered
run checks 1,200 consecutive steps per recording. `--full` checks every command.
The extended runs cover 161,862 consecutive steps in total: 133,496 on the
12 grid-start recordings and 28,366 on the checkpoint-based Nürburgring recording.
Original and native start from a shared native-built context; the test does not
establish that constructor's equality with a freshly started PS2 session.

Rdemo9 starts from its recorded checkpoint. The independent checkpoint oracle
compares original `0035ABA8` with native restoration using EE arithmetic; the
step comparator begins from the shared restored state. `003DDA08` imports exactly
the GT4M entry count; replay construction no longer adds a Time Trial ghost absent
from the recording. `disc_replay_lap_event` separately compares original
`003F5B80 -> 0035E550` and native queue/state updates at a forced completed lap,
including a changed-queue-byte negative. The former headless Time Trial handler
omitted those lap/time events. The frontend additionally
plays all 13 complete command streams, checks consumption, finite poses,
checkpoint boundaries and cancellation. Dropping a command fails its negative.

The old Bentley/Paris wall-sticking investigation is retained as history in
[replay setup](DISC_REPLAY_SETUP.md). No time shift, artificial steering,
teleportation or replacement driver was introduced.

## Licence gold reachability

`gtmode_license_gold_catalog` drives the current native player examinations
with all 80 original teacher command streams and their original driving options.
Every attempt must finish in successful state 2 with time at or below the gold
threshold read from its RACE row. Its negative substitutes an impossible target.
It does not write medals into a player's save.

All 80 satisfy that gold gate. In the separate recorded-time audit, 64 also
match the nonzero recorded finish time exactly. Fifteen GT4M headers have zero
for that field, preventing that timing comparison. B-6 finishes in 22,715 ms
against recorded 22,749 ms, while still below its 22,800 ms gold threshold.
This unresolved 34 ms difference prevents a blanket exact licence-trajectory
claim. The strict single-exam timing check was retained; the catalogue grade
gate is a separate test, not a weakened replacement.

## Surface-reference distinction

The older PCSX2 captures agree with the historical host multiplier, including
all 408 stored wave-number words across 17 captures. After the shared arithmetic
change, a surface word can differ from those emulator outputs: for example
surface 11 of ar_02 is `417B53D1` historically versus `417B53D3` with EE arithmetic.

`physics_surface_waves` retains the old captured-word check for the historical
backend and independently executes original `0035F808 +0x19C..0x1B8` on captured
inputs with EE arithmetic. The corrected backend must match all 408 resulting
words exactly. Both checks and the changed-bit negative pass. The emulator
capture alone is no longer cited as independent proof of EE multiplication.

## Validation boundaries

The initial Windows Release build and 62 focused positive/negative tests pass; no
whole-suite run was performed. Coverage includes native playback,
school return isolation, B-Spec, command/setup/checkpoint import, PS2 media
containers, split replay, transmission and the shared arithmetic boundary.
The cadence test retains identical audio registers/PCM at one, two and four
physics ticks per presentation frame; fixed 1/60-second stepping predates this
arithmetic correction. This does not certify every long-stall/input-sampling case.
The generated PS2 and historical sources plus host conversion environment also
compile with devkitA64 for Cortex-A57: 77 ARM64 objects, zero compilation errors.
This does not measure Switch FPS, execute on ARM hardware or install an NRO.

The disc fixture is NTSC-U SCUS-97328 v2.00. There is no independently recorded
PS2/PCSX2 memory card supplied for this task. Real imported save recordings,
other regional revisions, complete console initial state, native v1 ghosts and
the B-6 timing difference remain validation work. No whole-game E grade follows
from the bounded step and gold-gate tests. Game data, memory and generated
original-code references remain local.

The follow-up [EE arithmetic audit](EE_PHYSICS_AUDIT.md) records historical
regressions, the remaining oracle links, complete frontend command consumers,
additional native boundary fixes and before/after step costs.
