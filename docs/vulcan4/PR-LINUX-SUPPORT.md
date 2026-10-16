# PR — "build and link the engine outside MSVC" (branch `linux-port`)

**State 2026-10-16 — MERGED into his `main` (`20333874`) at 05:33:25Z; PR #13 is closed, the
`linux-support` branch was deleted from his repo. From now on the captain's rule is direct pushes to
`main` — no PRs ([COLLAB-JONHLINE.md](COLLAB-JONHLINE.md)). Kept below only as the record of what the
first contribution looked like.**

**State 2026-10-16 — PUSHED AND OPEN:** branch `linux-support` (local branch `linux-port`) is on his
repo, and the pull request is **https://github.com/Jonhline/MoonEngine/pull/13** — `OPEN`,
`MERGEABLE`, 2 commits, 5 files (+358/−262), head `37e06cb`, base `main` (`c1ca129`). Waiting on him.

* `32bbf82` **Linux: build and link the engine outside MSVC** — `CMakeLists.txt`,
  `src/platform/common/msvc_compat.h` (new), `tests/ui/outside_menu.c`, `tests/physics/race_setup.c`.
* `37e06cb` **docs: how to build, test and run on Linux** — the Linux section of
  `docs/BUILDING_AND_TESTING.md`, added on Boss's ask: *"push a small documentation file that will
  help us build it once we release it."*

Worktree `/mnt/ssd/moon-baseline`; build tree `/mnt/ssd/moon-baseline-build`; proof shot
`linux-proof.png` in that build tree. Verify from scratch with `gh pr view 13 --repo
Jonhline/MoonEngine`.

## The PR text, ready to paste

> **Linux: build and link the engine outside MSVC**
>
> The tree is Windows-first; these are the places where MSVC's leniency (archive rescanning,
> CRT-resident libm, tolerating an implicit declaration) hid a real portability problem. Nothing
> here changes Windows behaviour. Every item was found by building this tree on Linux, not by
> reading it.
>
> - `CMakeLists`: `-lm`, plus `OPENGT4_CORE_STACK` / `OPENGT4_SDL_STACK` — `opengt4_core` calls
>   `plat_file_*` (in `opengt4_platform_common`) and `opengt4_platform_common` calls back into core,
>   so **no linear archive order satisfies both**. `link.exe` rescans archives, GNU `ld` does not:
>   260 `target_link_libraries()` lists now use the group (a plain list on MSVC).
> - `CMakeLists`: `OpenGL::GL` on non-Windows — `fsr.c` calls GL entry points directly.
> - `CMakeLists`: force-include `src/platform/common/msvc_compat.h` on non-Windows.
> - `src/platform/common/msvc_compat.h` (new): `_putenv_s` — MSVC **removes** the variable on an
>   empty value, while `setenv(name, "", 1)` leaves it defined-but-empty, and `getenv()` returning
>   `""` is read as *on* by every switch in the tree — and `GetCurrentProcessId` (used by
>   `favorites_menu.c` for a per-run directory).
> - `tests/ui/outside_menu.c`: `<direct.h>` / `_mkdir` → POSIX `mkdir`.
> - `tests/physics/race_setup.c`: drop `static` from `run()` — GCC rejects a static definition after
>   a non-static declaration.
>
> **Verified on Linux/GCC:** configure + build `EXIT=0` for every target, **102/102 tests pass**,
> and the engine opens GT4 (USA) v2.00 and draws the arcade menu (`arcade / TopRoot: drawn 8, not
> yet drawn 1`).

## What was measured, not assumed

His tree **already carried three of our bring-up fixes** (he took them while the repo was public):
the C99 hex-float trap in `src/core/career_original.c` (`r + 0x2e + i * 2`) and both forward
declarations (`to_int` in `src/core/menu.c`, `watched_car` in `src/app/race_game.c`, comments and
all). So they are **not** in this PR — sending them back would be duplicates. Everything else in the
list above was still missing in `c1ca129`; that was checked item by item with grep before writing a
line, and then confirmed by the build.

## How this PR was built (repeat it for the next one)

1. `git worktree add -f /mnt/ssd/moon-baseline jonhline-baseline` — his `c1ca129`, never developed on.
2. `git apply --3way tools/patches/opengt4-linux.patch` — 6 of 9 files applied; the 3 conflicts were
   files where he had already fixed it (drop them) or where our delta was not Linux work at all
   (`vol.c`, `tools/vol_file.py` — Spec II, not bring-up; drop them too).
3. `CMakeLists.txt` was **re-transformed on his file**, not patch-applied: every
   `target_link_libraries()` that named `opengt4_core` / `opengt4_platform_common` was rewritten onto
   the stack variable. His tree has 260 such lines (ours had 186) because he added targets — a
   blind patch would have missed every one of his new ones.
4. Build it, `-k` to collect **all** errors: one real failure left, `favorites_menu.c` →
   `GetCurrentProcessId`. That is what put the second function in the shim header.
5. `ctest -j4` → 102/102, then a real disc run for the picture.

## Open when he answers

- Does he want the shim `#include`d from CMake (`-include`) or would he rather see the two names
  fixed at the call sites? (Force-include touches every non-Windows target; the call-site fix is
  smaller but scattered over 8 test files.)
- `OPENGT4_PYTHON` at `CMakeLists.txt:1057` still defaults to the Windows `.venv\Scripts\python.exe`
  path — the oracle targets need a `rabbitizer`-capable interpreter. Not touched here because we did
  not turn `-DOPENGT4_RECOMP_TESTS=ON` on in this build; say the word and we will.
