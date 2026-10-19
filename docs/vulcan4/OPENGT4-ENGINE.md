# VULCAN 4 ⨯ OpenGT4 — the engine decision (`opengt4-engine`) and the engine fork (`opengt4-fork`)

> **The captain's call, 2026-10-13:** *"This is what Vulcan4 will use for its engine."* and
> *"I actually have permission from the creator to do anything I want with the engine."*
>
> This page is the working record for that decision. It is the plan until a measurement says otherwise.

## 0. The fork — `opengt4-fork`: the engine IS this repository (2026-10-13)

*Branch `opengt4-fork`, cut from `opengt4-engine`. The captain, same day, in three moves:*

> *"we are going to fork his engine"* → *"Don't make its own repo. Put everything in the Vulcan4 repo
> in an archive folder in the repo and push the fork there. Including the Linux fix u did."* →
> *"**We are changing courses. Vulcan 4 is gonna be a fork of OpenGT4 and we are gonna use it to build
> Vulcan 4.**"*

**The engine is now the root of this repository.** Upstream's whole tree — `src/`, `include/`,
`tests/`, `tools/`, `docs/`, `config/`, `res/`, `splat/`, `third_party/`, `CMakeLists.txt`, `LICENSE`,
`NOTICE` — sits at the top level, with our Linux port applied inside it. The recompilation era moved
to **`archive/vulcan4-recomp/`** (the entire repository as it stood that morning); `archive/README.md`
states what an archive is for and what is still worth reading in it.

