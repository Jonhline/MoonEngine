"""Real launcher settings persistence and boot preparation on the native app."""
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from png_diff import read_png

player, disc, output = map(Path, sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
prefs = output / "launcher.ini"
prefs.write_text(f"disc={disc.resolve().as_posix()}\nsaves={output.resolve().as_posix()}/save\n"
                 "show=1\ncheck_updates=0\nmute_music=1\n", encoding="utf-8", newline="")
video = Path(str(prefs) + ".video.ini")
negative = "--negative" in sys.argv[4:]
def launcher(name, sequence):
    run = subprocess.run([str(player), "--launcher", "--launcher-config", str(prefs),
                          "--input", sequence, "--shot", str(output / (name + ".png"))],
                         capture_output=True, text=True, timeout=45)
    assert run.returncode == 0, run.stdout + run.stderr
launcher("settings", "video,set:8:1,set:9:3,apply,video")
values = dict(line.split("=", 1) for line in video.read_text().splitlines())
if negative:
    values["low_latency"] = "0"
assert values["low_latency"] == "1" and values["fps_limit"] == "120"
# Opening and applying another video choice must retain the two new settings.
launcher("reopened", "video,set:4:2,apply,video")
values = dict(line.split("=", 1) for line in video.read_text().splitlines())
assert values["low_latency"] == "1" and values["fps_limit"] == "120"
frames = []
for prepare in (True, False):
    env = os.environ.copy()
    env.update(MOON_WINDOW_HIDDEN="1", MOON_VK_CACHE_DIR=str(output.resolve()))
    env.pop("MOON_VK_SKIP_PREPARE", None)
    if not prepare:
        env["MOON_VK_SKIP_PREPARE"] = "1"
    shot = output / f"prepare-{prepare}.png"
    run = subprocess.run([str(player), str(disc), "option", "OptionRoot", "--renderer", "vulkan",
                          "--no-config", "--shot", str(shot)], env=env,
                         capture_output=True, text=True, timeout=60)
    log = run.stdout + run.stderr
    shot.with_suffix(".log").write_text(log, encoding="utf-8", newline="")
    assert run.returncode == 0 and "Validation Error" not in log, log
    assert ("Vulkan startup preparation: ready=1" in log) == prepare, log
    frames.append(read_png(shot))
assert frames[0] == frames[1], "preparing GPU programs changed the menu image"
print("latency/limit persist through launcher reopen; startup preparation preserves pixels")
