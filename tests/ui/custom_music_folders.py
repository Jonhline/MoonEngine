"""Jukebox Menu/Replay folders (audio.jukebox_folders) on the real page."""
import os
import re
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import wave
from png_diff import read_png

exe, disc, output = (Path(p).resolve() for p in sys.argv[1:4])
negative = len(sys.argv) > 4
output.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="moon-folders-") as folder:
    root = Path(folder)
    music = root / "Music"
    music.mkdir()
    # Subfolders first on disk, so the Library's grouping, not the scan, orders them.
    for name in ["Album da pasta/B Faixa.wav", "Album da pasta/A Faixa.wav",
                 "Album da pasta/CD 2/C Faixa.wav", "Som São Paulo.wav"]:
        (music / name).parent.mkdir(parents=True, exist_ok=True)
        with wave.open(str(music / name), "wb") as f:
            f.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
            f.writeframes(struct.pack("<hh", 5000, -5000) * 4800)
    config = root / "options.ini"
    config.write_text("preset=Enhanced\nmodified=0\ngraphics.renderer=opengl\n"
                      "audio.custom_music=1\n" + ("audio.jukebox_folders=0\n" if negative else ""),
                      encoding="utf-8")
    env = dict(os.environ, MOON_WINDOW_HIDDEN="1", SDL_AUDIODRIVER="dummy",
               MOONSTUDIO_FOLDER_SHOT=str(output / "folders"))
    # The second visit gets both lists back; adding the same song again keeps one.
    for run in range(2):
        result = subprocess.run([str(exe), str(disc), "slide", "MusicRoot", "--config-file", str(config),
                                 "--save-dir", str(root / "saves"), "--shot", str(output / "home.png")],
                                env=env, capture_output=True, timeout=90)
        log = (result.stdout + result.stderr).decode("utf-8", "replace")
        (output / f"folders-{run}.log").write_text(log, encoding="utf-8")
        assert result.returncode == 0, f"exit={result.returncode}\n{log}"
        assert "call of nil" not in log and "stopped with an error" not in log, log
        assert "jukebox: original Library, master=1 folders=1" in log, log
        assert f"jukebox: folders menu={run} replay={run}" in log, log
        assert "jukebox: folders fixture passed" in log, log
        # Help lines by mouse: the authored Racing ones with the folder's name,
        # the count kept after the click (slide.ad L1421 "%s%d").
        songs = 1  # the same song added again stays one
        for tip in ["Append3=Add to Menu playlist", "Append4=Add to Replay playlist",
                    f"PlayList3=Menu playlist/No. of songs:{songs}",
                    f"PlayList3-click=Menu playlist/No. of songs:{songs}",
                    f"PlayList4=Replay playlist/No. of songs:{songs}"]:
            assert re.search(re.escape(f"jukebox: tip {tip}") + r"\r?\n", log), log
        assert re.search(r"jukebox: tip PlayList1=Racing playlist/No\. of songs:\d+\r?\n", log), log
        library = Path(str(config) + ".music").read_text(encoding="utf-8")
        for name in ["Menu playlist", "Replay playlist"]:
            body = library.split(f"playlist={name}\n", 1)[1].split("end=", 1)[0]
            assert body.count("track=") == 1 and "Som São Paulo.wav" in body, body
        names = [line[9:] for line in library.splitlines() if line.startswith("playlist=")]
        for context, name in [(0, "Menu playlist"), (2, "Replay playlist")]:
            assert f"context={context},{names.index(name)}\n" in library, library
        assert "audio.custom_music=1\n" in config.read_text(encoding="utf-8")
    for shot in ["library", "menu", "replay", "groups"]:
        w, h, rows = read_png(output / f"folders-{shot}.png")
        assert w >= 640 and h >= 448 and len({p for row in rows for p in row}) > 500
print("PASS Jukebox Menu/Replay folders: authored append, folder modes, saved lists")
