"""Exercise the standalone launcher through its real window messages.

Without a disc only the picker is visible. A supplied user ISO exercises
Options artwork, configuration and Play without booting a race.
"""
import argparse
from pathlib import Path
import subprocess
import json


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--negative", action="store_true")
    parser.add_argument("--iso", type=Path)
    args = parser.parse_args()
    root = args.folder.resolve()
    root.mkdir(parents=True, exist_ok=True)
    saves = root / "saves"
    saves.mkdir(exist_ok=True)
    prefs = root / "launcher.ini"
    with prefs.open("w", encoding="utf-8", newline="") as f:
        f.write(f"disc=\nsaves={saves.as_posix()}\nshow=1\n")
    video = Path(str(prefs) + ".video.ini")
    options = Path(str(video) + ".game_options")
    with options.open("w", encoding="utf-8", newline="") as f:
        f.write("future_option 0 123\n")

    def run(name, inputs, shot=True):
        command = [str(args.executable.resolve()), "--launcher", "--launcher-config", str(prefs)]
        if inputs:
            command += ["--input", inputs]
        if shot:
            command += ["--shot", str(root / (name + ".png"))]
        completed = subprocess.run(command, capture_output=True, text=True, timeout=30)
        check(completed.returncode == 0, completed.stdout + completed.stderr)
        return completed.stdout

    video_before = video.read_bytes() if video.exists() else None
    plain = run("home", "")
    check("LAUNCHER STAGE disc CACHE 0 BUTTONS 1" in plain, "no ISO: only the disc picker")
    check("cursor=0 audio=0" in plain,"no ISO has no disc cursor or audio")
    blocked = run("blocked", "video,audio,controls,startup,play")
    check("LAUNCHER STAGE disc" in blocked and "BOOT " not in blocked, "no ISO cannot open settings or Play")
    check((video.read_bytes() if video.exists() else None) == video_before,
          "blocked settings cannot change video configuration")
    metadata = root / "latest.json"
    release = dict(version="0.3.0", setup="OpenGT4-0.3.0-setup.exe", sha256="0" * 64, size=3, notes="Test")
    metadata.write_text(json.dumps(release), encoding="utf-8")
    # The fixture compares against the version stamped into this build.
    version = plain.split(" VERSION ")[1].splitlines()[0]
    parts = version.split(".")
    release["version"] = f"{int(parts[0]) + 1}.0.0"
    metadata.write_text(json.dumps(release), encoding="utf-8")
    offered = run("update-offer", "update-manifest:" + metadata.as_posix())
    check("LAUNCHER UPDATE " + release["version"] in offered, "new version shows Update")
    release["version"] = version
    metadata.write_text(json.dumps(release), encoding="utf-8")
    equal = run("update-equal", "update-manifest:" + metadata.as_posix())
    check("LAUNCHER UPDATE none" in equal, "equal version stays silent")
    release["version"] = f"{int(parts[0]) + 1}.0.0"
    metadata.write_text(json.dumps(release), encoding="utf-8")
    disabled = run("update-disabled", "update-check:0,update-manifest:" + metadata.as_posix())
    check("LAUNCHER UPDATE none CHECK 0" in disabled, "disabled checks suppress offer")
    check("check_updates=0" in prefs.read_text(), "update preference persists")
    run("update-enabled", "update-check:1")
    wrong = root / "wrong.iso"
    wrong.write_bytes(b"Not an ISO")
    prefs.write_text(f"disc={wrong.as_posix()}\nsaves={saves.as_posix()}\nshow=1\n", encoding="utf-8")
    invalid = run("invalid", "")
    check("LAUNCHER STAGE disc CACHE 0 BUTTONS 1" in invalid, "invalid ISO cannot load artwork")
    if not args.iso:
        check("LAUNCHER STAGE " + ("options" if args.negative else "disc") in invalid, "deliberate stage mismatch")
        print("PASS launcher picker: empty/invalid ISO and blocked settings")
        return
    prefs.write_text(f"disc={args.iso.resolve().as_posix()}\nsaves={saves.as_posix()}\nshow=1\n", encoding="utf-8")
    full = run("options", "")
    check("LAUNCHER STAGE options" in full and "BUTTONS 8" in full, "valid ISO: complete menu")
    check("cursor=1 audio=1" in full,"validated ISO enables disc cursor and audio")
    check("LAUNCHER FILTER FSR1" in full,"validated ISO uses FSR for artwork")
    muted = run("music-muted", "music-mute:1")
    check("LAUNCHER MUSIC MUTE 1" in muted and "mute_music=1" in prefs.read_text(),
          "launcher music mute applies and persists")
    check("LAUNCHER MUSIC MUTE 1" in run("music-mute-reopen", ""), "mute survives reopening")
    check("LAUNCHER MUSIC MUTE 0" in run("music-restored", "music-mute:0"), "music can be restored")
    midpoint = run("hover-middle", "hover:0,animate:70")
    endpoint = run("hover-complete", "hover:0,animate:140")
    check("hover=0.50" in midpoint and "hover=1.00" in endpoint,"hover transition advances and finishes")
    check((root / "hover-middle.png").read_bytes() != (root / "hover-complete.png").read_bytes(),"hover changes rendered pixels")
    cache = run("cached", "")
    check("LAUNCHER STAGE options CACHE 1" in cache, "AppData artwork cache reused")
    returned = run("invalid-after-valid", f"disc:{wrong.as_posix()},controls,play")
    check("LAUNCHER STAGE disc CACHE 0 BUTTONS 1" in returned and "BOOT " not in returned,
          "selecting invalid ISO removes the complete menu even with cached art")
    check("cursor=0 audio=0" in returned,"invalid ISO removes pointer and audio")
    recovered = run("recovered", f"disc:{wrong.as_posix()},disc:{args.iso.resolve().as_posix()}")
    check("LAUNCHER STAGE options" in recovered and "BUTTONS 8" in recovered, "picker recovers after a valid choice")
    check("LAUNCHER FOCUS 1" in run("navigation-right", "focus:0,right"), "Right from Play reaches Game disc")
    check("LAUNCHER FOCUS 0" in run("navigation-wrap", "focus:4,right"), "the five-column row wraps")
    check("LAUNCHER FOCUS 7" in run("navigation-exit", "focus:4,down"), "Down from Controls reaches Exit")
    check("LAUNCHER FOCUS 6" in run("navigation-startup", "focus:1,down"), "Down from Game disc reaches startup")
    check("LAUNCHER FOCUS 0" in run("navigation-down", "focus:6,down"), "Down from startup returns to Play")
    check("LAUNCHER FOCUS 4" in run("hover", "hover:4"), "the mouse over Controls shows its details")
    run("apply", "video,set:2:3,apply,audio,set:0:40,apply,controls,set:1:16,set:2:15,apply,startup")
    values = dict(line.split("=", 1) for line in video.read_text().splitlines())
    check(values["width"] == "1920" and values["height"] == "1080", "video Apply")
    text = options.read_text()
    check("menu_bgm_volume_raw 0 40\n" in text and "future_option 0 123\n" in text, "audio Apply/preserved options")
    lines = dict((line.split()[0], line.split(maxsplit=2)[2]) for line in text.splitlines())
    pad = [int(n) for n in lines["pad_buttons_1p"].split(",")]
    check(pad[14] == 5 and pad[15] == 4, "1P remap")
    check("show=0\n" in prefs.read_text(), "startup switch")
    run("conflict", "controls,set:1:15,set:2:15,apply")
    check(options.read_text() == text, "duplicate assignment must not persist")
    run("cancel", "audio,set:0:10,back,controls,default,back")
    check(options.read_text() == text, "Cancel/Default without OK must not persist")
    run("player2", "controls,2p,set:1:16,set:2:15,apply")
    lines = dict((line.split()[0], line.split(maxsplit=2)[2]) for line in options.read_text().splitlines())
    pad2 = [int(n) for n in lines["pad_buttons_2p"].split(",")]
    check(pad2[14] == 5 and pad2[15] == 4, "2P remap")
    check(lines["pad_buttons_1p"] == ",".join(map(str, pad)), "1P unchanged by 2P")
    run("video", "video")
    run("audio", "audio")
    run("controls", "controls")
    if args.iso:
        with prefs.open("w", encoding="utf-8", newline="") as f:
            f.write(f"disc={args.iso.resolve().as_posix()}\nsaves={saves.as_posix()}\nshow=1\n")
        pumped = run("pump-play", "pump-play", shot=False)
        check("BOOT " in pumped, "Play inside SDL pump must finish launcher teardown and forward boot")
        boot = run("play", "play", shot=False)
        check("BOOT " in boot and str(saves).replace("\\", "/") in boot.replace("\\", "/"), "Play forwards ISO/save/config")
    # Deliberate wrong expectation: the comparator must detect a saved error.
    check(int(values["width"]) == (1921 if args.negative else 1920), "deliberate width mismatch")
    print("PASS launcher window: Apply, Cancel, conflicts, defaults, both players, persistence and startup switch")


if __name__ == "__main__":
    main()
