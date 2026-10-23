"""Real launcher: measured labels and values stay in separate columns on both APIs."""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile
from png_diff import read_png
exe, disc, output = (Path(p).resolve() for p in sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
negative = "--negative" in sys.argv[4:]
checked = 0
with tempfile.TemporaryDirectory(prefix="moon-launcher-layout-") as tmp:
    root = Path(tmp)
    (root / "no-mods").mkdir()
    prefs = root / "launcher.ini"
    for api in ("opengl", "vulkan"):
        prefs.write_text(f"disc={disc.as_posix()}\napi={api}\nsaves={root.as_posix()}/saves\ncar=gt40_race_69\ncolor=0\ntheme=0\nscene=laguna\n", encoding="utf-8", newline="")
        config = Path(str(prefs) + ".video.ini")
        config.write_text(f"preset=Enhanced\ngraphics.renderer={api}\nlauncher.guide_seen=1\n", encoding="utf-8", newline="")
        before = config.read_bytes()
        for tab in range(6):
            shot = output / f"{api}-{tab}.png"
            env = dict(os.environ, SDL_AUDIODRIVER="dummy", MOON_WINDOW_HIDDEN="1",
                       MOONSTUDIO_MUTE="1", MOONSTUDIO_LAYOUT_TRACE="1",
                       MOONSTUDIO_OPTIONS=str(tab), MOONSTUDIO_OPTIONS_ROW="0",
                       MOONSTUDIO_FRONT_SHOT=str(shot), MOON_OVERLAY=str(root / "no-mods"))
            run = subprocess.run([str(exe), "--launcher", "--launcher-config", str(prefs)],
                                 env=env, capture_output=True, timeout=180)
            log = (run.stdout + run.stderr).decode("utf-8", "replace")
            shot.with_suffix(".log").write_text(log, encoding="utf-8", newline="")
            assert run.returncode == 0, log[-3000:]
            rows = re.findall(r"STUDIO_ROW (\d+) (\d+) label=([\d.]+) value=([\d.]+) left=([\d.]+) right=([\d.]+) raw=([\d.]+)", log)
            assert rows, "no measured option rows"
            for _, _, label, value, left, right, raw in rows:
                label, value, left, right, raw = map(float, (label, value, left, right, raw))
                if negative:
                    label = right - left  # Deliberate overflow: the column check must reject it.
                assert left + label + value < right - 4, (api, tab, label, value, left, right)
                checked += 1
            w, h, pixels = read_png(shot)
            assert w > 600 and h > 400
            # The option panel's background must still cover the course at its left edge.
            x, y = int(w * .025), int(h * .25)
            assert sum(pixels[y][x][:3]) < 210, (api, tab, pixels[y][x])
            assert config.read_bytes() == before, "preview changed saved options"
print(f"PASS {checked} real option rows fit; both APIs captured without changing preferences")
