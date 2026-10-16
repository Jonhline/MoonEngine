"""Compare merged submission/descriptor reuse against the retained PC reference."""
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
targets_negative = "--targets-negative" in sys.argv[4:]
for width, height in ((1280, 896), (1920, 1080)):
    samples = []
    for reference in (False, True):
        env = os.environ.copy()
        for key in ("MOON_VK_SPLIT_PRESENT", "MOON_VK_UNCACHED_DESCRIPTORS", "MOON_VK_TRANSIENT_TARGETS",
                    "MOON_VK_STREAM_GEOMETRY", "MOON_REFLECTION_CPU", "MOON_VK_SYNC",
                    "MOON_RACE_SPEED", "MOON_RACE_SNAPSHOT", "MOON_RACE_PAUSE",
                    "PROBE_LICENSE", "PROBE_CAR", "PROBE_RIVAL"):
            env.pop(key, None)
        env.update(MOON_WINDOW_HIDDEN="1", MOON_GRAPHICS_API="vulkan", PROBE_BENCHMARK="1",
                   PROBE_WIDTH=str(width), PROBE_HEIGHT=str(height))
        if reference or negative:
            env["MOON_VK_SPLIT_PRESENT"] = "1"
        if targets_negative:
            env["MOON_VK_TRANSIENT_TARGETS"] = "1"
        if reference:
            env["MOON_VK_UNCACHED_DESCRIPTORS"] = "1"
            env["MOON_VK_TRANSIENT_TARGETS"] = "1"
        prefix = output / f"{width}-{'reference' if reference else 'optimized'}"
        run = subprocess.run([str(probe), str(disc), str(prefix)], env=env,
                             capture_output=True, text=True, timeout=90)
        log = run.stdout + run.stderr
        prefix.with_suffix(".log").write_text(log, encoding="utf-8", newline="")
        assert run.returncode == 0 and "Validation Error" not in log, log
        match = re.search(r"descriptors=(\d+) draws=(\d+) submissions=(\d+)", log)
        assert match, log
        descriptors, draws, submissions = map(int, match.groups())
        samples.append((descriptors, draws, submissions,
                        read_png(Path(str(prefix) + "-benchmark.png")),
                        prefix.with_suffix(".state").read_bytes(),
                        int(re.search(r"framebuffers=(\d+)", log)[1])))
        print(width, reference, re.search(r"race benchmark: 120 frames.*", log)[0], flush=True)
    optimized, reference = samples
    assert optimized[2] == 120 and reference[2] == 240, "redundant render/present submissions"
    assert optimized[0] < reference[0] and reference[0] == reference[1], "descriptor reuse lost"
    assert optimized[5] < reference[5], "framebuffers are being rebuilt every pass"
    assert optimized[1] == reference[1], "draws were removed"
    assert len(optimized[4]) == len(reference[4]) == 0x2000000
    assert optimized[4] == reference[4], "renderer changed physics memory"
    assert optimized[3] == reference[3], "renderer changed frame pixels"
    print(width, "unchanged pixels/state; submissions", optimized[2], reference[2],
          "descriptor updates", optimized[0], reference[0], flush=True)
