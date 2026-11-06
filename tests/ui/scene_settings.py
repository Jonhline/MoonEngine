"""Independent scene options, Classic/Enhanced, cancellation and persistence.
Each run opens the page as its Options tile does (MOON_SETTINGS_PAGE): 0 Screen,
1 Graphics (Spatial Visibility and Resident Reflections), 5 Presets."""
import pathlib, subprocess, sys, tempfile, re, os
exe, disc, output = map(pathlib.Path, sys.argv[1:4])
negative = "--negative" in sys.argv[4:]
output.mkdir(exist_ok=True, parents=True)
with tempfile.TemporaryDirectory(dir=output) as directory:
    config = pathlib.Path(directory) / "options.ini"
    config.write_text("preset=Classic\n", encoding="utf-8", newline="")
    def run(name, inputs=(), page=1):
        args = [str(exe.resolve()), str(disc.resolve()), "option", "ScreenRoot", "--video-settings",
                "--config-file", str(config), "--shot", str(output / (name + ".png"))]
        if inputs:
            args += ["--input", ",".join(inputs)]
        env = dict(os.environ, MOON_WINDOW_HIDDEN="1", MOON_UI_TREE="1", SDL_AUDIODRIVER="dummy",
                   MOON_SETTINGS_PAGE=str(page), MOON_OVERLAY=str(pathlib.Path(directory) / "no-mods"))
        result = subprocess.run(args, env=env, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
        log = result.stdout + result.stderr
        (output / (name + ".log")).write_text(log, encoding="utf-8", newline="")
        assert result.returncode == 0 and "call of nil" not in log and "undefined" not in log, log
        return log
    def visible_rows(log):
        return set(re.findall(r"^tree +Composite (AspectRatio|WindowMode|Resolution|UIFilter|TextSharpness|VSync|Upscaling|DesktopInput|PCPreset|MapEdgePan|InputHints|SpatialVisibility|ResidentReflections|MainMenuOverview|AntiAliasing|RenderScale|TextureSmoothing|TextureFiltering|ReflectionQuality) vis=1", log, re.M))
    log = run("screen", page=0)
    assert visible_rows(log) == {"AspectRatio", "WindowMode", "Resolution", "VSync", "Upscaling", "UIFilter", "TextSharpness"}, log
    log = run("classic")
    assert visible_rows(log) == {"AntiAliasing", "RenderScale", "TextureSmoothing", "TextureFiltering", "ReflectionQuality",
                                 "ResidentReflections", "SpatialVisibility"}, log
    # The Graphics page: AA, 3D Resolution, Smoothing, Filtering, Reflections, Resident, Spatial
    def edit(index, direction="down"):
        return ["down"] * index + ["accept", direction, "accept"]
    def commit(index, count=7):
        return ["down"] * (count - index) + ["accept"]
    assert "choice SpatialVisibility/OptionMenu index=0" in log and "choice ResidentReflections/OptionMenu index=0" in log, log
    run("spatial_on", edit(6) + commit(6))
    data = config.read_text()
    assert "graphics.spatial_visibility=1" in data and "graphics.resident_reflections=0" in data, data
    saved = config.read_bytes()
    run("cancel", edit(5) + ["back"])
    assert config.read_bytes() == saved, "cancel wrote draft"
    run("resident_on", edit(5) + commit(5))
    data = config.read_text()
    assert "graphics.spatial_visibility=1" in data and "graphics.resident_reflections=1" in data and "modified=1" in data, data
    log = run("reload")
    assert "choice SpatialVisibility/OptionMenu index=1" in log and "choice ResidentReflections/OptionMenu index=1" in log, log
    assert "choice PCPreset/OptionMenu index=3" in log, log  # Modified
    # The Presets page: PC Preset, Preset Files, Save Transfer (Classic: no file buttons)
    run("classic_apply", ["accept", "up", "up", "up", "accept"] + commit(0, 3), page=5)
    data = config.read_text()
    assert "graphics.spatial_visibility=0" in data and "graphics.resident_reflections=0" in data and "preset=Classic" in data, data
    # Enhanced turns Preset Files on: Load/Save appear under the rows; Save, then down, is OK
    run("enhanced_apply", ["accept", "down", "accept"] + ["down"] * 3 + ["right", "down", "accept"], page=5)
    data = config.read_text()
    assert "graphics.spatial_visibility=1" in data and "graphics.resident_reflections=1" in data and "preset=Enhanced" in data, data
    # Move across pages after editing: neither navigation nor Cancel may lose
    # the shared draft or write it prematurely.
    cross = edit(6, "up") + ["r1", "l1"]
    saved = config.read_bytes()
    run("cross_pages_cancel", cross + ["back"])
    assert config.read_bytes() == saved, "page navigation committed cancelled settings"
    run("cross_pages_apply", cross + commit(6))  # the focus stays on Spatial Visibility
    data = config.read_text()
    if negative:
        data = data.replace("graphics.spatial_visibility=0", "graphics.spatial_visibility=1")
    assert "graphics.spatial_visibility=0" in data and "preset=Enhanced" in data and "modified=1" in data, data
print("Scene settings: independent values, cancel, reload, Classic and Enhanced passed")
