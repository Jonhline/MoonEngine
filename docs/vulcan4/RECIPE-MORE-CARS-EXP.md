# RECIPE — MORE THAN SIX OPPONENTS, EXPERIMENTAL BUILD (2026-10-25)

Captain: *"Can we work on the more then 6 opponents and he can make an experimental build?"*

**SCOPE:** `global` — the field-size parameter is engine-wide (PC + Android + every later platform).
The Tier-1 guest patch only applies where CORE.GT4 is loaded, and is a no-op at 6 cars.

**Branch:** `experimental/more-than-6`, cut from current `main` (clean at `0783111`). Never push.
One commit per step, author `Or Golan <or024662@gmail.com>`. Never touch `/mnt/HDD`.

Source of truth (read first, do not re-derive): `docs/vulcan4/GOAL-MORE-THAN-6.md` §6c + §7 and
`docs/DESIGN-FLEXIBILITY.md`.

---

## STEP 0 — REPORT ONLY, NO EDITS

Confirm, from the shipped image, the six sites you intend to rewrite and print the **exact instruction
words** you will replace, plus the current contents of `0x01CE6910` (must still be zeros).

| what | from | to | sites |
|---|---|---|---|
| car blocks → states | `0x6698` | `0x7798` | 0x1572B4, 0x34BF54, 0x34C1AC |
| states → 0x10 array | `0xCA58` | `0xC158` | 0x14FDF0, 0x2D0CB0, 0x2D43B8 |

If **any** word differs from the expected instruction, **STOP** and report — do not patch a different
disc's code. Then post the one-line plan and continue.

## STEP 1 — ONE PARAMETER, ZERO BEHAVIOUR CHANGE

- Add `race.field_size` to the config layer. **Default 6**, range `1 … MOON_MAX_CARS`.
- `MOON_MAX_CARS = 7` for this brick, with the 16-bit reason written next to it (see the goal doc §7:
  `tail_shift = (N-6) * 0x21A0`; 12 is the addressable ceiling, 7 is what we implement now).
- Replace **every** ceiling in `docs/DESIGN-FLEXIBILITY.md`'s table with a `field_size`-derived value:
  `race_bspec.h:16`, `main.c:5900`, `race_game.c:970` clamp, `last_colors[5]`, results arrays,
  `race_hud.c:1156` grid letters, `race_run.c:97` entry table, `career_event.c:240`.
- **No silent clamp.** A refused value prints the value, the allowed range and the reason.
- Default run must be **byte-identical** to today.

**GATE A:** default-config race unchanged (same frame hashes as before the patch) + `ctest -R` on the
touched units green. State the hashes.

## STEP 2 — TIER 1 GUEST PATCH (guarded, only when field_size > 6)

- Patcher rewrites the six immediates above at load time, refusing on any word mismatch, logging
  `addr before → after` for each.
- Nothing else in the image is touched. The three `0xCA58` decoys (`lui 0x15 / 0x6a`) must be proven
  *not* patched by dumping them after the patch.

## STEP 3 — THE SEVENTH CAR

- Fill `entries[6]` with a block at `0x01CE6910`, initialised the way `make_race` does slots 1–5.
- Renderer already allows 8 (`race_view.c:46`); HUD, results, grid letters loop to `field_size`.
- Replay: a `>6` race **refuses to record**, with a visible message (never write an unloadable file).

**GATE B (the captain's own eyes):** Nürburgring screenshot with **7 cars visible**, seven start,
seven finish, results show seven rows, and a 5-minute race leaves the three conductor arrays sane
(dump counts/sentinels before and after — no corruption).
**GATE C:** replay refusal demonstrated in the log.

## STEP 4 — BUILD THE DISH

- Linux build + the Windows shapes still green (no behaviour change there).
- Android arm64 APK with the existing toolchain:
  ```bash
  JAVA_HOME=/mnt/ssd/android-dl/jdk/jdk-17.0.20.1+1 ANDROID_HOME=/mnt/ssd/android-sdk \
    /home/or/.hermes/hermes-agent/venv/bin/python3 tools/build_android.py \
    --sdk /mnt/ssd/android-sdk --gradle /mnt/ssd/gradle-8.14.3/bin/gradle
  ```
- Copy to `/mnt/ssd/share/apps/moonengine-more-cars-exp.apk`, print md5 + size.
- Report: what changed, the gates with real numbers, and what is deliberately **not** done
  (8/10/12 cars, pool mode, auto-scrolling grid).

## HOUSE RULES

- `-j4`, `nice -n 10`, `TMPDIR=/mnt/ssd/tmp`, `/` is nearly full.
- Do not open more than a handful of images — write the PNG, report the path; Caine looks.
- Do not restart any service. Do not write the captain's state files (`launcher.ini`, `video.ini`,
  `mods.ini`, `saves/`, `studio-rig.txt`).
- Honest out: if a step cannot be made safe, say what you measured and stop there.
