# Suite failure investigation

## GT Mode extras fixture lifetime

`gtmode_extras_menu` retained its synthetic inheritance manager only in a C
local. It then entered `StatusRoot` and advanced 60 menu ticks before using
that pointer again. `menu_tick` calls `ah_gc`; the C local is not a VM root.
A temporary finalizer diagnostic confirmed that the manager was collected
before the failing call. The crash also affected the secret negative case,
which executes this same fixture before its intentional record corruption.

The fixture now pins the manager for that lifetime and unpins it before
closing the menu. No game behavior, assertion or garbage-collection policy
changed. The disc's `StatusRoot` inheritance action retains the manager in
VM-managed script references; this fixture crash does not establish a crash
when a player enters a secret command. The corresponding README warning
has been removed.

The positive case and all four negative cases pass. Coverage still includes
1,401 calendar days, weekly stock and black-car boundaries, used wheels,
secret command gates and records, and inheritance failure/one-time/reload
checks. Validation logs are local under `build/suite_fixes/`.

The first full run executed all 538 tests: the four remaining known
failures persisted, and `gtmode_setting_race` reached the 600-second limit
while other test sessions were running. All extras cases passed in that
run. The isolated repeat passed in 335.93 seconds. The timed-out full run is
not counted as a clean validation; subsequent full runs allow 1,200 seconds
without changing their checks.

## Demonstration telemetry isolation

Both `license_teacher_gold` and `disc_demo_playback` failed because a live
attempt left `license_log_collecting` enabled. Restarting as replay kept the
tape and therefore skipped the branch that previously cleared that flag.
The concourse was already disabled correctly. This is an application bug,
not an obsolete assertion.

`race_game_start_impl` now clears collection independently of tape retention;
the existing live Logger setup enables it again only when appropriate.
Original scene isolation is evidenced by `0011D1B8 +28..38`, and Logger
sampling setup by `00122FA8 +9C..100`. No physics arithmetic changed.

The B-1 positive playback/teacher-grade tests pass, with their existing tape
and grade negatives. A new telemetry negative re-enables collection and is
rejected at the original assertion. All 13 disc Theater recordings still
play with finite poses. The six selected tests pass.

Despite its name, `disc_demo_playback` tests B-1, not the title's entire
attract-mode sequence. The scoped integration correction is graded C; it
does not certify all opening demos or full trajectory/pixel fidelity.

After the demonstration fix, the full suite ran 539 tests in 980.20 seconds:
537 passed; only `gtmode_series_awards` and `storage_audit` still failed.

## Series awards and intermediate completion gifts

`gtmode_series_awards` expected precisely one new car across an entire series.
The disc's `PresentData` also queues completion gifts during intermediate
rounds. In the reproduced failure, `ex_premium_0000` received present 169
in round four and the series' present 23 in round five: the garage correctly
grew from 24 to 26 cars. The fixture ignored the intermediate prize screen.

The test now finishes intermediate car-gift screens and checks their exact
garage, acquisition-day and prize-statistics changes before rebasing the
series-specific counters. The original exact `before + 1`, bodies, date,
statistics, eligibility and save/reload checks remain. Logging copies the
car label before save refresh replaces its VM object, avoiding a stale
string pointer in the diagnostic output. No game code changed.

The corrected positive case covers 144 authored series, 527 stages, 33
championships and 16 endurances. The original losing-result negative still
fails; a new gift-count negative expects one nonexistent extra gift and
must fail. The old README championship-prize warning was based on this
incorrect fixture assumption and has been removed.

After the awards fixture fix, all 540 tests ran in 1,062.98 seconds:
539 passed; only `storage_audit` still failed.


## Storage audit bootstrap and picture selection

The remaining failure was in the fixture. It created a profile before
capturing the initialized GT Mode options and never loaded the original
disc tables, including the result-name enum. The application already loads
these through `gt4_game_load_disc`. The fixture now follows that bootstrap
and captures options before creating profile 7.

The PNG round trip also used the film selector. The authored Labomode
script distinguishes `MODE_LOAD_PHOTO_FILM` and `MODE_LOAD_PHOTO_PICTURE`;
the native selector correctly filters `.ogt4film` and PNG files separately.
The fixture now saves and selects pictures using their authored modes,
and additionally asserts that its PNG is absent from the film selector.
Existing exact pixel, export-error, replay, deletion, slot and translation
checks remain, with an additional assertion against runtime script errors.
No game code changed.

The target rebuild completed without warnings. `ctest -R storage_audit`
passed both the positive and deliberate legacy-text negative (2/2). The
positive checks 17 authored storage modes and 10,370 UI strings across 34
project directories, including 308 remapped original storage references.
At the user's request, the full suite was not repeated: its latest run was
539/540, with only this audit failing. The focused correction passes, but
this is not a newly measured 540/540 full-suite result.
