#!/usr/bin/env python3
"""Create a private Switch ZIP from an explicitly selected, tested NRO.

No network calls or publication. Game images, player data and console SDK
products are never copied; the selected Windows Nxlink host tool is bundled.
The included source snapshot contains tracked build inputs
from the working tree; the manifest identifies the actual binary by SHA-256.
"""
import argparse
import hashlib
import json
import re
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

README = """MOON ENGINE - SWITCH - {version}
Private test package. Not an official public release.

Windows setup (Windows 10/11)
1. Extract the ENTIRE ZIP. Open MoonEngine-Switch-Setup.exe.
2. Choose your own GT4 USA ISO (SCUS-97328, any revision), then Continue.
3. Select USB / DBI or Network / Nxlink, then Install.
If retrying after the ISO was copied, keep "Reuse existing same-size ISO" on.
Failures write MoonEngine-Switch-Setup.log next to the setup app (or show
its temporary location if this folder is read-only). Nxlink output is kept
in nxlink.log under the temporary folder identified in the report.

USB / DBI: open DBI > Start MTP Connection and connect USB to the PC.
Network / Nxlink: enable FTP and Nxlink in Sphaira and leave its home
screen open in application mode. Enter the Switch IP address and FTP port
(usually 5000). FTP must allow anonymous access on the same local network.
FTP transfers the ISO and persistent files; Nxlink sends and launches the
app afterward. Nxlink alone cannot copy the ISO.

The setup copies your ISO and the app, configures the disc path and verifies
the installed NRO. It does not open CMD or require PowerShell, .NET, Python
or devkitPro on the player's PC. Its two-step screen follows the current
launcher's white room, centered content and lower action bar.
Keep the connection active until setup reports Done. This single-file ISO
workflow needs exFAT/large-file support for discs larger than 4 GB.
Setup NEVER formats the SD card.

Playing
Close MTP. Hold R while opening a game to enter Homebrew Menu with full RAM,
then open Moon Engine. Album mode has insufficient RAM. Use application
mode in Sphaira too. This package is an NRO, not an NSP.

Saves and settings
Existing saves are preserved. If switch/opengt4 exists and switch/moonengine
does not, setup updates the old folder without creating a second app.
Previous NRO/config backups go to moonengine-backups. Do not copy another
player's saves or config; each player creates their own profile.

Manual installation (optional)
Copy this package's switch folder to the SD root and put your ISO at
switch/moonengine/gt4.iso. Configuration is created on first launch.
For older installations with saves in switch/opengt4, prefer setup.

Controls: A=Circle, B=Cross, X=Triangle, Y=Square (by position).
Touch: tap to select, then tap the same item to confirm. Drag lists/map.
Touch options are under Options > Screen Settings.

Build status
Release ARM64/deko3d, with video decoding on the Switch. Based on a build
tested on the developer's console. Still experimental: steady 60 FPS on
all tracks is unproven; an error returning to Sphaira after quitting is
still being investigated.
USB transport still needs validation with actual DBI hardware. Local tests
cover copy/rename/hash and config preservation. Network tests use a local
FTP fixture for upload, backup, rename, hash and preserving the installed
file after failed verification. The full ISO-to-console installation still
needs player confirmation. A reused same-size SD ISO is not rehashed;
the selected local ISO is always verified.

This unofficial port is not affiliated with Sony or Polyphony Digital.
No game data, console keys, firmware or saves are included. Supply your own
disc copy. Gran Turismo is a trademark of its respective owners.

License: GPL-3.0 with NOTICE.txt terms. Project source/build scripts:
source.zip. Dependencies and fonts: THIRD_PARTY.txt.
Binary identity and hashes: manifest.json.
"""


