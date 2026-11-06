from pathlib import Path
import subprocess,os,sys
sys.path.insert(0,'tests/ui');from png_diff import read_png
exe,disc,output = sys.argv[1:4]
negative='--negative' in sys.argv[4:]
r=Path(output).resolve();r.mkdir(parents=True,exist_ok=True);p=r/'launcher.ini'
for api in ['opengl','vulkan']:
 p.write_text('disc='+Path(disc).resolve().as_posix()+'\napi='+api+'\nsaves='+r.as_posix()+'/saves\ncar=gt40_race_69\nscene=laguna\n',encoding='utf-8',newline='');Path(str(p)+'.video.ini').write_text('preset=Enhanced\ngraphics.renderer='+api+'\nlauncher.guide_seen=1\n',encoding='utf-8',newline='')
 for mode in ['baseline','closed','open','title']:
  e=dict(os.environ,SDL_AUDIODRIVER='dummy',MOON_WINDOW_HIDDEN='1',MOONSTUDIO_MUTE='1',MOONSTUDIO_LAYOUT_TRACE='1',MOONSTUDIO_FRONT_SHOT=str(r/(api+'-'+mode+'.png')))
  if mode!='baseline':e['MOONSTUDIO_OPTIONS']='0'
  if mode=='title':e['MOONSTUDIO_OPTIONS']='2';e['MOONSTUDIO_OPTIONS_ROW']='8'
  if mode=='closed':e['MOONSTUDIO_OPTIONS_CLOSE_AT']='1'
  z=subprocess.run([exe,'--launcher','--launcher-config',str(p)],env=e,capture_output=True,timeout=180);print(api,mode,z.returncode,flush=True);assert z.returncode==0
  log=(z.stdout+z.stderr).decode('utf-8','replace');(r/(api+'-'+mode+'.log')).write_text(log,encoding='utf-8')
  if mode=='title':
   import re
   tops=re.findall(r'STUDIO_GUIDE_TOP ([\d.]+) ([\d.]+)',log);assert tops
   assert all(float(top)>float(edge)+2 for top,edge in tops),tops[-1]
 w,h,a=read_png(r/(api+'-baseline.png'));_,_,b=read_png(r/(api+'-closed.png'));diff=[abs(a[y][x][c]-b[y][x][c]) for y in range(int(h*.2),int(h*.53)) for x in range(int(w*.5),int(w*.95)) for c in range(3)];
 if negative:diff[0]=255 # A persistent text shadow must be rejected.
 assert max(diff)==0, ('panel residue',api,max(diff),sum(diff)/len(diff))
print('PASS: no residual labels on either API; title ink below the tab divider')
