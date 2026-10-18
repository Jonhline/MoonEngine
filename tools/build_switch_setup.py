#!/usr/bin/env python3
"""Build the separate native Windows Switch installer, without the game target."""
import argparse
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def build(output):
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    tree = output.parent / "native-build"
    native_cmake = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "CMake/bin/cmake.exe"
    cmake = str(native_cmake) if native_cmake.exists() else "cmake"
    subprocess.run([cmake, "-S", str(ROOT / "tools/switch_setup"), "-B", str(tree),
                    "-G", "Visual Studio 17 2022", "-A", "x64"], check=True)
    subprocess.run([cmake, "--build", str(tree), "--config", "Release", "--parallel", "4"], check=True)
    binary = tree / "Release/MoonEngine-Switch-Setup.exe"
    subprocess.run([str(binary), "--self-test"], check=True)
    shutil.copy2(binary, output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    build(args.out)
