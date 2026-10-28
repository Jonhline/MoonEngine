"""Options > PC Settings: each tile opens its own page; back returns to that tile.
Disposable options file; no player data."""
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
    config.write_text("graphics.renderer=opengl\n", encoding="utf-8", newline="")
    env = dict(os.environ, MOON_WINDOW_HIDDEN="1", SDL_AUDIODRIVER="dummy",
               MOON_OVERLAY=str(Path(temporary) / "no-mods"))
    env.pop("MOON_SETTINGS_PAGE", None)

    def run(name, keys, interval=None):
        command = [str(exe.resolve()), str(disc.resolve()), "option", "OptionRoot",
                   "--config-file", str(config), "--shot", str(output / (name + ".png")),
                   "--input", ",".join(keys)]
        if interval:  # the list scrolls before the next press reaches lower rows
            command += ["--input-interval", interval]
        result = subprocess.run(command, env=env, capture_output=True, text=True,
                                encoding="utf-8", errors="replace", timeout=90)
        log = result.stdout + result.stderr
        (output / (name + ".log")).write_text(log, encoding="utf-8", newline="")
        assert result.returncode == 0 and "call of nil" not in log, log
        return log

    # General's row, then PC Settings' tiles under it, left to right
    tiles = [("PcGraphics", "Graphics"), ("PcLighting", "Light & Shadow"),
             ("PcControls", "Controls"), ("PcMusic", "Music"), ("PcPresets", "Presets")]
    for k, (tile, title) in enumerate(tiles):
        log = run(tile, ["down"] + ["right"] * k + ["accept"])
        if negative and k == 3:
            title = "Screen Settings"
        assert f"focus={tile}\n" in log, log
        assert f"video settings: page {title}\n" in log, log
    log = run("tweaks", ["down", "down", "accept"])
    assert "focus=PcTweaks\n" in log and "video settings: page Tweaks\n" in log, log
    log = run("screen", ["right", "right", "accept"])
    assert "video settings: page Screen Settings\n" in log, log
    # back from a PC page lands on its tile, not on Screen (the script's own item)
    log = run("back", ["down", "right", "accept", "back", "right", "accept"])
    assert "back -> option/OptionRoot focus=PcLighting\n" in log, log
    assert "video settings: page Controls\n" in log, log
    # Controllers: by player, Steering is the row's last tile, so right goes on to Exit (no
    # Driving Force Pro after it); Classic keeps the disc's ten tiles. Controller 1P opens
    # the button config.
    # the Tweaks tile has its own row under PC Settings
    controllers = ["down"] * 6 + ["right"] * 4
    for preset in ("Enhanced", "Classic"):
        config.write_text(f"preset={preset}\ngraphics.renderer=opengl\n", encoding="utf-8", newline="")
        log = run("controllers-" + preset, controllers, "0.5")
        by_player = preset == "Enhanced" and not negative
        last = [line for line in log.splitlines() if line.startswith("input ")][-1]
        assert last.endswith("focus=Exit") == by_player, log
    config.write_text("preset=Enhanced\ngraphics.renderer=opengl\n", encoding="utf-8", newline="")
    log = run("controller-1p", ["down"] * 6 + ["right", "accept"], "0.5")
    assert "accept -> option/KeyconfigRoot " in log, log
print("PC Settings section: six tiles open their pages, Screen keeps the display, back returns to the tile, Controllers by player")
