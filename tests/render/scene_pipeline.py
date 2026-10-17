"""Compare each optional PC scene optimization against its retained path."""
import os
from pathlib import Path
import re
import subprocess
import sys
from PIL import Image, ImageChops

probe, disc, output = map(Path, sys.argv[1:4])
backend = sys.argv[4]
negative = "--negative" in sys.argv[5:]
output.mkdir(parents=True, exist_ok=True)
frames = os.environ.get("MOON_SCENE_FRAMES", "120")
cases = [("race", {}), ("wet", {"PROBE_COURSE": "tsukuba_wet"}),
         ("replay", {"PROBE_REPLAY": "1"})]
selected = os.environ.get("MOON_SCENE_CASE")
if selected:
    cases = [case for case in cases if case[0] == selected]
    assert cases
for case, options in cases:
    samples = []
    for mode in ("reference", "spatial", "resident", "combined"):
        env = os.environ.copy()
        for key in list(env):
            if key.startswith(("PROBE_", "MOON_VK_", "MOON_RENDER_")) or key in ("MOON_REFLECTION_CPU", "MOON_FLAT_VISIBILITY"):
                env.pop(key)
        env.update(MOON_GRAPHICS_API=backend, MOON_WINDOW_HIDDEN="1", PROBE_BENCHMARK="1",
                   PROBE_WIDTH="1920", PROBE_HEIGHT="1080", PROBE_FRAMES=frames,
                   PROBE_SWEEP_CAMERA="1", PROBE_COURSE="tsukuba",
                   MOON_VK_CACHE_DIR=str(output.resolve()))
        env.update(options)
        if mode in ("reference", "resident") or negative:
            env["MOON_FLAT_VISIBILITY"] = "1"
        if mode in ("reference", "spatial") or negative:
            env["MOON_REFLECTION_CPU"] = "1"
        prefix = output / f"{case}-{mode}"
        run = subprocess.run([str(probe), str(disc), str(prefix)], env=env,
                             capture_output=True, text=True, timeout=240)
        log = run.stdout + run.stderr
        prefix.with_suffix(".log").write_text(log, encoding="utf-8", newline="")
        assert run.returncode == 0 and "Validation Error" not in log and "SYNC-HAZARD" not in log, log
        work = tuple(map(int, re.search(r"shared work: draws=(\d+) triangles=(\d+) merged=(\d+) culled=(\d+)", log).groups()))
        visibility = tuple(map(int, re.search(r"visibility work: tests=(\d+) spatial-rejected=(\d+) readbacks=(\d+)", log).groups()))
        image = Image.open(Path(str(prefix) + "-benchmark.png")).convert("RGB")
        samples.append((work, visibility, image, prefix.with_suffix(".state").read_bytes()))
        print(backend, case, mode, re.search(r"race benchmark: \d+ frames.*", log)[0], flush=True)
        print(re.search(r"visibility work:.*", log)[0], flush=True)
    reference, spatial, resident, combined = samples
    for sample in (spatial, resident, combined):
        assert sample[0] == reference[0], "triangle, draw or original visibility decisions changed"
        assert len(sample[3]) == len(reference[3]) == 0x2000000 and sample[3] == reference[3], "simulation changed"
    assert spatial[2].tobytes() == reference[2].tobytes(), "spatial hierarchy changed pixels"
    assert combined[2].tobytes() == resident[2].tobytes(), "hierarchy changed GPU-remapped pixels"
    # CPU and GPU use the same bilinear remap, but float/UNORM rounding can differ.
    error = ImageChops.difference(resident[2], reference[2])
    maximum = max(hi for lo, hi in error.getextrema())
    assert maximum <= 2, f"resident remap changed pixels beyond 2/255: {maximum}"
    assert spatial[1][0] < reference[1][0] and spatial[1][1] > 0, "spatial work was not reduced"
    assert combined[1][0] == spatial[1][0], "resident remap changed visibility work"
    assert reference[1][2] >= 6 * int(frames) and spatial[1][2] == reference[1][2], "CPU reference lost its six face readbacks"
    assert resident[1][2] == combined[1][2] == 0, "reflection readbacks remain"
    print("unchanged simulation/geometry; exact hierarchy RGB; resident RGB max error", maximum, flush=True)

# Switching to Classic after GPU allocation must stop using the retained GPU image.
if not negative and (not selected or selected == "race"):
    env = os.environ.copy()
    for key in list(env):
        if key.startswith(("PROBE_", "MOON_VK_", "MOON_RENDER_")) or key in ("MOON_REFLECTION_CPU", "MOON_FLAT_VISIBILITY"):
            env.pop(key)
    env.update(MOON_GRAPHICS_API=backend, MOON_WINDOW_HIDDEN="1", PROBE_BENCHMARK="1",
               PROBE_WIDTH="1920", PROBE_HEIGHT="1080", PROBE_FRAMES=frames,
               PROBE_SWEEP_CAMERA="1", PROBE_SCENE_MODE="3", PROBE_SCENE_SWITCH="1",
               PROBE_COURSE="tsukuba", MOON_VK_CACHE_DIR=str(output.resolve()))
    prefix = output / "race-return-classic"
    run = subprocess.run([str(probe), str(disc), str(prefix)], env=env,
                         capture_output=True, text=True, timeout=240)
    log = run.stdout + run.stderr
    prefix.with_suffix(".log").write_text(log, encoding="utf-8", newline="")
    assert run.returncode == 0 and "Validation Error" not in log and "SYNC-HAZARD" not in log, log
    reference = output / "race-reference"
    assert prefix.with_suffix(".state").read_bytes() == reference.with_suffix(".state").read_bytes()
    with Image.open(Path(str(prefix) + "-benchmark.png")) as image, Image.open(Path(str(reference) + "-benchmark.png")) as original:
        assert image.convert("RGB").tobytes() == original.convert("RGB").tobytes(), "Classic reused stale GPU reflection"
    print("GPU allocation followed by Classic restores exact CPU reference RGB/state", flush=True)
