"""PC quality through authored widgets on both backends; no player files."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

exe, disc, output = map(Path, sys.argv[1:4])
negative = "--negative" in sys.argv[4:]
edge_only = "--edge-only" in sys.argv[4:]
volume_only = "--volume-only" in sys.argv[4:]
output.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=output) as temporary:
    config = Path(temporary) / "options.ini"
    for backend in ("opengl", "vulkan"):
        config.write_text(f"preset=Classic\ngraphics.renderer={backend}\n", encoding="utf-8", newline="")
        # each page as its Options > PC Settings tile opens it: 1 Graphics, 2 Light & Shadow
        def run(name, keys, page=1):
            shot = output / f"{backend}-{name}.png"
            command = [str(exe.resolve()), str(disc.resolve()), "option", "ScreenRoot",
                       "--video-settings", "--config-file", str(config), "--shot", str(shot)]
            if keys:
                command += ["--input", ",".join(keys)]
            env = dict(os.environ, MOON_WINDOW_HIDDEN="1", SDL_AUDIODRIVER="dummy", MOON_OVERLAY=str(Path(temporary) / "no-mods"),
                       MOON_SETTINGS_PAGE=str(page))
            env.pop("MOON_GRAPHICS_API", None)
            result = subprocess.run(command, env=env, capture_output=True, text=True,
                                    encoding="utf-8", errors="replace", timeout=45)
            log = result.stdout + result.stderr
            shot.with_suffix(".log").write_text(log, encoding="utf-8", newline="")
            assert result.returncode == 0 and "call of nil" not in log, log
            return log
        def change(row, directions):
            return ["down"] * row + ["accept"] + directions + ["accept"]
        def commit(row, rows=9):
            return ["down"] * (rows - row) + ["accept"]
        if volume_only:
            run("ao",change(7,["down"])+commit(7,8),2)
            data=config.read_text(encoding="utf-8")
            if negative:data=data.replace("graphics.ambient_occlusion=1","graphics.ambient_occlusion=0")
            for value in ("graphics.ambient_occlusion=1","graphics.hdr=1","graphics.daylight=1"):
                assert value in data,data
            run("volume",change(6,["down","down"])+commit(6,8),2)
            data=config.read_text(encoding="utf-8")
            assert "graphics.sun_rays=2" in data and "graphics.hdr=1" in data,data
            before=config.read_bytes()
            run("ao-cancel",change(7,["up"])+["back"],2)
            assert config.read_bytes()==before
            log=run("volume-reload",[],2)
            assert "choice AmbientOcclusion/OptionMenu index=1" in log,log
            assert "choice SunRays/OptionMenu index=2" in log,log
            continue
        run("ssaa", change(0, ["down"] * 3) + commit(0))
        data = config.read_text(encoding="utf-8")
        if negative and not edge_only:
            data = data.replace("graphics.antialiasing=3", "graphics.antialiasing=0")
        assert "graphics.antialiasing=3" in data and "modified=1" in data, data
        run("edge-aa", change(0,["down"])+commit(0))
        data=config.read_text(encoding="utf-8")
        if negative and edge_only:
            data=data.replace("graphics.antialiasing=4","graphics.antialiasing=0")
        assert "graphics.antialiasing=4" in data and "graphics.upscaling=0" in data,data
        before = config.read_bytes()
        run("cancel", change(0, ["up", "up"]) + ["back"])
        assert config.read_bytes() == before
        if edge_only:
            log=run("edge-reload",[])
            assert "choice AntiAliasing/OptionMenu index=4" in log,log
            continue
        run("scale", change(1, ["up"]) + commit(1))
        data = config.read_text(encoding="utf-8")
        assert "graphics.render_scale=1" in data and "graphics.antialiasing=0" in data, data
        run("texture", change(2, ["down"]) + commit(2))
        data = config.read_text(encoding="utf-8")
        assert "graphics.texture_smoothing=1" in data, data
        run("filtering", change(3, ["down"] * 4) + commit(3))
        data = config.read_text(encoding="utf-8")
        assert "graphics.texture_filtering=4" in data, data
        run("shadows", change(0, ["down"]) + commit(0, 8), 2)
        assert "graphics.car_shadows=1" in config.read_text(encoding="utf-8")
        run("materials", change(4, ["down"]) + commit(4))
        assert "graphics.material_shading=1" in config.read_text(encoding="utf-8")
        run("reflections", change(5, ["down", "down"]) + commit(5))
        assert "graphics.reflection_quality=2" in config.read_text(encoding="utf-8")
        run("shadow-maps", change(1, ["down", "down", "down"]) + commit(1, 8), 2)
        assert "graphics.shadow_maps=3" in config.read_text(encoding="utf-8")
        run("lighting", change(6, ["down"]) + commit(6))
        assert "graphics.modern_lighting=1" in config.read_text(encoding="utf-8")
        run("daylight", change(2,["down"])+commit(2,8),2)
        run("solar-time", change(3,["down"]*4)+commit(3,8),2)
        run("solar-cycle", change(4,["down"]*2)+commit(4,8),2)
        run("hdr", change(5,["down"])+commit(5,8),2)
        run("sun-rays", change(6,["down"])+commit(6,8),2)
        data=config.read_text(encoding="utf-8")
        for key,value in [("daylight",1),("solar_hour",9),("solar_cycle",2),("hdr",1),("sun_rays",1)]:
            assert f"graphics.{key}={value}" in data,data
        log = run("reload", [])
        assert "choice AntiAliasing/OptionMenu index=0" in log, log
        assert "choice RenderScale/OptionMenu index=1" in log, log
        assert "choice TextureSmoothing/OptionMenu index=1" in log, log
        assert "choice TextureFiltering/OptionMenu index=4" in log, log
        assert "choice CarShadows/OptionMenu index=1" in log, log
        assert "choice MaterialShading/OptionMenu index=1" in log, log
        assert "choice ReflectionQuality/OptionMenu index=2" in log, log
        assert "choice ShadowMaps/OptionMenu index=3" in log, log
print("PC volume/AO: selection, dependencies, Cancel and persistence passed" if volume_only else "PC Edge AA: both renderers, selection, Cancel and persistence passed" if edge_only else "PC quality: both renderers, SSAA, reduced resolution, smoothing, anisotropic filtering, material shading, Cancel and persistence passed")
