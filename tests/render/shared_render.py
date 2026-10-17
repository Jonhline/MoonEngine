"""Check common GL/Vulkan batching against unbatched, uncached rendering."""
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
selected = os.environ.get("MOON_SHARED_CASE")
if selected:
    cases = [case for case in cases if case[0] == selected]
    assert cases, selected
backends = [sys.argv[4]] if len(sys.argv) > 4 and sys.argv[4] in ("opengl", "vulkan") else ["opengl", "vulkan"]
for backend in backends:
    assert backend in ("opengl", "vulkan")
    merged = 0
    for name, options in cases:
        samples = []
        for reference in (False, True):
            env = os.environ.copy()
            for key in list(env):
                if key.startswith(("PROBE_", "MOON_VK_", "MOON_RENDER_")):
                    env.pop(key)
            env.update(MOON_GRAPHICS_API=backend, MOON_WINDOW_HIDDEN="1", PROBE_BENCHMARK="1",
                       PROBE_WIDTH="1920", PROBE_HEIGHT="1080", PROBE_COURSE="tsukuba",
                       PROBE_FRAMES=os.environ.get("MOON_SHARED_FRAMES", "120"),
                       PROBE_SWEEP_CAMERA="1", MOON_VK_CACHE_DIR=str(output.resolve()))
            env.update(options)
            if reference or negative:
                env["MOON_RENDER_REFERENCE"] = "1"
            prefix = output / f"{backend}-{name}-{'reference' if reference else 'optimized'}"
            run = subprocess.run([str(probe), str(disc), str(prefix)], env=env,
                                 capture_output=True, text=True, timeout=240)
            log = run.stdout + run.stderr
            prefix.with_suffix(".log").write_text(log, encoding="utf-8", newline="")
            assert run.returncode == 0 and "Validation Error" not in log, log
            work = tuple(map(int, re.search(r"shared work: draws=(\d+) triangles=(\d+) merged=(\d+) culled=(\d+)", log).groups()))
            material = tuple(map(int, re.search(r"shared materials: textures=(\d+) texture-hits=(\d+) states=(\d+) state-hits=(\d+)", log).groups()))
            samples.append((work, material, read_png(Path(str(prefix) + "-benchmark.png")),
                            prefix.with_suffix(".state").read_bytes()))
            print(backend, name, reference, re.search(r"race benchmark: \d+ frames.*", log)[0], flush=True)
            print(re.search(r"shared work:.*", log)[0], flush=True)
            print(re.search(r"shared materials:.*", log)[0], flush=True)
        optimized, reference = samples
        assert optimized[2] == reference[2], "shared optimization changed RGB"
        assert len(optimized[3]) == len(reference[3]) == 0x2000000
        assert optimized[3] == reference[3], "shared optimization changed simulation"
        assert optimized[0][1] == reference[0][1], "triangle count changed"
        assert optimized[0][3] == reference[0][3], "visibility decisions changed"
        assert optimized[0][0] <= reference[0][0], "additional draws were introduced"
        assert reference[0][0] - optimized[0][0] == optimized[0][2], "merged draw accounting differs"
        merged += optimized[0][2]
        assert optimized[1][0] < reference[1][0] and optimized[1][1] > 0, "texture updates were not reused"
        assert optimized[1][2] < reference[1][2] and optimized[1][3] > 0, "material state was not reused"
    if not selected or selected == "race":
        assert merged > 0, "opaque groups were not merged"
print("tested backends preserve RGB, triangles, visibility and 32 MiB state with less API work")
