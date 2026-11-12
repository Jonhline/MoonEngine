"""Real optional-library editor; only native file choices are injected."""
import hashlib
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import wave
from png_diff import read_png

exe, disc, output = (Path(p).resolve() for p in sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="moon-m\u00fasica-ui-") as folder:
    root = Path(folder)
    music=root / "Music";music.mkdir()
    audio = music / "Som S\u00e3o Paulo.wav"
    with wave.open(str(audio), "wb") as f:
        f.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
        f.writeframes(struct.pack("<hh", 5000, -5000) * 48000)
    original = hashlib.sha256(audio.read_bytes()).hexdigest()
    album=music / "Album da pasta";album.mkdir()
    for n in range(9):
        with wave.open(str(album / f"Track {n+1:02d}.wav"),"wb") as f:
            f.setparams((2,2,48000,0,"NONE","not compressed"));f.writeframes(struct.pack("<hh",2000+n,-2000-n)*4800)
    prefs = root / "launcher.ini"
    prefs.write_text(f"disc={disc.as_posix()}\napi=opengl\nsaves={root.as_posix()}/saves\n"
                     "car=ford_gt_no_stripe_05\ncolor=0\ntheme=0\n", encoding="utf-8")
    config = Path(str(prefs) + ".video.ini")
    config.write_text("preset=Classic\nmodified=0\ngraphics.renderer=opengl\n"
                      "launcher.guide_seen=1\naudio.custom_music=1\n", encoding="utf-8")
    env = dict(os.environ, MOON_WINDOW_HIDDEN="1", SDL_AUDIODRIVER="dummy", MOON_SOUND_TRACE="1",
               MOONSTUDIO_FRONT_SHOT=str(output / "home.png"),
               MOONSTUDIO_MUSIC_SHOT=str(output / "library.png"))
    counts=[]
    for run in range(2):
        result = subprocess.run([str(exe), str(disc), "slide", "MusicRoot", "--config-file", str(config), "--save-dir", str(root / "saves"), "--shot", str(output / "home.png"), "--with-music"],
                                env=env, capture_output=True, timeout=90)
        log = (result.stdout + result.stderr).decode("utf-8", "replace")
        (output / f"library-{run}.log").write_text(log, encoding="utf-8")
        assert result.returncode == 0, f"exit={result.returncode}\n{log}"
        assert "call of nil" not in log, log
        assert "sound: cursor bank=" in log and "sound: ok bank=" in log and "sound: cancel bank=" in log, log
        assert "jukebox: original Library fixture passed, one Jukebox page" in log, log
        assert "jukebox: original Library, master=1" in log, log
        scan=re.search(r"Music folder=.* added=(\d+) catalog=(\d+)",log)
        assert scan and int(scan[1])==(10 if run==0 else 0), log
        counts.append(int(scan[2]))
        state=config.read_text(encoding="utf-8")
        assert "audio.custom_music=1\n" in state, state
        library=Path(str(config)+".music").read_text(encoding="utf-8")
        for name in ["Race playlist","Slideshow playlist"]:
            body=library.split(f"playlist={name}\n",1)[1].split("end=",1)[0]
            assert "Som S\u00e3o Paulo.wav" in body and "gt4:" in body, body
        assert "shuffle=1\n" in library if run==0 else "shuffle=0\n" in library
        match=re.search(r"custom music: rendered (\d+) external frames, peak (\d+)",log)
        assert match and int(match[1])>0 and int(match[2])>0,log
        # About one second of play (fixture steps 9..12): a stream restarted every
        # frame read ~260000 frames, the song running fast.
        assert int(match[1])<96000,log
    # Custom Music off, with the same Music folder: the disc's own page, no local song.
    clean=root/"off.ini"
    clean.write_text("preset=Classic\naudio.custom_music=0\ngraphics.renderer=opengl\n",encoding="utf-8")
    plain_env=dict(os.environ,MOON_WINDOW_HIDDEN="1",SDL_AUDIODRIVER="dummy")
    plain_env.pop("MOONSTUDIO_MUSIC_SHOT",None)
    result=subprocess.run([str(exe),str(disc),"slide","MusicRoot","--config-file",str(clean),
                           "--save-dir",str(root/"off-saves"),"--shot",str(output/"disc-only.png")],
                          env=plain_env,capture_output=True,timeout=60)
    plain=(result.stdout+result.stderr).decode("utf-8","replace")
    (output/"disc-only.log").write_text(plain,encoding="utf-8")
    assert result.returncode==0 and "jukebox: native disc-only entry, custom music off" in plain,plain
    assert "Music folder=" not in plain,plain
    assert "audio.custom_music=0\n" in clean.read_text(encoding="utf-8")
    assert counts[0]==counts[1], "folder scan duplicated songs on reentry"
    assert hashlib.sha256(audio.read_bytes()).hexdigest()==original
    assert not list((root/"saves").glob("slot*.ogt4"))
    w,h,rows=read_png(output/"library.png")
    assert w>=640 and h>=448 and len({p for row in rows for p in row})>500
print("PASS original Jukebox: folder scan, Unicode/nested files, mixed racing/slideshow lists, native remove/readd/play/shuffle, reentry without duplicates, PCM, source preserved")
