# How OpenGT4 works

OpenGT4 is a native C reimplementation of Gran Turismo 4 for PC. It reads
the user's own disc image. The playable application does not execute PS2
CPU instructions or boot the console's operating system.

## Original scripts, native menus

GT4.VOL contains menu projects: widget trees, images, localized text and
compiled **Adhoc** scripts. `src/core/adhoc/` interprets that bytecode;
`src/core/menu.c` supplies engine objects and methods for widgets, car
selection, career data and navigation. Scripts decide screen behavior;
native UI code draws widgets and routes input.

Adhoc is the game's scripting language. Interpreting it is different from
emulating the console CPU. Unsupported engine methods and incomplete widgets
can still cause differences. See [Adhoc](ADHOC.md) and [Fidelity](FIDELITY.md).

## Native physics with original data

Vehicle dynamics and race logic in `src/core/physics/` were rewritten in C
from the original executable. Formulas, constants and update order are
compared against original-code runs. Covered physics comparisons require
bit-for-bit agreement; other systems have their own bounded tests and grades.
This does not certify every branch or complete game mode.

The solver retains the PS2 state layout. `phys.h` calls it a **transitional
layout**: a byte buffer stores original fields at their offsets, and
`gt4_ptr` resolves 32-bit addresses into that buffer. Tests can swap a C
function into an original step and compare resulting memory directly.

Native race initialization reads CORE.GT4 tables from the disc, alongside
car parameters, collision geometry and driving lines from GT4.VOL. The C
solver consumes them as data. Retaining addresses and executable tables
does not make the application a CPU emulator.

## The original as a test oracle

An **oracle** supplies the independent reference for a comparison.
`tools/recomp/` generates C for selected executable routines from a local
`extracted/core.bin`. Test targets combine those chunks with register,
memory and arithmetic support in `src/ee/`, then compare them with the native
implementation on controlled inputs. Some physics tests also start from
locally captured PCSX2 RAM states.

Positive cases establish agreement. Intentional negative cases show that
the comparator detects a changed value or missing behavior. Each result
applies only to its tested inputs, fields and declared tolerance.

The playable `opengt4` target in `CMakeLists.txt` does **not** link `src/ee/`
or generated original-code chunks. The game can be built with recompilation
tests disabled. That tooling is for analysis and verification only.

## Disc contents and distributed files

The user's disc supplies scripts, fonts, textures, models, courses, car
tables, sounds, music, movies and CORE.GT4 data. Analysis can produce local
unpacked files, disassembly, captures and generated comparison code.
Those outputs stay local.

The project distributes its implementation, host backends, original tools,
tests, documentation and third-party libraries with their license notices.
It distributes no Gran Turismo 4 data, console BIOS or proprietary executable.
See [Architecture](ARCHITECTURE.md) for the source map and
[Building and testing](BUILDING_AND_TESTING.md) for the build paths.
