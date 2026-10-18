"""PC quality through authored widgets on both backends; no player files."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

exe, disc, output = map(Path, sys.argv[1:4])
negative = "--negative" in sys.argv[4:]
output.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=output) as temporary:
    config = Path(temporary) / "options.ini"
    for backend in ("opengl", "vulkan"):
        config.write_text(f"preset=Classic\ngraphics.renderer={backend}\n", encoding="utf-8", newline="")
        def run(name, keys):
            shot = output / f"{backend}-{name}.png"
            command = [str(exe.resolve()), str(disc.resolve()), "option", "ScreenRoot",
                       "--video-settings", "--config-file", str(config), "--shot", str(shot),
                       "--input", ",".join(keys)]
            env = dict(os.environ, MOON_WINDOW_HIDDEN="1")
            env.pop("MOON_GRAPHICS_API", None)
            result = subprocess.run(command, env=env, capture_output=True, text=True,
                                    encoding="utf-8", errors="replace", timeout=45)
            log = result.stdout + result.stderr
            shot.with_suffix(".log").write_text(log, encoding="utf-8", newline="")
            assert result.returncode == 0 and "call of nil" not in log, log
            return log
        def change(row, directions):
            return ["r1"] + ["down"] * row + ["accept"] + directions + ["accept"]
        def commit(row):
            return ["down"] * (9 - row) + ["accept"]
        run("ssaa", change(0, ["down"] * 3) + commit(0))
        data = config.read_text(encoding="utf-8")
        if negative:
            data = data.replace("graphics.antialiasing=3", "graphics.antialiasing=0")
        assert "graphics.antialiasing=3" in data and "modified=1" in data, data
        before = config.read_bytes()
        run("cancel", change(0, ["up", "up"]) + ["back"])
        assert config.read_bytes() == before
        run("scale", change(1, ["up"]) + commit(1))
        data = config.read_text(encoding="utf-8")
        assert "graphics.render_scale=1" in data and "graphics.antialiasing=0" in data, data
        run("texture", change(2, ["down"]) + commit(2))
        data = config.read_text(encoding="utf-8")
        assert "graphics.texture_smoothing=1" in data, data
        log = run("reload", ["r1"])
        assert "choice AntiAliasing/OptionMenu index=0" in log, log
        assert "choice RenderScale/OptionMenu index=1" in log, log
        assert "choice TextureSmoothing/OptionMenu index=1" in log, log
print("PC quality: both renderers, SSAA, reduced resolution, smoothing, Cancel and persistence passed")
