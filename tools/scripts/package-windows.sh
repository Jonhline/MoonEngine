#!/usr/bin/env bash
# package-windows.sh -- assemble the Windows release folder, ffmpeg included.
#
# WHY THIS EXISTS: the menu's cutscene and car are MPEG-PS movies decoded by
# spawning an external ffmpeg (src/platform/sdl/movie.c). The engine can only
# find ffmpeg if the release folder ships it: on the first run it adopts the
# copy beside MoonEngine.exe into the user's own folder, then uses that copy.
# Without it a fresh machine shows no cutscene and the menu loses its car.
#
# This script makes one folder the captain can copy to a machine as-is:
#     MoonEngine.exe   the game
#     ffmpeg.exe       the movie decoder (BtbN static win64-gpl)
#     README.txt       how to run it
#
# It never downloads anything. It refuses and exits non-zero rather than
# produce a package missing either the exe or ffmpeg.
#
# usage: bash tools/scripts/package-windows.sh [build-tree] [output-dir]
#   build-tree  default /mnt/ssd/moon-win      (must hold MoonEngine.exe)
#   output-dir  default /mnt/ssd/moonengine-ship
set -euo pipefail

BUILD=${1:-${BUILD:-/mnt/ssd/moon-win}}
OUT=${2:-${OUT:-/mnt/ssd/moonengine-ship}}
FFMPEG_SRC=${FFMPEG_SRC:-/mnt/ssd/tmp/ffmpeg-win/ffmpeg.exe}
BTBN="BtbN FFmpeg-Builds, win64-gpl (https://github.com/BtbN/FFmpeg-Builds/releases)"

die() { echo "package-windows: $*" >&2; exit 1; }

# Prefer the CMake output name, but accept the lowercase one older trees made.
EXE_SRC=
for cand in "$BUILD/MoonEngine.exe" "$BUILD/moonengine.exe"; do
    [ -f "$cand" ] && { EXE_SRC=$cand; break; }
done
[ -n "$EXE_SRC" ] || die "no MoonEngine.exe in $BUILD (build it first: cmake --build $BUILD --target moonengine)"

[ -f "$FFMPEG_SRC" ] || {
    echo "package-windows: ffmpeg not found at $FFMPEG_SRC" >&2
    echo "  Get the Windows static build: $BTBN" >&2
    echo "  Put ffmpeg.exe there (this script never downloads anything)." >&2
    exit 1
}

echo "package-windows: build tree $BUILD"
echo "package-windows: ffmpeg     $FFMPEG_SRC"
echo
for f in "$EXE_SRC" "$FFMPEG_SRC"; do
    printf '  %-40s %12s bytes  %s\n' "$(basename "$f")" "$(stat -c%s "$f")" "$(md5sum "$f" | cut -d' ' -f1)"
done
echo

# Assemble. Refuse to touch a directory that is not a previous package.
if [ -e "$OUT" ]; then
    if [ ! -d "$OUT" ]; then
        die "output $OUT exists and is not a directory"
    fi
    for entry in "$OUT"/*; do
        [ -e "$entry" ] || continue
        case "$(basename "$entry")" in
            MoonEngine.exe|moonengine.exe|ffmpeg.exe|README.txt) ;;
            *) die "output $OUT holds '$entry' which this script did not put there; move it away first" ;;
        esac
    done
fi
mkdir -p "$OUT"
rm -f "$OUT/MoonEngine.exe" "$OUT/moonengine.exe" "$OUT/ffmpeg.exe" "$OUT/README.txt"

cp "$EXE_SRC" "$OUT/MoonEngine.exe"
cp "$FFMPEG_SRC" "$OUT/ffmpeg.exe"

cat > "$OUT/README.txt" <<'EOF'
Moon Engine -- Windows release folder
=====================================

This folder is complete as it stands:

    MoonEngine.exe   the game (one file; no other engine files are needed)
    ffmpeg.exe       the movie decoder (menu cutscene and car)
    README.txt       this file

HOW TO RUN
  Double-click MoonEngine.exe. Point it at your own Gran Turismo 4 disc image.
  No disc image is included, and nothing is downloaded.

FFMPEG
  ffmpeg.exe must sit beside MoonEngine.exe on the first run. The first time
  the engine starts it moves ffmpeg.exe into your own user folder
  (%APPDATA%\MoonEngine\MoonEngine\) and uses that copy from then on, so this
  folder is left holding just the game. If ffmpeg.exe is missing on the first
  run, the menu loses its cutscene and its car, and the log names every place
  the engine looked for it.

  This is a static build of FFmpeg (BtbN FFmpeg-Builds, win64-gpl). FFmpeg is
  licensed under the LGPL/GPL; see its own terms.

YOUR DATA
  Saves, settings and mods live in %APPDATA%\MoonEngine\MoonEngine\.

Moon Engine is free software under the GNU GPL v3, with no warranty.
Gran Turismo 4 is a trademark of its owners; this project ships no game data.
EOF

for f in MoonEngine.exe ffmpeg.exe README.txt; do
    [ -s "$OUT/$f" ] || die "package is incomplete: $OUT/$f missing"
done
echo "package-windows: wrote $OUT"
ls -l "$OUT"
