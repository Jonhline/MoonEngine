"""First-drive guide: real input through every page, per-option comparisons, Skip writes
nothing, Done writes exactly the choices through the Options' own setters."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from png_diff import read_png

exe, disc, output = (Path(p).resolve() for p in sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
backend = "vulkan" if "--vulkan" in sys.argv[4:] else "opengl"

# Escape path: preset page (to Classic, full screen, flip), graphics rows, Lighting to
# Modern on top of Classic, full screen and flip, then Escape twice (full screen, guide).
BROWSE = ("@0-welcome RETURN @1-preset UP LEFT @1-preset-classic SPACE @1-full TAB @1-full-flip "
          "SPACE DOWN RETURN UP @2-aa UP @2-texture UP UP UP @2-hdr UP UP @2-shadow-maps "
          "UP UP @2-reflection UP UP @2-lighting RIGHT @2-lighting-modern "
          "SPACE @2-full TAB @2-full-original ESCAPE ESCAPE")
# Done path: texture filtering 3 -> 2, aspect 4:3 -> 16:9, then Done on the last page.
COMMIT = ("RETURN RETURN UP UP LEFT @c-texture DOWN DOWN RETURN @3-menus UP RIGHT @3-aspect "
          "DOWN RETURN @4-ready RETURN")


def run(name, script):
    with tempfile.TemporaryDirectory(prefix="moon-guide-") as directory:
        root = Path(directory)
        prefs = root / "launcher.ini"
        prefs.write_text(f"disc={disc.as_posix()}\napi={backend}\nsaves={root.as_posix()}/saves\n"
                         "car=ka_01\ncolor=1\ntheme=0\n", encoding="utf-8")
        config = Path(str(prefs) + ".video.ini")
        config.write_text(f"preset=Enhanced\ngraphics.renderer={backend}\n"
                          "graphics.material_shading=1\ngraphics.reflection_quality=1\n"
                          "graphics.texture_filtering=3\nlauncher.guide_seen=0\n"
                          "launcher.guided_intro=1\n", encoding="utf-8")
        before = config.read_bytes()
        env = dict(os.environ, MOON_WINDOW_HIDDEN="1", SDL_AUDIODRIVER="dummy",
                   MOONSTUDIO_GUIDE_SHOT=str(output / "guide"), MOONSTUDIO_GUIDE_SCRIPT=script,
                   MOONSTUDIO_FRONT_SHOT=str(output / f"home-{name}.png"), MOONSTUDIO_MUTE="1")
        result = subprocess.run([str(exe), "--launcher", "--launcher-config", str(prefs)],
                                capture_output=True, env=env, timeout=240)
        log = (result.stdout + result.stderr).decode("utf-8", "replace")
        (output / f"guide-{name}.log").write_text(log, encoding="utf-8")
        assert result.returncode == 0, log
        assert log.count("studio guide: disc icon ") == 5 and " missing" not in log, "disc icons"
        assert "course: world=" in log, "the course was not installed"
        assert not list((root / "saves").glob("slot*.ogt4")), "the guide created a player"
        return log, before, config.read_bytes()


def scene(name):
    w, h, rows = read_png(output / f"guide-{name}.png")
    assert w >= 640 and h >= 448, (name, w, h)
    assert len({p for row in rows[h // 3:2 * h // 3] for p in row}) > 200, (name, "empty scene")
    return rows


def differs(a, b, y0, y1, x0, x1):
    ra, rb = scene(a), scene(b)
    return sum(p != q for r, s in zip(ra[y0:y1], rb[y0:y1]) for p, q in zip(r[x0:x1], s[x0:x1]))


log, before, after = run("browse", BROWSE)
assert "studio guide: dismissed, graphics restored; preferences unchanged" in log, log
assert "unexpected preference mutation" not in log, log
assert after == before, "Skip/Escape wrote preferences"
for shot in ("0-welcome", "1-preset", "1-preset-classic", "1-full", "1-full-flip", "2-aa",
             "2-texture", "2-hdr", "2-shadow-maps", "2-reflection", "2-lighting",
             "2-lighting-modern", "2-full", "2-full-original"):
    scene(shot)
# full screen: Tab really switches the scene between the two looks (crop: below the labels)
assert differs("1-full", "1-full-flip", 300, 480, 250, 730) > 100, "preset flip unchanged"
assert differs("2-full", "2-full-original", 300, 480, 250, 730) > 100, "lighting flip unchanged"

log, before, after = run("commit", COMMIT)
text = after.decode("utf-8")
assert "studio guide: done, 2 change(s) saved" in log, log
assert "graphics.texture_filtering=2" in text, text
assert "graphics.aspect_ratio=1" in text, text
assert "graphics.material_shading=1" in text, "an untouched option changed"
for shot in ("c-texture", "3-menus", "3-aspect", "4-ready"):
    scene(shot)
print("PASS first-drive guide: pages, per-option comparisons, full-screen flips, "
      "Skip writes nothing, Done writes the two choices")
