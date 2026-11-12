"""The Options column's real prints: INTERFACE > Desktop Input shows the game's own menu printed
with the option off and on (the launcher runs itself hidden for each, launcher_prints.inc),
kept in launcher-prints/ next to launcher.ini; a second run reads them from there instead of
printing again. --negative focuses Map Edge Pan, which has no print (and no picture), so the
same checks must fail."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from png_diff import read_png

exe, disc, output = (Path(p).resolve() for p in sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
negative = "--negative" in sys.argv[4:]
row = "1" if negative else "0"

with tempfile.TemporaryDirectory(prefix="moon-launcher-prints-") as directory:
    root = Path(directory)
    prefs = root / "launcher.ini"
    prefs.write_text(f"disc={disc.as_posix()}\napi=opengl\nsaves={root.as_posix()}/saves\n"
                     "car=ka_01\ncolor=1\ntheme=0\n", encoding="utf-8")
    Path(str(prefs) + ".video.ini").write_text("preset=Enhanced\ngraphics.renderer=opengl\n"
                                               "launcher.guide_seen=1\n", encoding="utf-8")
    logs = []
    for run in range(2):
        shot = output / f"prints-{run}.png"
        env = dict(os.environ, MOON_WINDOW_HIDDEN="1", SDL_AUDIODRIVER="dummy",
                   MOONSTUDIO_MUTE="1", MOONSTUDIO_OPTIONS="2", MOONSTUDIO_OPTIONS_ROW=row,
                   MOONSTUDIO_FRONT_SHOT=str(shot))
        env.pop("MOONSTUDIO_SCENE", None)
        result = subprocess.run([str(exe), "--launcher", "--launcher-config", str(prefs)],
                                capture_output=True, env=env, timeout=600)
        log = (result.stdout + result.stderr).decode("utf-8", "replace")
        (output / f"prints-{run}.log").write_text(log, encoding="utf-8")
        assert result.returncode == 0, log
        logs.append(log)
        # the column's picture box: two prints of the game's menu, not the panel's dark ground
        w, h, rows = read_png(shot)
        x0, x1, y0, y1 = int(w * .48), int(w * .94), int(h * .3), int(h * .5)
        colours = {p for r in rows[y0:y1] for p in r[x0:x1]}
        assert len(colours) > 2000, ("no print in the column", run, len(colours))
    kept = root / "launcher-prints"
    assert (kept / "desktop-input-0.png").is_file() and (kept / "desktop-input-1.png").is_file(), \
        sorted(p.name for p in kept.iterdir()) if kept.exists() else "no launcher-prints"
    assert (kept / "version").is_file()

assert "studio: print Desktop Input (OFF) taken" in logs[0], logs[0]
assert "studio: print Desktop Input (ON) taken" in logs[0], logs[0]
assert "studio: print Desktop Input (OFF) from the cache" in logs[1], logs[1]
assert "studio: print Desktop Input (ON) from the cache" in logs[1], logs[1]
assert "Desktop Input (OFF) taken" not in logs[1], logs[1]
print("launcher prints: taken once, then from the cache,", len(colours), "colours")