**The files at the root that are ours, not upstream's:** `README.md` (rewritten for the fork),
`AGENTS.md` (the live brief, rewritten for the fork), `.gitignore` (upstream's rules ∪ ours),
`branding/` (VULCAN 4's own identity — the fork wears it), `archive/`, and this page.

- **Provenance.** Upstream `736124f` (`v0.2.0-alpha` + one commit) arrived as a **git subtree**, so
  its commits are ancestors of this branch: a fork with real history, not a copy-paste. Remote
  `opengt4` → `github.com/Jonhline/OpenGT4`. Two tags pin the reference points —
  **`opengt4-vendored`** (the revision we started from) and **`opengt4-forked`** (the fork with the
  Linux port complete, immediately before the flatten) — and the delta patch is generated from that
  pair.
- **Updating from upstream is now a deliberate merge**, not a `git subtree pull`: the engine shares
  the root with our files, so a pull would try to make the whole tree upstream's. The procedure is in
  `README.md`, "Keeping up with upstream".
- **No game data**, on both sides. Upstream's `.gitignore` rules are merged into ours at the root, so
  "code only, the user brings their own disc" is enforced at the top of the fork.
- **Our delta is applied in-tree**, mirrored in `tools/patches/opengt4-linux.patch` — regenerated,
  never hand-maintained. It lives at the **repo root's `tools/patches/`**, not inside `archive/`: the
  flatten moved the engine's `tools/` to the root, and the anchor this file used to sit under went into
  the archive with the rest of the old repository (moved out on 2026-10-13).

  ```bash
  git diff -M -p opengt4-vendored:engine/opengt4 HEAD: \
    -- src include tests tools CMakeLists.txt assets config res splat third_party \
    > tools/patches/opengt4-linux.patch
  ```

  Diffing two **trees** keeps the patch engine-relative (`a/CMakeLists.txt`), so `git apply -p1` inside a
  pristine upstream copy reproduces the port. The path list is what keeps *ours* out of it: `docs/`,
  `branding/` and `archive/` are not part of the delta, while everything the engine owns is. Note the
  content is **our whole engine delta**, not only the Linux-portability fixes — the two Spec II container
  fixes in `src/core/vol.c` and the new `tools/vol_file.py` ride in it too, because the point of the file
  is that *no change may exist only in a working tree*.

  **Verified 2026-10-13** (not assumed): after regeneration, `git apply --check -p1` against a pristine
  `opengt4-vendored` worktree reports a clean apply. Size at that moment: **9 files, 2,055 lines**
  (5 files / 1,692 lines before the Spec II and oracle-test work).

  For changes made after a new upstream release, diff the live tree against upstream per top-level path
  instead — `git fetch moonengine && git diff -M moonengine/main -- src include tests CMakeLists.txt`.
- **Measured build + run from the flattened root** (never build inside the repo — a build tree is
  ~176 MB):

  ```bash
  cmake -S . -B /mnt/ssd/vulcan4-build -DCMAKE_BUILD_TYPE=Release
  cmake --build /mnt/ssd/vulcan4-build -j4 -- -k          # EXIT=0, zero errors, every target
  cd /mnt/ssd/vulcan4-run && DISPLAY=:99 /mnt/ssd/vulcan4-build/opengt4 "…/Gran Turismo 4 (USA) (v2.00).iso"
  ```

  The result is not a log line: the launcher's *Welcome to OpenGT4* screen (4 panels, cursor on
  **New Player**) is drawn at 1280×960 with **7,277 distinct colours**, and the frame is
  **byte-identical** (`md5 68d8eb83dc75b6fe6e03cc2372ca186d`) to the pre-fork build of the same source.
  The port changed the compilation, not the picture — and it survives the move: the same hash comes out
  of `archive/OpenGT4`, of the pre-flatten layout, and of the flattened root.


## 1. What OpenGT4 is (measured 2026-10-13, not read off the README)

| | |
|---|---|
| Repo | `github.com/Jonhline/OpenGT4` — **public** since `2026-10-13T02:32Z` |
| Licence | GPL-3.0 **plus** a NOTICE under section 7: derivatives must say *"Based on OpenGT4"* in their docs and in a UI (About/credits/launcher); modified versions must not present themselves as official |
| Release | `v0.2.0-alpha` — 42 MB installer + 57 MB `win64` zip (portable), **Windows only** |
| Size | 5.8 MB of C (691 files at head), exe 4.6 MB, `src/core/physics` alone is 44 files |
| History | **2 commits**, squashed; author is literally `OpenGT4`; zero issues; **PRs not accepted** |
| Docs | 54 in `docs/`, **written in Portuguese** — author is Brazilian |
| Approach | native C re-implementation. No emulation, no static recompilation **in the runtime** — it reads the user's own disc image (`SCUS-97328`, v2.00 tested; `CORE.GT4` SHA-1 `30031137ce494323816bb707b90d3ec0d07937d2`) |

**Architecture — and this is why the join is possible at all:** `src/core` (physics, AI, menus,
career, saves, replays) reaches the machine through `include/platform/platform.h` — time, files,
2D drawing, audio, input, a dialog. That header is **135 lines, and it is the only seam that exists**;
`src/platform/sdl` (25 files) is its only implementation (SDL2 + OpenGL core, 4.3 → 3.3; `third_party/fsr1`
vendored; `showroom.c` keeps a 640×448 **software** rasteriser as the reference the GPU path is compared
against).

⚠️ **Correction, measured 2026-10-13:** `docs/ARCHITECTURE.md:19` claims "the renderer sits behind an
interface so backends can be swapped", and this page used to repeat it as fact — *"two documented seams,
both ours to use."* **The code refutes both sentences.** `platform.h` does not abstract 3D at all: no
mesh, scene, pipeline or render-target proc appears in it. The 3D renderer is
`src/platform/sdl/render3d.c` (1,249 lines) behind `render3d.h` — an *internal* header of the SDL
backend, included directly by `src/app` (`race_game.c`, `main.c`, `photo_travel.c`), together with
`video_gl.h`, `movie.h`, `plat_sdl.h` and the `race_view_*`/`showroom_*` families. There is no runtime
backend selection anywhere in the tree, and no Vulkan code: the word occurs once, in that architecture
document. **The seam is real for what it names and aspirational for what it does not** — §7 lists exactly
what has to be extracted before a second backend can exist.

`docs/EDITIONS.md` (PT) fixes the product shape: **Classic** and **Enhanced** are two modes of ONE
executable; everything added must look studio-made ("as if Polyphony had shipped and kept GT4 alive
on PC"); mods belong to Enhanced only. That is the same shape as our G6 (mods) ladder, and the
fidelity ledger is `docs/FIDELITY.md` (299 KB).

## 2. They already own half of what our harness hand-stubbed

`src/ee/{mem,thread,sif,mmi,fpu,syscall}.c` — a **native EE (R5900) core** with unit tests — plus
`tools/recomp/ee_recomp.py`, which turns EE code into `void f_XXXXXXXX(ee_ctx *c)` (function set =
root + everything reachable through `jal`; boundaries come from splat `text.s`; `--exclude` stops
the run). Their README's "no static recompilation" is true of the **runtime**; this recompiler
exists to build **test oracles** that answer "what did the original code do at this address" — LAW 13
by another route. Their oracle currency is ours too: `tools/pcsx2_capture.py` turns a PCSX2 `.p2s`
savestate (`Screenshot.png` 640x480 + `eeMemory.bin` + `GS.bin` + `SPU2.bin`) into a frame the port
re-draws and pixel-compares (`OPENGT4_RACE_SNAPSHOT`, `gt4_race_run_restore`). Verified PCs on record:
`0x00100008` (entry) and `0x001001f0` (after `_InitSys`).

## 3. What the join is worth, measured — the symbol diff

**Their side, re-measured 2026-10-13** (`splat/symbol_addrs.txt`, non-comment lines → **197 symbols**):
**124 verified / 65 provisional / 8 unnamed**. A naive `grep -c '[verified]'` reads 125 / 66 / 9 — the
file's own legend on line 2 carries all three tags once. This page used to publish the inflated pair
(66 / 9); corrected here.

- **120** of the 124 verified names are the EE kernel syscall stubs. The file's own comment puts the
  block at `0x005ad880`–`0x005ae120` (**138** stubs); the first verified one is `ResetEE 0x005ad890`, the
  last is `_InitTLB 0x005ae110`, and **102** of them sit inside `0x005ad000`–`0x005adfff`. Upstream's
  `analysis/SYMBOLS.md:41` agrees with the file (138 stubs, 120 verified). This page previously said
  *"122 of the 124 verified names sit in ONE 1.9 KB block, `0x5AD880`–`0x5ADFF0`"* — wrong on the count,
  on the size, and on the end address. Corrected.
- ⚠️ **Our side of that comparison is no longer on disk, and this page says so rather than quoting it as
  current.** The old numbers — our emitted engine's `engine-symbols.csv`, **19,403 addresses** spanning
  `0x001008D0`–`0x00616EE8`, with **46** of their 197 landing on it and only `DeleteSema 0x5adcb0` /
  `FlushCache 0x5adf20` overlapping the syscall band — were measured in the recompilation era and stored
  under `/mnt/ssd/vulcan4-build`, which was cleared on 2026-10-13 when that path was reused for the fork's
  build. **No copy survives, including in the archive** (the generated C++ was never committable — it is a
  derivative of the game). They are **regenerable**: the recompiler, its Linux patches and the disc are
  all still here, and the pipeline is `archive/vulcan4-recomp/`.
- What the comparison recorded, and what did **not** depend on that file, still stands: **the block they
  can name best is the block our recompiler never emitted, and it is exactly where boot went dark.** Our
  harness hand-stubbed the syscall vector (`syscall 0x5b = 0x80075000`); they have `syscall.c` and 120
  verified names for it. We had 19,403 function boundaries; their `ee_recomp.py` is bounded by root +
  `jal`. Each side was the other's missing half.

**Don't over-read it, though:** our generated bodies come from PS2Recomp's calling convention and
theirs assume `ee_ctx *`. The reusable asset today is the **map** (addresses, boundaries, reachability)
plus the oracle tooling — not drop-in object code. Any "swap one function" claim must be proven per
function against the oracle, which is the existing LAW 13 discipline.

## 4. Licence position (the part permission does not change)

- The creator's permission means **we may modify freely and work inside the repository** — which
  GPL-3.0 already grants to anyone; what it really buys is the absence of the PR gate and an open
  door to the author.
- It does **not** remove the GPL-3.0 + NOTICE obligations for anything we distribute: source stays
  available under the same terms and *"Based on OpenGT4"* must survive in our docs and in a UI.
  **Never strip `LICENSE`/`NOTICE` out of a vendored copy.**
- Our repo is **GPL-3.0 too**, so there is no licence conflict in either direction.
- LAW 1 still holds on both sides: code only, the user brings their own disc. OpenGT4 ships no game
  data either — that is why this join carries no asset risk.
- ⚠️ **The UI half of the attribution obligation is still unmet in the code.** `NOTICE:20-23` requires the
  notice *"Based on OpenGT4"* in the documentation **and**, because this program has a user interface, in
  a place users can see it. Measured 2026-10-13: `grep -rn "Based on OpenGT4" src include` returns
  **zero** hits — today the string exists only in `NOTICE` itself. The documentation half is satisfied;
  the launcher's about/credits screen is a real, small, owed task (it belongs with the branding pass, not
  in a hurry).

### 4.1 Upstream is gone — measured 2026-10-14 (this fork is the line now)

- `github.com/Jonhline/OpenGT4` returns **404** on the GitHub API and the account shows **0 public
  repos** — deleted or set private. The author said so himself, in public, on GTPlanet
  `threads/gran-turismo-4.439789/`: after pez2k (unreadable "minified" code, magic numbers, unnamed
  offsets), TheAdmiester (`src/ee` present under a "No emulation" claim) and Nenkai (*"another
  slopcomp"*) took the code apart, he wrote *"I've decided to take OpenGT4 out of the public eye and
  keep it as a personal hobby project"* — and the thread's opening post was edited away.
- **Nothing is lost.** `upstream/main` `736124f` (2026-10-12) is an **ancestor of our `main`**: whole
  tree, whole history, `LICENSE`/`NOTICE` intact. There is no upstream to pull from any more.
- A **public mirror** exists — `MrGoodGuy2144/OpenGT4`, created 2026-10-13, same HEAD `736124f`. Treat
  it as an archive for provenance questions, not as a source of updates.
- **Their strongest criticisms, measured against THIS tree (2026-10-14), do not land:**
  - *EE code under a no-emulation claim* — **provable here**: `CMakeLists.txt:225-226` links the shipped
    `opengt4` target against `opengt4_race_sound`, `opengt4_career_vehicle`, the SDL stack,
    `opengt4_physics` and `miniz` — **not** `opengt4_ee`, which is linked only into `*_oracle` and recomp
    test targets; `grep` for `src/ee` under `src/core`/`src/app`/`src/platform` returns **0**. Comparing
    native C against the original is this project's method (LAW 13), not a hidden emulator.
  - *Minified, unreadable* — **not here**: 78,458 lines of C/H under `src`+`include`, average line length
    **59** characters, biggest file `src/core/menu.c` at 6,405 lines; comments cite PS2 addresses.
  - *Code nobody can maintain* — that is a **standing obligation on us**, not a fact about the tree: the
    measured gates (byte-identical reference frame, 304 disc-gated tests) and human-reviewed commits are
    the difference we sell.
- **The one tell that DOES apply, and it is cheap:** `ASSIGN_OLD` — Nenkai's disambiguated name for an
  instruction that only exists in later Adhoc versions — appears in **6 places**
  (`src/core/adhoc/adhoc_vm.c`, `adhoc_code.c`, `adhoc_code.h`) with no attribution. Fix = cite Nenkai's
  disassembler at each site (not a blind rename: the name encodes a real version distinction). Also
  **8** "not rewritten yet / run the original" comments to make explicit as oracle bridges.

## 5. Bring-up on Linux — the MSVC-isms (state: all fixed, §0)

The `win64` release is Windows-only, but nothing in the design is: CMake **configures clean on Linux**
(SDL2 static + miniz fetched) and the whole project builds — `EXIT=0` with `-- -k`, every target.
The first seven blocked the *engine and the default test set*; the last three only surfaced on
2026-10-13 when the **oracle tests** (`-DOPENGT4_RECOMP_TESTS=ON`) were switched on for the first time
and the tree was built whole. Not one of them was a formality — each is a real difference between MSVC
and GCC/Clang. All are applied **in the tree** on this branch (`tools/patches/opengt4-linux.patch` is
the regenerated delta, so the list and the artefact cannot drift apart):

1. **`src/core/career_original.c:126` — the C99 hex-float trap.** `memcpy(..., r+0x2e+i*2, 2)`:
   GCC/Clang lex `0x2e+i` as a hexadecimal-floating literal and then choke on the suffix
   (*invalid suffix "+i" on integer constant*). MSVC has no hex floats, so it never noticed.
   Fix: a space — `r + 0x2e + i*2`.
2. **Two implicit-declaration clashes** (GCC rejects a `static` definition that follows an implicit
   declaration; MSVC tolerates it): `to_int` in `src/core/menu.c` (used ~line 928, defined ~2645) and
   `watched_car` in `src/app/race_game.c` (used 1914, defined 1997). Fix: forward declarations.
3. **GL linkage.** `opengl32` was linked only `if(WIN32)`, but `fsr.c` calls GL entry points directly
   — on Linux the link died with `undefined reference to glBindTexture`. Fix: `find_package(OpenGL)`
   + `OpenGL::GL` on the non-Windows branch.
4. **libm.** `fminf`/`fmaxf` are libm symbols on Linux and CRT symbols on MSVC, so
   `showroom_math_tests` and `render3d_frustum_tests` died at link time. Fix: `link_libraries(m)`
   on non-Windows.
5. **Archive rescan — the one that took real thought.** `opengt4_core` calls `plat_file_*`
   (in `opengt4_platform_common`) and `opengt4_platform_common` calls back into core, so **no linear
   order satisfies both**. MSVC's `link.exe` rescans archives and never noticed; GNU `ld` does not,
   and ten targets failed on it. Fix: two variables, `OPENGT4_CORE_STACK` and `OPENGT4_SDL_STACK`,
   holding `-Wl,--start-group … -Wl,--end-group` (a plain list on MSVC). **Every**
   `target_link_libraries` line in the tree that named one of those libraries was rewritten onto them
   — 186 lines — so the fault cannot return through a new target later.
6. **`_putenv_s`.** The tests and probes were written against the Windows CRT and call it unguarded
   (9 call sites). Fix: `src/platform/common/msvc_compat.h`, force-included on non-Windows builds.
   The subtlety is not the name: MSVC's `_putenv_s(name, "")` **removes** the variable, while
   `setenv(name, "", 1)` leaves it defined-but-empty and `getenv()` then returns `""` — which every
   switch in this tree reads as **on**. The shim unsets on an empty value.
7. **Nothing else in the engine.** `_commit`/`_fileno` in `src/core/save.c` and `GetCurrentProcessId`
   were already behind proper `#ifdef _WIN32` guards — upstream is more portable than its shipping target
   suggests. A CRT-call sweep (`_mkdir`, `Sleep`, `localtime_s`, `strcpy_s`, `fopen_s`, `itoa`, …) over
   the *engine* found no other unguarded use.
8. **`CMakeLists.txt` hard-coded the author's Windows Python** — the oracle tests generate C by running
   `tools/recomp/ee_recomp.py`, and the interpreter was set to
   `${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe`. On any other machine **every oracle target died with
   Error 127**, which reads like a build failure and is really a missing interpreter. Fix: an explicit
   `-DOPENGT4_PYTHON` wins, then a venv inside the tree (`bin/python` or `Scripts/python.exe`), then
   `find_program(python3)`, and a `message(STATUS …)` line that **prints which interpreter was chosen**.
   That interpreter must have **`rabbitizer`**; only the oracle tests need it.
9. **`tests/ui/outside_menu.c` included `<direct.h>`** and called `_mkdir` — Windows-only. Fix: guarded
   include, `#include <sys/stat.h>` and `#define _mkdir(p) mkdir((p), 0755)` on POSIX.
10. **`tests/physics/race_setup.c:86` — `static int run(…)` after a non-static declaration** — the same
    GCC rule as item 2, in the physics test support file. Fix: drop the `static`.

FFmpeg note: the shipped zip carries a 134 MB `ffmpeg.exe` — movies are played by **running the
binary**, not by linking libav. On Linux that means an `ffmpeg` on `PATH` (CMake's
`OPENGT4_FFMPEG`), otherwise MPEG-2 menu movies stay black.

## 6. Next steps on this branch

1. ~~**Prove the Linux build in the product's own terms**~~ — **DONE 2026-10-13** (§0): the vendored
   fork builds `EXIT=0` and its own `opengt4` draws the launcher's welcome screen from the captain's
   v2.00 image. A build is not a picture; there is now a picture.
2. **Feed them the map** — *and re-make it first.* The 19,403-boundary `engine-symbols.csv` is gone
   (§3); regenerating it is a re-run of the archived recompiler over the engine image, which is a
   known pipeline and a real dish, not a mystery. Until then, what is already in their tree —
   `splat/symbol_addrs.txt` (197 symbols, 124 verified) plus their own `ee_recomp.py` — is the
   function set the oracles use.
3. **Take their kernel side:** `src/ee/syscall.c` + the 120 verified syscall names against the vector
   our harness used to hand-stub, one call at a time, each checked against the oracle.
4. **Then the captain's goals, in his order:** the Enhanced layer is where our G6 work lands (mods
   format, new cars, custom music), the authentic path stays the default (LAW 12 — everything we add
   ships OFF), and Spec II / Android / distribution follow.

## 7. What a second backend actually costs (Vulkan / Metal) — measured against the code

The captain's call, 2026-10-13: **Vulkan for Windows/Linux/Android, native Metal for macOS.** Read §1's
correction first: there is nothing to plug into yet. The honest list, in the order it has to happen:

1. **A render interface that is not GL-shaped.** The surface is known — it is what `render3d.h` already
   exposes: mesh create/destroy, `begin`/`draw`/`end`, lights, layers, render targets **plus readback**
   (`r3d_begin_target`/`r3d_end_target` + `glReadPixels`), sprites, and stats. An interface that mirrors
   *those* is portable; one that mirrors `glEnable`/`glBlendFunc`/`glDepthFunc`/`glColorMask` is GL with
   extra steps and will fight Metal for the rest of its life. The per-draw state sequence in `draw_mesh`
   (`render3d.c:892-995`) becomes a small fixed set of pipelines, and the *interpreter* logic around it —
   which encodes GS blend, alpha-test, DATE, addressing and fog semantics — is reusable as-is.
2. **Move the GL-only types out of the public headers.** `video_gl.h` pulls in `gl.h`; `plat_texture` is
   an opaque `GLuint`; `r3d_mesh` is GL-backed. No `GLuint` may cross the new interface.
3. **The shaders leave the C files.** They are GLSL strings today (`#version 330 core` in
   `render3d.c`/`video_gl.c`, `430 core` in `fsr.c`) — ~10 programs. One source must compile offline to
   **SPIR-V** (Vulkan) and **MSL** (Metal). This is step 2 of the plan and it is independently testable.
4. **Frame and scene lifetime moves behind the interface** — `video_begin_scene`/`video_end_scene`, the
   offscreen FBO, the MSAA scene target, the FSR upscale, the UI panel, the license CLUT.
5. **The ES-incompatible list, if Android ever wants GL instead of Vulkan** (all measured in the code):
   `glClipControl` (4.5/`ARB_clip_control`) with reversed-Z float depth; `#version 330/430 core`;
   `textureGather` (ES 3.1+); the `noperspective` interpolation qualifier (**absent from GLSL ES
   entirely**); `PROFILE_CORE` context creation; `glClearDepth` (ES has only `glClearDepthf`);
   `GL_EXT_texture_filter_anisotropic`; runtime name loading. Everything else the backend uses — VAOs,
   FBOs, `glBlitFramebuffer`, MSAA renderbuffers, `texelFetch`, `dFdx`/`fwidth`,
   `glVertexAttribIPointer`, `GL_DEPTH24_STENCIL8`, `glReadPixels(GL_RGBA)` — is ES 3.0-legal.
6. **The movie path is Windows-only** (`movie.c` is `#ifdef _WIN32` end to end and shells out to an
   `ffmpeg` binary). Linux **and** Android lose menu movies entirely until that is behind the platform
   interface; but note `fsr.c` needs **GL 4.3** for its shaders, so on a 3.3-only context the upscale
   silently does not run.
7. **The acceptance gate already exists.** The port can capture a frame headlessly
   (`OPENGT4_CAPTURE_*`, `screenshot()` writing a PNG), and we hold a byte-identical reference
   (`md5 912bdc63bddc791afece574c7475eac8` on GL; `531acb651f5c8552f3540e9a57fdf08f` on Vulkan).
   **A new backend is correct when it draws its pinned reference frame** — not when it looks right. Keep
   the GL backend alive as the reference and the A/B control. The gate was re-pinned to the current tree
   on 2026-10-19; the two backends no longer draw the same bytes, so each is gated against its own capture.
   See `SESSION-LOG.md` for the exact command.

*Everything in this page is either measured on this box or marked as a plan. Numbers that come from a
single run say so; numbers whose artefact no longer exists say that too.*
