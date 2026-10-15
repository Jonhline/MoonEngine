# Windows atomic file replacement

This is a deliberate PC storage policy (grade P), authorized on 13 October
2026. It does not change save/replay/config formats, filenames or the
original game logic.

`include/platform/windows_file.h` defines the shared replacement helper.
The save writer, replay removal/rename and both launcher configuration
replacement paths use it, retaining their existing MoveFileExW flags.

Only ERROR_ACCESS_DENIED, ERROR_SHARING_VIOLATION and ERROR_LOCK_VIOLATION
are retried. There are ten attempts with nine waits of 10 through 90 ms
(450 ms total waiting, excluding system call/scheduling time). These waits
are a requested PC policy, not timings taken from GT4. Other errors return
immediately. The final Windows error is retained. Neither the helper nor
the Windows launcher failure paths delete the source temporary file or
existing destination after unsuccessful replacement.

`windows_file_replace` holds the destination without FILE_SHARE_DELETE,
releases its handle after 50 ms on a worker thread and checks that the
replacement succeeds. It also holds the lock through all retries and checks
failure, unchanged original content and retained temporary content. A
missing source tests immediate failure for an error outside the retry set.
`windows_file_replace_negative` deliberately bypasses retries and must fail
the positive replacement check; CTest marks this case WILL_FAIL.

Focused validation: both Windows tests and replay_file passed. A full
Release build completed without new compiler warnings. The first full `-j8 --timeout 600` run had the six baseline failures plus
an intermittent gtmode_setting_race timeout. The repeated full run completed
535 tests in 1140.88 seconds: 529 passed, none skipped, and exactly the six
baseline failures (license_teacher_gold, disc_demo_playback,
gtmode_series_awards, storage_audit, gtmode_extras_menu and
gtmode_extras_secret_negative). Both new lock tests passed; license_cycle
and mission_catalog passed in both runs. setting_race passed in the repeated
run in 504.53 seconds. Test timeouts and parallelism were unchanged; the
repeat discarded generated CTest scheduling-cost history only.
