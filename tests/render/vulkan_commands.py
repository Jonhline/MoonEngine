"""Long race/replay command-elision comparison; no machine-specific FPS gate."""
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
cases = [("race", {}), ("wet", {"PROBE_COURSE": "tsukuba_wet"}),
         ("replay", {"PROBE_REPLAY": "1"})]
selected = os.environ.get("MOON_COMMAND_CASE")
if selected:
    cases = [case for case in cases if case[0] == selected]
    assert cases, selected
frames = os.environ.get("MOON_COMMAND_FRAMES", "1200")
for name, options in cases:
    samples = []
    for reference in (False, True):
        env = os.environ.copy()
        for key in ("MOON_VK_REPEAT_COMMANDS", "MOON_VK_PROFILE_DRAWS", "MOON_VK_SKIP_VARIANTS",
                    "MOON_VK_SPLIT_PRESENT", "MOON_VK_UNCACHED_DESCRIPTORS",
                    "MOON_VK_TRANSIENT_TARGETS", "MOON_VK_STREAM_GEOMETRY", "MOON_REFLECTION_CPU",
                    "PROBE_REPLAY", "PROBE_GRID", "PROBE_LICENSE", "PROBE_CAR", "PROBE_RIVAL",
                    "PROBE_FULL_GRID"):
            env.pop(key, None)
        env.update(MOON_GRAPHICS_API="vulkan", MOON_WINDOW_HIDDEN="1", PROBE_BENCHMARK="1",
                   PROBE_WIDTH="1920", PROBE_HEIGHT="1080", PROBE_FRAMES=frames,
                   PROBE_COURSE="tsukuba", PROBE_SWEEP_CAMERA="1",
                   MOON_VK_CACHE_DIR=str(output.resolve()))
        env.update(options)
        if reference or negative:
            env["MOON_VK_REPEAT_COMMANDS"] = "1"
        prefix = output / f"{name}-{'reference' if reference else 'cached'}"
        run = subprocess.run([str(probe), str(disc), str(prefix)], env=env,
                             capture_output=True, text=True, timeout=240)
        log = run.stdout + run.stderr
        prefix.with_suffix(".log").write_text(log, encoding="utf-8", newline="")
        assert run.returncode == 0 and "Validation Error" not in log, log
        commands = tuple(map(int, re.search(r"draw commands: pipeline=(\d+) dynamic=(\d+) vertex=(\d+)", log).groups()))
        draws = int(re.search(r"descriptors=\d+ draws=(\d+)", log)[1])
        samples.append((commands, draws, read_png(Path(str(prefix) + "-benchmark.png")),
                        prefix.with_suffix(".state").read_bytes()))
        print(name, reference, re.search(r"race benchmark: \d+ frames.*", log)[0], flush=True)
        print(re.search(r"frame pacing:.*", log)[0], flush=True)
        print(re.search(r"draw commands:.*", log)[0], flush=True)
    cached, reference = samples
    assert cached[0][0] < reference[0][0], "pipeline binds repeated"
    assert cached[0][1] < reference[0][1], "dynamic state repeated"
    assert cached[0][2] < reference[0][2], "vertex bindings repeated"
    assert cached[1] == reference[1], "draws were removed"
    assert cached[2] == reference[2], "command reuse changed pixels"
    assert len(cached[3]) == len(reference[3]) == 0x2000000
    assert cached[3] == reference[3], "command reuse changed simulation"
print("long runs preserve all draws, RGB and 32 MiB state while issuing fewer commands")
