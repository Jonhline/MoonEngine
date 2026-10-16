"""Exercise the actual PC entry point without a disc or a visible window."""
import pathlib
import subprocess
import sys

executable = pathlib.Path(sys.argv[1]).resolve()
negative = "--negative" in sys.argv[2:]
cases = [
    (["missing.iso", "--no-config", "--renderer", "invalid"], "--renderer expects"),
    (["missing.iso", "--no-config", "--renderer"], "--renderer expects"),
]
for arguments, expected in cases:
    result = subprocess.run([str(executable), *arguments], capture_output=True,
                            text=True, timeout=15)
    if negative:
        expected = "Vulkan scene rendering is ready"
    assert result.returncode == 1 and expected in result.stderr, (
        arguments, result.returncode, result.stdout, result.stderr)
print("Renderer startup: invalid command-line backend names rejected")
