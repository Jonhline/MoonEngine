# Architecture

MoonEngine separates game rules and disc formats from PC presentation and host
services. [How it works](HOW_IT_WORKS.md) explains execution; this document
maps it to source directories.

| Directory | Responsibility |
|---|---|
| `src/core/` | Disc readers, menus, career, storage and presentation data |
| `src/core/adhoc/` | Interpreter for the original menu scripts |
| `src/core/physics/` | Native dynamics, rival driving and race logic |
| `include/platform/` | Interfaces for files, video, sound, input and time |
| `src/platform/common/` | Shared host file and media storage |
| `src/platform/sdl/` | SDL input/audio, OpenGL, movies and scene assembly |
| `src/app/` | Launcher, loading, menu/race lifecycle and PC integration |
| `src/ee/`, `tools/recomp/` | Reference runtime and recompiler for tests |
| `tests/` | Synthetic fixtures, disc integration and original-code comparisons |

Menu projects supply widget trees, images, text and Adhoc bytecode. The
native bridge exposes engine methods to scripts and dispatches requests to
career, storage and race code. Backends turn presentation data into host output.

Physics retains the **original memory layout**: original 32-bit addresses and
field offsets resolved through `gt4_phys_mem` and `gt4_ptr` in `phys.h`.
This permits direct memory comparisons against the original. It is a data
representation, not CPU emulation in the game. Native setup loads CORE.GT4
tables and constructs the state consumed by the C solver.

Every live mode uses the same scalar arithmetic. At build time,
`tools/ps2_replay_float.py` lowers float operators in our native C through
`pycparser` to the existing integer EE arithmetic model. This preserves
operand order, including the multiplier's asymmetric low bits. Generated
project C stays under `build/physics_math/`; the build never consumes an
original executable or original-code output. `cmake/ReplayMath.cmake` also
builds a historical host-float variant solely for native GT4R v1 playback.
New recordings are GT4R v2; original GT4M imports and live races use EE math.
See [arithmetic evidence and limits](notes/DISC_REPLAY_ARITHMETIC.md).

The `moonengine` target in `CMakeLists.txt` links native core, physics, vehicle,
sound and platform libraries. It does not link `moon_ee`; reference test
targets may link that library and locally generated original-code chunks.

Host services belong behind platform interfaces. A new backend implements
these services while game rules, arithmetic and ordering remain independently
testable. Moving this preserved state to native structures is separate
from changing physics formulas.

No disc contents, extracted executable, reference captures or generated
original-code C are distributed. See [Building and testing](BUILDING_AND_TESTING.md)
and the per-item [fidelity record](FIDELITY.md).

## Runtime flow

1. `src/app/launcher.c` and the launcher helpers select the disc and PC settings.
2. `src/app/main.c` opens the disc, platform devices, presentation resources and menu projects.
3. `src/core/mproject.c` and the UI code load the authored screen/widget data. The Adhoc interpreter
   in `src/core/adhoc/` executes the disc's compiled `.adc` scripts. `menu.c` supplies the native
   objects and callbacks those scripts call: widgets, `main.game`, sound, storage and race setup.
4. A script requests a project or race sequence. The application resolves that request, prepares
   the native race and displays the loading and pre-race presentation in the existing order.
5. `src/app/race_game.c` coordinates controls, the fixed race steps, audio, cameras, HUD, pause,
   results and replay. `src/core/physics/race_run.c` provides the native race session; the solver
   and AI are implemented in the other files under `src/core/physics/`.
6. Race results and career state return through the menu/career interfaces. The application's
   loop resumes the authored quick menu or destination project.

The Adhoc scripts remain the source of menu decisions and authored sequences. Native adapters
provide the missing engine services rather than replacing each screen with a separate PC flow.
Reading a new menu therefore usually requires following both its disc script and the corresponding
host callback. See [ADHOC.md](ADHOC.md) for interpreter details.

## System map

| System | Entry points and implementation |
| --- | --- |
| Application and project dispatch | `src/app/main.c`, `main_support.inc`, `main_race_flow.inc` |
| Race/career return helpers | `src/app/main_session.inc` |
| Menu movies and music preparation | `src/app/main_movie.inc`, `main_audio.inc`, `pregrid_audio.c` |
| Disc and game formats | `src/core/vol.c`, `mproject.c`, `specdb.c`, `model.c`, `course.c`, `tex1.c`, `gpb.c` |
| Adhoc bytecode and VM | `src/core/adhoc/adhoc_code.c`, `adhoc_vm.c` |
| UI layout, drawing and motion | `src/core/ui.c`, `ui_motion.c`, `font.c`, `font_layout.c`, `rtext.c` |
| Menu runtime and bindings | `src/core/menu.c` and the private fragments listed below |
| Career, garage, events and tuning | `src/core/gtmode_game.c`, `career_*.c` |
| Saves, PS2 media and replay files | `src/core/save.c`, `save_state.c`, `save_ps2.c`, `ps2_media.c`, `replay_file.c` |
| Native race session and physics | `src/core/physics/race_run.c`, `session.c`, `step.c`, `driver.c`, `engine.c`, `tyre.c` and related files |
| Race presentation | `src/app/race_game.c`; `src/core/race_hud.c`, `race_result.c`, `race_pause.c`, `race_bspec.c`, `race_pitmenu.c` |
| Race audio | `src/app/race_audio.c`; `src/core/race_car_sound.c`, `race_sound.c`, `sound_driver.c`, `spu.c` |
| Photo Travel and Photo Drive | `src/app/photo_travel.c`; `src/core/photo_mode.c`, `photo_pause.c` |
| Hardware-facing interfaces | `include/platform/platform.h` |
| Windows/SDL/OpenGL backend | `src/platform/sdl/plat_sdl.c`, `pad_sdl.c`, `video_gl.c`, `render3d.c`, `race_view.c` |
| Shared filesystem/storage backend | `src/platform/common/`; Windows atomic replacement policy in `include/platform/windows_file.h` |

