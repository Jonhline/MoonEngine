"""OPTIONS shows the first-drive guide's comparison: QUALITY's focused graphics option is the
course's frozen instant at the original value and at the current one, in the guide's right
column, and drawing it writes no preference. --negative opens VIDEO (one column, no guide),
so the same checks must fail."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from png_diff import read_png

exe, disc, output = (Path(p).resolve() for p in sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
negative = "--negative" in sys.argv[4:]
tab, row = ("1", "0") if negative else ("5", "8")  # QUALITY row 8: Lighting

with tempfile.TemporaryDirectory(prefix="moon-options-guide-") as directory:
    root = Path(directory)
    prefs = root / "launcher.ini"
    prefs.write_text(f"disc={disc.as_posix()}\napi=opengl\nsaves={root.as_posix()}/saves\n"
                     "car=ka_01\ncolor=1\ntheme=0\n", encoding="utf-8")
    config = Path(str(prefs) + ".video.ini")
    config.write_text("preset=Enhanced\ngraphics.renderer=opengl\ngraphics.modern_lighting=0\n"
                      "launcher.guide_seen=1\n", encoding="utf-8")
    before = config.read_bytes()
    shot = output / ("video.png" if negative else "quality-lighting.png")
    env = dict(os.environ, MOON_WINDOW_HIDDEN="1", SDL_AUDIODRIVER="dummy", MOONSTUDIO_MUTE="1",
               MOONSTUDIO_OPTIONS=tab, MOONSTUDIO_OPTIONS_ROW=row, MOONSTUDIO_FRONT_SHOT=str(shot))
    result = subprocess.run([str(exe), "--launcher", "--launcher-config", str(prefs)],
                            capture_output=True, env=env, timeout=300)
    log = (result.stdout + result.stderr).decode("utf-8", "replace")
    (output / ("video.log" if negative else "quality.log")).write_text(log, encoding="utf-8")
    assert result.returncode == 0, log
    assert config.read_bytes() == before, "the comparison wrote preferences"

assert "studio options: comparison Lighting" in log, log
w, h, rows = read_png(shot)
# the right column's picture box (panel x 302..612 of 640, under the tabs), in the shot's pixels
x0, x1, y0, y1 = int(w * 305 / 640), int(w * 610 / 640), int(h * 130 / 480), int(h * 250 / 480)
colours = {p for r in rows[y0:y1] for p in r[x0:x1]}
assert len(colours) > 400, ("no course in the comparison", len(colours))
print("options guide: comparison drawn,", len(colours), "colours")
