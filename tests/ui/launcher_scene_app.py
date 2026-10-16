"""Real launcher, isolated saves, hidden native preview and original disc fonts/icons."""
import os
from pathlib import Path
import re
import subprocess
import sys
from png_diff import read_png

player, disc, output = map(Path, sys.argv[1:4])
negative = "--negative" in sys.argv[4:]
resolve_negative = "--resolve-negative" in sys.argv[4:]
output.mkdir(parents=True, exist_ok=True)
prefs=output / "launcher.ini"
prefs.write_text(f"disc={disc.resolve().as_posix()}\nsaves={output.resolve().as_posix()}/save\nshow=1\ncheck_updates=0\nmute_music=1\n",encoding="utf-8",newline="")
images={}
for name,time,backend,legacy in (("wide",0,"opengl",False),("wheel",16,"opengl",False),
                                  ("vulkan",0,"vulkan",False),("gl_cpu",0,"opengl",True),
                                  ("vk_cpu",0,"vulkan",True)):
    env=os.environ.copy()
    if legacy: env["MOON_LAUNCHER_CPU_RESOLVE"]="1"
    Path(str(prefs)+".video.ini").write_text(f"renderer={backend}\n",encoding="utf-8",newline="")
    image=output/(name+".png")
    run=subprocess.run([str(player),"--launcher","--launcher-config",str(prefs),
                        "--input",f"scene-time:{time}","--shot",str(image)],capture_output=True,text=True,timeout=45,env=env)
    log=run.stdout+run.stderr
    image.with_suffix(".log").write_text(log,encoding="utf-8",newline="")
    assert run.returncode==0,log
    match=re.search(r"LAUNCHER DOCK 8 SIZE 1244 700 SCENE (\d+)",log)
    assert match and int(match[1])>=24,log
    width,height,rows=read_png(image)
    assert (width,height)==(1244,700)
    assert len({pixel for row in rows[240:440] for pixel in row[330:1180]})>1000,"blank/flat car preview"
    transfer=re.search(r"LAUNCHER TRANSFER pixels=(\d+)",log)
    assert transfer and int(transfer[1])==1244*700*(4 if legacy else 1),log
    images[name]=rows
changed=sum(a!=b for row_a,row_b in zip(images["wide"][235:440],images["wheel"][235:440]) for a,b in zip(row_a[330:1180],row_b[330:1180]))
assert changed>(999999 if negative else 10000),changed
print("PASS native preview changes cinematic framing:",changed,"pixels")

if resolve_negative:
    pixel=images["wide"][300][700]
    images["wide"][300][700]=(pixel[0]^64,pixel[1],pixel[2])
for optimized,reference in (("wide","gl_cpu"),("vulkan","vk_cpu")):
    maximum=max(abs(x-y) for ra,rb in zip(images[optimized],images[reference])
                for a,b in zip(ra,rb) for x,y in zip(a,b))
    assert maximum<=1,(optimized,maximum)
    print("PASS GPU 2x resolve vs CPU, maximum channel error:",optimized,maximum)
