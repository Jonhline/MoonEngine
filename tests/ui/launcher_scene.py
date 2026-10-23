"""The launcher's Scene: launcher.ini's scene=tsukuba puts the course behind the car instead of
the room; the picture is taken once ("studio: scene tsukuba") and fills the room's place.
--negative leaves the room (scene empty), so the same checks must fail."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from png_diff import read_png

exe, disc, output = (Path(p).resolve() for p in sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
negative = "--negative" in sys.argv[4:]
scene = "" if negative else "tsukuba"

with tempfile.TemporaryDirectory(prefix="moon-launcher-scene-") as directory:
    root = Path(directory)
    prefs = root / "launcher.ini"
    prefs.write_text(f"disc={disc.as_posix()}\napi=opengl\nsaves={root.as_posix()}/saves\n"
                     f"car=ka_01\ncolor=1\ntheme=0\nscene={scene}\n", encoding="utf-8")
    Path(str(prefs) + ".video.ini").write_text("preset=Enhanced\ngraphics.renderer=opengl\n"
                                               "launcher.guide_seen=1\n", encoding="utf-8")
    shot = output / "scene.png"
    env = dict(os.environ, MOON_WINDOW_HIDDEN="1", SDL_AUDIODRIVER="dummy", MOONSTUDIO_MUTE="1",
               MOONSTUDIO_FRONT_SHOT=str(shot))
    env.pop("MOONSTUDIO_SCENE", None)
    result = subprocess.run([str(exe), "--launcher", "--launcher-config", str(prefs)],
                            capture_output=True, env=env, timeout=300)
    log = (result.stdout + result.stderr).decode("utf-8", "replace")
    (output / "scene.log").write_text(log, encoding="utf-8")
    assert result.returncode == 0, log

assert "studio: scene tsukuba" in log, log
w, h, rows = read_png(shot)
# the middle of the window, left of the car, under the corner text: the course, not the sweep
x0, x1, y0, y1 = int(w * .05), int(w * .55), int(h * .3), int(h * .6)
colours = {p for r in rows[y0:y1] for p in r[x0:x1]}
assert len(colours) > 600, ("the room, not the course", len(colours))
print("launcher scene: the course behind the car,", len(colours), "colours")
