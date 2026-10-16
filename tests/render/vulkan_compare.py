"""Compare independently rendered GL/Vulkan fixture frames, including a negative."""
import os
import pathlib
import subprocess
import sys
import tempfile

executable = pathlib.Path(sys.argv[1]).resolve()
negative = "--negative" in sys.argv[2:]
with tempfile.TemporaryDirectory(prefix="moon-vulkan-") as temporary:
    frames = []
    for backend in ("opengl", "vulkan"):
        output = pathlib.Path(temporary) / (backend + ".rgba")
        env = os.environ.copy()
        env.pop("OPENGT4_GRAPHICS_API", None)
        result = subprocess.run([str(executable), backend, str(output)], env=env,
                                capture_output=True, text=True, timeout=45)
        print(result.stderr)
        assert result.returncode == 0, (backend, result.returncode, result.stdout)
        frames.append(bytearray(output.read_bytes()))
    assert len(frames[0]) == len(frames[1]) == 320 * 240 * 4
    if negative:
        frames[1][4 * (40 * 320 + 40)] ^= 255
    differences = [abs(a - b) for a, b in zip(*frames)]
    bad = sum(value > 2 for value in differences)
    print(f"GL/Vulkan: max channel difference {max(differences)}, channels >2: {bad}")
    assert bad == 0, "Vulkan fixture differs beyond two channel levels"