The public interfaces are the `.h` files next to their implementations and the platform header.
SDL-specific rendering and device types belong to the backend/application layers. Some native
physics data intentionally retains the original offsets in its memory arena; that is part of
the data contract, not an emulated PS2 execution loop. Float operation order matters for the
tests that compare native results against the original code.

## Private implementation fragments

`menu.c` and `race_game.c` each remain one translation unit. Their `.inc` files are textual
implementation fragments, included where their contents previously appeared. They are not
standalone compilation targets and must not be added as separate CMake sources. This preserves
static symbol visibility, initialization order and the tests that include a `.c` file directly.

The private menu layout is in `src/core/menu_internal.h`, included by the menu implementation.
The main fragments are:

| Fragment | Concern |
| --- | --- |
| `menu_widgets.inc` | Widget objects, callbacks and UI host methods |
| `menu_engine_objects.inc` | Script-facing engine services and page/event adapters |
| `menu_options.inc` | Option capture, restore and persistence |
| `menu_catalog.inc` | Course availability, Arcade catalogue and playlists |
| `menu_records.inc` | Course ranking, machine records and passcodes |
| `menu_car.inc` | Car data, paint, garage queries and script adapters |
| `menu_storage.inc`, `menu_save.inc` | Memory-card/replay storage and save entry actions |
| `menu_race_data.inc` | Race-data adapters and career/save binding refresh |
| `menu_quick.inc` | Quick-menu session, tuning and commands |
| `menu_license_requests.inc`, `menu_license_results.inc` | Licence requests and visit/result integration, kept at their existing positions |
| `menu_album.inc`, `menu_logger.inc` | Album/photo presentation and Logger integration |

The race fragments are `race_game_license.inc`, `race_game_replay.inc`, `race_game_bspec.inc`,
`race_game_pause.inc`, `race_game_controls.inc`, `race_game_cameras.inc`,
`race_game_pregrid.inc`, `race_game_logger.inc` and `race_game_hud.inc`. The HUD includes
`race_game_result_assets.inc` and `race_game_results.inc` at their original positions.
`race_cinematic.inc` handles the optional native cinematic camera. The application support fragments separate presentation helpers, session
returns, movie workers and music workers. `main_race_flow.inc` contains named helpers for race
returns, loading, update and drawing. Its state references alias the existing `main()` locals;
resource ownership and cleanup remain in `main()`.

## Build and run

Use Visual Studio 2022 Build Tools with the C/C++ toolchain and CMake. CMake fetches SDL2 and
miniz; movie support also uses FFmpeg. From the repository root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release --parallel
build/Release/MoonEngine.exe "D:/path/to/your/Gran Turismo 4.iso"
```

Running without arguments opens the launcher. `--save-dir <folder>` selects a separate profile
directory, useful for development. Do not use a personal save directory for destructive fixtures.
The tested disc is NTSC-U SCUS-97328 v2.00; see [the README](../README.md) for disc identification,
controls and current limitations.

## Tests and original-code oracles

Synthetic tests need no disc. To enable disc-backed tests, configure `MOON_TEST_DISC` with your
own image. The number of registered tests depends on the local inputs and CMake options.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -DMOON_TEST_DISC="D:/path/to/your/Gran Turismo 4.iso"
cmake --build build --config Release --parallel
ctest --test-dir build -C Release -j8 --timeout 600
ctest --test-dir build -C Release -N
ctest --test-dir build -C Release -R "^menu_navigation$" --output-on-failure
```

Some tests compare against original-code oracles and captured fixtures. `MOON_RECOMP_TESTS`
enables the optional recompiled test chunks; those need the user's local `extracted/core.bin`,
splat output and `MOON_PYTHON` with the required analysis dependencies. Generated C stays under
`build/`. `src/ee/`, `tools/recomp/` and `splat/` support these comparisons and analysis; they are
not the gameplay runtime. Their existence does not make the port an emulator or a statically
recompiled game.

In native gameplay, `race_run.c` installs `no_original` as the `gt4_orig_call` callback. It
handles two native virtual queries explicitly and reports unsupported calls; it never executes
original CPU code. Oracle tests can install the recompiled callback instead. The original Adhoc
menu scripts still execute in the native interpreter; this is separate from original CPU code.

CTest marks intentional negative cases with `WILL_FAIL` or runs a dedicated negative mode. A
passing negative test means that the checker rejected the deliberately wrong result. Tests
marked `RUN_SERIAL` still run one at a time even with `-j8`; disc and physical-session tests can
take several minutes. Keep full logs and compare the actual baseline failures when refactoring.

## Making a readability change

The root `.clang-format` uses LLVM style, four spaces and a 100-column limit. It preserves include
order, comment text, string literals and whitespace-sensitive CHECK arguments. Long authored
strings, source-reference comments and preserved CHECK arguments can therefore exceed the limit. Apply it only to project-owned `.c`, `.h` and `.inc` files in `src/`,
`include/` and `tests/`; do not format dependencies, disassembly or generated oracle code.

Move code with its source/offset comments, retain test-visible symbols and preserve all float
operations in their original order. Keep formatting separate from code movement in Git. Read
the fidelity rules in [FIDELITY.md](FIDELITY.md) before editing. Readability work does not raise a fidelity grade or resolve pre-existing defects.
