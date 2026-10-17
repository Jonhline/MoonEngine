# Nintendo Switch

MoonEngine runs as a native ARM64 homebrew application. The user has reached
menus and races on hardware; native deko3d improves performance but has reported
material defects under investigation. Cross builds and host checks alone do
not establish console image correctness or complete floating-point fidelity.
Windows keeps its existing launcher and renderer paths.

## Native renderer status (2026-10-14)

Switch builds select an experimental deko3d backend by default. Windows still
compiles the existing OpenGL renderer. Earlier native builds now open on the
console, but reported checker bands, dark/wrong car materials and subsequent
flicker remain visually unvalidated for the latest repairs. The new boot
transfer/particle/blend checks await hardware execution. Mesa measurements
below are historical; native log windows of 30.45-54.43 FPS do not establish
sustained 60 FPS for races.

For the native tree use `-B build-switch/deko -DMOON_SWITCH_RENDERER=deko3d`.
Use `-DMOON_SWITCH_RENDERER=mesa` explicitly to retain the previous Switch
renderer. The SDK already includes deko3d and UAM; no game assets are embedded.
Only shaders written for this project are compiled into the executable.

## Tools and build

Install the signed [official devkitPro updater](https://github.com/devkitPro/installer/releases)
in `C:\devkitPro`, selecting Switch Development. In its MSYS2 terminal:

```sh
pacman -S --needed switch-dev switch-sdl2 switch-mesa switch-glad ninja
cd /f/MoonEngine
cmake -S . -B build-switch/nro -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/Switch.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DMOON_SWITCH_ICON="$PWD/build/switch-setup/moonengine.jpg"
cmake --build build-switch/nro -j8
```

`DEVKITPRO` selects a different SDK installation. The wrapper uses the SDK's
[official Switch toolchain and packaging helpers](https://github.com/devkitPro/pacman-packages/tree/master/cmake/switch).
The icon argument must point to a local 256x256 RGB JPEG. This installation
converted the user's `Documents/MoonEngineico.png` into the ignored build folder.
No SDK, console keys, game data or packaged executable belongs in Git.

The build outputs `moonengine.nro` and the independent `switch_fpu_probe.nro`
under `build-switch/nro`. It uses packaged SDL2 and Mesa/EGL with glad,
OpenGL 3.3 core, and disables FSR.
Videos return unavailable through the
existing non-Windows movie backend. Direct FFmpeg decoding is a later milestone.
Native libraries and application code use the same disc readers and game rules.
Missing C declarations encountered by the ARM compiler were fixed explicitly.

## SD layout and first launch

Copy `moonengine.nro` into `sdmc:/switch/moonengine/`. Put your own disc dump at
`sdmc:/switch/moonengine/gt4.iso`. A single GT4 ISO exceeds FAT32's 4 GiB file
limit, so this single-file workflow needs exFAT. It does not include an ISO
split-file reader.

Open Homebrew Menu by holding R while launching a game (application mode,
full RAM). Album applet mode does not provide enough memory for this port.
Select MoonEngine. The application creates `config.ini` if missing:

```ini
disc=sdmc:/switch/moonengine/gt4.iso
saves=sdmc:/switch/moonengine/
show=0
```

The same file accepts `vsync=1`, `upscaling=0`, `ui_filter=0`,
`text_sharpness=0`. Switch always uses 16:9, including old files containing
`aspect_ratio=0`; Windows retains its aspect-ratio selection. Existing video-setting saves
preserve the disc and save-directory fields. Profiles, replay files, photos,
game options and the startup log live below the selected SD save directory;
`moonengine.log` itself remains in `sdmc:/switch/moonengine/`.

The renderer starts at 1280x720 handheld or 1920x1080 docked. devkitPro SDL
handles subsequent docking changes on its resizable window; its advertised
desktop size alone is not reliable at handheld startup. Controllers use SDL
GameController with positional labels: Nintendo A = Circle, B = Cross,
X = Triangle, Y = Square. Paired Joy-Cons and Pro Controller must be checked
on hardware. The standard PlayStation assignments remain in game options.

Place the independent probe at
`sdmc:/switch/switch_fpu_probe/switch_fpu_probe.nro`, in its own folder.
Homebrew Menu treats a folder containing a same-named NRO as an application
shortcut, hiding other NROs in that folder. The delivery ZIP separates the
probe from `moonengine` so both entries are visible.
Run `switch_fpu_probe.nro` before the game and report its PASS/FAIL message.
It checks [ARM FPCR](https://documentation-service.arm.com/static/5e7e16dfb2608e4d7f0a2f8b)
rounding toward zero and flush-to-zero, an arithmetic
rounding case and a denormal input. Use `nxlink --args "--negative"` with the probe to inject its negative case.
A `--negative` argument deliberately sets
rounding to nearest and must report FAIL. Startup performs the same check and
refuses to continue on failure. Inspection of the linked ARM code confirms
that SDK `fesetround(3)` writes FPCR bits 23:22 as `11`, and the host adapter
sets FZ bit 24. Neither variant has been executed on hardware
at milestone 1; physics fidelity on ARM remains unproved.

## Save replacement on SD

Switch fsdev routes `rename` to `fsFsRenameFile`; it cannot be used as the
POSIX overwrite operation expected by the desktop save writer. The supplied
SD listing contains valid-size `active.ogt4`/slots alongside `.tmp.4294967295`
and backup temporaries. Loading a profile also rewrites its active marker,
so failed replacement can leave an existing profile inactive at startup.
The config points to `sdmc:/switch/moonengine/`; no path migration is required.

Switch-only replacement moves an existing destination to `.replace-old`,
installs the fully written temporary file, commits the filesystem and removes
the rollback file after success. Failed installation restores the old name;
failed restoration retains its complete bytes at `.replace-old`. Reads restore
a rollback file when the destination is missing. This is recoverable SD
replacement, not power-loss atomicity. Windows/POSIX replacement paths and the
save record format remain unchanged. Preferences/video settings use the same
Switch helper. New save temporaries use the system tick and existence checks:
libnx's unavailable getpid returned -1, giving the repeated 4294967295 suffix.
Old pending `.tmp.*` files are preserved, not automatically imported or deleted.

The host fixture models rename rejecting existing destinations and exercises
real profile creation, reopening, repeated updates/backup rotation, failed
installation, failed commit before/after installation, failed rollback and
subsequent recovery. A negative rejects the destination-to-rollback move.
Six focused save/storage tests passed, including negatives; Windows Release
and both NROs built. The next console log reports active=1 and three readable
slots, and the user confirms saves now work. Recovery of the contents of older
pending temporaries still needs their individual validation. Startup logs the selected directory,
active slot/error and each slot's read status without publishing its contents.

## Network launch

Put the Switch and PC on the same network and activate nxlink in Homebrew
Menu (Y in versions that expose the netloader shortcut). From Windows:

```powershell
C:/devkitPro/tools/bin/nxlink.exe -a SWITCH_IP build-switch/nro/moonengine.nro
```

The disc and save directory still belong on SD. Use the local startup log to
report initialization failures. No network launch has been performed yet.

## Remaining milestones

1. Build NRO: completed locally; hardware execution pending.
2. Open the main menu on Switch with videos skipped.
3. Enter and drive an Arcade race, checking sound and controller input.
4. Add in-process libavcodec through switch-ffmpeg; no child process.
5. Measure menus, Tsukuba and a heavier course, handheld and docked, and
   identify CPU/GPU/loading/audio bottlenecks from measurements.

Pause and report after each milestone. Only after milestones 1-3 should an
installable NSP be packaged with hacBrewPack. Select and verify a project
Title ID without an official-title collision. The user's locally extracted
`prod.keys` path will be requested at that stage and stays outside the repository;
key filenames and NSP outputs are ignored. Installation uses the user's
existing DBI/Tinfoil/Goldleaf setup and compatible CFW signature patches for
unsigned homebrew.
No NSP has been generated or validated in this first milestone.

### Console-created HOME shortcut

The user prefers creating the shortcut on the console, as in their existing
homebrew package. Sphaira supports selecting Moon Engine in Homebrew, pressing
X for Options, then Install Forwarder. Its
[homebrew menu implementation](https://github.com/NaGaa95/sphaira/blob/master/sphaira/source/ui/menus/homebrew.cpp)
reads the NRO's name and embedded icon for the installation form. This is
an alternative to exporting keys for PC packaging, subject to the installed
Sphaira version and its setup. No console shortcut has been validated here.
Keep `switch/moonengine/moonengine.nro` in place after creating the shortcut.

The user's supplied cover replaces the local 256x256 JPEG embedded in the
NRO; it is not included in the repository. A forwarder created before the
icon change may need to be recreated to refresh its copied icon.

## First hardware feedback and performance diagnosis

The user reached the application and loaded a race, but reports severe
slowness in showroom, GT Mode and race 3D. Initial asset loading also stalls;
returning to cached content can improve responsiveness. No quantitative FPS
or console log has been supplied yet, so neither the bottleneck nor a
performance fix is established. Milestones 2-3 remain awaiting complete
menu/controller/race/audio validation.

### Baseline measurement build

The stage profiler uses libnx `armGetSystemTick` and `armTicksToNs`. It writes
one aggregate per two seconds to `switch/moonengine/moonengine.log`, plus context
changes and individual load events. It does not print for each frame. Reports
include frame mean/maximum, FPS, project/page/car/course, handheld/docked mode
and drawable size. Nested spans charge exclusive CPU wall time for the native
physics step, scene traversal, mesh and 2D GL submission, swap, device audio
mix, music synthesis, race audio control, stdio reads, inflate, asset preparation
and menu advancement/pacing. Independent threads overlap, so column sums are
not a frame duration. Submission spans include CPU batching/culling and driver
backpressure; they are not isolated GPU times. GL work outside the measured
mesh/2D boundaries remains in its parent scene time or uninstrumented time.
No new GPU synchronization or readback is used. Switch disables the desktop
`MOON_VIDEO_PROFILE` path that forces `glFinish`.

`Switch load` events measure project/root preparation and race/showroom asset
workers; nested loads retain their own names and inclusive elapsed time.
`Switch ready` records each observed page/car/course visit from its first
presentation request to a submitted frame with its observed CarFaces ready and
loading dismissed. This is not a button-to-photon measurement: earlier menu
script preparation is reported separately, and GPU scanout is not measured.
Visits are counted within a process; restart loses the in-memory assets and
visit counters. Shader compile/link, mesh upload and slow VOL read logs remain
available. First-use initialization is intentionally retained for this baseline.

The current SDK Release build uses `-O2 -DNDEBUG -march=armv8-a+crc+crypto
-mtune=cortex-a57`; it does not currently enable LTO or an explicit `-mcpu`.
The measurement round leaves these flags unchanged. Physics still uses
`-ffp-contract=off -frounding-math`. Audio synthesis/device mixing and race/
showroom asset loading already use SDL threads; core affinity has not been
changed. Prepared texture-binding and buffered-I/O experiments are saved
locally and excluded from this baseline. Resolution, MSAA, mip behavior,
shader initialization and game simulation remain at the earlier reference.
FSR remains unavailable on Switch. No boost, persistent shader/texture cache,
new read-ahead or background menu preparation has been enabled yet.

#### Reproducible console procedure

1. Replace only `switch/moonengine/moonengine.nro` on **1: SD Card** in DBI MTP.
   Keep the ISO, saves and config. Exit MTP/DBI, then launch Moon Engine through
   application-mode Homebrew Menu (hold R over a game).
2. Handheld: leave the main menu idle for 20 seconds. Enter the same showroom
   twice, returning between visits; wait for the car and remain for 20 seconds.
   Enter GT Mode twice as well. Record the selected car.
3. Arcade: drive Tsukuba for at least 20 seconds after loading, then drive
   Nürburgring Nordschleife for the same duration, using the same car/settings.
   Repeat a course load to compare first and second asset preparation.
4. Exit the application, enter DBI MTP, copy `switch/moonengine/moonengine.log` to
   the PC and rename the copy `switch-handheld.log` **before relaunching**.
5. Repeat the same scenes docked and copy `switch-docked.log`. A second full
   launch can separately check cold-process loading; keep its log too.

#### First measured baseline and GPU work

The supplied log contains handheld 1280x720 operation only (Mesa nouveau NV120,
performance configuration 0x20003). FPCR startup check passed. There is one
Arcade race, Camaro SS '69 at Tsukuba; choosing Nürburgring in the menu is not
execution of that course. GT Mode and docked samples are absent.

The following summaries weight each report by its frame count. Windows with
maximum frame time >=500 ms are excluded from the steady-state comparison;
the Tsukuba row additionally requires physics activity, excluding held/paused
windows. This filter is explicit and does not erase the loading stalls.

| Handheld baseline sample | Frames | Mean frame ms | FPS (1000/mean) |
|---|---:|---:|---:|
| Main menu, initial TopRoot | 700 | 16.740 | 59.74 |
| Camaro showroom | 45 | 64.988 | 15.39 |
| Tsukuba, windows with physics activity | 214 | 166.761 | 6.00 |

For those Tsukuba windows, exclusive physics averaged 5.317 ms/frame, scene
work 126.114 ms, measured GL submission 31.268 ms and swap 2.234 ms. Audio
synthesis consumed 22.562 ms/frame on its overlapping worker; it cannot be
added to main-thread frame duration. Scene time includes GL outside the measured
mesh boundaries. The maximum recorded frame was 2474.788 ms. The model shader
first compile/link took 393.94 ms. A Camaro asset read/decode took 184.293 ms;
first observed showroom request-to-ready was 922.339 ms versus 498.915 ms on
return. These are observation boundaries, not input-to-photon latency.

Source inspection identifies six synchronous glReadPixels calls per race
frame in course_reflection -> r3d_end_target, followed by CPU sphere-map
conversion and another texture upload. Showroom also composed four room images
and reflection panels on the CPU every frame. Shadow/cast meshes uploaded large
texture arrays even when their geometry referenced very few entries.

The next Switch build retains the six environment views but copies them into
a resident GPU atlas, remaps the same sphere-map formula in a fragment shader
and borrows that GPU texture for the cars. It performs no CPU framebuffer
readback on this path. Room/panel composition similarly uses four resident
textures and a GPU weighted sum. Native mip levels and geometry order remain;
mesh upload skips unreferenced textures, including checking alternate wheel
indices, and a shared child falls back to its own upload if its texture was
absent from the parent. Failed GPU initialization retains the CPU fallback.

The principal model, new composition and environment shaders are warmed behind
the boot loading screen. Remaining lazy specialized programs (wheel accumulation,
CarFace layers, shadow, sprites and image filtering) are not yet all warmed.
Persistent Mesa shader cache is not claimed: the official SDK
[20.1.0 Switch patch](https://github.com/devkitPro/pacman-packages/blob/master/switch/mesa/switch-mesa-20.1.0-5.patch)
explicitly disables shader-cache support for Horizon in meson.build. Merely
setting MESA_SHADER_CACHE_DIR cannot enable the feature in these libraries.
A patched SDK or application-level program cache needs separate implementation
and validation. Persistent converted texture cache, read-ahead, affinity/boost,
LTO trials and lower internal resolution remain subsequent measured steps.

Framebuffer binding queries remain at temporary render-target boundaries to
preserve nested Photo/CarFace targets; removing all GL state queries requires
a tested binding/state tracker. Six synchronous pixel readbacks are removed
from the successful GPU reflection path, not all outstanding rendering costs.

The host GPU fixture compares all channels against the former CPU sphere-map
and four-layer formulas (one-byte tolerance), across 12 yaw/pitch camera frames
and 24 weight sets, including saturation. It also compares a reflective model
using CPU versus borrowed GPU environments, checks an unused texture is not
uploaded and verifies a shared child can upload a missing parent texture.
The negative corrupts a compared channel. This is a port-path comparison, not
PS2 rasterization fidelity. Eight focused renderer/capture checks passed with
negatives; Windows Release and the ARM64 NRO are rebuilt. Console gains remain
unmeasured until the same handheld route is repeated with this build.

The NRO cross build and Windows Release application build completed. Eleven
focused host tests passed (profiler, renderer, capture and race audio), including
negative cases. No complete suite was run. The existing ui.c const-qualifier
warning is outside this diagnostic change.

The host fixture checks exclusive nested timing, window FPS/mean/maximum,
context visits, pending CarFaces, nested load events and two simultaneous
workers with thread-local stacks/atomic counters. Its negative changes the
expected disc time and must fail. These checks establish diagnostic accounting,
not Switch performance or PS2 fidelity.

## Validation for milestone 1

The cross build generates both NROs with project NACP metadata and the local
icon. The Windows application and affected test targets are rebuilt separately.
A final host test run excludes tests taking at least 30 seconds in the latest
full-suite timing report; the omitted names are retained locally in
`build/switch-setup/slow-tests.txt`. Console playback and ARM numerical tests
are not inferred from the host results.


The final Windows rebuild completed. Seven focused launcher/controller/video
tests passed. The single final host run passed 523 of 524 tests in 495.24
seconds (20 slow tests omitted). `quick_options_app_return` alone failed
because its fixed ten-second capture did not observe the second PCM start;
its destination and return rendering assertions reached the expected route.
The unchanged positive and negative both passed when repeated separately
(2/2, 29.67 seconds). This intermittent timing result is recorded, not counted
as a clean full-suite pass. No host assertions were weakened.

The diagnostic NRO cross build completed; seven focused Windows video/renderer
tests passed, including their existing negative cases. No complete suite was
rerun for this diagnostic-only round. Hardware timings and the aspect-ratio
result remain pending. When configuring from devkitPro MSYS2 on Windows,
`-DPython3_EXECUTABLE=/f/MoonEngine/.venv/Scripts/python.exe` selects this checkout's
host interpreter for the locally generated release header.

### Second handheld sample and next render budget

The next console run confirms successful GPU warm-up and profile activation.
It includes Focus ST170 and Camaro SS '69 races at Tsukuba, still handheld.
Using the same frame-weighted filter above (maximum <500 ms, >=8 frames),
the Camaro showroom changed from 64.988 to 49.937 ms/frame: 15.39 to 20.03 FPS
(70 frames in the second sample). This is consistent with the user's report
of a modest improvement, not a playable-performance result.

Physics-active Camaro race windows average 148.215 ms/frame (211 frames,
6.75 FPS), with physics 4.512 ms, scene 45.771 ms, measured GL submission
94.541 ms and swap 0.262 ms. The opponent grid differs from the baseline,
so this is not a controlled speedup measurement. The Focus sample averages
152.894 ms/frame (123 frames, 6.54 FPS). Moving synchronous reflection work
off the CPU shifts driver waiting into GL spans; these spans still do not
isolate GPU execution time. Physics is a small part of either frame.

Arcade GameRoot's first observed request-to-ready takes 1349.991 ms; a frame
in that window takes 1352.472 ms, predominantly scene preparation rather
than disc/inflate. CategoryRoot has a separate 1717.631 ms first visit.
The first Ka showroom has a 1055.67 ms submission stall. These are distinct
first-use stalls, not a measured delay in the authored animation itself.
Font decoding/SDF creation now have nested load markers to identify how
much of the UI preparation is repeated font processing; a font/texture disk
cache is still pending rather than assumed to solve the stall.

The next build limits the race/full showroom 3D scene to 960x540 for a
1280x720 output, or 1280x720 for 1920x1080, with linear enlargement and
ordinary single-sample renderbuffers. UI/HUD draws afterward at native size.
Existing CarFace service offscreen layers receive the same size cap and
retain their destination viewport/clipping. Ordinary CarFaces drawing
straight over a page are not yet redirected to a reduced target. This
explicit P adaptation reduces 3D detail and antialiasing, not simulation.
There is no per-frame GL_MAX_SAMPLES query on the Switch scene path.

All eleven enabled GL programs are initialized at boot: UI, model, CarFace
composition, wheel accumulation, shadow cast/filter, target composition,
sprites, image filtering, room mixing and environment remapping. The warm-up
exercises real paths on a 16x16 temporary target without readback or forced
completion; FSR/launcher shaders are unavailable in this build. It is not a
claim that every driver-internal variant will be precompiled. The host fixture
observes nine programs before the two Switch composition programs, then checks
that a second warm-up creates none. Further compile events log their program
index and elapsed time, only when a program is created.

The GPU fixture checks actual single-sample storage (multisample storage with
a request of one can be rounded up by a driver), scene/CarFace target sizes,
page-color composition and restoration of the native UI viewport. Its added
negative deliberately changes the expected width. Fifteen focused renderer,
profiler, capture and screen-setting checks passed, including negatives.
Windows Release and the ARM64 NRO built. This render-budget build still needs
the same showroom/race route on the console to establish its FPS and first-use
stall changes. No new hardware gains are inferred from host tests.

#### Menu preference replacement

The same second log contains three `failed to save game option preferences`
messages despite successful profile activation. `menu_options.inc` still used
POSIX overwrite-style rename for `config.ini.game_options`, separately from
the launcher/profile writers. Its Switch branch now uses the existing recoverable
replacement helper and restores an interrupted rollback before reading. Windows
and the preference-file syntax/path are unchanged. Host helper fault tests and
menu option lifecycle/profile tests validate their respective paths; the Switch
menu integration still needs the next console log (absence of this write error
and preferences retained after a restart).

### Third handheld log: render budget and Arcade preparation

The next log confirms all eleven GL programs compiled during boot (560.231 ms
for warm-up), with none compiled later in that run. Same steady-window filtering
and frame weighting as above yields:

| Scene | Frames | Mean ms | FPS |
|---|---:|---:|---:|
| Arcade Ka showroom | 377 | 21.098 | 47.40 |
| Arcade Focus showroom | 152 | 21.839 | 45.79 |
| Ka race at Laguna Seca, physics active | 1813 | 46.909 | 21.32 |
| GT Mode map | 773 | 33.272 | 30.06 |
| Used-car list | 686 | 16.658 | 60.03 |
| CR-X purchase CarFace | 178 | 38.851 | 25.74 |

The race differs from the earlier Tsukuba/Camaro reference, so it is not a
controlled race speedup. Its measured GL submission averages 42.264 ms/frame,
physics 1.812 ms and scene 1.811 ms. GT Mode's map averages 20.324 ms in scene
and 5.790 ms in the measured GL spans. Ordinary purchase CarFaces still draw
at the page resolution, unlike the reduced full showroom scene.

The Arcade-entry pause now has a concrete font marker: GameRoot's first visit
waits 1365.125 ms, with `hn_14` preparation consuming 1201.492 ms. On re-entry
another `hn_14` preparation takes 1193.826 ms. The preparation code used a
TextFace default for ToolTipFace, while drawing selects hn_14. Shader warming
alone cannot eliminate this CPU preparation cost.

Switch Arcade project loading now warms GameRoot, CategoryRoot and static
CourseRoot 2D resources, including hidden visibility variants and cursor images,
before TopRoot becomes interactive. This does not enter pages, run their
callbacks, draw their contents or advance actors; 3D/video remain separate.
The preparation uses ToolTipFace's actual fallback font and pumps the independent
loading screen during SDF generation. It adds up-front loading rather than
changing the authored animation's duration or omitting it.

The real-disc host fixture verifies all widget bytes unchanged by preparation,
no active page entry, no new font/image/FlashFace cache entries throughout
40 GameRoot frames, and identical final RGBA pixels against the cold path.
One host run measured first submission at 232.841 ms cold versus 0.091 ms warm,
with 649.884 ms paid in preparation; these are PC timings, not Switch estimates.
A negative omits preparation and fails the resource-cache check. A synthetic
hidden-state fixture separately checks no resource reads at first paint, with
its own skipped-preparation negative. Four focused checks passed; console
confirmation of this Arcade change is still pending.

### Dedicated Switch rendering direction

The requested Switch adaptation shares simulation, career, disc data and scripts
with the desktop game. Presentation-specific differences remain P, with measured
costs and image checks recorded per change. The next work targets draw/state
submission, GPU sampling and consistent 3D targets across race, map and CarFace;
persistent preparation caches and background loading address separate stalls.
Streaming must not remove physics/collision data needed by the shared simulation.
The current renderer already performs group/frustum culling and uploads static
mesh buffers once; streaming is not assumed to solve every GL bottleneck.

A separate [deko3d backend](https://github.com/devkitPro/deko3d) is an option for
replacing Mesa overhead on Switch, with build-time shaders and explicit command
lists. It is not installed in the game by this preparation change, and no FPS
target or compatibility is promised before implementation and console tests.
Desktop OpenGL and the original presentation rules remain reference paths;
new shadows/lighting are separate options rather than prerequisites for speed.

### Adapted GL submission path

The first requested Switch-specific renderer iteration remains OpenGL/Mesa,
identified by `Switch renderer: adapted-gl` in the startup log. It is not a
completed deko3d backend. Its changes are compile-time Switch boundaries:

- Resident mesh texture bindings and the corresponding uniform block are reused
  across consecutive groups when both source metadata and actual GPU texture
  match. Wheel accumulation/alternate banks and texture-to-null transitions
  invalidate the cache. No state is reused across a mesh submission.
- Consecutive visible opaque VBO slices with identical material/state are issued
  together, retaining primitive order and selected model-list boundaries.
  Blending, alpha tests/fail actions, destination-alpha masks, active skinning
  and ghost passes keep their separate submissions. Culled slices are not
  included in a batch; the collision/physics world remains fully available.
- PC-generated mip chains and 8x anisotropy are omitted. Authored mip buffers,
  their maximum level and the existing native mip selection shader remain.
  This filter-cost choice is a P adaptation, not an unchanged PC pixel claim.

`MOON_SWITCH_UNCACHED` is an internal diagnostic environment override for
comparing binding/batch submission with the previous path. It is not a game
option and does not turn PC texture extras back on. Showroom logs include
last-pass texture updates/hits and merged groups; those counts are not totals
for every pass of a frame.

The host GPU fixture compares complete RGBA buffers for cached versus uncached
paths, including native mip textures, DATE, null textures, normal/accumulated/
alternate wheel sources. It checks 12 unchanged draws with 3 rather than 12
texture-state uploads in the basic case. A separate 300-triangle fixture reduces
three opaque draws to one with identical pixels, and verifies exclusions for
culling gaps, blending, alpha-fail, DATE, ghosts and model-list limits. Deliberate
pixel corruptions have separate negative tests. Upload checks retain an authored
mip level, omit a generated level and verify anisotropy remains 1.

Console FPS improvement of this submission iteration is pending. Reference
samples remain Ka/Laguna 21.32 FPS, GT map 30.06 FPS and CR-X purchase 25.74 FPS;
compare the same route/settings and retain both first-use and steady windows.
Ordinary direct CarFace resolution, map state boundaries, persistent font/texture
caches, background preparation and a deko3d backend are subsequent work, not
features claimed by this build. Desktop renderer defaults remain unchanged.

Seventeen focused renderer/capture/UI preparation checks passed with the
existing and new negative cases. Windows Release and the ARM64 NRO built;
no complete suite was run for this renderer iteration.

### Adapted submission: next console sample and shader work

The next supplied handheld log (`moonengine (3).log`) confirms the submission
profile and Arcade warm-up were active. The first GameRoot readiness marker
fell from 1360.377 ms in the preceding log to 15.001 ms (15.107 ms on return).
This moves 4151.567 ms of preparation behind the initial loading screen; it
does not eliminate that boot cost. Showroom first readiness still took
2374.891 ms, including a 1016.29 ms first submission. A later return took
610.917 ms. Static room/panel preparation remains a first-use issue.

Weighted steady frame windows from this log, excluding windows with maximum
frame time at least 500 ms and fewer than eight frames:

| Context | Frames | Mean ms | FPS | Physics ms | Scene ms | GL submission ms | Swap ms |
|---|---:|---:|---:|---:|---:|---:|---:|
| Arcade GameRoot | 261 | 16.643 | 60.08 | 0 | 0.225 | 1.408 | 0.175 |
| Ka showroom | 250 | 22.460 | 44.52 | 0 | 2.775 | 15.235 | 0.362 |
| Focus showroom | 135 | 22.503 | 44.44 | 0 | 1.413 | 18.138 | 0.379 |
| Ka, Tsukuba, physics active | 356 | 57.233 | 17.47 | 2.021 | 16.189 | 37.970 | 0.263 |
| GT Mode TopRoot, mixed navigation/idle | 8562 | 23.044 | 43.40 | 0 | 10.802 | 3.417 | 1.671 |

The map is not uniformly slow: its last stationary windows reach 60 FPS with
scene preparation around 2.68 ms. The combined average includes substantially
slower navigation windows. Timing columns are CPU wall times, not GPU timer
queries; GL submission can include driver/GPU waits. They establish expensive
rendering work, not a precise split between shader execution and Mesa overhead.
The opponent grid/course differ from the earlier Laguna Seca sample, so these
race FPS cannot establish a controlled gain or regression. Last-pass showroom
counts show texture reuse (Ka: 67 updates, 73 hits; Focus: 85 updates, 71 hits),
but zero merged groups: the opaque synthetic batching gain does not apply to
these actual showroom groups.

The next Switch-only shader iteration removes work whose result is overwritten
or unused: default lights when explicit scene lights replace them, normal
transforms for flat non-reflective materials, sphere coordinates for materials
without reflection, and derivative/logarithm calculations without authored mip
selection. Base-only simple-wrap sampling bypasses manual address preparation;
reflection bilinear interpolation uses one hardware base-level sample instead
of four explicit fetches. Native mip/nearest/region-addressing paths remain.
`MOON_SWITCH_SHADER_REFERENCE` selects the previous shader branches for
internal comparison; the startup marker now reports `shader_fast=1`.

The host GPU test compares 240 combinations of flat/lit/specular/reflection
materials, zero/one/three lights, vertex light limits, mirror, fog, colour
operations, unlit/shadow, base/nearest/region/native-mip textures. Complete RGBA
buffers differ by at most one byte per channel, with a nonblack-output guard.
A separate negative corrupts a byte and must fail. This is a comparison against
the existing port shader, not a PS2 image proof. Eighteen focused renderer/UI
checks passed; Windows Release and the ARM64 NRO built. The regular desktop
shader branches remain unchanged. This is grade P, and actual console FPS gain
from this shader iteration is still pending. Persistent caches, background
room preparation, direct CarFace resolution and deko3d remain unfinished.

### Shader iteration: console feedback and draw-state reuse

The supplied `moonengine (4).log` confirms `shader_fast=1`, handheld application
mode, unchanged settings, and readable save slots 1-3. Arcade GameRoot readiness
remains short (14.515 ms first visit, 14.888 ms return). The first showroom still
takes 2387.294 ms, including 1017.82 ms first submission; shader work did not
remove static room/panel preparation from the main thread.

Using the same weighted-frame filter as above (maximum frame below 500 ms,
at least eight frames/window, physics-active windows only for live races):

| Context | Frames | Mean ms | FPS | Physics ms | Scene ms | GL submission ms |
|---|---:|---:|---:|---:|---:|---:|
| Ka, Laguna Seca, six cars | 9570 | 41.597 | 24.04 | 1.644 | 1.842 | 36.969 |
| Escort WRC, Tsukuba, six cars | 82 | 49.300 | 20.28 | 1.567 | 17.515 | 25.356 |
| Escort WRC, Tsukuba, Time Trial | 6219 | 35.683 | 28.02 | 0.773 | 13.404 | 20.662 |

The six-car Escort sample is brief (two windows). Ka/Laguna had reached 21.32 FPS
in the earlier budget build, but the opponent grid, paint and driven route are
different: the 24.04 sample is not a controlled shader speedup. Showrooms vary
by car and animation: PT Cruiser 59.91 FPS, Crossfire 58.06, Ka/Laguna 41.39,
Midget II 39.52, Copen 40.61. Some short selection windows include entrance/load
activity. This log has no GT Mode map visit. No current GT Mode gain is claimed.
The scene column also includes GL operations outside mesh submission; its
13-18 ms in Tsukuba has not been isolated to CPU scene logic.

The next Switch-only change reuses an identical material/draw-state block
across consecutive groups, independently of texture changes. Fog/material
uniforms, culling, alpha-test uniforms, blend equations, depth function/write
and channel masks are applied only when needed. The cache is local to a mesh
submission. A second alpha-fail pass invalidates it, and ghost passes bypass it.
DATE still takes a fresh destination-alpha snapshot before every tested group;
only framebuffer-binding queries are reused within the unchanged submission.
The uncached diagnostic executes the former per-group applications/queries.
Primitive order, draw count, scene dimensions and shader output are unchanged.
Startup reports `draw_state_cache=1`; showroom last-pass logs add state updates
and hits, allowing its effectiveness on actual car materials to be measured.

The host GPU fixture compares complete RGBA and float-depth buffers exactly in
ten cases: repeated state with different textures, fog overrides, blending,
alpha tests/channel masks, alpha-fail side effects, freshly changing DATE alpha,
ghost depth/blend passes, culling/depth changes and material/brake branches.
Overlapping geometry advances towards the camera and leaves earlier surfaces
exposed, so later state changes are visible. A distinct read framebuffer tests
nested DATE restoration. The common case keeps all 12 draws and reduces state
applications from 12 to 1; 76 blocks are reused across the fixture. Independent
pixel and depth corruptions must fail their respective comparisons.

Twenty focused renderer/UI checks passed; Windows Release and the ARM64 NRO
built. This is a P adaptation compared against the port's previous renderer,
not a new PS2 fidelity proof. Console performance of state reuse remains
unmeasured. First-use room generation, direct CarFace targets, persistent caches
and a native deko3d backend remain pending; this build still uses Mesa/OpenGL.

## Next rendering optimization work

The [rendering research plan](notes/SWITCH_RENDER_PLAN.md) audits existing
frustum/Vls0 culling and race assets, ranks pass reduction, early actor rejection,
indexed geometry, projected-size LOD and consistent 3D targets, and sets a
measurement gate for a native backend. These are proposals, not implemented
features or a 60 FPS guarantee.

## First integrated deko3d build

The user selected immediate Switch-specific backend migration rather than
requiring reproduction of every PS2 graphics pass. The game executable now
links `deko_device.cpp`, `deko_video.cpp` and `deko_render.cpp` for Switch-native
resource ownership, presentation, UI and 3D. It runs the same game entry point,
Adhoc scripts, physics, input, audio and save services. PC source selection and
the inactive desktop branches in shared files remain unchanged.

The native device owns its swapchain, tiled images, texture/sampler descriptors,
static mesh buffers, three fenced frame-streaming slots and deferred resource
retirement. Upload scratch blocks beyond the ordinary frame budget are freed
after their slot's fence completes. Shader modules are compiled by UAM at build
time and embedded as generated code; there is no runtime GLSL compiler. UI,
model lighting/skinning and four-layer studio composition use native commands.
Resident geometry, frustum/model-list selection, installed car parts and sprite
streaming feed those commands. Captured menus and explicit photo readbacks use
native image transfers. Normal rendering has no synchronous framebuffer readback.

The startup marker is `Switch renderer: deko3d native shaders=5 gl_context=0`.
The packaged SDL archive still brings dormant EGL/Mesa driver symbols into the
final executable. Their presence does not mean they render the game: the native
platform archive has no GL/EGL/SDL_GL references, and excludes the application's
legacy GL graphics objects. `tools/verify_switch_deko.py build-switch/deko`
checks AArch64/NRO packaging, native compilation units, shader containers,
application graphics imports and deko draw/presentation symbols. Its negative
injects a legacy GL-context reference and must fail.

### Intentional adaptations and unfinished visual coverage

- Race environment reflections use the resident disc environment image. Six
  freshly drawn cube views and the road-sheen reflection/blur passes are bypassed
  in the native backend. Main-car and showroom geometry is retained.
- Materials use native forward shading, source-alpha masks and blending. The
  legacy per-group destination-alpha snapshot and alpha-fail second pass are
  not reproduced. Base textures use hardware linear repeat/clamp; native mip
  selection and region addressing are not yet integrated. Check cutout decals,
  glass, brake/rim layering and distant textures before relying on this build.
- Contact/cast meshes remain; the additional blurred studio silhouette is
  omitted. Service layers are composed without the original multi-sample
  accumulation. Morph endpoints for hand closure are not integrated yet.
- The palette/matrix boundary for license/photo image filters has a native GPU
  implementation. Captures and photo focus readbacks compile but require console
  testing, as do split-screen UI, changed operation mode and full scene coverage.
- Dynamic distance LOD, early whole-actor rejection, indexed geometry and a
  complete loading/cache overhaul remain separate work. This migration does
  not claim those features were completed.

These are P adaptations, with no new PS2 pixel-equivalence claim. Visual defects
in the new backend must be reported as defects or explicit limitations, rather
than hidden behind a fidelity claim. Physics/AI/career are not simplified.

### Validation and first console run

The game ARM64 Release NRO built using the SDK's A57 tuning, `-O2`, `NDEBUG`, and
five UAM shader modules. New native C++ files build with `-Wall -Wextra` and no
new compiler warnings. The Windows Release executable built; 22 focused host
checks passed, including negative ABI/image-comparator tests and existing GL,
capture and UI checks. A source-branch comparison confirms desktop text was
preserved in photo_travel.c, race_view.c and video_gl.h. No full suite was run.
The positive/negative native artifact audit passed separately.

These host checks do not execute the new GPU renderer. At native boot an upload
and a textured offscreen quad check four corner colours against readback. The
3D warm-up draws a known triangle and checks both colour and depth. Look for:

```
Switch deko3d selftest: upload/2D/corners=PASS
Switch deko3d selftest: mesh/depth=PASS
```

Those hardware selftests have not run here. An upload/2D failure stops video
initialization; a mesh failure reports failed warm-up. Copy only the new
`moonengine.nro`, retain ISO/configuration/saves and keep the old NRO as a rollback.
Start in application mode by holding R over a game. Check the main menu,
GT map/showroom and a fixed Arcade race; copy moonengine.log before the next start.
The next milestone is console startup/image/control validation, followed by
measuring the same race and addressing the remaining native-renderer issues.

### Native startup repair (2026-10-14)

The first native hardware attempt closed before the renderer-ready marker.
The supplied log passed FPCR and read the three saved profiles; it cannot
identify the exact crashing instruction. Code inspection found a definite
startup defect: embedded DKSH control data was passed directly from read-only
executable storage. [dkShaderInitialize](https://github.com/devkitPro/deko3d/blob/master/source/dk_shader.cpp)
patches the program entrypoint and constant-buffer offsets in that input.
It now receives a private writable copy, freed after the SDK copies the
program header. Every code block also reserves DK_SHADER_CODE_UNUSABLE_SIZE
beyond the code payload, as required by that implementation.

The shared host contract checks a read-only fixture, mutable separate control,
unchanged embedded bytes, repeat initialization and rejected truncated or
invalid controls. Its deliberate alias negative fails before attempting to
write read-only memory. All three focused contract tests passed, the ARM64
NRO rebuilt and the native artifact audit passed (injected GL import rejected).
These are CPU/build proofs, not console startup or GPU pixel proofs. The
renderer remains P and hardware validation remains pending.

Startup now flushes SDL/device/queue/framebuffer/swapchain/shader/acquire
milestones and the SDK debug callback to moonengine.log. This is boot/error logging,
not per-frame output. Replace only the NRO, retaining configuration and saves.
If startup fails again, copy the new log before reopening; it should identify
the last completed stage. The previous Mesa NRO can still be restored.

### Native swapchain ownership repair (2026-10-14)

The next console log reaches `SDL window=OK`, device, queue and framebuffers,
then ends at `swapchain`, before any shader initialization. Thus the preceding
shader correction did not address this observed abort. The installed SDK
SDL 2.28.5 archive's SWITCH_CreateWindow relocation table calls
SDL_EGL_CreateSurface unconditionally, matching
[SDL_switchvideo.c](https://github.com/devkitPro/SDL/blob/switch-sdl-2.28/src/video/switch/SDL_switchvideo.c).
An SDL window without SDL_WINDOW_OPENGL is still an EGL display owner on this
port. [deko3d Swapchain::initialize](https://github.com/devkitPro/deko3d/blob/master/source/dk_swapchain.cpp)
then sets dimensions/configures the same NWindow; libnx rejects already
registered buffers. The release SDK can terminate through diagAbortWithResult
without invoking the application's debug callback.

The native backend now initializes SDL events/audio/controllers without making
any SDL window. SDL's Switch event pump remains active, and background controller
events are explicitly enabled. Only deko3d owns the default NWindow. A flushed
valid/buffer-mask checkpoint and precondition precede swapchain creation.
Windows and the optional Switch Mesa backend retain their existing windows.
The native plat_sdl_window getter is NULL; controller navigation and the game's
controller-operated keyboard remain available, while external keyboard/text
focus and native keyboard-overlay delivery still require console validation.

Five focused host checks pass, including an actual virtual SDL controller event
through plat_sdl_poll with no window/context, its discarded-event negative,
and the existing shader/ABI checks. The native artifact audit now rejects
application SDL window creation/destruction imports as well as GL imports;
separate injected-window and injected-GL negatives fail as expected. The ARM64
NRO builds. Console swapchain creation, GPU selftests and final opening remain
unconfirmed until this updated NRO runs on hardware. This repair stays P,
without a PS2 fidelity or frame-rate claim.

### Native boot probe geometry (2026-10-14)

The subsequent hardware log confirms `NWindow valid=1 buffers=0`, successful
swapchain/shader initialization and first image acquisition. It then reports
`upload/2D/corners=FAIL` and returns video initialization failure. This is a
failed startup comparator, not another abort at swapchain creation. The log
combined upload/draw results, so it cannot establish that uploads alone passed.

The 2D fixture had a definite geometry error: layout_view always fits 16:9,
but the target and view were 16x16. The draw occupied y=3.5..12.5; samples at
y=12 have pixel centers on the excluded bottom edge, rather than inside the
blue/white corners. The target/view are now 32x18, with sample centers strictly
inside the clamped corner quarters of the original 2x2 image. Upload comparison
still requires exact bytes; drawn corners still allow only one byte per channel.
Nothing is bypassed and failed comparison still stops startup.

The host contract checks the shared fixture's aspect and sample positions;
a new negative restores 16x16 and fails. All six focused deko checks pass,
the native ARM64 NRO builds, and the native artifact audit passes. Boot logs
now distinguish upload status and print actual/expected RGBA at every corner.
On failure SDL's error string points to the GPU startup check rather than
leaving the generic window error empty. Console GPU correctness and arrival
at the main menu remain pending; this Switch adaptation remains P. No desktop
rendering code or configuration changed.


### Native material and image-copy repair (2026-10-14)

The user confirms that the probe-fix NRO opens and reports about 60 FPS.
That is a user observation, not a controlled timing result. The supplied
photo is rotated; the console image itself is upright. Checker bands in
the showroom and incorrect race-car paint/reflections remain reported bugs.
A local desktop capture of CarRoot has no such bands; this only establishes
a reference, not the exact cause of the console corruption.

This iteration corrects concrete differences in the native backend:

- Colour images declare Usage2DEngine, required by the installed deko3d 0.5
  transfer path for image blits. Boot now compares all pixels after a GPU
  copy of the 32x18 probe, logging image-blit=PASS/FAIL. Readback is boot-only.
- Transparent UI uses the desktop destination-alpha blend factor. Material
  DATE snapshots the prior framebuffer on the GPU; the second alpha-fail
  pass retains the authored colour/depth masks (00455708 commands 29/31).
- Cropped TEX1 images retain decoded dimensions. Nominal TW/TH, nearest
  filtering and REGION_CLAMP/REGION_REPEAT apply per tap instead of padding
  the crop with black pixels. Alpha coefficients are quantized before ATST.
- Native lighting uses the existing desktop fallback lights, scene specular
  scale and VU fourth/eighth/sixteenth-power selection (0049EA18). Brake
  callback selection retains the decoded material kind (0049EB90). Reflection
  does not overwrite unlit colour, and missing environments bind black.
  Environment wrapping is independent of the surface texture's repeat mode.
- Fog uses the same byte quantization and screen interpolation as the
  desktop path. Authored colour operations stay in the unlit/shadow branch.

The host GPU comparator executes the actual shipped mesh.frag and mesh.vert
GLSL against the existing desktop shaders: 2048 addressing/alpha/DATE/reflection
cases and 324 lighting/specular/fog cases, tolerance one byte per RGBA channel.
Four deliberate negatives corrupt a pixel, invert DATE, change wrapping and
alter unlit colour scale. All eleven focused deko checks pass. The ARM64 NRO
and all five UAM shader modules build; native artifact checks reject injected
GL and SDL-window imports. No PC rendering source or game logic changes.

These are host shader/build proofs, not Switch command-encoder or PS2 image
proofs. Native AFAIL write masks, UI blending, image copies and full scenes
still require hardware validation. Checker removal and race-car appearance
are not declared fixed until that comparison. The renderer remains P; static
race environments, simplified shadows, authored mip levels and hand morph
retain the limitations documented above. Copy only the NRO, retaining saves,
ISO and configuration. If the image remains wrong, preserve the new log and
capture a showroom plus a race with the same car.


### Native regression audit and replay diagnosis (2026-10-14)

Hardware feedback for the material-fix NRO reports flickering/incorrect car
surfaces and a Replay Theater crash. Its host shader comparisons did not
exercise native command ordering. That build is not visually validated;
the fixes below are still unpublished pending console checking.

A concrete ordering defect was found in nd_copy: Fragments before a 2D blit
orders 3D fragments, while None plus image invalidation after it performs
no completion ordering. The shader could therefore consume a DATE snapshot
or capture before its transfer completed. Uploads also lacked a dependency
on earlier readers, and the target-to-atlas copy lacked completion/cache
invalidation. The shared deko_transfer.h helpers now order all preceding
GPU commands before transfers and order completion plus texture-cache
invalidation afterwards. There is no new queue flush or CPU idle wait.
The installed SDK header and deko3d 0.5 gpu_base.cpp define these semantics.
This identifies a defect, not a hardware proof of the reported crash cause.

The broader audit also found invalid sprite metadata: borrowed particle
images left nominal dimensions at zero for the new material shader. Native
sprites now use their resident dimensions, clamp and selected filtering,
retain destination alpha, and implement the existing RGB-only/alpha-test
modes. The shared extent helper falls back to resident dimensions for
images without TW/TH. The existing desktop near/far depth_bias is now also
consumed for shadow passes; geometry and physics are unchanged. Frame-slot
fences, stream reuse, deferred descriptor retirement, image-layout flags,
vertex stride/attribute offsets and nested target handling were reviewed;
this is not an exhaustive hardware correctness certificate.

Sixteen focused host checks pass: 3072 fragment cases (including borrowed
extents), 432 vertex lighting/fog/depth-bias output cases and 120 delayed
blit/copy/upload schedules. Actual GLSL has zero byte/channel difference
against the desktop fixtures (tolerance one). Negatives remove pre/post
transfer ordering or cache invalidation, corrupt dimensions, and retain
the earlier shader negatives. The schedule fixture executes the native
helpers against real SDK signatures with delayed host command sinks;
it does not execute Maxwell hardware. ARM64/UAM builds and the native
artifact audit pass. Native boot now additionally checks reused DATE and
a textured particle quad through actual deko commands; these new checks
have not yet run on the console. No new NRO package has been released.

The supplied moonengine (9).log identifies the automatic presentation as
/replay/Rdemo0/Rdemo0, Bentley Speed 8 / paris_rally. Its sustained measured
windows range from 30.45 to 54.43 FPS; the regular late windows retain
about 120 simulation calls per two seconds. Thus that sample does not
show a globally frozen solver. The earlier 60 FPS observation must not
be generalized to this race. The file has no exception/fault address for
the reported Replay Theater crash, so its cause remains open.

A new optional host diagnostic runs the first disc replay for 5400 ticks:

```
build/Release/disc_demo_probe.exe <ISO> --theater-long
```

It reproduces near-stationary movement in Paris after the initial drive:
at ticks 1500/1800 positions are approximately (-245.91, 0.26, -180.27)
and (-245.83, 0.27, -180.55); later ticks stay near (-271, 0.15, -181).
All 5400 tape commands and solver frames advance. This reproduces a replay
trajectory problem on the existing desktop path too, without deko3d or
any solver change. Finite poses and command consumption alone do not prove
original trajectory fidelity; this remains A, and its exact cause still
needs the original replay state comparison. Do not patch the steering or
teleport the car to conceal it. The Switch startup now enables the already
existing five-second replay pose trace, to compare its trajectory with
that local baseline. The log received before this change has no pose trace.
Six focused existing playback/checkpoint checks pass; they do not resolve
the trajectory defect or establish crash-free Replay Theater on hardware.

### Disc B-Spec demo import (2026-10-14)

The user confirmed that native saved replays work; the reported stuck or
misdirected cars are recordings supplied on the GT4 disc, also on desktop.
The El Capitan/Nissan example is /replay/Rdemo1/Rdemo1. Four of the thirteen
authored title demos are B-Spec: Rdemo1, Rdemo3, Rdemo5 and Rdemo8. Their
complete decoded tapes contain no player steering or accelerator samples.
They carry driver orders and require the original model driver.

00388E38 +6C..70 writes world+CC4 to GT4M+C. The 00387CF0 mode setter retains
the underlying A/B-Spec choice when entering replay. The importer discarded
that field, constructing an A-Spec player from these zero-pedal tapes. It
now retains disc_bspec as disc-only metadata and passes it to the existing
race descriptor's B-Spec path. Native replay serialization, driving physics
and desktop rendering are unchanged.

disc_bspec_playback checks the raw mode, all four zero-pedal tapes, actual
running mode, finite poses and movement over 1200 fixed ticks per demo. Its
negative discards the mode to reproduce the old failure. These integration
checks pass, together with native replay round trips/types, disc command
oracles, checkpoint and existing playback tests: 29 focused tests including
the sixteen graphics checks. Windows Release and native ARM64 NRO builds
pass. This is C for mode import and driving integration, not proof of every
B-Spec trajectory. The A-Spec Bentley/Paris divergence above remains A and
is a separate unresolved defect. Replay Theater crash and native material
appearance still require console validation; no new package is published.

### Native blend-state follow-up (2026-10-14)

The graphics audit found another concrete difference from the desktop renderer:
for a 1+C source/destination coefficient, the native mapping selected C where
the desktop set_blend explicitly falls back to One. This could darken surfaces
in those material modes. The shared native deko_blend.h now matches the existing
desktop state, preserving its documented approximation rather than claiming
exact GS arithmetic. No desktop rendering implementation changed.

deko_blend executes 8192 RGBA cases across all 256 equation bytes, enabled and
disabled blending, four fixed coefficients and four source/destination alpha
pairs. Actual native SDK states are adapted to host GL and compared with the
existing desktop set_blend on the same GPU: zero byte/channel difference
(tolerance one). The negative restores the old 1+C mapping and fails at equation
04. This validates state selection, not Maxwell execution or full car images.
All eighteen focused deko checks pass; the ARM64 NRO builds and native artifact
audit passes. The actual GPU boot probe additionally checks material-blend;
reused-DATE and particle probes from the earlier audit remain enabled. Console
visual checking and Replay Theater crash diagnosis are still pending.

The desktop investigation handoff is in notes/DISC_DEMO_HANDOFF.md. Its remaining
A-Spec trajectory problem is independent of these Switch-only rendering changes.

### Partial-surface flicker: scene reset and frame reuse (2026-10-14)

The user reports that individual roof/window/body sections flicker rapidly;
the last graphics-audit package did not resolve it. Supplied moonengine (10).log
confirms upload, image-blit, reused-DATE and particle PASS on hardware, but
material-blend FAIL and combined mesh/depth FAIL. This is a native GPU failure
that the host shader/blend-state tests did not cover; complete scenes remain A.
The old diagnostic did not print the actual failing blend RGBA, so the failing
channel and its relation to the surface flicker are not yet established.

Source review found a missing scene boundary: desktop render3d.c r3d_begin
explicitly enables depth writes, clears depth, and resets only destination
alpha to one while preserving page RGB. The native r3d_begin cleared depth
but omitted that alpha reset. Native clears also inherited write state from
particles/UI. Shared deko_clear.h now emits explicit depth/colour write masks
and resets scene DATE alpha. Every subsequent draw binds its own full state.
No desktop renderer, physics or recorded input changes are part of this fix.

The SDK-header command fixture covers 96 combinations of prior depth-write,
RGBA write mask and stale alpha, retaining RGB while resetting depth/alpha;
three negatives omit depth state, colour mask or alpha clearing. This is a
clear contract fixture, not a Maxwell hardware implementation or proof that
the old depth mask caused the reported flicker. All 22 focused deko tests pass.

Boot now prints actual RGBA/depth after clearing the particle pass and actual
versus expected material-blend RGBA. A further frame-ring test queues twelve
red/green/blue renders and GPU captures through the actual three frame slots
before reading any of those captures. This exercises reuse across frames,
which earlier single-frame readback probes did not. These new console tests
and the visual result await hardware validation. A successful cross build is
not counted as flicker removal.

### Native alpha command repair (2026-10-14)

The scene-reset NRO still flickers according to the user. Log11 narrows the
boot failure: clear-after-particle PASS (RGBA 0,0,0,255 / depth 1), frame-ring
PASS for twelve captures across three slots, but material-blend writes
128,0,0,255 instead of 128,0,0,128. These fixture passes do not establish that
all resource lifetime paths are correct or that complete scenes are stable.

Review against [NVIDIA's Maxwell B method definitions](https://github.com/NVIDIA/open-gpu-doc/blob/master/classes/3d/clb197.h)
found that the per-target separate-alpha enable at 1E00 was never emitted.
The installed deko3d 0.5.0-1 encoder starts at 1E04 and also writes the RGB
destination coefficient into the alpha destination field at 1E18. Both the
[0.5 encoder source](https://github.com/devkitPro/deko3d/blob/v0.5.0/source/maxwell/gpu_3d_state.cpp)
and local disassembly of dkCmdBufBindBlendStates confirm the latter field
selection. Thus the requested independent alpha state was not fully sent to
the GPU. This is a concrete command defect; elimination of all reported
flicker still requires hardware feedback.

The native nd_bind_blend_commands helper emits the complete target-0 state
through dkCmdBufReplayCmds: enable, RGB equation/factors, alpha equation and
its own factors. Target 0 is the renderer's only colour output. The existing
deko command packet format and documented hardware fields are used; no SDK
binary or source is vendored and there is no second draw/CPU idle wait.
UI and 3D both use the repaired boundary. Desktop rendering is untouched.

The host blend comparison now decodes the emitted GPU packet rather than
reading the input SDK struct. It verifies method/count/subchannel and the
separate-alpha enable, then executes the decoded blend operations on a host
GPU against desktop set_blend: 8192 RGBA cases, zero byte/channel difference
(tolerance one). New negatives omit the enable and substitute the RGB
destination factor; both fail. All 24 focused deko checks pass. The clear
fixture also reflects the SDK macro's conditional depth-write update when
the depth test is enabled.

Boot material-blend now checks equations 08, 44 and 48 and prints actual versus
expected RGBA independently, retaining alpha 128 in all three cases. The
new startup marker is per-target-separate-alpha=1 independent-dst-alpha=1.
This repair awaits execution on the Switch; the visual defect remains A.


### Failed alpha repair and same-console regression comparison (2026-10-14)

Hardware log12 identifies the 36fe696 NRO by its new blend marker. It still
fails all three material probes: 08 returns 128,0,0,255 instead of
128,0,0,128; 44 and 48 return 128,0,0,255 instead of 64,0,0,128. Upload,
32x18 blit, reused-DATE, particle, explicit clear and twelve-frame ring pass.
The user confirms both the partial-surface flicker and checker bands remain.
The alpha-command change therefore did not fix the reported image. These
passes neither certify a complete car nor identify the failing shader/state.

The combined material probe changed the shader input and fixed-function
blend at once. Its alpha/RGB failure does not establish which boundary is
wrong. A standalone switch_gpu_compare NRO now isolates that distinction:

- Compile byte-identical vertex and fragment sources from 7715ae8, the first
  native NRO the user reported working, alongside the current sources.
- Cross first/current vertex and fragment independently. Check unblended
  fractional alpha before checking blending, at quarter/half/three-quarter
  alpha, against explicit expected RGBA rather than merely comparing two
  potentially wrong images.
- Compare the current factor encoder, the original SDK factor encoder and
  an equivalent direct colour-enable/write-mask override. The latter is a
  diagnostic, not a declaration that the SDK colour macro is defective.
- Exercise a separate UI shader and vertex layout with the same alpha
  values to distinguish common state problems from mesh shader problems.
- Upload and copy every RGBA byte, including varying alpha, at 32x18,
  256x128, 960x540 and 1280x720. Small one-GOB probes do not cover full-size
  block-linear captures. The comparator rejects a deliberately changed byte.

There are 98 comparisons; the SDK factor-encoder cases retain its known
alpha-destination defect and can deliberately differ from the requested
state. All actual/expected values and shader/state selections are logged.
No failures are disabled, relabeled PASS or used to claim visual fidelity.
The two first shaders load lazily only when the diagnostic requests them;
ordinary game calls use the unchanged zero probe flags. Windows sources
and game logic are untouched. The separate test reads no disc, profile or
configuration and writes only switch/moonengine/gpu-compare.log. It returns to
the Homebrew Menu after displaying a short result colour.

Build the switch_gpu_compare_nro target. The ARM64 ELF/NRO and seven UAM
modules compile; both first shader sources match 7715ae8 byte for byte.
The 24 existing focused deko host checks pass, including their negatives.
These host checks do not validate the new 98 hardware comparisons. Hardware
execution and the actual renderer correction remain pending, grade A for
complete scenes. Deliver this as a separate diagnostic, not another game
update asserted to fix flicker. The exact original probe-fix NRO is retained
locally as recovery, but rollback alone does not complete the requested fix.


### Hardware-isolated vertex alpha regression (2026-10-14)

The user ran GPU Compare through Sphaira's file browser. Its supplied
`gpu-compare.log` completes all 98 cases: 53 pass and 45 fail. The split is
conclusive for the tested vertex alpha, not a certificate for whole scenes:

- All 36 combinations with the 36fe696 vertex shader fail: fractional
  .25/.5/.75 becomes RGBA 128,0,0,255, even without blending. Swapping the
  fragment shader or colour-state path does not fix it.
- With only the vertex shader changed to the exact 7715ae8 source, all 24
  fixed/direct colour-state cases pass with either fragment shader.
- The current UI shader/state passes its six fractional-alpha cases.
  Direct colour state also passes. The original SDK encoder fails nine
  blended-alpha cases across first-vertex meshes and UI, independently
  confirming the alpha-destination coefficient defect.
- Upload and complete RGBA copies at all four sizes, through 1280x720,
  pass all eight comparisons, including varying alpha. This rules out
  those sampled transfer layouts as the cause of the alpha failure; it
  does not rule out every full-scene lifetime or texture-addressing issue.

This supersedes the idea that changing only the blend commands would
resolve the console result. The fixed commands were correct for the
first vertex shader; the later vertex shader still overwrote their input.

The production vertex shader now computes shadow opacity using an arithmetic
mix of authored alpha and min(alpha*shadow_alpha,1), preserving the desktop
formula without the conditional alpha merge. The UAM TGSI changes from a
UIF/ELSE merge to LRP for that value. The GPU log isolates the old vertex
program, not the precise compiler instruction at fault: the arithmetic
rewrite is a targeted workaround that still requires hardware execution.
The fragment/addressing/material/lighting repairs are retained, rather than
returning the game to its first renderer. No desktop renderer or physics
source changes are included.

Host vertex coverage now includes alpha .25, .5, .75, 1.2 and 1.5 through
2160 lighting/specular/fog/depth comparisons, retaining the one-byte channel
tolerance. A dedicated negative forces the production vertex alpha to one;
the fractional-alpha comparison detects it. All 25 focused deko tests pass.
The game boot now separately checks unblended vertex alpha at .25/.5/.75
before its three existing blend probes. It prints
`vertex-alpha=arithmetic-merge` in the renderer marker. These runtime checks
must pass on the console before treating the workaround as validated.

GPU Compare v2 retains the failing 36fe696 vertex as a native negative and
runs the fixed/first/broken vertex programs against both fragment programs,
state variants and UI/copies: 134 comparisons. Its old SDK and broken vertex
cases deliberately remain capable of failure; the log keeps explicit actual
and expected bytes. First shader sources remain byte-identical to 7715ae8,
and the broken vertex source matches c8087e4. The ARM64 game and standalone
diagnostic NROs build. The standalone package now places its NRO directly in
`/switch/switch_gpu_compare.nro`: Sphaira's default fast scan finds
`/switch/moonengine/moonengine.nro` and can skip a second NRO in that same folder.
Its log continues to use `/switch/moonengine/gpu-compare.log`.

Full car flicker, checker bands, race colours and the new vertex workaround
remain A pending the next console log and visual check. In particular, the
checker bands already existed in the first native build, so isolating the
later vertex regression does not itself establish their cause or removal.


### Full-image composition and race-entry crash report (2026-10-14)

Hardware log13 confirms the e8e9976 arithmetic-alpha marker and PASS for
all three unblended fractional-alpha values, three material equations,
DATE/particle/clear/frame-ring and combined mesh/depth. The user confirms
showroom car visibility, while reflections and top/bottom checker bands
remain wrong. Those observations validate the narrow alpha repair, not
complete native imagery or racing stability.

The checker/reflection investigation found a separate deterministic layout
error. nv_draw_mix built its offscreen quad via
the ordinary UI layout, which fits 16:9. Its 640x448 studio target therefore
had a 640x360 drawn rectangle with 44 untouched rows above and below; its
256x256 reflection target had a 256x144 rectangle with 56 untouched rows on
each side vertically. New GPU image storage was not initialized in those
bands, then the whole image was sampled during presentation/reflection.
The console's 1280x720 display is already 16:9: the correction belongs to
these internal image compositions, not to the console aspect setting.

nv_draw_mix now uses a shared clip-space full-image quad, independent of
UI fitting/panels, and keeps the same weights, shader and six-vertex draw.
The host deko_composition fixture executes the shipped ui.vert/mix.frag
with those vertices and checks every RGBA pixel across 15 compositions:
640x448, 256x256, 960x540, 1280x720 and 1920x1080, three weight sets, one-byte
tolerance. Targets start magenta/zero-alpha so uncovered rows are observable.
The negative restores the old 16:9 fit and fails at 640x448 pixel (0,0).
All 27 focused deko tests pass. Boot also adds actual-GPU full-image checks
at 640x448 and 256x256, with composition-full PASS/FAIL markers; those await
console execution. Full band/reflection removal remains A until observed.

The user reports a Nürburgring Single Race crash, initially described with
the Ka. The supplied log13 instead records Escort WRC selection and its
five opponents. It completes course/driver loading and world GPU creation
(528798 triangles, six grid positions), then reaches quick-arcade/ArcadeRoot
with driving=3 and prepares fonts; no failing instruction or GPU/memory
error is logged. This narrows the failure window but does not prove memory
exhaustion, a graphics timeout, physics failure or a replay-import defect.
The latest Atmosphère crash report is requested for symbolication against
the retained e8e9976 ELF. Do not declare the race crash resolved or release
this composition change as a fix for it. Desktop renderer and physics
implementation are untouched by the composition change.


### Symbolicated race-entry GPU failure (2026-10-14)

The latest supplied Atmosphere report has module ID
9A2186DE649A77F85A3BCA545FC0D969D05BF6F5, exactly matching the retained
vertex-alpha NRO's ELF build ID. ARM64 addr2line/objdump identify the
application stack as main -> plat_video_end_frame -> nd_present ->
dkQueuePresentImage -> Queue::flush -> RaiseError(DkResult_Fail) -> svcBreak.
The offsets are 0x38cc, 0x12dd28, 0x146548, 0x56eca4, 0x56c5e8 and
0x56fd18. Result 0x367 is MAKERESULT(359,1), the release deko3d SDK's generic
failure, not evidence of a CPU physics crash or an allocation failure.
The three older system-process reports and the Sphaira/hbl reports do not
identify an Moon Engine instruction. Older Moon Engine reports also contain 0x367;
their differing build IDs are not symbolicated with this ELF.

The release SDK removes its GPU fault warnings. Its installed diagnostic
archive retains Queue::checkError's NvError classification: page fault and
address/access, method error and command, rejected list or GPU timeout.
Use -DMOON_DEKO_DIAGNOSTICS=ON for the next console reproduction; game
code remains Release with its existing optimization. The callback flushes
these warnings to moonengine.log and logs the three frame slots' draw/upload
counts, stream/command blocks and last vertex extent/target only on failure.
There is no per-draw printf or additional GPU idle wait. SDK error callbacks
abort after recording the error instead of opening its interactive dialog.
The default production selection remains the release SDK archive.

Native draws now check vertex byte extents without arithmetic overflow;
a failed frame fence stops before reclaiming its in-flight buffers. Neither
change is asserted to be the root cause of the reported race-entry failure.
The shared host range check covers the actual 528798-triangle course extent,
UI draws, truncated storage and overflow. Its negative truncates the final
course vertex and fails. All 28 focused deko tests pass, including negatives.
The native diagnostic ELF/NRO is checked for retained SDK fault messages;
a deliberate diagnostic-removal audit must fail. Full console crash
resolution remains pending the diagnostic moonengine.log. This build also
includes the independently tested full-image composition repair above.
Desktop graphics and physics source are unchanged.


### Bounded native loading submissions (2026-10-14)

Hardware log14 executes the diagnostic SDK archive and passes both full-image
composition probes (640x448 and 256x256), fractional vertex alpha and material
blending. The crash now identifies a driver submission rejection:
`dkQueuePresentImage: gpu channel kickoff failed, but no error was reported`.
There is no accompanying SDK GPU page-fault/method/timeout notification.
The frame retains 592 uploads / 60157548 bytes, four streaming blocks and
372 draws; ordinary preceding frames have 3 draws and 1/13 uploads.
This does not identify the driver's Result or prove a particular hardware
limit, memory exhaustion or a physics problem.

The first 7715ae8 renderer uploaded with one full barrier afterward and used
fragment/none barriers for blits. The later transfer-order repair added full
barriers before and after transfers; SDK 0.5 gpu_base.cpp splits GPFIFO entries
at full barriers. Thus later correct ordering fragments the loading command
list differently. This explains a changed submission workload, not a proven
single causal commit for the console rejection.

The Switch adapter now submits and flushes completed transfer chunks after
32 transfers or 4 MiB of upload bytes. These are P work-budget choices, not
claimed driver limits. Draws, transfer barriers, materials and upload contents
are retained in FIFO order. finishList advances recording without Clear;
command/staging storage is retained until the existing final frame fence.
No WaitIdle or readback is added to ordinary loading/rendering. Readback's
existing synchronization resets the pending budget too. Windows is untouched.
A failure-only GNU link wrapper around nvGpuChannelKickoff preserves the raw
libnx Result, module/description, entry count and fence increments, without
retrying or changing the channel. The SDK diagnostic archive remains enabled
for this console reproduction; default production still selects release SDK.

The host deko_submission test executes the shared production helper on an
asynchronous command sink: 674 transfers, including the log14-sized initial
burst, interleaved draws, a single 5 MiB upload and following frame boundaries.
It verifies complete FIFO command order exactly once, <=32 transfers per
submission and no idle wait. Negatives restore the unbounded burst, duplicate
completed lists and ignore uploaded byte accounting; each fails. All 32 focused
deko tests pass. These host contracts do not emulate the Switch driver or prove
the race crash solved.

Diagnostic boot additionally queues 592 synthetic 160x160 images (60620800
bytes) through real SDK partial submissions and checks complete RGBA at five
images spanning chunk boundaries, including first/last. Only this startup
probe reads back; it runs before disc data loading. Its `upload-burst=PASS`
marker and successful Nürburgring race entry are pending hardware execution.
The full scene/race stability remains A until that reproduction. SDK and game
data are not vendored. The existing full-image composition repair is retained.



## EE physics cross build (2026-10-16)

The current native deko3d Release package builds in `build-switch/ee-release`,
including the EE arithmetic and replay corrections from f7322bb. A fresh build
folder avoids stale toolchain paths left by the repository rename. The delivery
copy retains the requested filename `opengt4.nro`; only the NRO is packaged,
without configuration, saves or disc data.

Build repairs select the OpenGL composition source only with the OpenGL backend,
expose the environment-copy declarations shared by both renderers, and use the
showroom's current raster width for its Switch studio layers. The native backend
also implements the existing fill-window and presentation-options interfaces used
by the first-run wizard, and links its font rasterizer without the desktop launcher.
Windows source selection and implementations remain unchanged.

The ARM64 ELF and NRO build successfully. NRO magic and ELF machine checks pass.
Of 40 focused deko/render host tests, 39 pass; `deko_materials` fails again in an
isolated repeat (vertex mode 0, two lights, kind 3: PC byte 86 versus native 0).
No shader was changed in this build; that comparison remains unresolved and this
package is not a claim of verified console materials or performance. Hardware
execution of this EE build and physics step timing on Switch remain pending.


## Restored movies and live diagnostics (2026-10-16)

The first EE package accidentally selected the generic non-Windows movie stub.
The frontend also excluded movie requests under `__SWITCH__`. Restored the native
FFmpeg adapter and required SDK packages from the hardware-tested Switch tree;
`MOON_SWITCH_MOVIES` now reaches the frontend, which transfers PSS ownership to
the decoder worker instead of copying a complete opening movie. Desktop movie
selection is unchanged. IPU still-picture archives remain unsupported on Switch.

Eight host checks pass against independent FFmpeg CLI RGBA fixtures (one byte per
channel tolerance): opening, start-game/category clips, audio, progressive and
interlaced loops, and deliberate pixel/field-order failures. Native ARM64 links
FFmpeg successfully; the frontend integration and playback still need console
confirmation. These fixtures do not establish equality with the PS2 IPU.

For wireless diagnosis, enable Nxlink in Sphaira's Network settings, keep PC and
console on the same LAN, then send with the SDK tool:

```sh
nxlink -a SWITCH_IP -p opengt4/opengt4.nro -s --args "--nxlink-host PC_LAN_IP" opengt4.nro
```

The optional host argument handles PCs with VPN/network adapters. Network launches
send stderr directly to the receiving PC; normal launches keep the SD log. Startup
preserves additional fixture arguments (for example `--drive --race-course tsukuba`)
instead of dropping them. Launching from application-mode homebrew still requires
holding R over a game. Logs received from the console confirm that wireless upload,
boot and live performance reporting work. This is not yet an unattended full-console
test runner: the user must return to Sphaira between uploads, especially after a crash.


## Arcade still pictures and scoped LTO (2026-10-16)

The Switch frontend now preloads the disc's 56 `course.ipic` pictures through
FFmpeg's IPU demuxer/decoder on the existing movie worker. IPU streams retain one
output per source picture (no movie FPS filter); the common archive reader restores
448/512-height groups to the original name order. The rendering-thread poster path
is the same as desktop: a card remains ready while its PSS decoder is pending.

Twelve focused checks pass, including all 56 batch/single-picture comparisons,
112 forward/back card changes without a PSS decoder, a missing-poster negative,
eight PSS cases and both arithmetic-fast-path checks. Hardware live output confirms
`preloaded 56 original menu pictures`; perceived transition smoothness is pending.
The host FFmpeg fixture must include IPU decoder, parser and demuxer (its original
minimal PSS-only build did not). RGBA comparisons are not proof against PS2 IPU.

Wireless race measurements before scoped LTO show Laguna/Ka with five opponents
settling near 20 FPS, 49-51 ms/frame and 14-16 ms/frame in physics (about 5 ms/step).
The initial loading windows have much larger peaks. These CPU spans do not sum to
complete GPU execution time. A global IPO experiment was not accepted: hardware
feedback reported audio loss during course changes. The current build restricts IPO
to physics, its legacy recording implementation, vehicle summaries and ps2float;
audio, renderer and general game/UI sources retain ordinary Release compilation.
No fast-math flags, timing, steps or arithmetic results are intentionally changed.
The global experiment did not provide a controlled same-race performance comparison;
no gain is asserted for scoped IPO until the next hardware sample.


## Arcade navigation diagnostics (2026-10-16)

Live hardware traces isolate a 305-427 ms pause inside `swp_ready`, after scene
and page drawing and before presentation. The corresponding presentation spans
are 0.26-0.35 ms. Authored stills are already ready in approximately 2-13 ms;
loading a 26-36 MB PSS takes 306-435 ms on this SD card. Scripted startup changes
keep presenting at about 60 FPS, while ordinary main-loop changes encounter the
synchronous readiness diagnostic. This isolates a diagnostic-output stall; it
does not establish that all SD, decoder or renderer bottlenecks are solved.

Switch profiler output now enters a bounded 32-record queue. A libnx worker on
core 2 writes the records outside the queue mutex. A full queue or failed worker
initialization drops diagnostics rather than waiting for output on the render
thread. Desktop profiling fixtures retain their synchronous deterministic sink.
Host checks cover FIFO order, wraparound, saturation without overwriting queued
records, empty reads, oversized-record rejection and the existing deliberate
negative. The Switch NRO builds; console confirmation of the fix is pending.

All experimental LTO has been removed from the current diagnostic build. Earlier
scoped-LTO notes above describe an experiment, not an accepted performance gain.
The diagnostic NRO replaces `switch/diagnostics/moon_movie_timing.nro`; it does
not replace the player's primary `switch/opengt4/opengt4.nro` or save files.


## Native specular-only lighting (2026-10-16)

The existing host material comparator reproduced a native vertex failure:
mode 0, two lights, specular-only material, PC 86 versus native 0. The native
shader merged and cleared specular before selecting the specular-only output.
Match `0049EB90 +0x84..0x94` as already implemented in the desktop shader:
select specular alone before vertex saturation; select the separate specular
output only when vertex saturation is disabled. No alpha, transform or shader
compiler workaround is changed.

`deko_materials` now passes 3,072 fragment and 2,160 vertex cases against the
existing desktop GLSL with zero byte/channel difference. Six deliberate material,
address, DATE, vertex and fractional-alpha negatives pass. These are host GPU
comparisons, not a claim that every Switch material matches a PS2 capture.
The native NRO compiles; a fresh console capture is being collected.


## Race GPU waits, state reuse and direct captures (2026-10-16)

Native profiling now separates CPU submission from frame-fence and swap-image
acquisition waits. In portable Laguna with Ka and five fixed opponents, initial
post-loading windows show 39.20 FPS / 25.512 ms and 50.98 FPS / 19.614 ms.
Fence waits consume 13.722 / 10.141 ms; physics consumes 6.784 / 5.238 ms.
A later crowded view falls to approximately 20 FPS with 27 ms in fence waits.
These are CPU wait spans, not GPU timestamp queries or a universal track result.

Cache only shader-program pairs and immutable mesh/UI vertex layouts, resetting
at each frame. Clear, texture/uniform, blend, depth and draw order remain intact.
An 8,000-draw host fixture checks all bindings and attributes across repeated
programs, material changes and four frame resets: 80 shader and 40 layout binds
replace 8,000 of each. Three deliberate shader/layout/reset negatives fail as
expected; 18 focused pipeline, clear, transfer, submission and profiler checks pass.
The comparable early race window changes submission from 1.770 to 1.540 ms,
but FPS stays 39.20. This change reduces CPU work; it does not solve GPU pressure.
Temporary per-frame stall prints have been removed after isolating the menu issue.

Five extra NROs were moved outside `/switch` with `.disabled` suffixes, retaining
recoverable backups. Only `/switch/opengt4/opengt4.nro` remains visible; its FTP
installation was verified by SHA-256. Saves, ISO and configuration were untouched.
Direct native PNG capture and an equivalent desktop capture are local diagnostics;
images/game assets are not committed. Current captures still show missing rank
text and material differences; full graphical parity and 60 FPS are not claimed.


## Clean exit after native captures (2026-10-16)

The first direct race PNG was saved successfully, then shutdown reported
`dkDeviceDestroy: unfreed memory blocks`. Renderer-owned environment, UI depth,
DATE destination, photo target/depth and composition layer/depth targets had no
shutdown path. The native renderer now retires those seven owned targets before
device teardown, without releasing borrowed environment or saved caller targets.
A repeated ten-second Laguna/Ka capture saved its PNG without the prior SDK
failure. The user still observed an operating-system error dialog after exit.
The subsequent reports resolve the faulting program to Sphaira, not MoonEngine;
the handoff cause is still under investigation. A clean return is not established.


## Widescreen HUD rank clipping (2026-10-16)

Direct 1280x720 console capture shows the position box with an absent digit;
the matched desktop capture has the digit. `RaceRankDisplay` (003A3B58) clips
across virtual width 640, while 003AFD18 moves its rank element 96 units left
in widescreen. Desktop extends a full-width clip into both side margins; the
native backend clipped it back to the central virtual area, hiding the rank.
Native horizontal clipping now uses the same full-content rule and integer
rounding, retaining local clip widths and UI-panel boundaries. Host tests check
rank visibility at three scales, local/panel clips, and a negative restoring
the central-only clip. Both pass; fresh console confirmation remains pending.


## Native dynamic course reflections (2026-10-16)

The native race path skipped `course_reflection` and its GPU environment API was
stubbed, so cars sampled a static course texture instead of the sky/scenery
sphere map used by the PC port. Restore that existing boundary on deko3d:
six reusable 128-square face renders, a resident 768x128 atlas, and a 128-square
sphere-map fragment pass. Capture/remap uses GPU commands and barriers only;
there is no ordinary CPU image readback, per-frame image allocation or CPU wait.
Target captures temporarily use projection 1 and restore the prior projection.
The atlas is released with its owning course environment; the new shader is
compiled at boot and freed at shutdown. Desktop execution paths are unchanged.

The formulas match the retained PC six-face adaptation around original
0044E528/0044EEA8, rather than introducing a new reflection style. The new
production shader is checked against the independent CPU sphere-map formula:
three asymmetric six-face camera orientations, <=1 byte/channel; a reversed
camera-basis negative must fail. All 21 focused environment/material/transfer/
pipeline/clip/profiler checks pass. On the console, Laguna/Ka runs, saves its
PNG; its log ends normally, but the user reports an error dialog on return to
Sphaira. Exit remains under investigation. The image shows sky/scenery and the restored
rank digit. Full matched-tick PS2 image equality is not established.

This restoration adds graphics work. With the default five-opponent diagnostic
grid, early post-loading windows are 34.54 FPS / 28.952 ms and 44.66 FPS /
22.393 ms, with 15.092 / 11.142 ms in frame-fence waits. The previous fixed-grid
capture differs in opponents, so those runs do not give a controlled speed delta.
There is no 60 FPS claim. Next performance work must preserve this reflection
producer rather than silently returning to a static probe.


## GPU-copy experiments and retained baseline (2026-10-16)

The repeatable portable Laguna/Ka ten-second capture now counts GPU snapshot
submissions separately (`gpu_copy`, `gpu_date`). These spans measure CPU command
encoding, not GPU execution. The early steady window contains 3,224 DATE copies
in 70 frames (46.1 full-target copies/frame), with 15.205 ms in frame-fence waits.

With the production deko3d SDK, the comparable windows remain 34.61 / 44.71 FPS;
disabling SDK diagnostics alone does not solve the bottleneck. Reusing an alpha
snapshot until an alpha write occurs produced 34.63 / 44.68 FPS and did not
reduce copies in this scene, so that experiment was removed. Replacing equal-size
filtered blits with direct image copies regressed these windows to 25.56 / 36.15
FPS (fence waits 22.789 / 15.059 ms); that experiment was also removed. The
retained renderer uses the original filtered transfers and full ordering barriers.
No quality reduction, physics change, or claim of constant 60 FPS follows from
these experiments. Console artifacts and exact executables remain local.

Two post-capture error reports resolve the faulting PC/stack to Sphaira (module
79B02E3E2733E18E8219D53717321F68AF5C7971), after the MoonEngine log ends. This
narrows the exit failure to the loader handoff/re-entry; its root cause remains
unproven and must not be described as a fixed exit.


## Bounded DATE snapshots (2026-10-16)

DATE still samples the prior framebuffer for every authored group, in order.
The native backend copies only the conservative projected horizontal span of
an unskinned, non-transient group's model-space box, at identical source and
destination coordinates. Height, allocation size, fragment addressing and full
GPU ordering barriers are unchanged. Positive-depth perspective extrema come
from the eight affine box corners, expanded for float operation error and
outward pixel rounding. Boxes crossing the near plane, invalid projections,
skinned/transient geometry keep the full-copy path. This removes redundant
transfer pixels, not geometry, effects or original rendering behavior.

The host fixture retains 100,000 independently projected float positions across
1,000 translated/rotated/mirrored boxes with jitter and offset viewports, plus
near-plane/skin fallbacks; a deliberate empty-span negative fails. All 23 focused
snapshot/material/environment/pipeline/transfer/clip/profiler checks pass.
Portable Laguna/Ka with the same ten-second fixture changes early steady
windows from 34.61 / 44.71 to 60.36 / 59.56 FPS, and fence waits from 15.205 /
11.291 to 4.677 / 0.762 ms. A later window is 44.97 FPS versus 39.11; constant
60 FPS is not established. Direct console captures have zero differing RGB
channels in the compared 1280x340 scene region and 540x275 car region. A separate
19x14 HUD region differs; full-frame equality is not claimed. All captures,
exact executables and logs remain local. Desktop paths and physics are unchanged.


## Two-axis snapshot bounds (2026-10-16)

Apply the same conservative interval projection vertically, using the SDK's
UpperLeft/YAxisPointsUp viewport sign (`gpu_3d_base.cpp`, `viewportFlipY`).
The saved rectangle retains framebuffer X/Y on both images. Skin/transient/near
fallbacks are unchanged. The delayed transfer fixture now also checks 60 cropped
snapshots: source writes and prior readers are ordered, outside pixels retain
their previous values, and a translated-destination negative fails. Projection
coverage includes both axes, rotation, mirror, jitter and offset viewports.
Seven focused snapshot/transfer tests pass, including four transfer negatives.

Portable Laguna/Ka windows are 60.00 / 59.98 FPS, with fence waits 0.108 / 0.251
ms; the later window improves to 47.26 FPS, so constant 60 remains unproved.
The newly retrieved native capture again has zero differing RGB channels in the
compared 1280x340 scene region against full-target snapshots. The same 19x14 HUD
region differs independently. A longer portable Tsukuba/Ka run ranges around
46.57–60.55 FPS in ordinary windows, with 28.80 / 43.25 FPS windows containing
803 / 211 ms outliers. Loading windows are excluded. Other tracks, docked mode,
long-race stability and the exit error remain pending.


## Fragment-engine DATE snapshots (2026-10-16)

A resident snapshot shader copies exact RGBA8 texels at gl_FragCoord into the
bounded destination rectangle, with blending/depth writes disabled. This keeps
producer, snapshot and consumer on the 3D fragment engine. Fragment completion
and texture-cache barriers surround the target changes; unrelated 2D/upload/
copy-engine transfers retain their full ordering barriers. The shader is built
at boot and released with the other programs. No image-quality setting changes.

Host GPU full/cropped framebuffer comparisons preserve every byte, including all
256 alpha values and outside-crop pixels; the wrong-scissor negative fails.
The console boot fixture additionally passes 40 dependent ping-pong copies plus
a cropped copy, checked byte-for-byte after completion. No intermediate CPU wait
is used in that dependent sequence; boot readbacks are diagnostic-only.
A portable twenty-second Tsukuba/Ka run improves ordinary windows from roughly
46.57–60.55 to 52.69–60.73 FPS. A captured racing frame is visually intact; the
two runs end at different simulation ticks, so full-scene pixel equality is not
claimed here. START/font-related 800 ms outliers still appear in that isolated
renderer run and are handled separately. Constant 60 FPS is still pending.
The lossless depth-compression experiment changed the figures only slightly and
was removed; the retained depth format/layout policy is unchanged.


## START and HUD preparation before reveal (2026-10-16)

Console timing traced the large START outlier to lazy HUD atlas creation:
FC_48 costs 787.604 ms when first drawn. Opening styles hb_24#1 and its scaled
variant cost 497.778 / 491.814 ms after readiness, causing additional wall-time
catch-up in the following physics update. Font preparation is presentation work;
no solver arithmetic or physical step interval is changed.

On Switch only, race_game_poll now retains the loading state until twelve
existing opening/countdown/START/meter/music styles are prepared. One font style
is prepared per loading frame; the consumed race-view worker is not polled twice.
The final readiness boundary resets the existing presentation clock as before.
Equal base-face styles share their immutable SDF/GPU atlas pair, while retaining
separate decoded metrics, palette and scale. Stable cache nodes own shared images
once, and close releases them once. Windows keeps its previous lazy path.
The first all-at-once experiment failed in dkMemBlockCreate during a glyph upload;
the retained staged/shared implementation completes loading, racing and capture.

The positive warmup fixture checks incremental progress, resident styles and
shared atlas identity, then compares replay labels/pause frames with fresh HUDs
in both aspects byte-for-byte. Missing-preparation and missing-glyph negatives
fail as intended. All 30 focused HUD/material/snapshot/environment/transfer/
pipeline/profiler tests pass. Console logs put FC_48's 772.562 ms before the final
ready event and show no HUD-font loads afterward. The twenty-second Tsukuba/Ka
repeat has 59.95 / 60.03 / 59.96 FPS early steady windows, max 17.010 / 16.969 /
18.792 ms. The previous 800 ms START and roughly 460 ms catch-up spikes disappear
in this run. Later ordinary windows range 51.61–60.38 FPS, with an independent
85.813 ms outlier. The first 261.976 ms report includes the final loading handoff,
not a steady race frame. Loading takes the preparation work instead; its font
bakes are still synchronous, so fully smooth loading animation remains pending.
Split-screen warmup, other-track/docked coverage, constant 60 FPS and the separate
Sphaira exit error are not claimed fixed.


## Scalar DATE masks (2026-10-16)

The native DATE snapshot now keeps the destination alpha in one R8_UNORM
channel rather than four RGBA8 channels. The snapshot shader copies the same
8-bit alpha into red, and the material shader tests red with the existing
threshold. No resolution, draw order, material precision or physics changes.
Other render targets remain RGBA8; scalar masks are never uploaded through
the RGBA upload interface or sent through the general 2D engine.

The host full/crop GPU fixture checks all 256 values and untouched pixels; its
wrong-scissor negative fails. Material comparisons retain the PC reference
shader and pass 3,072 fragment/2,160 vertex cases using the scalar mask; the
wrong-channel negative fails. All 34 focused tests pass. The console full/crop
alpha256 fixture passes as well; a racing capture is visually intact.

Portable Tsukuba/Ka's matched later windows improve from 54.71 / 51.61 /
54.09 / 57.23 to 56.48 / 53.33 / 55.84 / 58.84 FPS. The early steady windows
remain approximately 60 FPS. An 83 ms isolated outlier remains. These are
short, same-route measurements, not constant-60 or all-track proof. Removing
a duplicate fragment barrier gave no measurable additional gain and was
discarded, as was skipping unused snapshot uniforms.


## Lossless indexed geometry (2026-10-16)

Static native meshes now upload a unique table of complete 108-byte vertex
records plus a uint32 index stream in one allocation. Keys include all bytes:
positions, normals, UVs, colors, alpha, specular and skin weights. Triangle
order, group boundaries and material operations are unchanged. Transient meshes
keep their streamed array path; a position update restores the expanded array
and clears its index address. The vertex shader does not use vertex IDs.

The host fixture reconstructs 6,000 vertices byte-for-byte, including distinct
signed-zero/NaN payloads and attribute seams; a wrong index fails. A native
boot comparison renders six references to three shared vertices and matches
the original triangle's complete RGBA8 and depth buffers exactly. A racing
capture remains visually intact. Portable Tsukuba/Ka later windows improve
from scalar masks' 56.48 / 53.33 / 55.84 / 58.84 to 59.95 / 57.71 / 60.38 /
60.44 FPS. Constant 60, long races and other tracks/docked mode remain pending.


## Clamped scene scaling (2026-10-16)

Scene enlargement now samples the existing UI shader through a clamp-to-edge
sampler with blending disabled, instead of the general 2D filtered blit. It
keeps the 960x540/1280x720 internal sizes and full-resolution HUD. The prior
capture has a bright rightmost column; the new path prevents sampling tile
padding beyond the last source texel. A fresh racing capture still needs to be
retrieved to confirm the reported line visually. No edge is cropped or hidden.

The GPU host comparator checks every pixel for 7x5, 1280x720 and 1920x1080
against clamped bilinear math, with two 8-bit levels allowed for filter rounding
and exact corner bytes. Repeat wrapping is the failing negative. The native
3x2-to-7x5 probe passes with a one-level tolerance. All 40 focused render/HUD
tests pass. With indexing enabled, portable Tsukuba later windows are 59.52 /
56.76 / 59.44 / 60.75 FPS versus 59.95 / 57.71 / 60.38 / 60.44 using the 2D
scaler. The correction costs about one FPS in the slowest measured window;
constant 60 remains unresolved. Exit-error investigation is deferred.


## Full-screen viewport rounding (2026-10-16)

The retrieved capture disproved the scaler hypothesis above: the last column
was identical with both scalers. The actual discrepancy was earlier: native
video_viewport truncated fitted full-screen bounds, while video_gl.c rounds
content_x/content_width by adding 0.5. A fitted width just below the integral
framebuffer size therefore left the last 3D column outside the viewport. The
background remained visible there and enlargement blended it into the edge.

The native full-screen viewport now uses the PC rounding boundary. The direct
2D scene scaler is restored; the ineffective fragment-scaler implementation,
boot probe and host fixture are removed. No edge is cropped, and no resolution
or image-quality option changes. Tests cover the preceding representable width
for 960/1280/1920 pixels, with a truncation negative, plus the existing HUD/local/
panel clip cases. All 39 focused render/HUD tests pass. The native racing
capture confirms the line is gone: at y=500, columns 1278/1279 now read pavement
(50,49,49), rather than the former background-contaminated (77,84,92)/(122,143,165).
The safe bucket-coalescing trial gained only about 0.28 FPS in the slow window
and was removed as inconclusive. The accepted indexed geometry remains;
constant 60 FPS is still unproved.

### Desktop setup isolation (2026-10-17)

The desktop first-run wizard and its renderer API selection are excluded from
Switch compilation. Switch continues reading its SD configuration directly;
this does not introduce the desktop launcher on the console. Native Release
and Windows Release builds verify the platform guards. This build correction
is separate from the texture-allocation crash investigation during race entry.

### Main menu exit entry (2026-10-17)

The Switch build omits the host-added Exit Game entry from the Arcade main
menu. New Career remains present and desktop builds retain Exit Game. This is
an explicit console UI adaptation requested by the user; system HOME remains
available. Native Release compilation checks the console branch.
