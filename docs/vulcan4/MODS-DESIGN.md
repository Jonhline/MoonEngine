# THE MOD DESIGN — two sources, in plain language

**Captured 2026-10-15. This is the captain's own design, written down so it cannot drift.**
Wishlist entry: `G6.34`. Technical detail: `docs/NEW-CARS.md`, `docs/NEW-MAPS.md`.

---

## THE DESIGN, IN ONE LINE

**The engine reads two files: your original `GT4.VOL` from the disc, and a second VOL that holds only the
modded stuff.**

Nothing is injected into the disc. Nothing is repacked. The disc stays exactly as Sony pressed it.

```
        disc.iso  ──[ GT4.VOL ]──┐
                                 ├──►  the engine's reader
        mods/cars.vol ───────────┘     (looks in the mod first, then the disc)
```

## WHY THIS AND NOT "PATCH THE ISO" (decided 2026-10-15)

| | patch the disc's own VOL | **two sources (this design)** |
|---|---|---|
| cost per edit | rewrite the whole 5.3 GB image | nothing — edit one small file |
| two mods at once | they fight over the same bytes | they stack, in an order you choose |
| your disc | destroyed/duplicated | untouched |
| writing a mod | needs a VOL writer that does not exist | needs **no writer at all** |

The one thing patching wins: a **patched ISO runs in PCSX2 and on real hardware** — that is how Spec II
ships (a diff). So: **author in this format, and offer an "export → patched ISO" button later**, for sharing
with people who are not running our engine. Same reflex as `G6.25`: never make a second ISO to run one.

## THE SDK'S JOB, IN THE CAPTAIN'S WORDS (2026-10-15)

> *"Sdk will be the middle man. U make ur models and stuff on blender and the sdk will be a pre game prep.
> Will give u all the things u need to make the game communicate best with ur mod and then it converts it to a
> way the game can read it."*

That is the whole product statement, and it names the part every existing tool skips:

```
Blender ──► SDK ─────────────────────────────────────────► mod VOL ──► game
            1. import the mesh + textures
            2. FILL THE GAPS the game demands (physics numbers, SpecDB row,
               price/PP/power/weight, paint, name, wheel tags, a free id)
            3. validate — "this drives like a trolley because…"
            4. convert: booklet (CAR4/MDLS) / Tex1 / .ad
            5. package + test
```

**Step 2 is the middle man's whole value.** The community's tools convert bytes and stop there; a car that is
missing its data is a car the game either refuses or drives badly. The SDK's job is to make a Blender model
**legal in GT4**, not merely similar to one.

## ITERATION SPEED — no, you never recompile the game to test a mod (measured 2026-10-15)

The captain's worry: *"the entire C has to compile ur code first. What if I gotta do testing? No way I gotta
recompile the entire game and wait ages before I can test a 2 second thing."* The design already answers it,
and the numbers are measured on this box:

| what you change | what has to happen | time |
|---|---|---|
| **a mod** (car, track, row, `.ad`, texture) | nothing compiles. The engine reads it at launch, tables merge at load | **0 s** — relaunch |
| **our engine code** (a `.c` in `src/`) | recompile that file + relink | **4 s** (241 build steps, 12 cores; no-op check 2 s) |
| a **cold full build** | everything | minutes — and you never need it for mod work |

Two consequences worth remembering:

1. **Mods are data.** That is exactly why patching the ISO (route 1) was rejected: that path *would* force a
   5.3 GB rewrite per test. The overlay path cannot.
2. **The SDK is the iteration loop, the game is the confirmation.** Validate and preview inside the app —
   the car/course previews already exist in the engine — and launch GT4 only when the tool says it is legal.

## THE COMFORT PRINCIPLE (the captain, 2026-10-15: *"this is all about the comfort here"*)

Modding must never feel like surgery. Three rules follow, and they outrank convenience:

1. **Test before installing.** The SDK is a **test bench**: (a) a static legality check — instant, no
   rendering; (b) a **visual preview** drawn by the engine's own renderer (the car and course previews already
   exist); (c) a **headless test lap** — same engine, no menus — to see whether it drives and whether the AI
   line holds. The game is the *confirmation*, never the experiment.
2. **Your game is never touched.** The disc is read-only by construction (`vol.c`), mods live in their own
   folder or container, and nothing is written back. Uninstalling is deleting a file.
3. **One tick returns you to vanilla.** `LAW 12` is not only a purist's promise — it is the panic button. A
   mod that misbehaves is switched off, not repaired.

## THE ONE RULE — files shadow, lists merge

With two sources, a path can exist twice. What happens then is the whole design:

- **A file** (a car's 3D model, a track, a texture) → **the mod's copy wins.** The disc's copy is hidden for
  that path. Simple.
- **A list** (the car table, the track list) → **the two are combined.** Taking the mod's list alone would
  hide your 722 cars. So our reader appends the mod's rows to the disc's, in the order the table requires.

Adding a car, then, is: **one row in the list + its files in the mod VOL.** Not a swap. Nothing lost.

## WHAT "AS SOON AS POSSIBLE" ACTUALLY LOOKS LIKE — three steps

1. **The switch.** One function in the engine gains one rule: *check the mod file first, then the disc.*
   Verified with an existing path — and with no mods loaded, the game must be **byte-identical** to the disc
   (`LAW 12`: off by default). **✅ DONE 2026-10-16 — `docs/W278-RESULTS.md`.** `gt4_overlay_read`
   (`src/core/vol.c`, called at the top of `gt4_vol_read`): off ⇒ frame md5 `68d8eb83dc75b6fe6e03cc2372ca186d`
   unchanged; on ⇒ one file we authored shadows `/font/system.fnt` and 6.1 % of the frame's pixels change.
   Files shadow; the next dish is **lists merge**.
2. **Prove it with one car.** One added row, pointing at a 3D model that **already exists on your disc**
   (so no model-making is needed yet). Gate: it appears in the car list. If that works, the whole idea works.
   **✅ DONE 2026-10-16 — `docs/W279-RESULTS.md`.** Lists merge (`src/core/mod_merge.c`: a list's rows are
   added, an ordinary file shadows), and `tools/c/moonmod_addcar` writes a **127-byte** mod folder that puts a
   car of its own in the catalog: the engine's own survey goes **722 → 723** with 0 failed loads, and the
   added car loads the source car's model (`OK moon_first_added_car model=ford0015 … 4 wheel assemblies`).
   The same for a course: `moonmod_addtrack` gives `20r60r` (id 43) a second label, `moon_first_added_track`
   → id 177, and the engine reads its 4,452,864-byte course file and line from the mod folder with an
   identical world signature.
3. **The manager screen.** A list where mods are ticked on/off and ordered. UI on top of step 1 — not before.

**Tracks come after cars, using the same switch** — they are just a different folder (`/crs/` instead of
`/car/`).

## THE LIMITS, HONESTLY

- An event's allowed-car list is a **60-slot row**; the save holds **128 track keys** and 110 are already
  used, so **18 free additions** — the 19th would be dropped silently. The manager must **show that meter**
  rather than lie about it.
- Those limits are *our* numbers in *our* C code. When we want more, **we** raise them in a release — a
  modder never touches machine code, and you never patch a disc.
- The **AI line is no longer a mystery** (answered 2026-10-15): it is the course's **`.ad` companion file**,
  the game's "Autodrive" data (`ADLN` records; measured on your disc — 8 records per course). So a custom
  track is **two artifacts**, and the work is authoring an `.ad`, not hunting for the line.
- The community's own wall is the **asset side**, in the author's words: *"we can't make new cars yet"* — no
  tool can author a GT4 mesh or a GS-legal texture set. **That is the hole this SDK fills.**

## THE FENCE (never changes)

We ship the **readers and the manager**; the user brings the **cars and tracks**. No mod content, and no game
content, ever enters the repository (`LAW 1`, `G7.0`). Credits travel in each mod's manifest.

---

## FEASIBILITY AUDIT — 2026-10-15 (is this plan actually possible?)

**Verdict: yes, and more than half of it is already supported by code that exists today.**

**Already proven by the tree (measured, not assumed):**

1. `src/core/vol.c:217-219` — when the ISO lookup fails the reader falls back to **`v->base = 0`
   (comment: "assume a bare GT4.VOL")**. A standalone mod container is therefore *already* an accepted input;
   no new format code is needed to open one.
2. `src/core/vol.c:237-256` — the index page is found by trying the known page then **scanning forward**,
   with both discs' real values recorded in the comment (retail `0x2159`, Spec II `0x22b7`). A mod volume's
   index can sit anywhere in its first 128 MB.
3. The volume reader handles **raw and compressed entries** (`type 1` / `type 2`, `vol.c:21-29`) — a mod
   container may store files uncompressed, which keeps any future writer simple.
4. **One funnel, high coverage:** 140 `gt4_vol_read` call sites across 33 files; the only file reads that
   bypass it are saves, config, PNG export, the launcher, EE memory and the mission pad — none of which are
   content.
5. **Precedent exists:** `src/core/ui_hd.c:18-37` already reads a *user-supplied* file by path (the HD
   texture pack, `HDI2`). Serving user files is not a new idea in this engine.

**Genuinely missing (do not pretend otherwise):**

1. **No RoFS writer exists.** Nothing in the repo writes a volume, and `docs/VOL_FORMAT.md` is 33 lines — the
   reader code *is* the spec. Bounded work, but it is real work.
2. **No merge code.** Combining a mod table with the disc's is new; the sizes make it cheap
   (`GENERIC_CAR.dbt` ≈ 18 KB, `COURSE.dbt` ≈ 4 KB, whole set < 50 KB), but it is new.
3. **Fifteen files each open their own volume** (`gt4_vol_open`, 27 references) — the second source must be
   attached at every open site, via a helper, not installed globally.
4. The tracks half still lacks the AI line.

**SEQUENCING DECISION (this is the risk-lowering move):** step 1 accepts a **plain directory** as the mod
source — no format work, nothing new to write — so the read path (shadow + merge) is proven on its own.
**The VOL container is added afterwards**, once the writer exists. Same design, less to lose in one go.

