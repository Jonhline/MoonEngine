# Vehicle physics and original-code comparisons

Implementation: `src/core/physics/ and tests/physics/`.

The simulation step is func_0034DB30. Native functions preserve original formulas, constants and update ordering. phys.h retains a transitional memory layout so a C replacement can run on the same state as the original routine. The comparison harness can check car blocks and broader RAM state, excluding documented harness-only regions. Snapshot coverage and randomized inputs are recorded separately; neither implies that unexercised branches are exact.

## Evidence and limitations

The [development record](../notes/analysis/PHYSICS.md) retains addresses, experiments,
test results and outstanding limitations. Older entries describe the state
at their recorded date and may be superseded. Consult [Fidelity](../FIDELITY.md)
for per-item grades; implementation coverage is not full-game certification.

## Out of the snapshot

Native race setup can construct state directly from disc data rather than
requiring a captured running race. The reference builder in
`tests/physics/race_build.c` constructs the initial conductor, race, world,
course, driver and garage objects at comparable addresses. The native
application path is in `src/core/physics/race_run.c`. Both preserve the
transitional representation used by the comparisons.

The [snapshot-independent setup record](../notes/analysis/PHYSICS.md)
describes which initial fields were isolated, how CORE tables and course
files are loaded, and which setup/step comparisons cover them. Construction
without a snapshot does not imply that every race mode has been verified.
