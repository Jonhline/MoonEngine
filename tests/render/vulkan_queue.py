"""Running race/replay images and state must survive both frame-queue modes."""
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
cases = [
    ("grid", dict(PROBE_COURSE="tsukuba", PROBE_GRID="1")),
    ("city", dict(PROBE_COURSE="hongkong", PROBE_CAR="ford_gt_05",
                  PROBE_RIVAL="ford_gt_05", PROBE_FULL_GRID="1")),
    ("long-course", dict(PROBE_COURSE="nurburgring", PROBE_CAR="ford_gt_05")),
    ("wet", dict(PROBE_COURSE="tsukuba_wet", PROBE_CAR="ford_gt_05")),
    ("replay", dict(PROBE_COURSE="tsukuba", PROBE_REPLAY="1")),
]
# Permit an isolated validation-layer rerun without repeating every circuit.
selected = os.environ.get("MOON_VK_QUEUE_CASE")
if selected:
    cases = [case for case in cases if case[0] == selected]
    assert cases, f"unknown queue case: {selected}"
for name, options in cases:
    result = []
    for low_latency in (False, True):
        env = os.environ.copy()
        for key in ("MOON_VK_LOW_LATENCY", "MOON_VK_SPLIT_PRESENT", "MOON_VK_TRANSIENT_TARGETS",
                    "MOON_VK_UNCACHED_DESCRIPTORS", "MOON_VK_STREAM_GEOMETRY", "MOON_REFLECTION_CPU",
                    "PROBE_CAR", "PROBE_RIVAL", "PROBE_FULL_GRID", "PROBE_REPLAY", "PROBE_GRID",
                    "PROBE_LICENSE", "MOON_VK_SKIP_PREPARE"):
            env.pop(key, None)
        env.update(MOON_WINDOW_HIDDEN="1", MOON_GRAPHICS_API="vulkan", PROBE_BENCHMARK="1",
                   PROBE_WIDTH="1920", PROBE_HEIGHT="1080", MOON_VK_CACHE_DIR=str(output.resolve()))
        env.update(options)
        if low_latency or negative:
            env["MOON_VK_LOW_LATENCY"] = "1"
        # The serial reference also leaves static geometry lazy: preparation
        # must change loading work only, never race pixels or simulation.
        if low_latency:
            env["MOON_VK_SKIP_PREPARE"] = "1"
        prefix = output / f"{name}-{'low-latency' if low_latency else 'balanced'}"
        run = subprocess.run([str(probe), str(disc), str(prefix)], env=env,
                             capture_output=True, text=True, timeout=120)
        log = run.stdout + run.stderr
        prefix.with_suffix(".log").write_text(log, encoding="utf-8", newline="")
        assert run.returncode == 0 and "Validation Error" not in log, log
        rotations = int(re.search(r"queue rotations: (\d+)", log)[1])
        assert rotations == (0 if low_latency else 120), "frame queue selection ignored"
        assert "GPU timing:" in log and "frame pacing:" in log
        assert ("Vulkan geometry preparation:" in log) != low_latency
        result.append((read_png(Path(str(prefix) + "-benchmark.png")),
                       prefix.with_suffix(".state").read_bytes()))
        print(name, low_latency, re.search(r"race benchmark: 120 frames.*", log)[0], flush=True)
        print(re.search(r"GPU timing:.*", log)[0], flush=True)
        print(re.search(r"frame pacing:.*", log)[0], flush=True)
    assert result[0][0] == result[1][0], f"{name}: frame queue changed pixels"
    assert len(result[0][1]) == len(result[1][1]) == 0x2000000
    assert result[0][1] == result[1][1], f"{name}: frame queue changed physics state"
print("all queue modes preserve the recorded frames and complete physics state")
