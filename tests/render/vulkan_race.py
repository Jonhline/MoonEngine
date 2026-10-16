"""Check race reflection residency and pixels without a machine-specific FPS gate."""
import os
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "ui"))
from png_diff import read_png

probe, disc, output = map(Path, sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
negative = "--negative" in sys.argv[4:]
frames = []
for cpu in (False, True):
    env = os.environ.copy()
    for key in ("OPENGT4_VK_SYNC", "OPENGT4_PROBE_APPEAR", "OPENGT4_UPSCALE",
                "OPENGT4_REFLECTION_CPU", "OPENGT4_WINDOW"):
        env.pop(key, None)
    env.update(OPENGT4_GRAPHICS_API="vulkan", OPENGT4_WINDOW_HIDDEN="1",
               OPENGT4_COURSE="tsukuba", OPENGT4_PROBE_FRAMES="60")
    if cpu or negative:
        env["OPENGT4_REFLECTION_CPU"] = "1"
    image = output / f"{'cpu' if cpu else 'gpu'}.png"
    run = subprocess.run([str(probe), str(disc), "ka_01", str(image)],
                         env=env, capture_output=True, text=True, timeout=90)
    log = run.stdout + run.stderr
    image.with_suffix(".log").write_text(log, encoding="utf-8", newline="")
    assert run.returncode == 0 and "Validation Error" not in log, log
    match = re.search(r"Vulkan benchmark: (\d+) submissions, (\d+) readbacks", log)
    assert match, log
    submissions, readbacks = map(int, match.groups())
    print(cpu, submissions, readbacks)
    if cpu:
        assert readbacks == 6 * 60, (submissions, readbacks)
    else:
        assert readbacks == 0 and submissions == 120, (submissions, readbacks)
    width, height, rows = read_png(image)
    frames.append([c for row in rows for pixel in row for c in pixel])
assert len(frames[0]) == len(frames[1]) == width * height * 3
differences = [abs(a - b) for a, b in zip(*frames)]
print("reflection pixel maximum", max(differences))
assert max(differences) <= 2
