"""Private native lamp coverage gallery; disc pixels remain under build/."""
import html,json,os,subprocess,sys
from pathlib import Path
exe,disc,out=map(Path,sys.argv[1:4]);out.mkdir(parents=True,exist_ok=True)
cars='focus_rs_02 mustang_gt_05 svt_f150_lightning_03 gt40_race_69 beat_91 ka_01 crx_sir_90 _787b_91 r92cp_92 r390_race_98 gt_one_race_exxon_99 r34_gtr_v_gtcolor_pc_01'.split()
records=[]
for api in ('opengl','vulkan'):
 for car in cars:
  env=dict(os.environ,SDL_AUDIODRIVER='dummy',MOON_WINDOW_HIDDEN='1',MOON_GRAPHICS_API=api,NIGHT_CAPTURE_CAR=car)
  prefix=(out/(api+'-'+car)).resolve()
  r=subprocess.run([str(exe.resolve()),str(disc.resolve()),'route5',str(prefix)],env=env,capture_output=True,timeout=45)
  log=(r.stdout+r.stderr).decode('utf-8','replace');Path(str(prefix)+'.log').write_text(log,encoding='utf-8')
  records.append(dict(api=api,car=car,exit=r.returncode,emitters=[s for s in log.splitlines() if s.startswith(('lamp geometry','emitter '))]))
  print(api,car,r.returncode,flush=True)
  (out/'manifest.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
page='''<!doctype html><meta charset="utf-8"><title>Moon Engine — cobertura de faróis</title><style>body{background:#191d21;color:#eee;font:16px system-ui;margin:25px}section{border-top:1px solid #777;padding:16px}figure{display:inline-block;width:min(45%,640px);margin:10px}img{width:100%}button{padding:10px;margin:5px;cursor:pointer}pre{white-space:pre-wrap;color:#bcc6d0}</style><h1>Faróis e lanternas — 12 modelos, Route5</h1><p>Capturas nativas. Detecção de emissores aproximada, sem posições específicas por carro. Sem comparação equivalente no PCSX2.</p><button onclick="mode('lamps')">Classic</button><button onclick="mode('enhanced-lamps')">Car Lamps</button><button onclick="mode('enhanced-night')">Céu noturno + Car Lamps</button><script>function mode(m){for(const i of document.querySelectorAll('[data-prefix]'))i.src=i.dataset.prefix+'-'+m+'.png'}</script>'''
for car in cars:
 page+='<section><h2>'+html.escape(car)+'</h2>'
 for api in ('opengl','vulkan'):
  prefix=api+'-'+car;page+=f'<figure><figcaption>{api}</figcaption><img data-prefix="{prefix}" src="{prefix}-enhanced-lamps.png"></figure>'
 page+='</section>'
(out/'index.html').write_text(page,encoding='utf-8')
assert all(r['exit']==0 for r in records),records
print('PASS 24 native car/API inspections; no whole-frame parity or all-car certification')
