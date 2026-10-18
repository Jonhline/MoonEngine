#!/usr/bin/env python3
"""Build a private ARM64 debug APK with the official Android NDK and cached Gradle."""
import argparse,os,subprocess,sys,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk',type=Path,default=Path(os.environ.get('LOCALAPPDATA',''))/'Android/Sdk')
p.add_argument('--gradle',type=Path)
a=p.parse_args()
gradle=a.gradle or next(iter((Path.home()/'.gradle/wrapper/dists').glob('gradle-8.14.3-bin/*/gradle-8.14.3/bin/gradle.bat')),None)
if not gradle: raise SystemExit('Install Gradle 8.14.3 or pass --gradle PATH.')
sdl=ROOT/'build/_deps/sdl2-src'
if not (sdl/'include/SDL.h').exists():
 sdl=ROOT/'build/android-deps/sdl2-src';sdl.parent.mkdir(parents=True,exist_ok=True)
 subprocess.run(['git','clone','--depth','1','--branch','release-2.30.9','https://github.com/libsdl-org/SDL.git',str(sdl)],check=True)
notices=ROOT/'build/android-notices';notices.mkdir(parents=True,exist_ok=True)
for name in ['LICENSE','NOTICE']:
 shutil.copyfile(ROOT/name,notices/(name+'.txt'))
env=os.environ.copy();env['ANDROID_HOME']=str(a.sdk);env['ANDROID_SDK_ROOT']=str(a.sdk)
java=Path('C:/Program Files/Android/Android Studio/jbr')
if java.exists():env['JAVA_HOME']=str(java)
subprocess.run([str(gradle),'--no-daemon','--max-workers=4','-p',str(ROOT/'android'),'-PmoonSdl='+sdl.as_posix(),'-PmoonPython='+Path(sys.executable).as_posix(),':app:assembleDebug'],env=env,check=True)
print(ROOT/'android/app/build/outputs/apk/debug/app-debug.apk')
