# Physics validation status

The native physics keeps original formulas, constants and update order, with a transitional address-based state representation for comparisons. Snapshot and constructed-state tests exercise different paths. Exact results are scoped to their fixtures; use the per-item record rather than assuming every branch is covered.

## Evidence and limitations

The [development record](notes/PHYSICS_STATUS.md) retains source addresses,
experiments, positive and negative cases, and historical limitations.
Consult [Fidelity](FIDELITY.md) for each comparison's grade and scope.
Earlier entries may be superseded by later results.

## Shared scalar arithmetic (16 October 2026)

All live modes now lower native C scalar operators through the existing integer
EE model. This corrects replay drift without changing vehicle parameters or
recorded controls. Existing native GT4R v1 playback retains its historical
arithmetic; new recordings are v2. See the [arithmetic record](notes/DISC_REPLAY_ARITHMETIC.md)
for original-code comparisons, 80 licence gold attempts and remaining limits.
Earlier host-float comparisons do not independently establish all EE low bits.
