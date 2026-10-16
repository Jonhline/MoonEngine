"""Publisher contract tests, using fixtures and a recorded SSH transport."""
import configparser
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"tools"))
from publish_update import manifest,publish
from configure_update import generate
with tempfile.TemporaryDirectory() as directory:
    root=Path(directory);setup=root/"MoonEngine-0.3.0-setup.exe";setup.write_bytes(b"fixture")
    m=manifest(setup,"0.3.0","Notes")
    assert m["size"]==7 and m["sha256"]==hashlib.sha256(b"fixture").hexdigest()
    latest=root/"latest.json";latest.write_text(json.dumps(m))
    c=configparser.ConfigParser(interpolation=None)
    c["ssh"]={"host":"example.invalid","user":"fixture","key":str(root/"key"),"directory":"/releases/moonengine"}
    calls=[]
    publish(setup,latest,c,lambda args,**kwargs:calls.append(args))
    assert [x[0] for x in calls]==["scp","scp","ssh"]
    assert calls[0][-1].endswith("setup.exe.incoming") and calls[1][-1].endswith("latest.json.incoming")
    assert calls[2][-1].index("setup.exe.incoming") < calls[2][-1].index("latest.json.incoming")
    assert all("StrictHostKeyChecking=yes" in x for x in calls)
    c["ssh"]["directory"]="/releases/../etc"
    try:publish(setup,latest,c,lambda *a,**k:None)
    except ValueError:pass
    else:raise AssertionError("unsafe destination accepted")
    private=root/"publish.ini";header=root/"generated.h"
    private.write_text("[update]\nurl=https://example.invalid/moonengine/\ntoken=fixture-token\n")
    generate("0.3.0",str(private),str(header))
    assert '#define MOON_VERSION "0.3.0"' in header.read_text()
    assert "fixture-token" in header.read_text()
    generate("0.3.0",str(root/"missing.ini"),str(header))
    assert '#define MOON_UPDATE_URL ""' in header.read_text()
    try:generate("0.3.0-alpha",str(private),str(header))
    except ValueError:pass
    else:raise AssertionError("unsupported version accepted")
print("PASS launcher publish: digest, upload order, atomic promotion, local config and invalid values")
