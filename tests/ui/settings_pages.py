"""Open every native settings page with disposable preferences and no focused Options tile."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
exe, disc, output = map(Path, sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
negative = "--negative" in sys.argv[4:]
with tempfile.TemporaryDirectory(dir=output) as directory:
    config = Path(directory) / "options.ini"
    config.write_text("preset=Classic\ngraphics.renderer=opengl\n", encoding="utf-8")
    for i, title in enumerate(("Screen Settings", "Graphics", "Light & Shadow", "Controls", "Music", "Presets")):
        shot = output / f"page-{i}.png"
        result = subprocess.run([str(exe.resolve()), str(disc.resolve()), "--video-settings",
            "--config-file", str(config), "--save-dir", directory, "--shot", str(shot)],
            env=dict(os.environ, SDL_AUDIODRIVER="dummy", MOON_WINDOW_HIDDEN="1", MOON_SETTINGS_PAGE=str(i)),
            capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=45)
        log = result.stdout + result.stderr
        assert "choice PartPreviewFades/OptionMenu" not in log
        (output / f"page-{i}.log").write_text(log, encoding="utf-8")
        assert result.returncode == 0, (result.returncode, log)
        assert shot.stat().st_size > 1000
        if negative and i == 2:
            title = "Shadows"
        assert f"video settings: page {title}" in log
        if i == 2:
            assert "choice Lighting/OptionMenu" in log, log
            assert "choice Daylight/OptionMenu" in log, log
            assert "choice CarLamps/OptionMenu" in log, log
            assert "choice ShadowFilter/OptionMenu" in log, log
print("PASS all six settings pages and the four intended Light & Shadow controls")
