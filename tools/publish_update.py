"""Package, compile and publish an Oracle update using external configuration."""
import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
from configure_update import generate
ROOT=Path(__file__).resolve().parent.parent

def manifest(setup,version,notes):
    with setup.open("rb") as file:
        digest=hashlib.file_digest(file,"sha256").hexdigest()
    return dict(version=version,setup=setup.name,sha256=digest,size=setup.stat().st_size,notes=notes)

def publish(setup,metadata,c,run=subprocess.run):
    host=c["ssh"]["host"];user=c["ssh"]["user"];key=Path(c["ssh"]["key"]).expanduser()
    folder=c["ssh"]["directory"]
    if not re.fullmatch(r"[A-Za-z0-9.-]+",host) or not re.fullmatch(r"[A-Za-z0-9_-]+",user):
        raise ValueError("invalid SSH host/user")
    if not re.fullmatch(r"/[A-Za-z0-9_./-]+",folder) or "/../" in folder+"/":
        raise ValueError("invalid remote directory")
    target=user+"@"+host
    options=["-i",str(key),"-o","BatchMode=yes","-o","StrictHostKeyChecking=yes"]
    for file in [setup,metadata]:
        run(["scp",*options,str(file),target+":"+folder+"/"+file.name+".incoming"],check=True)
    # Manifest is exposed last: readers never see metadata for a partial setup.
    q=shlex.quote
    command="mv -- "+q(folder+"/"+setup.name+".incoming")+" "+q(folder+"/"+setup.name)
    command+=" && mv -- "+q(folder+"/latest.json.incoming")+" "+q(folder+"/latest.json")
    run(["ssh",*options,target,command],check=True)

def main():
    p=argparse.ArgumentParser()
    p.add_argument("version")
    p.add_argument("--config",type=Path,default=Path(os.environ.get("APPDATA",str(Path.home())))/"MoonEngine"/"publish.ini")
    p.add_argument("--notes",default="")
    p.add_argument("--build",type=Path,default=ROOT/"build-release")
    p.add_argument("--no-upload",action="store_true")
    a=p.parse_args()
    if a.config.resolve().is_relative_to(ROOT): raise ValueError("publish.ini must be outside repository")
    c=configparser.ConfigParser(interpolation=None); c.read(a.config,encoding="utf-8")
    if "update" not in c: raise ValueError("external [update] configuration missing")
    generate(a.version,str(a.config),str(a.build/"generated"/"launcher_release.h"))
    subprocess.run(["cmake","-S",str(ROOT),"-B",str(a.build),"-DMOON_VERSION="+a.version,
                    "-DMOON_UPDATE_CONFIG="+str(a.config),"-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
                    "-DSDL_FORCE_STATIC_VCRT=ON","-DMOON_LINKED_MOVIES=ON","-DMOON_DEV_BUILD=OFF"],check=True)
    subprocess.run(["cmake","--build",str(a.build),"--config","Release","--target","moonengine"],check=True)
    command=[sys.executable,str(ROOT/"tools/package_release.py"),a.version,"--build",str(a.build/"Release"),
             "--repo",c.get("package","page",fallback=c["update"]["url"])]
    for option in ["icons"]:
        value=c.get("package",option,fallback="")
        if value:command +=["--"+option,value]
    subprocess.run(command,check=True)
    iscc=c.get("package","iscc",fallback="ISCC.exe")
    subprocess.run([iscc,"/DVersion="+a.version,str(ROOT/"tools/installer/moonengine.iss")],check=True)
    setup=ROOT/"dist"/("MoonEngine-"+a.version+"-setup.exe")
    metadata=ROOT/"dist/latest.json"
    metadata.write_text(json.dumps(manifest(setup,a.version,a.notes),ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    if not a.no_upload:publish(setup,metadata,c)
    print("Release generated" if a.no_upload else "Release published")

if __name__=="__main__":main()
