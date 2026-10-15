# Menu movies

Implementation: `src/platform/sdl/movie.c and src/app/main.c`.

Movie playback uses host decoding and is composed with native menu layers. Loading, playback completion and transitions are coordinated by the application. Boot notices are handled separately in src/core/boot_notice.c; reproducing their page flow does not establish movie frame accuracy.

## Evidence and limitations

The [development record](notes/MOVIES.md) retains addresses, experiments,
test results and outstanding limitations. Older entries describe the state
at their recorded date and may be superseded. Consult [Fidelity](FIDELITY.md)
for per-item grades; implementation coverage is not full-game certification.

## Boot notices

`src/core/boot_notice.c` loads the still notices shown before the opening
movie: the legal notice and the regional presentation screen. These are
separate from FFmpeg movie playback. The [boot-notice record](notes/MOVIES.md)
contains their original-source evidence and timing checks.
