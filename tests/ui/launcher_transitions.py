"""Real launcher dissolve pixels and settings composition after a live preview."""
import os
from pathlib import Path
import subprocess
import sys
from png_diff import read_png

player,disc,output=map(Path,sys.argv[1:4])
negative="--negative" in sys.argv[4:]
viewport_negative="--viewport-negative" in sys.argv[4:]
output.mkdir(parents=True,exist_ok=True)
prefs=output/"launcher.ini"
prefs.write_text(f"disc={disc.resolve().as_posix()}\nsaves={output.resolve().as_posix()}/save\nshow=1\ncheck_updates=0\nmute_music=1\n",encoding="utf-8",newline="")

def capture(name,sequence,backend="opengl",hard_cut=False):
    Path(str(prefs)+".video.ini").write_text(f"renderer={backend}\n",encoding="utf-8",newline="")
    env=os.environ.copy()
    if hard_cut: env["MOON_LAUNCHER_HARD_CUT"]="1"
    shot=output/(name+".png")
    run=subprocess.run([str(player),"--launcher","--launcher-config",str(prefs),"--input",sequence,
                        "--shot",str(shot)],env=env,capture_output=True,text=True,timeout=50)
    shot.with_suffix(".log").write_text(run.stdout+run.stderr,encoding="utf-8",newline="")
    assert run.returncode==0,run.stdout+run.stderr
    width,height,rows=read_png(shot)
    assert (width,height)==(1244,700)
    return rows

if not viewport_negative:
    for backend in ("opengl","vulkan"):
        old=capture(backend+"-outgoing","scene-time:11.99",backend)
        incoming=capture(backend+"-incoming","scene-time:11.99,scene-time:12.75",backend,True)
        mixed=capture(backend+"-dissolve","scene-time:11.99,scene-time:12.75",backend)
        if negative:
            mixed=old
        # Midpoint smoothstep is 1/2. The production alpha is quantized to 128.
        maximum=0
        different=0
        for y in range(245,360):
            for x in range(530,1170):
                a,b,c=old[y][x],incoming[y][x],mixed[y][x]
                different+=a!=b
                maximum=max(maximum,*(abs(value-(u*128+v*127+127)//255)
                                      for u,v,value in zip(a,b,c)))
        assert different>10000,(backend,"unchanged camera",different)
        assert maximum<=3,(backend,"dissolve mismatch",maximum)
        print("PASS GPU midpoint dissolve",backend,"maximum error",maximum,flush=True)

fresh=capture("settings-fresh","video")
after=capture("settings-after-preview","scene-time:0,video")
if viewport_negative:
    pixel=after[200][1200]
    after[200][1200]=(pixel[0]^64,pixel[1],pixel[2])
assert fresh==after,"settings pixels depend on the previous scene / leaked viewport"
print("PASS complete settings composition after preview, no stale right edge")
