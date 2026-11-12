"""Capture the disc's night environments with the actual renderer, no simulation."""
import html,json,os,subprocess,sys
from pathlib import Path
exe,disc,out=map(Path,sys.argv[1:4]);out.mkdir(parents=True,exist_ok=True)
courses="hongkong lasvegas motegi motegi_east motegi_west paris_rally r_hongkong r_paris_rally r_route5 r_route5_club r_seattle route5 route5_club sarthe sarthe24 sarthe_old sarthe_old24 seattle ss_brooklyn ss_fremont ss_gion ss_sanmarco ss_shibuya ss_times ss_tsukiji".split()
api=sys.argv[4] if len(sys.argv)>4 else "opengl"
assert api in ("opengl","vulkan")
records=[]
for course in courses:
    env=dict(os.environ,SDL_AUDIODRIVER="dummy",MOON_WINDOW_HIDDEN="1",MOON_GRAPHICS_API=api)
    p=subprocess.run([str(exe.resolve()),str(disc.resolve()),course,str((out/course).resolve())],env=env,capture_output=True,timeout=40)
    log=(p.stdout+p.stderr).decode("utf-8","replace")
    (out/(course+".log")).write_text(log,encoding="utf-8")
    records.append(dict(course=course,exit=p.returncode,audit=[l for l in log.splitlines() if l.startswith("AUDIT ")]))
    (out/"manifest.json").write_text(json.dumps(records,indent=2),encoding="utf-8")
    print(course,p.returncode,flush=True)
page="<meta charset=utf-8><title>Night environment audit</title><style>body{background:#222;color:white;font:16px sans-serif}img{width:48%;height:auto}section{border-top:1px solid #888}</style>"
page+=f"<h1>Native disc night audit: {api}</h1><p>Our renderer only. Equivalent PCSX2 images still required. sarthe_old is a daytime control. Brooklyn and Tsukiji author glow=0. No r_ Photo Travel files exist in this disc.</p>"
for r in records:
    c=r["course"];page+=f"<section><h2>{c}</h2><p>{html.escape(' '.join(r['audit']))}</p>"
    for suffix in ["race","lamps","pregrid","daylight","enhanced-lamps","enhanced-night"]:
        page+=f'<figure><figcaption>{suffix}</figcaption><img src="{c}-{suffix}.png"></figure>'
    page+="</section>"
(out/"index.html").write_text(page,encoding="utf-8")
assert all(r["exit"]==0 for r in records),records
print("PASS 25 source environments captured; no original-pixel parity claimed")
