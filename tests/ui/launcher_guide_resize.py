from pathlib import Path
import os,subprocess,sys
from png_diff import read_png
exe,disc,out=sys.argv[1:4]
negative="--negative" in sys.argv[4:]
r=Path(out).resolve();r.mkdir(exist_ok=True)
for api in ['opengl','vulkan']:
 p=r/(api+'.ini');p.write_text('disc='+Path(disc).resolve().as_posix()+'\napi='+api+'\nsaves='+r.as_posix()+'/saves\ncar=ka_01\n',encoding='utf-8',newline='');Path(str(p)+'.video.ini').write_text('preset=Enhanced\ngraphics.renderer='+api+'\nlauncher.guide_seen=0\nlauncher.guided_intro=1\n',encoding='utf-8',newline='');script='@welcome RESIZE:1920x1080 @large RESIZE:640x480 @small RESIZE:1280x720 @restored ESCAPE';before=Path(str(p)+'.video.ini').read_bytes();script=script.replace('RESIZE:1920x1080','') if negative else script;e=dict(os.environ,SDL_AUDIODRIVER='dummy',MOON_WINDOW_HIDDEN='1',MOONSTUDIO_MUTE='1',MOONSTUDIO_GUIDE_SHOT=str(r/api),MOONSTUDIO_GUIDE_SCRIPT=script,MOONSTUDIO_FRONT_SHOT=str(r/(api+'-home.png')));z=subprocess.run([exe,'--launcher','--launcher-config',str(p)],env=e,capture_output=True,timeout=180);(r/(api+'.log')).write_bytes(z.stdout+z.stderr);assert z.returncode==0,(api,z.returncode,(z.stdout+z.stderr)[-2000:])
 assert Path(str(p)+'.video.ini').read_bytes()==before
 for name,size in [('large',(1920,1080)),('small',(640,480)),('restored',(1280,720))]:
  w,h,rows=read_png(r/(api+'-'+name+'.png'));assert (w,h)==size,(api,name,w,h)
  assert len({pixel for row in rows[h//3:2*h//3] for pixel in row})>200
 print(api,'resize/restore and return to launcher passed',flush=True)
