"""Optional live transport test; all infrastructure values stay external."""
import configparser
import json
import os
from pathlib import Path
import subprocess
import sys
import urllib.error
import urllib.request
exe,config,local=sys.argv[1:]
c=configparser.ConfigParser(interpolation=None);c.read(config,encoding="utf-8")
url=c["update"]["url"];token=c["update"]["token"]
def get(name,credential):
    return urllib.request.urlopen(urllib.request.Request(url+name,headers={"Authorization":"Bearer "+credential}),timeout=10)
with get("latest.json",token) as r:manifest=json.load(r)
for name in ["latest.json",manifest["setup"]]:
    for credential in ["","invalid"]:
        try:get(name,credential)
        except urllib.error.HTTPError as e:assert e.code==401
        else:raise AssertionError("unauthorized binary access")
env=dict(os.environ,MOON_TEST_UPDATE_URL=url,MOON_TEST_UPDATE_TOKEN=token)
newer=tuple(map(int,manifest["version"].split(".")))>tuple(map(int,local.split(".")))
subprocess.run([exe,"--network","available" if newer else "hidden"],env=env,check=True,timeout=90)
env["MOON_TEST_UPDATE_TOKEN"]="invalid"
subprocess.run([exe,"--network","hidden"],env=env,check=True,timeout=30)
print("PASS Oracle HTTPS authorization, native transport and silent check errors")
