# NOTE FOR JONHLINE — ffmpeg is gone: libav is linked into the binary now

*Written 2026-10-19 by Caine (the captain's agent). Purpose: your custom-music work depends on ffmpeg; the
ffmpeg dependency no longer exists in this tree, so here is exactly what replaced it and how to attach to it.*

## What changed (already committed, `cd518b9` "movies: link libav into the binary on every platform")

The spawned external `ffmpeg.exe` (**168,964,608 B — 12.7× the engine binary**) is replaced by **linking libav
into the binary** on Windows, Linux and Android arm64 — the same route the Switch already used.

Measured, not estimated:
- Windows exe: 14,334,290 → **16,051,337 B** (+1.64 MiB)
- Android APK: 15.93 MB → **17.81 MB** (+1.88 MB)
- the decoded movie frames are **byte-identical** to what the spawned ffmpeg produced (same md5 before/after)

So: same pictures, **one artifact, no download, no child process, no PATH search, nothing beside the exe.**

## The mechanics, so your branch can match them

- `tools/build_libav.sh` builds **FFmpeg n7.1** static with `--disable-autodetect --disable-gpl
  --disable-programs --disable-network`, and exactly the components the movies need:
  - demuxers: `mpegps`, `ipu`, `mpegvideo`
  - decoders: `mpeg2video` **and** `ipu` — GT4 uses IPU as well as MPEG-2, an mpeg2-only build silently misses
    half the movies
  - parsers: `mpegvideo`, `ipu`; protocol: `file`
  - filters: `scale format null fps setfield yadif il`; plus swscale / avfilter / avformat / avcodec / avutil
- CMake (`CMakeLists.txt:286-315`) picks a per-platform archive set from `build/libav/<target>/lib/`
  (`windows`, `linux`, `android-arm64`) and **`MOON_LINKED_MOVIES` defaults to ON when those archives exist**.
- With `MOON_LINKED_MOVIES` defined:
  - the entire Windows spawner in `src/platform/sdl/movie.c` is **compiled out** (`movie.c:9` —
    `#if !defined(MOON_LINKED_MOVIES) && defined(_WIN32)`); the search order (beside the exe → user folder →
    build-time `MOON_FFMPEG` → PATH) is gone
  - `src/app/prepare_ffmpeg.c` is **removed from the launcher sources** (`CMakeLists.txt:355-357`): not compiled,
    **no download**, no unzip-verify-adopt step
  - decoding happens in-process; the FFmpeg attribution lives in `NOTICE`
- Android is no longer an exception: it takes the same linked path.

## What this means for custom music

1. **Do not spawn or download anything for decoding.** Any code that expects an `ffmpeg.exe` beside the binary
   or a `MOON_FFMPEG` program path is dead here — with `MOON_LINKED_MOVIES` on it will do nothing or fail.
2. **Name the decoder before adding it** (G6.1's own rule). Two supported routes:
   - **Preferred for the formats G6.1 specifies (MP3 / OGG): `minimp3` (public domain) + `stb_vorbis` (public
     domain)** — tiny, no FFmpeg, no LGPL question, nothing to build.
   - **Or extend libav**: add the audio demuxers/decoders you need (mp3, vorbis, flac, opus + ogg/matroska/mp4
     demuxers) to the `tools/build_libav.sh` configure line. One place, every platform benefits.
3. **The hook is the engine-side half and is decoder-independent** — that is where your work matters most to us:
   find where the game requests "play BGM id" and serve it from a user folder + a mapping table
   (game state → file). Background music only; dynamic audio (engine notes, tyre squeal) stays from the disc.
4. Whatever PCM you produce should go through the existing music path (`src/core/music.h` →
   `gt4_music_render*`), the same way the disc's own tracks do.

## Checklist for the branch

- [ ] delete any ffmpeg search / download / exec code
- [ ] no `MOON_FFMPEG` dependency for audio
- [ ] decode in-process, to PCM, through the existing music API
- [ ] if you add decoders: extend `tools/build_libav.sh`, keep `--disable-gpl`, attribution into `NOTICE`
- [ ] say which formats you intend to support, so the two build routes do not diverge

Cross-reference: `docs/MOVIES.md` (in-process pipeline), `docs/vulcan4/WISHLIST.md` **G6.1 — CUSTOM MUSIC**.
