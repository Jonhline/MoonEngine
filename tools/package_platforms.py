#!/usr/bin/env python3
"""Package the Linux and Switch release files, beside the Windows zip.

Usage:
  python tools/package_platforms.py VERSION --linux PATH/MoonEngine --switch PATH/moonengine.nro

Builds (both stable, -DMOON_DEV_BUILD=OFF -DMOON_VERSION=VERSION):
  Linux:  tools/build_linux.sh (configure line with those two options)
  Switch: docs/SWITCH.md, -DMOON_SWITCH_RENDERER=deko3d; the NRO icon is res/moonengine_switch.jpg

Output: dist/MoonEngine-VERSION-linux-x86_64.tar.gz and dist/MoonEngine-VERSION-switch.zip.
No game data is packaged.
"""
import argparse
import io
import sys
import tarfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from package_release import FFMPEG_NOTICE, NOTICES  # noqa: E402

COMMON = """Moon Engine {version} - {platform}
==================

An unofficial native port of Gran Turismo 4 (work in progress). It is not an
emulator and contains no game data. You need your own copy of Gran Turismo 4,
NTSC-U (SCUS-97328), as a disc image (.iso).

Not affiliated with or endorsed by Sony Interactive Entertainment or
Polyphony Digital. Gran Turismo is a trademark of Sony Interactive
Entertainment.

"""
LINUX = """Getting started
---------------
1. Run ./MoonEngine (it needs OpenGL; a desktop with X11 or Wayland).
2. Choose your Gran Turismo 4 disc image.
3. Press Start.

Optional: copy moonengine.desktop to ~/.local/share/applications and
moonengine.png to ~/.local/share/icons, and fix the Exec= path, for a menu entry.

Your data lives in ~/.local/share/MoonEngine/MoonEngine/.
To update, download the new release and replace these files.

Project page: {repo}
"""
SWITCH = """Getting started
---------------
You need a Switch that runs homebrew, an SD card formatted as exFAT (the disc
image is bigger than 4 GB) and your own disc image.

1. Copy the "switch" folder from this zip to the root of the SD card.
   The program ends up at switch/moonengine/moonengine.nro.
2. Copy your disc image to switch/moonengine/gt4.iso.
3. Open the Homebrew Menu by holding R while launching a game (full memory;
   the album applet does not have enough) and start Moon Engine.

Saves stay in switch/moonengine/. To update, replace moonengine.nro.

Project page: {repo}
"""
DESKTOP = """[Desktop Entry]
Type=Application
Name=Moon Engine
Comment=Gran Turismo 4, native (bring your own disc)
Exec=MoonEngine
Icon=moonengine
Terminal=false
Categories=Game;
"""


SWITCH_FFMPEG_NOTICE = """FFmpeg 7.1 (devkitPro portlib switch-ffmpeg 7.1-5, a GPL build) - GNU GPL 2
or later - FFMPEG_LICENSE.txt. Linked into moonengine.nro for the game's
movies. Source: https://ffmpeg.org/ and the build recipe at
https://github.com/devkitPro/pacman-packages (switch/ffmpeg).
"""


def licences(ffmpeg, ffmpeg_notice=FFMPEG_NOTICE):
    files = {
        "LICENSE.txt": (ROOT / "LICENSE").read_bytes(),
        "NOTICE.txt": (ROOT / "NOTICE").read_bytes(),
        "STB_LICENSE.txt": (ROOT / "third_party" / "stb" / "LICENSE").read_bytes(),
        "PDTOOLS_LICENSE.txt": (ROOT / "third_party" / "PDTools" / "LICENSE").read_bytes(),
    }
    for source in sorted((ROOT / "third_party" / "vulkan").glob("*_LICENSE.txt")):
        files["VULKAN_" + source.name] = source.read_bytes()
    if ffmpeg:
        files["FFMPEG_LICENSE.txt"] = ffmpeg.read_bytes()
    notices = NOTICES.format(tango="", ffmpeg=ffmpeg_notice if ffmpeg else "")
    files["THIRD_PARTY_NOTICES.txt"] = notices.encode()
    return files


def main():
    p = argparse.ArgumentParser()
    p.add_argument("version")
    p.add_argument("--linux", type=Path)
    p.add_argument("--switch", type=Path)
    p.add_argument("--ffmpeg-license", type=Path,
                   default=ROOT / "build" / "libav" / "ffmpeg-src" / "COPYING.LGPLv2.1")
    p.add_argument("--repo", default="https://github.com/Jonhline/MoonEngine")
    a = p.parse_args()
    (ROOT / "dist").mkdir(exist_ok=True)
    if a.linux:
        if not a.linux.is_file() or a.linux.read_bytes()[:4] != b"\x7fELF":
            sys.exit(f"not a Linux program: {a.linux}")
        name = f"MoonEngine-{a.version}-linux-x86_64"
        files = licences(a.ffmpeg_license if a.ffmpeg_license.is_file() else None)
        files["README.txt"] = (COMMON.format(version=a.version, platform="Linux") +
                               LINUX.format(repo=a.repo)).encode()
        files["moonengine.desktop"] = DESKTOP.encode()
        files["moonengine.png"] = (ROOT / "res" / "moonengine_icon.png").read_bytes()
        out = ROOT / "dist" / (name + ".tar.gz")
        with tarfile.open(out, "w:gz") as t:
            def add(rel, data, mode=0o644):
                info = tarfile.TarInfo(f"{name}/{rel}")
                info.size, info.mode = len(data), mode
                t.addfile(info, io.BytesIO(data))
            add("MoonEngine", a.linux.read_bytes(), 0o755)
            for rel, data in sorted(files.items()):
                add(rel, data)
        print(out)
    if a.switch:
        data = a.switch.read_bytes()
        if data[0x10:0x14] != b"NRO0":
            sys.exit(f"not an NRO: {a.switch}")
        name = f"MoonEngine-{a.version}-switch"
        gpl = a.ffmpeg_license.parent / "COPYING.GPLv2"
        if not gpl.is_file():
            sys.exit(f"missing {gpl}: the FFmpeg source tools/build_libav.sh builds from")
        files = licences(gpl, SWITCH_FFMPEG_NOTICE)
        files["README.txt"] = (COMMON.format(version=a.version, platform="Nintendo Switch") +
                               SWITCH.format(repo=a.repo)).replace("\n", "\r\n").encode()
        out = ROOT / "dist" / (name + ".zip")
        with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
            z.writestr("switch/moonengine/moonengine.nro", data)
            for rel, blob in sorted(files.items()):
                z.writestr(rel, blob)
        print(out)


if __name__ == "__main__":
    main()
