# VULCAN 4 — session log

Append-only. One dated entry per working session: **what was done, what was measured, what it proved,
and what is still open.** Chat is not a record; this file is. Newest entries at the bottom.

Rules for this file: numbers come from runs and name the command or file that produced them; a claim
without a measurement is marked as a claim; failures are written down as loudly as successes.

---

## 2026-10-13 — the engine era begins: fork landed, Linux is real, the test suite turned on, Spec II's container opened

### 0. Where the project stands at the end of this session

- **The engine is the repository.** `main` is the OpenGT4 fork; the static-recompilation era lives whole
  under `archive/vulcan4-recomp/`. Nothing about the old project was deleted.
- **The fork builds and runs on Linux**, from the captain's own v2.00 disc, and draws the same launcher
  frame it drew before any of today's changes — **md5 `68d8eb83dc75b6fe6e03cc2372ca186d`**, the
  project's acceptance gate for a renderer change.
- **The complete test suite is available for the first time**: `500` tests registered, `493` pass.
- **The Spec II disc's container now opens** — and the next wall behind it is named exactly
  (its scripts are `ADCH007`; the engine implements `ADCH005`).

### 1. The course change (context for anyone reading later)

> **"We are changing courses. Vulcan 4 is gonna be a fork of OpenGT4 and we are gonna use it to build
> Vulcan 4."**

