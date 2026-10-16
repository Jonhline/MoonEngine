# Readability refactor â€” 13 October 2026

The readability changes preserve game logic, test-visible symbols, float
expression ordering and original-function/offset references. Source movement
uses private textual fragments to keep each implementation in one translation
unit. main's extracted race helpers borrow its existing state and do not take
ownership of resources.

Commits:
- `2480050`: FSR line endings only, normalized to LF.
- `29ea6ed`: root clang-format and project-owned C formatting only.
- `c5364fb`: PID suffixes for concurrent test fixtures, separate from formatting.
- `7398b12`: bounded Windows replacement retries, separately authorized PC policy P.
- `aae8881`: 29 private fragments/header plus the three source entry files.
- The documentation/comment phase explains runtime boundaries and PDTools opcode names.

## Validation

All full runs use Release, `-j8` and `--timeout 600`. The baseline has
533 tests and six failures. Adding the two Windows lock tests brings the
suite to 535. No original tests were removed or skipped.

| Completed run | Tests | Passed | Failed | Time (s) |
| --- | ---: | ---: | ---: | ---: |
| Baseline | 533 | 527 | 6 | 1391.00 |
| Formatting, final matching run | 533 | 527 | 6 | 1189.00 |
| Windows storage, final matching run | 535 | 529 | 6 | 1140.00 |
| Source split | 535 | 529 | 6 | 1166.00 |
| Documentation/comments | 535 | 529 | 6 | 1122.00 |

Each completed row above has the same failure set: license_teacher_gold,
disc_demo_playback, gtmode_series_awards, storage_audit, gtmode_extras_menu
and gtmode_extras_secret_negative. The two Extras failures are segmentation
faults, including the negative case; they are not newly passing tests.

Earlier full attempts had intermittent save failures and settings timeouts.
Unique PID paths fix fixture collisions. A diagnostic copy then captured
Windows error 5 in replacement of active.ogt4 with unique paths, leading to
the separately authorized storage fix. Its first full run had the six
baseline failures plus a settings timeout; the repeated full run matched
all six. Both license_cycle and mission_catalog passed in both storage runs.
Generated CTest cost history was reset for matching runs; no test timeout,
parallelism or assertions were relaxed.

Formatting was checked across 539 files for token identity. CHECK is listed
as a whitespace-sensitive macro because stringified arguments matter in
tests. The movement checker reconstructs menu.c and race_game.c including
comment provenance and token order, reverses main's borrowed-state aliases,
and reconstructs the entire original main.c. Deliberately removing a token
is rejected. The comment checker compares every code token in the 11 edited
source/header files and rejects an injected declaration. Protected fidelity
and logic-validation documents retain their hashes.

Both Windows lock tests pass, including the deliberately broken no-retry
mode marked WILL_FAIL. The regular test checks success after a worker releases
a lock after 50 ms, failure under a persistent lock, retained original/temp
contents, and immediate failure for a missing source.

The full split and comment builds introduce no compiler warnings. Existing
C4090 in tests/ui/race_audio.c and C4005 for SDL's main macro in the
transmission test remain. Reference-file logs and comparison scripts are
local under build/readability and are not distributed.

## Size of the formerly worst files

Entries give line count / longest line in characters. The final C entry
files exclude their private fragments; no implementation was discarded.

| File | Before | After formatting | Final |
| --- | ---: | ---: | ---: |
| `src/core/menu.c` | 6402 / 819 | 10870 / 130 | 1881 / 102 |
| `src/app/main.c` | 2642 / 364 | 4712 / 123 | 2565 / 116 |
| `src/app/race_game.c` | 3583 / 291 | 5248 / 143 | 2700 / 143 |
| `src/core/save.c` | 146 / 1361 | 773 / 99 | 775 / 99 |
| `src/core/save_ps2.c` | 149 / 3657 | 955 / 100 | 955 / 100 |

Long source-reference comments, authored strings and protected CHECK arguments
can exceed the 100-column target; their text is preserved. Build, execution,
system mapping and private-fragment guidance are in [Architecture](../ARCHITECTURE.md).
The storage adaptation is described in [Windows storage](WINDOWS_STORAGE.md).

Final documentation/comment suite: 535 tests, 529 passed, the same six baseline failures,
zero skipped; 1122.58 seconds. setting_race passed in 485.39 seconds. No new
failures or compiler warnings. All three requested phases are complete.


