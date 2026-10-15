### B-Spec experience progression (14/10/2026)

E (bounded arithmetic): career_bspec ports 00468710 seed and 00468860
writeback, band selector 004683B8/00468418, victory doubling, u16/u8
saturation and course aggregate deltas. bspec_progress compares 4096 full
banks/style tables/course bytes with locally recompiled original code;
negative flips one output byte and fails. Boundaries replace profile lookup,
course lookup/label and course byte store. The original kind=0 branch leaves
four temporary bytes unwritten; this synthetic boundary supplies zero for
those bytes. Runtime 00468A40 integration returns -1/1, not that branch.

C (integration): prepared GT events/championship rounds seed the existing
003F1E60 player-style consumer before any driver samples, using live initial
parameters +FE0. Existing native driver_stats accumulates data unchanged.
Result writeback, before wear/replay, persists the 0x180 bank and per-course
8-byte skill through the existing profile backend; imported +228 course
bytes are retained. Reverse suffix 24 uses 00468690. Once-only writeback
and replay exclusion prevent duplicate awards. No solver, dynamics constant,
AI formula, input recipe or physics update cadence changed.

bspec_career runs a prepared Sunday Cup B-Spec race to physical finish
(8517 ticks), compares writeback, Retry seed, duplicate result, native
save/store/reload and replay exclusion. Its negative changes the checked
bank. gtmode_home getter oracle regressions also pass: 6/6 focused tests.
Logs: build/bspec-completion-tests.log and Testing/Temporary/LastTest.log.
Release executable updated locally; no server publication in this task.
Full PS2 trajectory, every car/course/mode and visual status comparison
remain A. This closes the missing career experience producer, not all
B-Spec presentation/behaviour equivalence.