VULCAN 4 is a fork of **OpenGT4** (a native C re-implementation of Gran Turismo 4, GPL-3.0, used with
the author's blessing) instead of a static recompilation. The whole old repository was archived, the
engine moved to the repo root, the `README`/`AGENTS.md` were rewritten around the fork, and the Linux
delta is mirrored in `tools/patches/opengt4-linux.patch`. See `docs/vulcan4/OPENGT4-ENGINE.md` §0.

### 2. Linux bring-up finished — 10 Windows-only assumptions, not 7

The engine and the default test set needed **seven** fixes (documented in `OPENGT4-ENGINE.md` §5). Today
the tree was built **whole, with the oracle tests on**, and that found **three more**:

| # | What | Why it only appeared now |
|---|---|---|
| 8 | CMake hard-coded the author's Windows Python for the oracle generator | only the oracle targets invoke it — Error 127 on every one |
| 9 | `tests/ui/outside_menu.c` included `<direct.h>` and called `_mkdir` | test file, not engine |
| 10 | `tests/physics/race_setup.c` — `static int run(…)` after a non-static declaration | test support file |

**Result:** `cmake --build /mnt/ssd/vulcan4-build -j4 -- -k` → **`BUILD=0`, 269 targets, no errors.**

### 3. The test suite — from 304 to 500, and what it measured

Three things were missing, and each was a real unlock rather than a formality:

1. **`extracted/core.bin`** — generated with `tools/unpack_core.py` from the captain's retail disc.
   **Proof it is the right file:** its sha1 (`38cc7f0d…`) matches the hash recorded upstream in
   `splat/gt4.yaml` exactly (6,118,908 bytes). Test count: **304 → 326**.
2. **splat output** — the oracle tests need `splat/asm/core/text.s`. Installed splat in a venv
   (`pip install "splat64[mips]"` — the plain `splat64` package ships **without** its dependencies:
   `spimdisasm`, `rabbitizer`, `pygfxd`, `n64img`, `crunch64` all had to come from the `[mips]` extra)
   and ran `python -m splat split gt4.yaml` from `splat/`. **Result: 86,846,092 bytes of disassembly,
   `SPLIT=0`.** `splat/asm/` is gitignored, so none of it is in the repository.
3. **`OPENGT4_RECOMP_TESTS=ON`** — off by default upstream; the tests that recompile game code only
   register when it is on. Test count: **326 → 500**.

**The measured result, first full run on Linux:**

```
99% tests passed, 7 tests failed out of 500
  54 - pause_confirm (SEGFAULT)
  58 - license_teacher_gold (Failed)
  60 - disc_demo_playback (Failed)
  84 - gtmode_series_awards (Failed)
 314 - gtmode_setting_motion (Failed)
 352 - storage_audit (Failed)
 390 - gtmode_extras_menu (Failed)
```

Before this session, only 304 of the suite could even be built here, and 16 of those failed; the seven
above are the honest remaining list, with **one real crash** (`pause_confirm`) at the top of it.

**Operational lesson, recorded because it cost a run:** the first attempt died with `exit -9` at
357/500. Background processes started from this agent are killed when they cross roughly **4 GB**, and
500 tests in parallel crossed it. The fix is to give the job its own cgroup:

```bash
systemd-run --user --unit=vulcan4-ctest --collect \
  --property=WorkingDirectory=/mnt/ssd/vulcan4-build \
  --property=MemoryHigh=10G --property=MemoryMax=14G \
  /bin/sh -c 'ctest -j3 --timeout 900 > /tmp/ctest_full2.log 2>&1; echo CTEST=$? >> /tmp/ctest_full2.log'
```

`ctest -j4` also produced a **false** "4 failures" mid-run; the honest final number is the one above.

### 4. Spec II — the container opens, and the wall behind it is named

`G6.24` in the wishlist is "native Spec II support", and it was the release gate nobody had measured.
Today the engine went from **refusing the disc outright** to **reading its container**. Two format
differences had to be found first, both by measurement, both now in `src/core/vol.c`:

1. **The RoFS index page is not at a fixed page.** Retail v2.00 keeps it at **`0x2159`**; the Spec II /
   Online-Beta disc keeps it at **`0x22b7`**. Page 0 carries the RoFS magic on *both* discs and is *not*
   the index — so the discriminator is the **declared page size (`0x800`)**, not the magic.
   `gt4_vol_open` now tries the known retail page first (one read: retail behaviour is untouched) and
   then scans forward for the first page that validates, reporting its choice under `OPENGT4_VOL_TRACE`.
2. **A leaf page's u16 count field has a FLAG in its low bit, not part of the count.** Sweeping **every
   page of both discs**: retail has it clear on all 407 pages, Spec II has it **set on 575 of 582**.
   Reading `pairs = count >> 1` parses both discs with **zero failing pages — 18,562 entries retail,
   27,242 Spec II** — where the old `count % 2` rejection failed 5 pages and refused the whole volume.

**A bug worth remembering:** the scan initially never ran, because `read_at()` returns *success*, not a
byte count, and the loop treated it as one. It was found by **instrumenting** (printing `locate_in_iso`
and `base`) rather than by re-reading the code. The trace is kept behind `OPENGT4_VOL_TRACE`.

**Regression proof:** retail still draws the byte-identical frame (`68d8eb8…`) and still reports its
index at `0x2159` with `toc_pages=407`. The movie volume `GT4L1.VOL` also still resolves.

**What the Spec II disc does now, in the engine's own words:**

```
vol: index at page 0x22b7 (toc_pages 582), source …(Spec II v1.10).iso
menu: /projects/boot/boot.adc: not an ADCH005 script
career: original calendar, price or oil constants unavailable for this disc
loading: original assets unavailable for this disc
```

- The middle line is **the next wall, and it is precise**: retail's `boot.adc` begins **`ADCH005`**
  (104,613 bytes) — the version the port's VM implements; the Spec II disc's begins **`ADCH007`**
  (126,438 bytes) with the internal path `projects/gt4o_us/boot/_tmp_boot.ad` — a **GT4 Online** build.
  `adhoc_code.c:144` accepts nothing but version 5 (`atoi(data + 4) != 5`). Whether 007 differs only in
  the digit or in its frame/opcode layout is the next measurement; `archive/vulcan4-recomp/docs/`
  `REFERENCES-OPENADHOC.md` is the reference for it (OpenAdhoc re-creates GT4's scripts from source).
- The last two lines are **expected, not new**: this disc has no `CORE.GT4`, so the career constants
  (read at fixed addresses inside retail's `CORE.GT4`) are still missing. That is the constant hunt.

### 5. New tool: `tools/vol_file.py`

List, read and extract inside `GT4.VOL` on **either** disc — carrying both format rules discovered
above, plus the compressed-entry path (`DATA_MAGIC` + raw inflate). It is what produced every number in
§4, and it is the tool the constant hunt needs next.

```bash
python3 tools/vol_file.py <disc.iso|GT4.VOL> ls [DIR]
python3 tools/vol_file.py <disc.iso|GT4.VOL> cat FILE [OFFSET] [BYTES]
python3 tools/vol_file.py <disc.iso|GT4.VOL> extract FILE OUT_PATH
```

Measured with it today, and worth knowing: **the disc already ships character models** — the volume root
has `character/` on both discs, and retail holds `driver.mdl`, `driver2..4.mdl`, `driver_mtr.mdl`,
`driver_nike.mdl`, `pitman00/01/03.mdl`, `workman00.mdl`. Drivers, pit crew, workers — the foundation
`G6.30` needs. (And a correction kept on purpose: `/icon` holds **Photo Mode overlay frames** such as
`GT4_film_16_to_9_.ico`, *not* avatar art, which is what I first assumed.)

### 6. Six new wishlist entries — the idea file now carries 34 registered entries

`grep -cE '^#{2,3} .*G[67]\.[0-9]' docs/vulcan4/WISHLIST.md` → **34** (G6.0 … G6.31, including the
`G6.0a` split, plus G7.0). Today added six of them, all the captain's own ideas, registered with their
real prerequisites rather than as slogans:

| Entry | What | Honest position |
|---|---|---|
| `G6.26` | **Custom race modes** — elimination, and the rule-layer family it belongs to | a rule layer, not engine surgery: field + positions + retire already exist |
| `G6.27` | **The Uma career** — one car, upgraded, the arc *is* that car | mostly a new front-end over career tables `career.c` already reads |
| `G6.28` | **Browser build** | hard, not absurd: portable C + WASM suits a PS2-era workload. Blockers: the 4.4 GB disc (File System Access + streaming) and **WebGPU — the same seam Vulkan needs** |
| `G6.29` | **Accounts and cloud saves** | saves are the player's data, never game content. Local-first, off by default, self-hostable, namespaced per disc profile |
| `G6.30` | **The lobby and the avatar** | needs `G6.0a` + `G8` + `G6.22`; first version deliberately small — stand in a lobby as the game's own driver model |
| `G6.31` | **Profile pictures** | the cheap slice of `G6.30`: zero 3D work, zero netcode, and `stb_image.h` is already in the tree |

**Online multiplayer (`G6.22`) was also sharpened into a real design after the captain corrected me**
("this is literally full on online racing"): the goal is real people racing in real time. Every machine
runs the same deterministic 60 Hz sim and sends its **input for the next tick** — ~20 bytes, so a six-car
room is about **1.2 KB/s per player and ~25 MB an hour of racing**. **Bandwidth was never the problem;
latency is**, answered by delay-based lockstep (~60–80 ms comfortable) or **rollback** (local response is
instant; late input rewinds and re-runs those ticks — what fighting games do at 60 Hz on less
deterministic sims). The command-tape re-simulation is the **referee**, not the gameplay: it is what makes
a result trustworthy and a cheater impossible. Hosting is correspondingly small — a lobby, a relay and an
off-line referee — with three tiers: **LAN / direct IP → self-hosted → public ladder**, and the lowest
tier needs no server at all. The original beta's own online mode (`NET/`, `IRX/`, `DNAS310.IMG`) is kept
as a **deliberate fallback**, because it is the only door that lets a real PS2 join.

### 7. Open items, in the order they should be taken

1. **Triage the 7 failing tests** — `pause_confirm` (segfault) first, then the five assertion failures,
   then `storage_audit` / `disc_demo_playback`.
2. **Spec II rung 2 — the `ADCH007` script format.** Measure what actually differs from 005 before
   writing any code; the reference is OpenAdhoc in the archive.
3. **Spec II rung 3 — the career constants**, now with `tools/vol_file.py` in hand: retail read them at
   fixed addresses inside `CORE.GT4`; this disc has no `CORE.GT4`, so they live in `SCUS_974.36` (6.5 MB,
   versus retail's 273 KB) or in the volume.
4. **The renderer seam (`G8`)** — the prerequisite for Vulkan, Metal *and* the browser build.
5. Small and owed: the **credits screen** (licence discipline) and a **launcher disc picker**.

### 8. Does it get **past** the launcher on Linux? Measured, because nobody had

The README says plainly that a full playthrough is *not claimed*. But the cheaper question — does the
engine respond to input and move on, or does it sit there and die — had never been measured either.
Script: `/tmp/interact-test.sh`; frames in `/mnt/ssd/vulcan4-run/interact/` (kept on the SSD, never in
the repo: a screenshot carries rendered game content).

| step | what | frame | distinct colours |
|---|---|---|---|
| 1–3 | idle, 25 s apart, no input | `309,102 B` **× 3, byte-identical** | 7,277 |
| 4 | **after pressing Return** | `353,047 B` — **changed** | 3,333 |
| 5 | after pressing Down | `323,434 B` — **changed again** | 3,328 |

- **The launcher takes keyboard input and advances to a new screen.** That is more than the project had
  confirmed on Linux before today: the frame is stable while idle (three identical frames, no drift), the
  process stayed alive through all five steps, and `stderr` held exactly one line — `OpenGL: reversed
  float depth`, which is informational and appears on every run.
- **What is NOT yet known:** *which* screens those are. Changing colour count proves content rendered,
  not that the flow is correct — the honest next step is a human looking at `step4.png` / `step5.png`
  (the captain judges pictures; per `LAW 9` I do not open images into context).
- Still unproven, and still the reason `E2` is ⏳: a race, a car, a career, a save.

### 9. Commands that reproduce today's results

```bash
# build everything, oracle tests included, on this box
cmake -S /home/or/vulcan4 -B /mnt/ssd/vulcan4-build -DCMAKE_BUILD_TYPE=Release \
      -DOPENGT4_TEST_DISC="/mnt/ssd/gt4/Gran Turismo 4 (USA) (v2.00).iso" \
      -DOPENGT4_RECOMP_TESTS=ON -DOPENGT4_PYTHON=/mnt/ssd/venvs/splat/bin/python
cmake --build /mnt/ssd/vulcan4-build -j4 -- -k          # BUILD=0, 269 targets

# the full suite, in its own memory cgroup
systemd-run --user --unit=vulcan4-ctest --collect \
  --property=WorkingDirectory=/mnt/ssd/vulcan4-build --property=MemoryMax=14G \
  /bin/sh -c 'ctest -j3 --timeout 900 > /tmp/ctest_full2.log 2>&1'

# the retail acceptance gate (frame must stay md5 68d8eb83dc75b6fe6e03cc2372ca186d)
cd /mnt/ssd/vulcan4-run && DISPLAY=:99 /mnt/ssd/vulcan4-build/opengt4 \
  "/mnt/ssd/gt4/Gran Turismo 4 (USA) (v2.00).iso"

# what the Spec II disc says about itself
OPENGT4_VOL_TRACE=1 DISPLAY=:99 /mnt/ssd/vulcan4-build/opengt4 \
  "/mnt/ssd/gt4/spec2/Gran Turismo 4 (Spec II v1.10)/Gran Turismo 4 (Spec II v1.10).iso"
```

## 2026-10-15 — the first Windows build, and the engine reaches a RACE from the captain's own disc

**What was done.** Cross-compiled the engine for Windows with mingw-w64 (`/mnt/ssd/mingw-sdl2/toolchain-mingw64.cmake`,
SDL2 linked statically) → `opengt4.exe`, 6,246,521 bytes, PE32+ console x86-64, importing **only Windows system
DLLs**. Shipped to Baki with a CRLF-correct `run-and-log.cmd`; copied the **retail v2.00 ISO** to Baki and
verified it by hashing both ends (`md5 6e03af5873c30e418021553a8832ca42`, 5,314,478,080 B).

**What was measured.** (1) The cross build is clean: the only two failures were a missing Python module for a
*generator* (`rabbitizer`) and the `launcher_tests.exe` **test** target's SDL2main/`SDL_main` link quirk —
building `--target opengt4` links fine. (2) On **retail v2.00** the engine runs with **one** benign stderr note
(`OpenGL: reversed float depth`) and reports `arcade / TopRoot: drawn 9, not yet drawn 0`, `movie frames=1`.
(3) On the **Spec II disc** it is much worse by construction, exactly as `G6.24` predicted:
`menu: /projects/arcade/arcade.adc: not an ADCH005 script`, `career: ... unavailable for this disc`,
`loading: original assets unavailable for this disc`, `drawn 22, not yet drawn 1`. (4) **The captain then drove
the game himself** and the log records the whole path: `boot / MovieRoot` → `arcade / TopRoot` →
`gtmode / TopRoot` (drawn 123) → `EventHallRoot` (54) → `EventCourseRoot` (40) → `scene: 2560x1440 -> 2560x1440,
MSAA 4x, native` → `quick-event / RaceEventRoot` (42) → **six race cars with engine audio**
(`race audio: car 0..5 engine=… layers=…`) and `race music: PCM/racing`. **Zero errors, zero warnings in the
whole run.**

**What it proved.** The engine walks the real GT Mode path — including the event hall and an event's course
list — and reaches a **race event** with a 6-car field, scene set-up and audio, from the captain's own retail
disc, on Windows, on his own machine. That is the first time this project has been driven by a human.

**What is still open.** The captain reports the **handling feels off**. Two facts bear on it and neither
settles it: `src/core/physics/` is **20,753 lines** and explicitly a hybrid — **22** sites call
`gt4_orig_call` (the original game's code) for "parts not rewritten yet", and `LESSONS-FROM-UPSTREAM.md`
records that in the **shipped product** those call-outs are NULL (the emulation lives in the test harness).
So "off" is expected in a *nameable* way, but it is not yet a number. **The next honest step is the one LAW 13
already mandates:** log the physics state through one driven lap and diff it against PCSX2 at the same point on
the same course, and name the disagreement as a value — not an impression. Also open: the `.exe` was built
from a tree that still has 7 local commits unpushed.

## 2026-10-16 (2nd) — MOON ENGINE gets its face: icons off the captain's own disc, a first-run wizard, and the prep window deleted

**What was done, in order.**
1. **Every button icon in the game, harvested from the captain's own disc.** `/mnt/ssd/harvest_icons.py` walked all
   **575 UI packages (.gpb)** on his retail v2.00 image and decoded **9,905 image assets** to PNG
   (`/mnt/ssd/gt4-icons/`, `INDEX.txt` = package + asset + size for each). Deduped by content into **382 unique
   menu icons** (`BUTTONS/`, `BUTTONS.txt`, plus 4 contact sheets). First pass produced **0 images** because
   `gt4img`'s asset list goes to *stderr*; fixed, re-ran, then handed to the captain.
2. **The captain assigned the dock icons and dropped DISC** (the flow already asks for the ISO at the start):
   `MODS = gtmode/license/coffee_gold.png` (LicenseDetailRoot) · `OPTIONS = gtmode/home/assessment.png`
   (CarViewRoot) · `START = gtmode/course/icon_freerun.png` (CourseEntryRoot) ·
   `EXIT = quick/JP/quick_exit.png` (**quick-arcade**'s ArcadeRoot — the asset is *not* in `/projects/option`,
   which is what the first attempt tripped on). Icons are decoded **at run time out of his own image**; nothing
   shipped, nothing added to the repo. Local commit `a8fb103`.
3. **The restyle moved to Sanji (opencode), who now builds and the captain drives him from the browser.** Panel
   and scrim gone, icons markedly bigger, names in the disc's own face (`hm_16`) instead of ours, and the
   selection is the **game's own cursor** (`image/cursor/cursor.png`) driven by the engine's own cursor motion —
   `gt4_cursor_update` in `src/core/ui_motion.c` at a fixed 60 ticks/s, sprite rotated about its tip
   (`src/core/ui.c:2074-2082`) — rather than the invented 3 px bob the first pass shipped.
4. **First-run wizard** (a plain white screen, not the room): pick the disc with the OS file dialog, then
   OpenGL + Continue; saved to `launcher.ini` in the engine's own user-data root, and the wizard **never appears
   again** once that exists. Then the white screen **fades into the room with the car**.
5. **START fades the launcher to black**, so the game's own opening — which rises from black to white — is the
   first thing seen: the handover is seamless by construction.
6. **The old Win32 preparation window is deleted.** `src/app/launcher.c` went **743 → 16 lines**; the non-UI work
   it carried (argv rewriting for the disc, `launcher_validate_disc`, preferences) is kept, and `main.c:1835`
   now reads a no-argument launch as the studio front-end (`prepared == 2`).
7. **The engine-wide resize attempt failed and was pulled back.** Making the game's render follow the window —
   and having START apply the settings resolution — produced a **2560x1440 (16:9) frame with 4:3-authored
   content zoomed and cropped**, and the game no longer played right. Reverted on the game side; the
   **launcher keeps its uniform, non-stretching resize**. Reference frame for the comparison:
   `/mnt/ssd/vulcan4-ship-win/shot-menu.png` (**1280x960**, the correct 4:3 framing).
8. **`G6.19a` registered and parked** (`docs/vulcan4/WISHLIST.md`, local commit `740a318`): no stretch at any
   window size, for every screen, one map from window to uniform scale — the captain's call was *"lets maybe do
   it later"*.
9. **The Baki lane is now shared.** Sanji's global instructions gained `BAKI.md` (`ssh baki`, scp only, the disc
   path on both machines, the cross-build, `--shot` over ssh and back), and the captain got the opencode web UI
   at `http://10.100.102.11:4096` with the project added so sessions appear.
10. **Ponytail disabled as a plugin** (per-turn injection off, the rules skill stays on the shelf to load on
    demand) — the captain's call after seeing the forced trailing line every reply.

**What was measured.** The Linux tree builds clean (`BUILD=0`) with the day's changes in it, and the full
`ctest` suite (500 tests with the disc) was run as the checkpoint gate. `video_set_aspect_follow_window` is
**gone** from the tree, i.e. the last fix (pulling the game-side resize back) did land.

**What is still open.** (a) The zoom verdict is the captain's, not ours: the game side must reproduce the
1280x960 reference framing before this is closed. (b) Sanji's day of work sat **uncommitted** in the working
tree — banked locally as one checkpoint, **not pushed** (the captain's rule: push only when he says). (c) The
profile/import screen the log calls `save / LoadRoot` still appears on the START path and is part of the
launched prompt. (d) The capture/`save / LoadRoot` screen was drawn over an **uncleared black frame** — visible
in the reproduction at `/mnt/ssd/studio/after-start.png`.
