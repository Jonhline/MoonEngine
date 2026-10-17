# Building and testing

Run commands from the repository root. The development setup is Windows,
Visual Studio 2022 Build Tools with the C++ workload and Windows SDK, Git,
and CMake 3.20 or later. Python also needs `pycparser==3.0`
(`python -m pip install pycparser==3.0`) for native scalar physics arithmetic.
CMake fetches miniz 3.0.2, SDL2 2.30.9 and, for PC,
Khronos Vulkan-Headers v1.3.296, volk vulkan-sdk-1.3.296.0 and glslang 14.3.0;
the first configure needs network access. The Vulkan SDK is not required.
OpenGL and the experimental Vulkan renderer are selectable in the PC launcher;
see [PC graphics backends](VULKAN.md) for requirements and focused tests.

## Native application

```powershell
cmake -S . -B build-native -G "Visual Studio 17 2022" -A x64 -DMOON_RECOMP_TESTS=OFF
cmake --build build-native --config Release --target moonengine -- /m:1
.\build-native\Release\MoonEngine.exe "C:/discs/GT4.iso"
```

Replace the path with your own dump. NTSC-U SCUS-97328 v2.00 is the reference
edition; other revisions are not equally validated. With no disc argument
the program opens the launcher. `--save-dir <folder>` selects another storage
directory. Movie playback uses FFmpeg: put `ffmpeg` on PATH or set the
`MOON_FFMPEG` CMake cache entry to its executable.

Building only `moonengine` does not build all test programs. Build the default
target before running the registered suite:

```powershell
cmake --build build-native --config Release -- /m:1
ctest --test-dir build-native -C Release -N
ctest --test-dir build-native -C Release --output-on-failure
```

`-N` lists tests without running them. Registration depends on options and
local files. An omitted test has not passed.

## Linux

GCC or Clang, CMake 3.20+, the OpenGL development libraries, and `ffmpeg` on PATH for
movies; SDL2 and miniz are fetched by CMake, so the first configure needs network access.
Python 3 drives the application tests (`MOON_PYTHON`); the original-code oracles
additionally need an interpreter with `rabbitizer`.

```bash
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native -j4
ctest --test-dir build-native -j4
./build-native/moonengine "path/to/GT4.iso"
```

Measured 2026-10-16 (Ubuntu 24.04, GCC 13.3.0, CMake 3.28.3): every target builds,
`ctest` reports **102/102 passing** without a disc, and the engine opens the NTSC-U
v2.00 image and draws the arcade menu. `-DMOON_TEST_DISC=<image>` registers the
disc-based tests on Linux exactly as on Windows.

Known limits: the standalone disc-preparation launcher is still Windows-only (`Standalone
preparation currently requires Windows. Pass the ISO on the command line.`), and
`-DMOON_RECOMP_TESTS=ON` has not been exercised on this host. The tests and probes call
`_putenv_s()` unguarded; `src/platform/common/msvc_compat.h` maps that onto the POSIX call
(as MSVC does, an empty value removes the variable) and `CMakeLists.txt` force-includes it
on non-Windows builds only.

## Disc integration tests

Synthetic unit tests create their own inputs. Disc tests load real scripts,
tables and assets. Set `MOON_TEST_DISC` to register those tests:

```powershell
cmake -S . -B build-native -DMOON_TEST_DISC="C:/discs/GT4.iso"
cmake --build build-native --config Release -- /m:1
ctest --test-dir build-native -C Release --output-on-failure
```

Some presentation tests open windows or require OpenGL. Python application
tests also depend on `MOON_PYTHON`. Consult the relevant `add_test` and
conditions in `CMakeLists.txt` before selecting a subset with `ctest -R`.

## Intentional negative cases

A negative case changes a value, drops a layer or disables required behavior.
The comparator must reject it. Many are registered with `WILL_FAIL TRUE`:
an ordinary nonzero exit status is expected, so CTest reports success when
the comparison rejects the corrupted case. If it exits successfully, CTest
reports failure. A crash or timeout is not a valid negative result.

Run the positive case too and inspect diagnostics. Together they show that
the check detects an intentional error rather than merely failing to run.

## Optional original-code oracles

The game does not need the original-code reference runtime. Oracle development
needs Python with the recompiler dependencies (including `rabbitizer`), the
user's unpacked CORE.GT4 and local `splat/asm/core/text.s` output for targets
that depend on disassembly.

```powershell
python tools/unpack_core.py "C:/discs/GT4.iso"
cmake -S . -B build-oracle -G "Visual Studio 17 2022" -A x64 -DMOON_RECOMP_TESTS=ON -DMOON_PYTHON="C:/Python/python.exe" -DMOON_TEST_DISC="C:/discs/GT4.iso"
```

Unpacking writes `extracted/core.bin` and `extracted/core.elf`; it does not
generate disassembly. See [CORE.GT4](analysis/CORE_FORMAT.md) for local analysis
setup and [physics](notes/analysis/PHYSICS.md) for snapshot comparisons. Isolated
oracles can also build controlled inputs without RAM captures.

A small original-code test example:

```powershell
cmake --build build-oracle --config Release --target photo_film_oracle -- /m:1
ctest --test-dir build-oracle -C Release -R "^photo_film_(format|format_negative)$" --output-on-failure
```

Many oracles need both the disc and `core.bin`. Some tests register when
`core.bin` exists even without `MOON_RECOMP_TESTS`. Use separate directories
for native and oracle builds. Generated C, disc dumps, disassembly and
captures must never be committed or shipped.

The [fidelity record](FIDELITY.md) states each comparison's scope and tolerance.
A passing oracle does not certify every possible race or rendered frame.
