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
        def commit(row, rows=7):
            return ["down"] * (rows - row) + ["accept"]
        if volume_only:
            run("lighting",change(0,["down"])+commit(0,4),2)
            data=config.read_text(encoding="utf-8")
            if negative:data=data.replace("graphics.ambient_occlusion=1","graphics.ambient_occlusion=0")
            for key in ("hdr","modern_lighting","material_shading","ambient_occlusion","car_shadows"):
                assert f"graphics.{key}=1" in data,data
            assert "graphics.sun_rays=0" in data and "graphics.shadow_maps=2" in data,data
            run("skybox",change(1,["down"])+commit(1,4),2)
            data=config.read_text(encoding="utf-8")
            assert "graphics.daylight=1" in data and "graphics.night_sky=1" in data,data
            assert "graphics.solar_hour=24" in data and "graphics.solar_cycle=1" in data,data
            log=run("bundles-reload",[],2)
            assert "choice Lighting/OptionMenu index=1" in log and "choice Daylight/OptionMenu index=1" in log,log
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
        run("lighting",change(0,["down"])+commit(0,4),2)
        run("skybox",change(1,["down"])+commit(1,4),2)
        run("reflections", change(4,["down","down"])+commit(4))
        data=config.read_text(encoding="utf-8")
        for key,value in [("modern_lighting",1),("material_shading",1),("car_shadows",1),("shadow_maps",2),("hdr",1),("ambient_occlusion",1),("daylight",1),("night_sky",1),("solar_hour",24),("solar_cycle",1),("sun_rays",0)]:
            assert f"graphics.{key}={value}" in data,data
        log = run("reload", [])
        assert "choice AntiAliasing/OptionMenu index=0" in log, log
        assert "choice RenderScale/OptionMenu index=1" in log, log
        assert "choice TextureSmoothing/OptionMenu index=1" in log, log
        assert "choice TextureFiltering/OptionMenu index=4" in log, log
        assert "choice CarShadows/OptionMenu index=1" in log, log
        assert "choice MaterialShading/OptionMenu index=1" in log, log
        assert "choice ReflectionQuality/OptionMenu index=2" in log, log
        assert "choice ShadowMaps/OptionMenu index=2" in log, log
print("PC volume/AO: selection, dependencies, Cancel and persistence passed" if volume_only else "PC Edge AA: both renderers, selection, Cancel and persistence passed" if edge_only else "PC quality: both renderers, SSAA, reduced resolution, smoothing, anisotropic filtering, material shading, Cancel and persistence passed")
