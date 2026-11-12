"""Real launcher key/hover/click path, using isolated preferences and silent audio."""
import os, subprocess, sys, tempfile
from pathlib import Path
exe,disc=(str(Path(v).resolve()) for v in sys.argv[1:3])
negative="--negative" in sys.argv[3:]
with tempfile.TemporaryDirectory(prefix="moon-binding-list-") as temp:
    root=Path(temp);prefs=root/"launcher.ini"
    prefs.write_text(f"disc={disc}\napi=opengl\ncar=ka_01\ncolor=1\nsaves={root.as_posix()}/saves\n",encoding="utf-8")
    Path(str(prefs)+".video.ini").write_text("launcher.guide_seen=1\ngraphics.renderer=opengl\n",encoding="utf-8")
    env=dict(os.environ,SDL_AUDIODRIVER="dummy",MOON_WINDOW_HIDDEN="1",MOONSTUDIO_MUTE="1",
        MOONSTUDIO_OPTIONS="4",MOONSTUDIO_BINDING_PICKER="1",MOONSTUDIO_BINDING_CHECK="2" if negative else "1",
        MOONSTUDIO_FRONT_SHOT=str(root/"screen.png"))
    result=subprocess.run([exe,"--launcher","--launcher-config",str(prefs)],env=env,capture_output=True,timeout=90)
    log=(result.stdout+result.stderr).decode("utf-8","replace")
    if result.returncode or "STUDIO_BINDING_INPUT PASS" not in log:
        print(log);sys.exit(1)
    print("PASS actual launcher key browsing/cancel and mouse hover/direct selection; other player unchanged")
