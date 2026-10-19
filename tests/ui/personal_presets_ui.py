"""Exercise original ScreenRoot actions with an injected host-file choice.
All paths/preferences are disposable. No native-dialog automation or player data.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

exe, disc, output = map(Path, sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
negative = "--negative" in sys.argv[4:]
with tempfile.TemporaryDirectory(dir=output) as directory:
    directory = Path(directory)
    preset = directory / "touring.moonpreset"
    config = directory / "options.ini"
    content = "MoonEnginePreset=1\nname=Touring\ngraphics.ui_filter=1\ngraphics.text_sharpness=75\ngraphics.texture_filtering=4\n"
    initial = "preset=Classic\ninterface.preset_tools=1\ngraphics.renderer=opengl\n"
    config.write_text(initial, encoding="utf-8")
    preset.write_text(content, encoding="utf-8")
    if negative:
        preset.write_text(content.replace("graphics.ui_filter=1", "graphics.ui_filter=99"), encoding="utf-8")
    original_preset = preset.read_bytes()
    initial_bytes = config.read_bytes()
    # the Presets page, as Options > PC Settings > Presets opens it
    env = dict(os.environ, MOON_WINDOW_HIDDEN="1", SDL_AUDIODRIVER="dummy", MOON_SETTINGS_PAGE="5",
               MOON_TEST_PRESET_PATH=str(preset.resolve()), MOON_OVERLAY=str(directory / "no-mods"))

    def run(name, keys):
        command = [str(exe.resolve()), str(disc.resolve()), "option", "ScreenRoot",
                   "--video-settings", "--config-file", str(config), "--shot",
                   str(output / (name + ".png")), "--input", ",".join(keys)]
        result = subprocess.run(command, env=env, capture_output=True, timeout=45)
        (output / (name + ".log")).write_bytes(result.stdout + result.stderr)
        assert result.returncode == 0, result.stderr.decode("utf-8", "replace")

    load = ["down"] * 3 + ["left", "accept"]
    run("cancel", load + ["back"])
    assert config.read_bytes() == initial_bytes
    assert preset.read_bytes() == original_preset
    # Import refreshes the authored controls on the Presets page, focused on PC Preset.
    run("apply", load + ["down"] * 4 + ["accept"])
    applied = config.read_text(encoding="utf-8")
    assert "preset=Touring\n" in applied and "modified=0\n" in applied, applied
    assert "graphics.ui_filter=1\n" in applied and "graphics.text_sharpness=75\n" in applied, applied
    assert "graphics.texture_filtering=4\n" in applied, applied
    assert preset.read_bytes() == original_preset
    # Saving a draft creates a portable file but leaves the active config alone.
    before = config.read_bytes()
    preset.unlink()
    run("export", ["down"] * 3 + ["accept", "back"])
    exported = preset.read_text(encoding="utf-8")
    assert "name=Fixture preset\n" in exported and "graphics.text_sharpness=75\n" in exported
    assert "graphics.width=" not in exported and "graphics.renderer=" not in exported
    assert config.read_bytes() == before
print("Personal presets UI: authored Load/Save buttons, draft Cancel, Apply, name, values and export passed")
