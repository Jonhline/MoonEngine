# Jukebox: music folder — 19 October 2026

GT Mode > Jukebox keeps the original slide/MusicRoot layout. There is no added
Playlists sidebar, management screen, file picker, F9 shortcut or launcher editor.
Local songs are bound into the page GT Mode itself opens (before it is entered);
no second Jukebox page runs over it. Its own Exit leaves the project, which
finalizes MusicRoot and stores Racing/Slideshow.
Library shows disc songs and local songs together. Use the original Racing and
Slideshow folders, Append1/Append2, Trash, Play/Stop, ordering and Shuffle controls.

With `audio.jukebox_folders` (not Classic) two more folders follow them: Menu
(the music under the game's menus) and Replay (replays). Add songs with their
buttons in the Library, below the Racing and Slideshow ones; Trash, order and
Shuffle are the same. Shuffle is shared by all folders, as on the PS2. An empty
Menu plays the original menu music; an empty Replay plays the Racing list. They
are saved as `Menu playlist` and `Replay playlist` in `<options file>.music`.

With `audio.jukebox_groups` (not Classic) local songs follow the disc's, grouped
by subfolder of Music and sorted by name; the subfolder shows where the artist goes
(`Track / Album/CD 2`). Files at the top of Music show Unknown Artist.

## Adding songs

Put audio in `Music` beside the active options file and reopen Jukebox. Subfolders
are scanned too. For the standard Windows profile this is
`%APPDATA%/MoonEngine/MoonEngine/Music`. A portable/custom options file uses its own
sibling Music directory. The directory is created on entering Jukebox. Scans are
bounded to 8 nested levels, 4096 entries, and a combined Library of 256 songs.
Reopening does not duplicate tracks. Audio files stay in place and are never deleted
by removing songs from a playlist. References from older library versions are retained.
Moving/removing a source leaves its existing reference unavailable.

Recognized extensions: WAV, MP3, FLAC, OGG, M4A. Since 19/10/2026 they decode in-process
through the libav linked into the binary (`tools/build_libav.sh`, `windows-msvc` for the
Visual Studio build via `tools/build_libav_msvc.cmd`): WAV PCM, MP3, FLAC, OGG Vorbis/Opus,
M4A AAC/ALAC, resampled by libswresample to 48 kHz stereo s16. No ffmpeg.exe is searched,
spawned or downloaded. Stereo 48 kHz 16-bit WAV is still read directly. Without linked libav
(`MOON_LINKED_MOVIES=OFF`) Windows falls back to an existing ffmpeg.exe, as the movies do.
`custom_music_formats` encodes a 440 Hz tone with the ffmpeg program (fixtures only) and
requires 440 sign changes per 0.5 s at 48 kHz from MP3/FLAC/OGG/M4A and a 44.1 kHz WAV;
a 400 Hz MP3 must fail the same check. Embedded tags are not read yet: filename (no extension) / Unknown Artist.
The non-Windows folder scanner is implemented, but Android/Switch playback and
storage access have not been built or validated in this delivery.

Editing a mixed racing/slideshow list opts into `audio.custom_music=1`.
Merely opening the Library does not enable it. Classic sets the option to 0 and
restores original playback without deleting extended lists. With no external songs, the original VM handles editing and PS2 playlist persistence. The extension stores references in `<options file>.music`,
separate from the original PS2 playlist blobs; source IDs never enter those blobs.

## Playback and evidence

`gt4:<ADS racing index>` references the selected disc without extracting audio.
One worker/ring handles local and original PCM; the callback does no file I/O.
Manual preview plays exactly the selected track, including its buffered tail,
leaving the original script in charge of continuation and text. Pause and volume
use existing routing; original jingles are preserved. The shuffle RNG is independent
of the original saved PS2 state: no identical sequence is claimed.

`custom_music_ui` enters the original project twice with an isolated Unicode profile,
scans ten generated WAV files including a nested folder, selects a file in the
original Library, adds it to both original playlists, removes/readds, previews,
uses native Shuffle, and exits via the original Exit control. It checks PCM,
persistence, no duplicate entries on reopening, source integrity, no player save,
no undefined script calls, and absence of added management widgets.
`custom_music_mixed` compares 8192 frames following WAV -> disc transition byte for
byte with the original PCM decoder, plus pause and mixed-reference persistence.
`custom_music_formats` verifies generated MP3/FLAC/OGG/M4A on Windows.

The old model still reads named lists/M3U for compatibility, but those operations
are no longer exposed as a separate UI. This folder integration is an optional PC
adaptation (P), not a claim of complete PS2 screen equivalence.
