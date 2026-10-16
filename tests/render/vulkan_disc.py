"""Compare disc-backed output; all assets/captures stay in the local build tree."""
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "ui"))
from png_diff import read_png

probe, player, disc, output = map(Path, sys.argv[1:5])
negative = "--negative" in sys.argv[5:]
output.mkdir(parents=True, exist_ok=True)
for scene in ("tsukuba", "showroom", "menu"):
    frames = []
    for backend in ("opengl", "vulkan"):
        env = os.environ.copy()
        for key in ("MOON_COURSE", "MOON_SHADOW_CPU", "MOON_UPSCALE",
                    "MOON_VK_SYNC", "MOON_RENDERER", "MOON_PROBE_APPEAR"):
            env.pop(key, None)
        env["MOON_WINDOW_HIDDEN"] = "1"
        env["MOON_GRAPHICS_API"] = backend
        path = output / f"{scene}-{backend}.png"
        if scene == "menu":
            command = [str(player), str(disc), "option", "ScreenRoot", "--renderer", backend,
                       "--no-config", "--save-dir", str(output / "isolated-save"),
                       "--window", "640x480", "--shot", str(path), "--shot-delay", "2"]
        else:
            command = [str(probe), str(disc), "ka_01", str(path)]
            if scene == "tsukuba":
                env["MOON_COURSE"] = scene
            else:
                # Restart the entrance so asynchronous load time does not
                # choose different samples of the studio's light animation.
                env["MOON_PROBE_APPEAR"] = "1"
                command += ["0", "3"]
        result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=90)
        log = result.stdout + result.stderr
        (output / f"{scene}-{backend}.log").write_text(log, encoding="utf-8", newline="")
        assert result.returncode == 0 and "Vulkan command failed" not in log, (scene, backend, log)
        assert "Validation Error" not in log, log
        width, height, rows = read_png(path)
        frames.append([channel for row in rows for pixel in row for channel in pixel])
    assert len(frames[0]) == len(frames[1]) == width * height * 3
    if negative:
        frames[1] = [255 - value for value in frames[1]]
    differences = [abs(a - b) for a, b in zip(*frames)]
    mean = sum(differences) / len(differences)
    large = sum(value > 32 for value in differences) / len(differences)
    print(f"{scene}: mean {mean:.6f}/255; channels >32: {large:.6%}")
    # Host-backend raster comparison, not PS2 certification. Allow subpixel
    # edge/filter rounding, but reject missing layers or broad colour drift.
    assert mean <= 1 and large <= 0.001, (scene, mean, large)
