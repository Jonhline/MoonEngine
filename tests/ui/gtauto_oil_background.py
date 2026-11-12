"""GT Auto oil background: real page entry, matched GL/Vulkan crops under both presets."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from png_diff import read_png

exe, disc, output = (Path(p).resolve() for p in sys.argv[1:4])
negative = "--negative" in sys.argv[4:]
output.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="moon-oil-background-") as temp:
    root = Path(temp)
    for preset in ("Classic", "Enhanced"):
        captures = []
        for api in ("opengl", "vulkan"):
            name = preset.lower() + "-" + api
            config = root / (name + ".ini")
            config.write_text(f"preset={preset}\ngraphics.renderer={api}\n"
                              "graphics.width=1920\ngraphics.height=1080\n"
                              f"graphics.aspect_ratio=1\ngraphics.texture_smoothing={int(preset == 'Enhanced')}\n", encoding="utf-8", newline="")
            shot = output / (name + ".png")
            env = dict(os.environ, SDL_AUDIODRIVER="dummy", MOON_WINDOW_HIDDEN="1",
                       MOON_TEST_CAR="cappuccino_91", MOON_TEST_MONEY="10000")
            run = subprocess.run([str(exe), str(disc), "gtmode", "GTAutoRoot",
                                  "--config-file", str(config), "--save-dir", str(root / name),
                                  "--input", "right,accept", "--input-interval", "0.1",
                                  "--shot", str(shot), "--shot-delay", "1"],
                                 env=env, capture_output=True, timeout=150)
            log = (run.stdout + run.stderr).decode("utf-8", "replace")
            shot.with_suffix(".log").write_text(log, encoding="utf-8", newline="")
            assert run.returncode == 0, log[-2000:]
            assert "gtmode/OilChangeRoot focus=Change" in log, "did not enter oil page"
            w, h, rows = read_png(shot)
            assert (w, h) == (1920, 1080), (w, h)
            # Authored heading/description and textured floor, outside the moving car/crew.
            coords = [(x, y) for y in range(150, 320, 3) for x in range(150, 700, 3)]
            crop = [rows[y][x][:3] for x, y in coords]
            if negative and api == "vulkan":
                crop = [rows[150][x][:3] for x, y in coords]  # Repeated row / vertical bands.
            captures.append(crop)
            assert len(set(crop)) > 100, (name, "missing background detail")
        errors = [abs(a[c] - b[c]) for a, b in zip(*captures) for c in range(3)]
        assert max(errors) <= 2, (preset, "GL/Vulkan background differs", max(errors))
        print(preset, "oil background agrees in both APIs", flush=True)
