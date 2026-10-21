"""Launcher requests the selected paint before its first asynchronous load."""
from pathlib import Path
import os,re,subprocess,sys,tempfile
from png_diff import read_png
exe,disc,out=(Path(p).resolve() for p in sys.argv[1:4]);out.mkdir(parents=True,exist_ok=True)
negative="--negative" in sys.argv[4:]
with tempfile.TemporaryDirectory(prefix="moon-paint-") as tmp:
 root=Path(tmp)
 for api in ("opengl","vulkan"):
  prefs=root/(api+".ini")
  prefs.write_text(f"disc={disc.as_posix()}\napi={api}\nsaves={root.as_posix()}/saves\ncar=cappuccino_91\ncolor=1\nscene=tsukuba\n",encoding="utf-8",newline="")
  Path(str(prefs)+".video.ini").write_text(f"preset=Enhanced\ngraphics.renderer={api}\nlauncher.guide_seen=1\n",encoding="utf-8",newline="")
  shot=out/(api+".png");env=dict(os.environ,SDL_AUDIODRIVER="dummy",MOON_WINDOW_HIDDEN="1",MOONSTUDIO_MUTE="1",MOONSTUDIO_CINE_AT="0",MOONSTUDIO_FRONT_SHOT=str(shot))
  run=subprocess.run([str(exe),"--launcher","--launcher-config",str(prefs)],env=env,capture_output=True,timeout=180)
  log=(run.stdout+run.stderr).decode("utf-8","replace");shot.with_suffix(".log").write_text(log,encoding="utf-8",newline="")
  assert run.returncode==0,log[-2000:]
  paints=re.findall(r"showroom: cappuccino_91 .*?color=(\d+)",log)
  assert paints,log
  if negative:paints[0]="1"
  assert int(paints[0])==2,(api,"initial paint",paints)
  assert "studio: scene tsukuba, the car at" in log,log[-1000:]
  w,h,rows=read_png(shot);assert w>=640 and h>=448
  print(api,"first model uses selected second paint before scene entry",flush=True)
