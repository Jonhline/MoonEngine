# Reference thread audit

Implementation: `src/ee/thread.c and tests/initsys/`.

Reference-runtime thread behavior is checked against local captures. Stack reservation, status, scheduler state and context handling have separate evidence and known limitations. These details belong to original-code testing, not the native application runtime.

## Evidence and limitations

The [development record](../notes/analysis/THREAD_REFERENCE.md) retains addresses, experiments,
test results and outstanding limitations. Older entries describe the state
at their recorded date and may be superseded. Consult [Fidelity](../FIDELITY.md)
for per-item grades; implementation coverage is not full-game certification.
