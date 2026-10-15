"""Generate an ignored build header from private, external publish.ini."""
import argparse
import configparser
import json
import re
from pathlib import Path
from urllib.parse import urlparse

def generate(version, config, output):
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version):
        raise ValueError("version must be X.Y.Z")
    c = configparser.ConfigParser(interpolation=None)
    c.read(config, encoding="utf-8")
    url = c.get("update", "url", fallback="")
    token = c.get("update", "token", fallback="")
    if url or token:
        u = urlparse(url)
        if u.scheme != "https" or not u.hostname or u.username or u.password or u.query or u.fragment or not url.endswith("/"):
            raise ValueError("update URL must be an HTTPS directory")
        if not token or any(ord(x) < 32 for x in token):
            raise ValueError("invalid update token")
    text = "/* Generated locally; never commit this file. */\n"
    text += "#define OPENGT4_VERSION_COMMAS " + version.replace(".", ",") + ",0\n"
    for name, value in [("OPENGT4_VERSION", version), ("OPENGT4_UPDATE_URL", url), ("OPENGT4_UPDATE_TOKEN", token)]:
        text += "#define " + name + " " + json.dumps(value, ensure_ascii=True) + "\n"
    Path(output).parent.mkdir(parents=True, exist_ok=True)
    Path(output).write_text(text, encoding="utf-8")

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("version")
    p.add_argument("config")
    p.add_argument("output")
    a = p.parse_args()
    generate(a.version, a.config, a.output)