NOTICES = """Moon Engine: LICENSE.txt (GPL-3.0) and NOTICE.txt.
The project source/build scripts are in source.zip. This is a private
development package, not a public update feed or an official game release.

Switch libraries come from the devkitPro packages used by the build:
https://github.com/devkitPro/pacman-packages
https://github.com/devkitPro/portlibs
https://github.com/switchbrew/switch-tools
https://github.com/switchbrew/libnx (ISC)
https://github.com/devkitPro/deko3d (zlib)
https://github.com/libsdl-org/SDL (zlib)
https://ffmpeg.org/releases/ffmpeg-7.1.tar.xz (this SDK build: GPL-2.0-or-later)
FFmpeg legal/build information: https://ffmpeg.org/legal.html
ps2float: LGPL-3.0-or-later; complete source and licenses in source.zip.
stb_image: MIT/public domain; source and license in source.zip.
miniz 3.0.2: MIT; https://github.com/richgel999/miniz/tree/3.0.2
The installed FFmpeg configuration reports --enable-gpl and GPL version 2
or later. See the package recipes for transitive dependencies and licenses.
The console SDK, commercial game data and keys are not distributed here.
The Windows Nxlink host tool is included (switch-tools 1.13.1); its ISC
license is in NXLINK-ISC.txt. Barlow launcher fonts retain SIL OFL 1.1;
see BARLOW-OFL.txt. These are host tool/font licenses, not game assets.
"""


def source_names(root):
    names = subprocess.check_output(["git", "ls-files", "-z"], cwd=root).decode().split("\0")
    roots = {"CMakeLists.txt", "LICENSE", "NOTICE", "README.md", ".clang-format"}
    prefixes = ("src/", "include/", "cmake/", "third_party/", "tools/", "tests/", "config/")
    private, private_prefixes = set(), ()  # the shared tree: nothing private is in it
    export = Path(__file__).resolve().parent / "export_public.py"
    if export.is_file():  # the development tree: leave out what its public export leaves out
        import importlib.util
        spec = importlib.util.spec_from_file_location("export_public", export)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        private, private_prefixes = module.EXCLUDE, module.EXCLUDE_PREFIXES
    for name in names:
        if name in private or name.startswith(private_prefixes):
            continue
        if name in roots or name.startswith(prefixes):
            path = root / name
            if path.is_file():
                yield name


def build_package(nro, output, version, root=ROOT, setup_exe=None, nxlink=None):
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", version):
        raise ValueError("Invalid version label")
    data = nro.read_bytes()
    if len(data) < 0x20 or data[0x10:0x14] != b"NRO0":
        raise ValueError("Not a Switch NRO")
    if setup_exe is None or nxlink is None:
        raise ValueError("Provide the native setup executable and Windows Nxlink host tool")
    setup_data, nxlink_data = setup_exe.read_bytes(), nxlink.read_bytes()
    if not setup_data.startswith(b"MZ") or not nxlink_data.startswith(b"MZ"):
        raise ValueError("Expected Windows PE executables")
    if output.exists():
        raise FileExistsError(f"Refusing to overwrite {output}")
    output.mkdir(parents=True)
    sd = output / "switch/moonengine"
    installer = output / "installer"
    sd.mkdir(parents=True)
    installer.mkdir()
    (sd / "moonengine.nro").write_bytes(data)
    (output / "MoonEngine-Switch-Setup.exe").write_bytes(setup_data)
    (installer / "nxlink.exe").write_bytes(nxlink_data)
    for name in ("BARLOW-OFL.txt", "NXLINK-ISC.txt"):
        (output / name).write_bytes((root / "tools/licenses" / name).read_bytes())
    for name in ("LICENSE", "NOTICE"):
        (output / (name + ".txt")).write_bytes((root / name).read_bytes())
    (output / "README.txt").write_text(README.format(version=version), encoding="utf-8", newline="")
    (output / "THIRD_PARTY.txt").write_text(NOTICES, encoding="utf-8", newline="")
    manifest = {
        "version": version,
        "private": True,
        "nro_sha256": hashlib.sha256(data).hexdigest(),
        "setup_sha256": hashlib.sha256(setup_data).hexdigest(),
        "nxlink_sha256": hashlib.sha256(nxlink_data).hexdigest(),
        "disc_serial": "SCUS-97328",
        "source_snapshot_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "source_snapshot_worktree": True,
        "usb_hardware_validated": False,
        "network_fixture_validated": True,
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="")
    with zipfile.ZipFile(output / "source.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        for name in source_names(root):
            archive.write(root / name, name)
    zip_path = Path(str(output) + ".zip")
    if zip_path.exists():
        raise FileExistsError(f"Refusing to overwrite {zip_path}")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(output.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(output))
    return zip_path


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version")
    parser.add_argument("--nro", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--setup-exe", required=True, type=Path)
    parser.add_argument("--nxlink", required=True, type=Path)
    args = parser.parse_args()
    print(build_package(args.nro, args.out, args.version, setup_exe=args.setup_exe, nxlink=args.nxlink))
