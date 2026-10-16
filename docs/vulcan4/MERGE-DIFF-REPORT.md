# HIS CODE vs OURS — the measured difference, before anything is carried across

*2026-10-16, after the captain asked "how did we push before?" and "check the actual differences, don't push
yet". Everything here is measured with git, not remembered.*

---

## 1. HOW THE FIRST PUSH WORKED (the answer to his question)

The branch we pushed was **`linux-port`**, and its **parent is `c1ca129` — HIS commit**
(`git log --oneline --parents -1 32bbf82` → `32bbf82 c1ca129`). It carried exactly **2 commits**
(`32bbf82` Linux bring-up, `37e06cb` the Linux build docs) and nothing else.

**That is the whole method: a branch started from *his* commit, carrying only feature commits.** Our `main`
was never pushed and never can be — it holds 653 commits of our own project history.

## 2. THE COMMIT AUDIT (is it a graveyard? no — it is two projects in one history)

All 651 of our commits are authored `Or Golan` (plus 3 with a typo'd name from the authorship rewrite). They
are small and frequent — the agents commit every step:

| day | commits | day | commits |
|---|---|---|---|
| 2026-09-29 | 16 | 2026-10-07 | 46 |
| 2026-09-30 | 86 | 2026-10-08 | 71 |
| 2026-10-01 | 76 | 2026-10-09 | 32 |
| 2026-10-02 | 42 | 2026-10-10 | 10 |
| 2026-10-03 | 65 | 2026-10-11 | 2 |
| 2026-10-04 | 34 | 2026-10-13 | 32 |
| 2026-10-05 | 27 | 2026-10-15 | 52 |
| 2026-10-06 | 15 | 2026-10-16 | 46 |

**The era split matters:** 520 of them are 2026-09-29 → 2026-10-12 = the **old recompilation project**
(nothing to do with Moon Engine — it is our own archived work). Only the **135 since 2026-10-12** are the
fork/Moon Engine era, and today's 46 are the mods work plus the documents.

So: nothing needs deleting. Our repo is the workshop; his repo is the shared one; only feature commits cross.

## 3. WHAT HIS TWO COMMITS ACTUALLY CHANGED (`2033387 → bcd1871`)

| area | files | + | − |
|---|---|---|---|
| `src/platform` | 51 | 3,498 | 189 |
| `src/app` | 48 | 2,223 | 849 |
| `src/core` | 104 | 471 | 264 |
| `tests/ui` | 48 | 978 | 138 |
| `tests/render` | 10 | 635 | 0 |
| `third_party/vulkan` | 4 | 1,285 | 0 |
| `CMakeLists.txt` | 1 | 1,089 | 914 |
| `docs/notes` | 26 | 171 | 140 |

**The two big things in it:** a **Vulkan** path (`third_party/vulkan` = volk + glslang licences, `tests/render`
= its tests) and **his own launcher** (`src/app`: `launcher_scene.c` **new, 556 lines**, `launcher_skin.c`
**new, 588 lines**, `launcher_update.c/.h` new, and `launcher.c` rewritten **+611/−344**).

**Structural difference to respect:** his `main.c` is **split** (`main_audio.inc`, `main_movie.inc`,
`main_race_flow.inc`, `main_session.inc`, `main_support.inc`); ours is one 3,290-line file. Our studio work
must be ported into his split shape, not pasted over it.

## 4. THE LAUNCHER — THE CAPTAIN'S CALL: HIS IS A TEMPLATE, OURS IS THE ONE

> *"Yes his launcher. His launcher is just a template. Our launcher is the chosen one we gonna use. So delete
> his. Ur allowed."*

| file | his tree | ours | decision |
|---|---|---|---|
| `src/app/launcher.c` / `.h` | his rewrite (+611/−344) | ours (`--launcher-page/-shot/-install`) | **ours wins** |
| `src/app/launcher_scene.c/.h` | new (556) | — | **delete his** |
| `src/app/launcher_skin.c/.h` | new (588) | — | **delete his** |
| `src/app/launcher_update.c/.h` | new | — | **delete his** |
| `src/app/launcher_ui.c/.h` | — | ours (923 lines: pages, mod card, zip install) | **carry ours** |
| `src/app/launcher_text.c/.h`, `launcher_fonts_data.h` | — | ours | **carry ours** |
| `launcher_config/​icons/​photos/​gallery.c` | shared | shared | keep, merge by delta |
| `src/app/main.c` | his split version (owns the studio draw + a dock) | ours (monolith, the studio) | **port our studio into his split shape** |
| `CMakeLists.txt` | +1,089/−914 (Vulkan targets + his launcher sources) | +96 | merge: his Vulkan targets stay, his launcher sources come out, ours go in |

**Consequence to accept:** deleting his launcher means his `main.c` references to
`launcher_scene`/`launcher_skin` must be removed or redirected to ours, and `CMakeLists.txt` must stop
compiling his launcher files. That is the first thing to do, and it must leave his Vulkan path untouched.

## 5. WHAT CROSSES, WHAT STAYS

**Crosses (ours):** the whole mods layer (`mod_merge`, `mod_manifest`, `vol.c`'s overlay + merge, `model_write`),
the menus/dealer plumbing in `menu.c`, our launcher (`launcher.c`, `launcher_ui.*`, `launcher_text.*`), the
camera separation in `showroom.c/.h`, the prefs-folder helper, the tools/tests/gates, and
`docs/vulcan4/*` (including `LAUNCHER-ARCHITECTURE.md` for him).

**Stays (his):** Vulkan, `tests/render`, the Switch work, the refactored `src/core` and `src/platform`, the
split `main.c` includes, the release tooling (and the `moonengine.iss` installer rename).

**Not pushed, ever:** our 653-commit history.

## 6. STATE RIGHT NOW

- His work: fetched, parked in `moonengine/main` (`bcd1871`); a clean clone of his repo exists at
  `/mnt/ssd/moonengine-clean` (8 commits, nothing of ours).
- Our work: `main` @ `e7deea3`, clean, untouched.
- `push/features-1016`: his `main` + our 3 replayable commits (the switch, `gt4table_dump`, lists merge).
- **Nothing has been pushed. Nothing has been deleted in either tree.**
