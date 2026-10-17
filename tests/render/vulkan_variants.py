"""Recorded pipeline descriptions warm on linking; corruption falls back safely."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
probe, output = map(Path, sys.argv[1:3])
output.mkdir(parents=True, exist_ok=True)
cache = Path(tempfile.mkdtemp(prefix="variants-", dir=output.resolve()))
negative = "--negative" in sys.argv[3:]
images = []
for stage in ("cold", "warm", "corrupt"):
    if stage == "corrupt":
        files = list((cache / "vulkan").glob("*.variants"))
        assert files
        for file in files:
            data = bytearray(file.read_bytes())
            assert len(data) > 40
            data[-1] ^= 1  # A valid header must not admit corrupted recipe data.
            file.write_bytes(data)
    env = os.environ.copy()
    env.pop("MOON_VK_SKIP_VARIANTS", None)
    env.pop("MOON_VK_NO_PIPELINE_CACHE", None)
    env["MOON_VK_CACHE_DIR"] = str(cache)
    if negative and stage == "warm":
        env["MOON_VK_SKIP_VARIANTS"] = "1"
    frame = output / (stage + ".rgba")
    run = subprocess.run([str(probe), "vulkan", str(frame)], env=env,
                         capture_output=True, text=True, timeout=60)
    log = run.stdout + run.stderr
    frame.with_suffix(".log").write_text(log, encoding="utf-8", newline="")
    assert run.returncode == 0 and "Validation Error" not in log, log
    prepared = sum(map(int, re.findall(r"variants: prepared (\d+) pipelines", log)))
    assert (prepared > 0) == (stage == "warm"), (stage, prepared)
    images.append(frame.read_bytes())
assert images[0] == images[1] == images[2], "preparation changed output"
print("known variants prepared at link; corrupt metadata rejected; exact output preserved")
