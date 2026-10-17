"""Independent scene options, Classic/Enhanced, cancellation and persistence."""
import pathlib, subprocess, sys, tempfile, re, os
exe, disc, output = map(pathlib.Path, sys.argv[1:4])
negative = "--negative" in sys.argv[4:]
output.mkdir(exist_ok=True, parents=True)
with tempfile.TemporaryDirectory(dir=output) as directory:
    config = pathlib.Path(directory) / "options.ini"
    config.write_text("preset=Classic\n", encoding="utf-8", newline="")
    def run(name, inputs=()):
        args = [str(exe.resolve()), str(disc.resolve()), "option", "ScreenRoot", "--video-settings",
                "--config-file", str(config), "--shot", str(output / (name + ".png"))]
        if inputs:
            args += ["--input", ",".join(inputs)]
        env = dict(os.environ, MOON_WINDOW_HIDDEN="1", MOON_UI_TREE="1")
        result = subprocess.run(args, env=env, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
        log = result.stdout + result.stderr
        (output / (name + ".log")).write_text(log, encoding="utf-8", newline="")
        assert result.returncode == 0 and "call of nil" not in log and "undefined" not in log, log
        return log
    log = run("classic")
    assert "choice DesktopInput/OptionMenu index=0" in log, log
    def visible_rows(log):
        return set(re.findall(r"^tree +Composite (AspectRatio|WindowMode|Resolution|UIFilter|TextSharpness|VSync|Upscaling|DesktopInput|PCPreset|MapEdgePan|InputHints|SpatialVisibility|ResidentReflections|MainMenuOverview) vis=1", log, re.M))
    assert visible_rows(log) == {"AspectRatio", "WindowMode", "Resolution", "VSync", "PCPreset"}, log
    # Button geometry remains inside the enlarged pane; no squeezed tabs.
    bounds = re.findall(r"^tree +Composite (?:DisplayTab|RenderingTab|ControlsTab) .*rect=(\d+),(\d+),(\d+),(\d+)", log, re.M)
    assert len(bounds) == 3, log
    for x, y, w, h in bounds:
        assert int(x) >= 0 and int(x) + int(w) <= 400 and int(y) + int(h) <= 60, log
    def tab(index):
        # The shared preset sits between the first row and the rightmost tab.
        return ["up", "up"] + ["left"] * (2 - index) + ["accept"]
    def edit(index, direction="down"):
        return ["down"] * index + ["accept", direction, "accept"]
    def commit(index, count):
        return ["down"] * (count - index) + ["accept"]
    assert "choice SpatialVisibility/OptionMenu index=0" in log and "choice ResidentReflections/OptionMenu index=0" in log, log
    run("spatial_on", tab(1) + edit(3) + commit(3, 5))
    data = config.read_text()
    assert "graphics.spatial_visibility=1" in data and "graphics.resident_reflections=0" in data, data
    saved = config.read_bytes()
    run("cancel", tab(1) + edit(4) + ["back"])
    assert config.read_bytes() == saved, "cancel wrote draft"
    run("resident_on", tab(1) + edit(4) + commit(4, 5))
    data = config.read_text()
    assert "graphics.spatial_visibility=1" in data and "graphics.resident_reflections=1" in data and "modified=1" in data, data
    log = run("reload", ["r1"])
    assert visible_rows(log) == {"UIFilter", "TextSharpness", "Upscaling", "SpatialVisibility", "ResidentReflections", "PCPreset"}, log
    assert "choice SpatialVisibility/OptionMenu index=1" in log and "choice ResidentReflections/OptionMenu index=1" in log, log
    run("classic_apply", ["up", "accept", "up", "up", "accept"] + ["down"] * 5 + ["accept"])
    data = config.read_text()
    assert "graphics.spatial_visibility=0" in data and "graphics.resident_reflections=0" in data and "preset=Classic" in data, data
    run("enhanced_apply", ["up", "accept", "down", "accept"] + ["down"] * 5 + ["accept"])
    data = config.read_text()
    assert "graphics.spatial_visibility=1" in data and "graphics.resident_reflections=1" in data and "preset=Enhanced" in data, data
    # Move across tabs after editing: neither navigation nor Cancel may lose
    # the shared draft or write it prematurely.
    cross = ["l1", "r1", "r1"] + edit(3, "up") + ["r1"] + edit(0, "up")
    saved = config.read_bytes()
    run("cross_tabs_cancel", cross + ["back"])
    assert config.read_bytes() == saved, "tab navigation committed cancelled settings"
    run("cross_tabs_apply", cross + commit(0, 4))
    data = config.read_text()
    if negative:
        data = data.replace("graphics.spatial_visibility=0", "graphics.spatial_visibility=1")
    assert "graphics.spatial_visibility=0" in data and "controls.desktop_menu=0" in data, data
print("Scene settings: independent values, cancel, reload, Classic and Enhanced passed")
