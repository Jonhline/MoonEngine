"""Cold/warm/corrupt driver cache must preserve the synthetic frame."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

probe, output = map(Path, sys.argv[1:3])
output.mkdir(parents=True, exist_ok=True)
cache = Path(tempfile.mkdtemp(prefix="driver-", dir=output.resolve()))
negative = "--negative" in sys.argv[3:]
images = []
for stage in ("cold", "warm", "corrupt"):
    if stage == "corrupt":
        files = list((cache / "vulkan").glob("*.bin"))
        assert files
        for file in files:
            data = bytearray(file.read_bytes())
            data[:32] = bytes(32)
            file.write_bytes(data)
    env = os.environ.copy()
    env.pop("MOON_VK_NO_PIPELINE_CACHE", None)
    env["MOON_VK_CACHE_DIR"] = str(cache)
    if negative and stage == "warm":
        env["MOON_VK_NO_PIPELINE_CACHE"] = "1"
    image = output / (stage + ".rgba")
    run = subprocess.run([str(probe), "vulkan", str(image)], env=env,
                         capture_output=True, text=True, timeout=45)
    log = run.stdout + run.stderr
    image.with_suffix(".log").write_text(log, encoding="utf-8", newline="")
    assert run.returncode == 0 and "Validation Error" not in log, log
    match = re.search(r"pipeline cache: loaded (\d+) bytes", log)
    loaded = int(match[1]) if match else 0
    assert (loaded > 0) == (stage == "warm"), (stage, loaded)
    images.append(image.read_bytes())
assert images[0] == images[1] == images[2]
print("driver cache reused; invalid header rejected and rebuilt; output remains identical")