## Named memory offsets

The follow-up uses constants for the original address-based layouts, without
converting state to C structures. Unknown fields retain `UNK` and the offset.
Unsigned variants retain an `_U` suffix so a replacement keeps the literal's
integer type as well as its value. Existing field names are reused where their
values and provenance already match.

The inventory covers project-owned C in `src/core/physics/`, `race_game.c` and
its private `race_game_*.inc` fragments. The starting revision's source has
5,956 raw offset literals: distinct source occurrences added in the first
address argument of `GT4_*`, `gt4_ptr`, read/write helpers, plus direct literal
arguments of local field-access macros. Nested calls count each occurrence
once; comments, numeric game-function addresses and named constants are excluded.
General `base + literal` expressions give a broader 6,334-candidate inventory,
which also includes arithmetic and address construction and is not an access count.

The car phase replaces 1,137 literal occurrences (1,040 of the counted memory
accesses). It introduces 105 constants, 70 retaining `UNK`, and reuses the
existing car enum. `CAR_BLOCK_*` is relative to `car + CAR_POSITION`; it is
applied only where the function's block has that provenance, not to every `b`.
Expansion back to integer tokens retains value, suffix/type and all surrounding
tokens in 32 source/fragments; the shared header changes only by its include.
An injected declaration is rejected by the same checker.

Release `dumpbin /disasm` comparisons retain symbol names, instruction offsets,
bytes, continuation bytes, instructions and relocation targets. Only the dumper
banner and archive section-size summary are excluded. Both `moon_physics.lib`
and `moon_core.lib` match the baseline; changing an instruction byte is rejected.
The full car run has 535 tests, 529 passes, the same six known failures and no
skipped cases (`-j8`, eight physical cores, timeout 600). The 13 selected physics,
session, race-audio, transmission and owned-car tests also pass. No new compiler
warnings are introduced. Build logs, original source copies and disassembly stay
local in `build/field_names/`; no original game data is published.

The conductor/session phase introduces 65 constants (28 UNK) and replaces
344 literal occurrences in 25 source/fragments. Selection prioritizes the
most-used offsets and fields whose meanings have direct code evidence.
Together the two structures introduce 170 constants, 98 UNK. The remaining
4,645 raw memory-offset occurrences are listed completely, by base expression,
offset, use count and source files, in [Offset inventory](OFFSET_INVENTORY.md).
The inventory includes address arithmetic; it does not assign unsupported
field meanings. Further work can select one subsystem at a time.

The conductor full suite has 535 tests, 529 passes, the same six baseline
failures and zero skipped cases (-j8, timeout 600; 1209.91 seconds).
setting_race passed in 527.97 seconds. All 13 selected tests pass.
Both Release library disassemblies match the original baseline byte for byte
after the documented banner/summary normalization. No new warnings appear.

## Laps and positions offset round

The laps.c round reduces raw memory-offset occurrences from 41 to 18.
Twenty of its 23 previously unknown constants receive names supported by
original-function disassembly and native consumers; three remain unknown.
Sixteen additional constants replace 23 raw literal occurrences. Shared
car/session fields remain in their layout headers; fields_laps.h contains
the two race-object recording gates. Other users only receive identifier
replacements. The complete original access list, name evidence and unresolved
fields are recorded in [Offset inventory](OFFSET_INVENTORY.md). The global
inventory now has 4,622 raw occurrences.

Exact token expansion preserves all surrounding code and literal types in
16 sources/headers; an injected declaration is rejected. Release physics
disassembly matches the initial baseline. A concurrent controls change
updates pad_config.c and adds three tests independently of this round.
Core disassembly therefore uses a separately captured baseline with those
controls changes frozen, and matches it; the initial baseline is retained
separately. No controls changes belong to the laps commit.

All 38 selected pit, physics and position tests pass (33.55 seconds).

The full suite has 538 tests, 532 passes and exactly the six known failures,
with zero skipped tests (-j8, timeout 600; 1218.91 seconds). All 535 original
registered tests are retained and the three independently added control tests
pass. setting_race passes in 511.51 seconds. No new compiler warnings appear.
Temporary build failures from concurrent regeneration, running executables
and one resource-compiler invocation were resolved by serialized configuration,
rebuilding the affected target and closing the running game.
