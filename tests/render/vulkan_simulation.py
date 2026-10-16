"""Compare resident and streamed geometry during a running six-car race."""
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
results = []
for stream in (False, True):
    env = os.environ.copy()
    for key in ("MOON_VK_STREAM_GEOMETRY", "MOON_VK_SYNC", "MOON_REFLECTION_CPU",
                "MOON_RACE_SPEED", "MOON_RACE_SNAPSHOT", "MOON_RACE_PAUSE",
                "PROBE_LICENSE", "PROBE_CAR", "PROBE_RIVAL"):
        env.pop(key, None)
    env.update(MOON_WINDOW_HIDDEN="1", MOON_GRAPHICS_API="vulkan", PROBE_BENCHMARK="1")
    if stream:
        env["MOON_VK_STREAM_GEOMETRY"] = "1"
    prefix = output / ("streamed" if stream else "resident")
    run = subprocess.run([str(probe), str(disc), str(prefix)],
                         env=env, capture_output=True, text=True, timeout=90)
    log = run.stdout + run.stderr
    prefix.with_suffix(".log").write_text(log, encoding="utf-8", newline="")
    assert run.returncode == 0 and "Validation Error" not in log, log
    match = re.search(r"uploaded=(\d+) state=([0-9a-f]+)", log)
    assert match, log
    uploaded, checksum = int(match[1]), int(match[2], 16)
    image = Path(str(prefix) + "-benchmark.png")
    results.append((uploaded, checksum, read_png(image), prefix.with_suffix(".state").read_bytes()))
    print(re.search(r"race benchmark: 120 frames.*", log)[0])
    print("streamed", stream, "uploaded", uploaded, "physics CRC", hex(checksum))
assert results[0][0] < results[1][0], "static meshes are being uploaded repeatedly"
if negative:
    corrupt = bytearray(results[1][3])
    corrupt[0] ^= 1
    results[1] = (*results[1][:3], bytes(corrupt))
assert len(results[0][3]) == len(results[1][3]) == 0x2000000
assert results[0][3] == results[1][3], "geometry path changed the simulation memory"
assert results[0][1] == results[1][1], "geometry path changed the simulation"
assert results[0][2] == results[1][2], "resident geometry changed rendered pixels"
