"""Opt-in, isolated proof of the production installer's Files/Run behavior."""
import argparse
import configparser
import os
from pathlib import Path
import re
import subprocess
import time
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
p.add_argument("version")
p.add_argument("--config",type=Path,default=Path(os.environ["APPDATA"])/"MoonEngine/publish.ini")
a=p.parse_args()
assert re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+",a.version)
root=ROOT/"build/launcher-install-proof";root.mkdir(parents=True,exist_ok=True)
install=root/"installed";install.mkdir(exist_ok=True)
marker=install/"existing-save.fixture";marker.write_bytes(b"keep this save")
s=(ROOT/"tools/installer/moonengine.iss").read_text()
s=s.replace("AppId={{6B0E5C2A-4F1D-4B8E-9A47-0D3C5E2F7A91}","AppId=MoonEngineUpdaterIsolatedTest")
s=s.replace("AppName=Moon Engine","AppName=Moon Engine Updater Test").replace("DefaultGroupName=Moon Engine","DefaultGroupName=Moon Engine Updater Test")
s=s.replace('#define Source "..\\..\\dist\\MoonEngine-" + Version + "-win64"', '#define Source "'+str(ROOT/"dist"/("MoonEngine-"+a.version+"-win64"))+'"')
s=s.replace("SetupIconFile=..\\..\\res\\moonengine.ico","SetupIconFile="+str(ROOT/"res/moonengine.ico"))
# Same Files/Run; a separate AppId and no shortcuts isolate the test.
s=s[:s.index("[Icons]")]+s[s.index("[Run]"):]
script=root/"test.iss";script.write_text(s)
c=configparser.ConfigParser(interpolation=None);c.read(a.config,encoding="utf-8")
subprocess.run([c["package"]["iscc"],"/DVersion="+a.version,"/O"+str(root),"/Ftest-setup",str(script)],check=True,stdout=subprocess.DEVNULL)
exe=install/"MoonEngine.exe"
ps="C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"
query="Get-CimInstance Win32_Process | Where-Object {$_.ExecutablePath -eq '"+str(exe).replace("'","''")+"'}"
def close_test_game():
    subprocess.run([ps,"-NoProfile","-Command",query+" | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }"],check=True)
try:
    for cycle in range(2):
        subprocess.run([str(root/"test-setup.exe"),"/VERYSILENT","/CURRENTUSER","/SUPPRESSMSGBOXES","/NORESTART","/DIR="+str(install)],check=True,timeout=60)
        found=""
        for i in range(15):
            found=subprocess.check_output([ps,"-NoProfile","-Command",query+" | Select-Object -ExpandProperty ProcessId"]).decode().strip()
            if found:break
            time.sleep(1)
        assert found,"silent setup did not reopen game"
        assert marker.read_bytes()==b"keep this save"
        close_test_game()
    print("PASS isolated Inno: install/overwrite, retained file and native relaunch")
finally:
    close_test_game()
    uninstaller=install/"unins000.exe"
    if uninstaller.exists():subprocess.run([str(uninstaller),"/VERYSILENT","/SUPPRESSMSGBOXES","/NORESTART"],check=True,timeout=60)
