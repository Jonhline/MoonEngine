# Car catalog

Implementation: `src/core/specdb.c and src/core/menu.c`.

Car identity, names, manufacturers, paint variations and parts come from the disc tables. Selection loads the appropriate model and passes the chosen data into the race request. Catalog completeness and race behavior are separate checks.

## Evidence and limitations

The [development record](notes/CAR_CATALOG.md) retains addresses, experiments,
test results and outstanding limitations. Older entries describe the state
at their recorded date and may be superseded. Consult [Fidelity](FIDELITY.md)
for per-item grades; implementation coverage is not full-game certification.
