#!/usr/bin/env python3
"""Build the Windows release folder and zip from a release build.

Usage:
  cmake -S . -B build-release -G "Visual Studio 17 2022" -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DSDL_FORCE_STATIC_VCRT=ON
  cmake --build build-release --config Release --target moonengine
  python tools/package_release.py VERSION --icons PATH

The exe must link libav (MOON_LINKED_MOVIES, from tools/build_libav_msvc.cmd): the movies
and the player's music decode inside it, so no ffmpeg.exe ships or is looked for.

The release holds no game data: the player supplies their own disc image.
Output: dist/MoonEngine-VERSION-win64/ and dist/MoonEngine-VERSION-win64.zip
"""
import argparse
import shutil
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

README = """Moon Engine {version}
==================

An unofficial native PC port of Gran Turismo 4 (work in progress).
It is not an emulator and contains no game data. You need your own copy of
Gran Turismo 4, NTSC-U (SCUS-97328), as a disc image (.iso).

Not affiliated with or endorsed by Sony Interactive Entertainment or
Polyphony Digital. Gran Turismo is a trademark of Sony Interactive
Entertainment.

Getting started
---------------
1. Run MoonEngine.exe.
2. Choose "Game disc" and select your Gran Turismo 4 disc image.
3. Adjust Video, Audio and Controls if you like, then choose "Play".

Your disc image stays where it is. Profiles, replays and photographs are
kept in the "Player data" folder shown in the launcher.

Controls
--------
A controller is recommended (Xbox and PlayStation pads work through SDL).
Buttons can be remapped in the launcher or in the game's own Options.
Keyboard: arrows, Enter and Esc in menus.

Checking your disc
------------------
The supported executable on the disc, CORE.GT4, has the SHA-1
30031137ce494323816bb707b90d3ec0d07937d2.

Movies
------
The game's movies and your own music files are decoded inside MoonEngine.exe
(FFmpeg libraries, see THIRD_PARTY_NOTICES.txt). Nothing else is needed.

License
-------
MoonEngine is free software under the GNU General Public License, version 3
(LICENSE.txt), with additional terms in NOTICE.txt: works based on Moon Engine
must say "Based on Moon Engine", and modified versions must be marked as
different from the original. There is no warranty.

Source code, issues and updates: {repo}
"""

NOTICES = """Third-party components
======================

MoonEngine itself is released under the GNU General Public License, version 3
(LICENSE.txt), with the additional terms in NOTICE.txt. Its source code is
published at the project page given in README.txt.

SDL2 - zlib License - https://www.libsdl.org/
miniz - MIT License - https://github.com/richgel999/miniz
stb_image - MIT License - third_party/stb in the source code (STB_LICENSE.txt)
PDTools (Nenkai) - MIT License - third_party/PDTools in the source code (PDTOOLS_LICENSE.txt)
AMD FidelityFX Super Resolution 1 - MIT License - third_party/fsr1 in the source code
volk - MIT License - VULKAN_VOLK_LICENSE.txt
glslang - collected upstream licenses - VULKAN_GLSLANG_LICENSE.txt
Vulkan-Headers - Apache-2.0 or MIT - VULKAN_HEADERS_LICENSE.txt
ps2float (PCSX2) - GNU LGPL 3.0 or later - third_party/ps2float in the source code.
  It is linked into MoonEngine.exe; the complete source code of Moon Engine is
  published, so the program can be rebuilt with a modified ps2float.
{tango}
{ffmpeg}"""

FFMPEG_NOTICE = """FFmpeg n7.1 - GNU LGPL 2.1 or later - FFMPEG_LICENSE.txt
  libavcodec, libavformat, libavutil, libswscale, libswresample and libavfilter are
  linked statically into MoonEngine.exe to decode the movies and the player's music,
  built with --disable-gpl by tools/build_libav.sh in the source code. FFmpeg is
  Copyright (c) 2000-2024 the FFmpeg developers; its source code is available at
  https://ffmpeg.org/download.html. The complete source code of Moon Engine is
  published, so the program can be rebuilt with a modified FFmpeg.
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("version")
    parser.add_argument("--build", type=Path, default=ROOT / "build-release" / "Release")
    parser.add_argument("--icons", type=Path, help="the Tango launcher_icons folder")
    parser.add_argument("--repo", required=True, help="the project's public page")
    args = parser.parse_args()

    exe = args.build / "MoonEngine.exe"
    if not exe.is_file():
        sys.exit(f"missing {exe}: build the release first")
    cache = args.build.parent / "CMakeCache.txt"
    if not cache.is_file() or "MOON_LINKED_MOVIES:BOOL=ON" not in cache.read_text(errors="replace"):
        sys.exit(f"{exe} was not built with MOON_LINKED_MOVIES=ON: run tools/build_libav_msvc.cmd "
                 "and reconfigure the release build")
    ffmpeg_license = ROOT / "build" / "libav" / "ffmpeg-src" / "COPYING.LGPLv2.1"
    if not ffmpeg_license.is_file():
        sys.exit(f"missing {ffmpeg_license}: the FFmpeg source tools/build_libav.sh builds from")
    if args.icons and not (args.icons / "SOURCE.txt").is_file():
        sys.exit(f"missing {args.icons / 'SOURCE.txt'}")

    name = f"MoonEngine-{args.version}-win64"
    out = ROOT / "dist" / name
    # Validate the absolute destination before any recursive removal.
    release_root = (ROOT / "dist").resolve()
    if out.resolve().parent != release_root or out.name != name:
        sys.exit(f"invalid release destination: {out}")
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    shutil.copy2(exe, out / "MoonEngine.exe")
    if args.icons:
        shutil.copytree(args.icons, out / "launcher_icons")
    shutil.copy2(ffmpeg_license, out / "FFMPEG_LICENSE.txt")
    shutil.copy2(ROOT / "LICENSE", out / "LICENSE.txt")
    shutil.copy2(ROOT / "NOTICE", out / "NOTICE.txt")
    shutil.copy2(ROOT / "third_party" / "stb" / "LICENSE", out / "STB_LICENSE.txt")
    shutil.copy2(ROOT / "third_party" / "PDTools" / "LICENSE", out / "PDTOOLS_LICENSE.txt")
    for source in sorted((ROOT / "third_party" / "vulkan").glob("*_LICENSE.txt")):
        shutil.copy2(source, out / ("VULKAN_" + source.name))
    readme = README.format(version=args.version, repo=args.repo)
    (out / "README.txt").write_text(readme.replace("\n", "\r\n"), encoding="utf-8", newline="")
    notices = NOTICES.format(tango="Tango Icon Library - public domain - launcher_icons/SOURCE.txt\n" if args.icons else "", ffmpeg=FFMPEG_NOTICE)
    (out / "THIRD_PARTY_NOTICES.txt").write_text(notices.replace("\n", "\r\n"), encoding="utf-8", newline="")

    # nothing of the game may ride along
    for path in out.rglob("*"):
        if path.suffix.lower() in (".iso", ".bin", ".img", ".elf", ".irx", ".gt4", ".vol"):
            sys.exit(f"refusing to package game data: {path}")
        if path.name.lower() == "ffmpeg.exe":
            sys.exit(f"refusing to package {path}: libav is linked into the exe")

    archive = ROOT / "dist" / (name + ".zip")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
        for path in sorted(out.rglob("*")):
            if path.is_file():
                z.write(path, Path(name) / path.relative_to(out))
    print(f"{archive} ({archive.stat().st_size // 1024} KiB)")


if __name__ == "__main__":
    main()
