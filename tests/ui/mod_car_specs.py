"""Disc-backed additive specs, native display and race contracts. No game data shipped."""
import argparse, os, pathlib, re, struct, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('tools',type=pathlib.Path);p.add_argument('disc');p.add_argument('out',type=pathlib.Path);p.add_argument('--negative',action='store_true');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDL_AUDIODRIVER='dummy',MOON_OVERLAY=str((a.out/'no-mods').resolve()))
def run(tool,args,overlay=None,ok=True):
    e=dict(env)
    if overlay:e['MOON_OVERLAY']=str(overlay.resolve())
    r=subprocess.run([str((a.tools/(tool+'.exe')).resolve()),a.disc,*map(str,args)],env=e,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=100)
    assert (r.returncode==0)==ok,r.stdout+r.stderr
    return r.stdout
def row(label,table,at,overlay=None):
    text=run('gt4spec',[label,table,at],overlay)
    return bytes(int(x,16) for line in text.splitlines() if re.match(r'^  [0-9a-f]{3}:',line) for x in line.split(':',1)[1].split())
sources=['ka_01','m5_05','nsx_01','btr_86','evo6_wrc_99']
tables=[('ENGINE','0x58'),('CHASSIS','0x28'),('DRIVETRAIN','0x48'),('GEAR','0x50')]
stock={(label,t):row(label,t,at) for label in sources for t,at in tables}
survey=run('gt4car_survey',['--labels',*sources])
with tempfile.TemporaryDirectory(dir=a.out) as tmp:
    root=pathlib.Path(tmp)
    def export(label='ka_01',options=(),name='car',ok=True):
        target=root/name
        output=run('moonmod_addcar',[label,'moon_banana',target,'ford',50,'--model-name','moonbanana',*options],ok=ok)
        return target,output
    options=['--power','150','--torque','200','--torque-rpm','4500','--redline','7500','--weight','1100','--drivetrain','FR','--gears','6','--final','3.9','--ratios','3.5,2.2,1.6,1.2,1,0.8']
    target,output=export(options=options);print(output.splitlines()[0])
    chassis=row('moon_banana','CHASSIS','0x28',target)
    if a.negative:chassis=chassis[:6]+b'\0\0'+chassis[8:]
    assert struct.unpack_from('<H',chassis,6)[0]==1100 and struct.unpack_from('<H',chassis,12)[0]==1100,'physics/catalog weight mismatch'
    assert chassis[:6]==stock['ka_01','CHASSIS'][:6] and chassis[14:]==stock['ka_01','CHASSIS'][14:]
    engine=row('moon_banana','ENGINE','0x58',target);assert engine[0x51]==75 and engine[24:26]==stock['ka_01','ENGINE'][24:26]
    gear=row('moon_banana','GEAR','0x50',target);assert gear[0x24]==6 and gear[0x25]==0
    assert struct.unpack_from('<6H',gear)==(3500,2200,1600,1200,1000,800)
    assert struct.unpack_from('<3H',gear,0x18)==(3900,3900,3900)
    # The real disc scripts populate CarRoot, including RPM strings from appended UnistrDB.
    ui=run('mod_car_specs_ui',['moon_banana'],target);print(ui)
    run('mod_car_specs_ui',['moon_banana','--negative'],target,ok=False)
    native=run('mod_car_specs_probe',['moon_banana','--race'],target);print(native)
    m=re.search(r'PS=([\d.]+) Nm=([\d.]+) kg=(\d+) redline=(\d+)',native);assert m
    ps,nm,kg,red=map(float,m.groups());assert abs(ps-150)<0.5 and abs(nm-200)<0.099 and (kg,red)==(1100,7500)
    assert 'race fields: drive=0 gears=6 redline=7500 mass=1170.000' in native
    assert 'overall=13.64999' in native and 'race tick 1200:' in native
    for label in sources:
        for t,at in tables:assert row(label,t,at,target)==stock[label,t],(label,t,'source changed')
    assert run('gt4car_survey',['--labels',*sources],target)==survey
    print('All four stock parts rows of Ka/M5/NSX/BTR/Lancer and complete survey identical with/without package')
    for drive,kind in [('FF',1),('MR',0),('RR',0),('4WD',4)]:
        variant,_=export(options=['--drivetrain',drive],name=drive)
        code={'FF':1,'MR':3,'RR':4,'4WD':2}[drive]
        raw=row('moon_banana','DRIVETRAIN','0x48',variant);assert raw[0x10]==code
        text=run('mod_car_specs_probe',['moon_banana','--race'],variant)
        assert f'race fields: drive={kind} gears=5' in text,text
        print(f'{drive}: catalog={code}, native drive={kind}, 1200 race ticks passed')
    for flag,value in [('weight','264'),('weight','3501'),('weight','nan'),('weight','1000.5'),('torque','10000'),('torque-rpm','4501'),('redline','18100'),('redline','-100'),('drivetrain','AWD'),('gears','9'),('gears','6'),('final','0.5'),('ratios','3,4'),('ratios','3.5,'),('power','996')]:
        export(options=[f'--{flag}',value],name='invalid',ok=False)
    export(options=['--power','50','--torque','200','--torque-rpm','4500','--redline','7500'],name='conflict',ok=False)
    export(options=['--gears','5','--ratios','3,2,1'],name='count',ok=False)
    # Independent optional changes must not silently rescale the other fields.
    for opts,changed in [(['--weight','1200'],{'CHASSIS'}),(['--final','4.1'],{'GEAR'}),(['--torque','150'],{'ENGINE'}),(['--redline','8000'],{'ENGINE'}),(['--gears','4'],{'GEAR'})]:
        variant,_=export(options=opts,name='optional'+opts[0][2:])
        for t,at in tables:
            if t not in changed:assert row('moon_banana',t,at,variant)==stock['ka_01',t]
        run('mod_car_specs_probe',['moon_banana'],variant)
    print('PASS optional inheritance, rejected ranges/NaN/RPM granularity/ratios/incompatible peaks and deliberate UI negative')
