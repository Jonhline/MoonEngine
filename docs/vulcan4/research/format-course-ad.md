# Format: the course companion — `/crs/<label>.ad`

Scope: read-only trace of `src/` (VULCAN 4, the OpenGT4 fork) plus **measured reads of the
user's own disc** through `tools/vol_file.py`. Every field is quoted with `path:line`; every
byte reported was printed by a command in §6. Where nothing in this tree reads a byte it says
**UNKNOWN** — no offsets are invented. Companion of
`docs/vulcan4/research/format-course.md` (the `/crs/<label>` container); that file already
summarised this one in its §5, this file *is* the full read.

`COURSE` = the resident course object (`race_run.c:42` `#define COURSE 0x00bcd180u`),
`COURSE_DATA` = `COURSE_DATA + runway` (`race_run.c:43-44`).

---

## 1. WHAT READS IT

**The physics/AI race loader reads it, and refuses a race without it.**

- `race_run.c:212` `load_course` builds both paths (`:219`, `:221`):
  `snprintf(path, sizeof path, "/crs/%s", name)` then `"/crs/%s.ad"`, and reads both with
  `gt4_vol_read`.
- `race_run.c:223-231` — `if (!crs || !adf || an < 24 || !gt4_course_open(&co, crs, n))` →
  `"cannot read course %s"` (`:224`), `return 0`. So a `.ad` shorter than 24 bytes or missing
  fails the whole race load. `an > 0x40000u` is rejected as `"course %s too big"` (`:232`).
- `race_run.c:238` `memcpy(gt4_ptr(ad), adf, an);` — the `.ad` is copied **verbatim** into
  physics RAM, at `ad = (rw + size + 15u) & ~15u` (`:230-231`), i.e. 16-byte aligned right
  after the relocated runway.
- `race_run.c:242-245` — the header's five offsets become five pointers in the course object:
  ```c
  for (uint32_t k = 0; k < 5; k++) {
      uint32_t off = le32(adf + 4 + k * 4);
      w32(COURSE + 0x9c + k * 4, off ? ad + off : 0);
  }
  ```
  → `COURSE+0x9C` (lines), `+0xA0`, `+0xA4`, `+0xA8`, `+0xAC` (AI settings).

The same requirement is duplicated in the harnesses, so the tests fail the same way:
`tests/physics/race_setup.c:115-119`, `tests/physics/setup_run.c:506-511`.

**Who consumes the five slots**

| Slot | Reader | What it is |
|---|---|---|
| `COURSE+0x9C` | `lines.c:372-378` `gt4_lines_setup(lines, GT4_U32(course + 0x9c))` | the course's authored lines |
| `COURSE+0x9C` | `ai.c:37` `if (GT4_U32(GT4_U32(GT4_U32(lines) + 0x9c)) == LINE_MAGIC)` (`LINE_MAGIC 0x4e4c4441u /* "ADLN" */`, `ai.c:21`) | picks the wide 0x40-byte point format |
| `COURSE+0x9C` | `race_pit_crew.c:113-116` `table=GT4_U32(lines+8)` … `line=GT4_U32(table+16)` | pit-box root, 8 points of line 4 |
| `COURSE+0xA0` | `init.c:880-884` `course_ai_settings`: `GT4_U32(GT4_U32(cond + 0xcbd8u) + 0xa0)`, else `0x00623dc0u` | the course's AI settings row |
| `COURSE+0xA0+n*4`, `n<4` | `ai.c:2174` `row = GT4_U32(GT4_U32(cond + 0xcbd8u) + (n < 4 ? n : 0) * 4 + 0xa0);` then `RB(0)`…`RB(0x17)` (`ai.c:2175-2213`) | the AI profile row **per class n** |

The lines are relocated and cooked **once per session** into `cond+0xda50`
(`init.c:1044-1045` `gt4_lines_setup(cond + 0xda50u, …)` / `gt4_lines_clear(cond + 0xda50u)`),
and every AI/driver read goes through that object: `ai.c:33-52` `line_get` (table at
`lines+8`), `driver.c:289`, `race_run.c:621` `GT4_U32(car + 0x14 + 0xec) = … + 0xda50u`,
`race_run.c:790` `gt4_line_course_point(r->cond + 0xda50u, 6, d, p)`, `init.c:381` rolling start,
`race_pit_crew.c:27`.

**Who does NOT read it**

- The renderer's course preview: `race_view.c:293-300` reads only `/crs/%s` and requires only
  `gt4_course_open` — no `.ad` anywhere in that file. Same for the licence scene loader
  (`docs/FIDELITY.md:412`: *"loader da física carrega só RNW4/.ad"* — the physics loader loads
  only `RNW4` and `.ad`).

---

## 2. FIELD LAYOUT

### 2.1 Header (required by `an < 24`, `race_run.c:223`)

| Off | Type | Meaning | Evidence |
|---|---|---|---|
| `+0x00` | u32 | **UNKNOWN** — `0` in **110/110** files, nothing in `src/` reads it | measured; no reader in `src/` |
| `+0x04..+0x17` | 5×u32 | file-relative offset of block 0..4, in that order; written to `COURSE+0x9C..+0xAC` | `race_run.c:242-245` |
| `+0x18..+0x7F` | — | zero padding (all 110 files) — first block starts 16-aligned at 0x80 | measured |

**Measured invariants (110/110 on `Gran Turismo 4 (USA) (v2.00)`):** word `+0x04` = `0x80`
always; the five offsets are monotonic and `< size`; blocks 1..4 are **exactly 0x80 bytes**
each and back to back (`o[k+1]-o[k] == 0x80`, `size == o[4]+0x80`); file size = `0x80` +
block0 size + `4*0x80`. Block0 therefore occupies `0x80 .. o[1]`, and its size runs from
`0x200` (gymkhana) to `0xf700` (nurburgring, 63872 B file).

### 2.2 Block 0 — the line set (`COURSE+0x9C`, "ADLN" or the legacy form)

Two authored formats, both handled by `lines.c`:

**Wide — `ADLN` (93/110 files):** `lines.c:282-287` `int wide=GT4_U32(adln)==LINE_MAGIC;`
then `adln_relocate(adln); GT4_U32(lines+4)=GT4_U32(adln+0x14); GT4_U32(lines+8)=GT4_U32(adln+0x18);`

| Off | Meaning | Measured |
|---|---|---|
| `+0x00` | `"ADLN"` (0x4e4c4441) | `41 44 4c 4e` at 0x80 in 93 files |
| `+0x04` | 0 — the relocate writes its own address here as the "already relocated" marker (`lines.c:216-218` `if (GT4_U32(adln + 4)) return; GT4_U32(adln + 4) = base;`) | 0 on disc |
| `+0x0C` | **UNKNOWN** — equals block0-size − 0x30 in only 50/93 files, so it is not a size | measured |
| `+0x10` | 0 | measured |
| `+0x14` | line count = **8** | 8 in 93/93; read by `lines_build` (`lines.c:284`) |
| `+0x18` | file-relative offset of the line table = `0x30` | read by `lines.c:285`; table then read at `adln+0x18+k*4` (`lines.c:219-221`) |
| `+0x30` | 8×u32 file-relative offsets, one per line | measured `0x50,0x90,0xe10,…` |
| line+`0x00` | u32 point count | measured |
| line+`0x40` | first point, stride `0x40` | `lines.c:233` `for (uint32_t p = line + 0x40; …) p += 0x40` |

**Legacy — fixed point (17/110 files):** `lines.c:236-248` *"the older fixed-point format used
by the licence courses. Its points start at +4 and have stride 0x28"*. Measured block start =
a u32 (`0x1d8`, `0x930`), `+0x04` = `8` (line count), `+0x08` = the 8-entry offset table
(`0x28, 0x2c, 0x30, …`), each line = u32 count then points at `+4` stride `0x28`
(`lines.c:244-247`, scaling `K(0x39800000u)` = 2⁻¹² = 0.000244140625). The player's line index
check is on the same magic: `race_pit_crew.c:115` `int wide=GT4_U32(GT4_U32(GT4_U32(lines)+0x9c))==0x4e4c4441;`.

**Both formats are mutated in place at load** (`lines.c:212-234` / `:236-248`): offsets→addresses,
`x` and `z` negated, heading `+= π` (wide) or `*2π + π` (legacy). So on disc the points are
"raw"; in RAM they are the driver model's coordinates.

**Line point (`ai.c:25-31`, verified against the bytes):** `+0 kind` (0 straight, else arc),
`+1 s8 speed trim %`, `+2 s8` (−128 skip), `+4 x`, `+8 z` (wide, negated at load), `+0xC` ground
height (**filled by the game, 0 on disc**), `+0x10 s16`, `+0x12 s8 surface`, `+0x14 distance
along the course`, `+0x18 arc radius`, `+0x1C turn angle`, `+0x24 arc start angle`, `+0x28 byte`.
Everything downstream of the authored polyline (height, pitch, roll, surface, distance) is
computed at load from the runway: `lines.c:277-345` (`ground_at`, `course_distance`).

### 2.3 Blocks 1..4 — the AI settings rows (`COURSE+0xA0`, `+0xA4`, `+0xA8`, `+0xAC`)

Exactly `0x80` bytes each, 4 of them, in every file measured. Read as a **byte table**:
`ai.c:2174-2213` takes the row for class `n<4` and reads `RB(0)`…`RB(0x17)` with mixes like
`RB(0) * hundredth` (0.01), `(U8(row+6)*2) * K(0x3e8e38e3u)`, `U8(row+0x11)*10u`. The first
bytes are plain per-course data, not a magic (measured: yosemite `6e 64 64 1c 64 00 5a 03 1e 64
03 00 00 1e 64 00 09 00 4e 55 64 96 00 50`; gymkhana_spral `64 56 64 08 64 00 5a 0f 28 0f 05 0a
00 3c 64 05 09 00 50`; akasaka `7d 64 64 14 64 00 5a 03 1e 64 08 00 00 32 64 00 06 00 4e 55 64
a0 00 8c`). The four rows differ **within** a course (all four `!=` each other in yosemite,
akasaka, gymkhana_spral, ss_snow) — they are the 4 difficulty/class variants.
Field-by-field meaning beyond the reads at `ai.c:2174-2213` is **UNKNOWN**.

---

## 3. WHAT IT CARRIES

**Yes — it is the opponents' driving line, and the player's own driver model line.**

`ai.c:1-4`: *"The driver model's course line (the AI's racing line, also run for the player)"*.
The car's model block `obj` (+0x14 in the car) holds `+0x1c` = **line index**, `+0x1d` = section,
`+0x1e` = state (`ai.c:6-10`), and the controller reads the line every step
(`ai.c:97`, `:225`, `:707` `aim_point(lines, car, OU8(0x1c), OU8(0x1d), …)`, `:803`, `:878`).
Line switches go through `line_pick` + `gt4_ai_line_switch` (`ai.c:1875-1900`, `:1232-1250`),
whose fallback list is guest static data (`0x00624030`) — see §5.

**Line identities readable in `src/`:**

| Index | Role | Evidence |
|---|---|---|
| 4 | pit lane + start grid (8 slots) | `laps.c:197` *"The pit lane: the marks are the points of line 4"*; `ai.c:1279-1281` *"where car i starts on line 4 (point 8 - i)"*; `lines.c:295-309` snaps line 4's first points onto the runway grid slots (`rw+0x90`, within 5 m); `ai.c:1438` `GT4_U8(car + 0x14 + 0x1c) == 4` = "out of the reckoning" (in the pits); `race_pit_crew.c:113-116` |
| 6 | middle line / rolling start / course point | `ai.c:1904-1905` *"the line to the left / right of `line` (1 left, 6 middle, 2 right)"*; `init.c:381` rolling start defaults to line 6; `race_run.c:786-790` *"the course's line 6 at distance d"*; `ai.c:1380-1385` |
| 1, 2 | left / right lane | `ai.c:1904-1920` `lane_left` / `lane_right`; `ai.c:1258-1266` nearest of 6, 2, 1 (`0x00624020`) |
| 5 | per-race cleared line | `lines.c:380-397` `gt4_lines_clear` zeroes line 5's `+0x38/+0x3C` per point |
| 0, 3, 7 | present in some courses, absent in others | measured: yosemite lines 0/5/7 = count 0, line 3 = 61 points; nurburgring 0/5/7 = 0; akasaka line 0 = 0 |

**Measured content** (93 wide files, line count field = 8 in every one):
yosemite line1 = 53 points (first point x=−118.9, z=−53.3), line2 = 53, line3 = 61,
line4 = 12, line6 = 53; akasaka line1 = 55, line4 = 12; nurburgring line1 = 249, line4 = 13.
The same course run in reverse is a **separate file** with different lines
(`/crs/yosemite.ad` 16128 B vs `/crs/r_yosemite.ad` 16512 B, block0 bytes unequal).

**It does NOT carry** the checkpoints/sectors, the start grid table, the road mesh or any render
data. Checkpoints are the runway's: `session.c:25-31` *"The course's checkpoint set: the runway's
own (v < 0, course+0xB0) or one of its variants (32 bytes each at runway+0x9C)"*; grid slots are
`rnw+0x90` (`lines.c:302-308`). Both live in `/crs/<label>`, not here.

---

## 4. IS IT REQUIRED

**Yes, for any race.** `race_run.c:223` fails the load — no `.ad`, no race, in the engine and
in the test harness (`tests/physics/race_setup.c:119`, `tests/physics/setup_run.c:510`).

**But only block 0 is load-bearing.** The five offsets are written as
`off ? ad + off : 0` (`race_run.c:244`), i.e. a zero offset is tolerated as a **NULL slot**:
- blocks 1..4 = 0 → `init.c:883-884` falls back to the built-in default `0x00623dc0u` (+0x40
  for race modes 2/3) and `ai.c:2175-2176` the same; the race runs with default AI settings.
- block 0 = 0 → the lines object gets a 0 pointer, `lines_build` would dereference guest address 0
  and `lines+0xc` stays 0, and every consumer is guarded: `ai.c:1215` `if (!GT4_U16(lines + 0xc))
  return 0;`, `lines.c:377` `if (!line_of(lines, 3) || !line_of(lines, 4)) GT4_U16(lines + 0xc) = 0;`,
  `race_pit_crew.c:113` `if (!GT4_U16(lines+0xc) || !table || entry>8) return;`.
  Whether a PS2 race survives that is **UNKNOWN** — no disc file produces it.

**On disc the question does not arise:** `/crs` holds **110 courses and 110 `.ad` files, 1:1, no
orphans** (measured). The only `/crs` entry without an `.ad` is `bestline`, and its index type is
`0` — not a file (the tool prints it as `dir bestline`), so it is not a course.

**Degradation seen in practice:** the renderer-only path never opens `.ad`, so preview/photo
scenes and the licence loader work from `/crs/<label>` alone (`race_view.c:293`;
`docs/FIDELITY.md:412` — the licence loader borrows `Env0` from the resident course because
*"loader da física carrega só RNW4/.ad"*).

> For a custom map the practical rule: **copy a stock `.ad` whose line count and lap length are
> closest** — the loader validates none of its geometry, and the whole thing is cooked against
> the runway at load (`lines.c:277-345`), which is why a mismatched line still runs (badly)
> instead of refusing.

---

## 5. UNKNOWNS

- Header `+0x00` (0 in 110/110, no reader) and `ADLN+0x0C` (not the block size: matches in only
  50/93) / `+0x10` (0) — nothing in `src/` reads them.
- **Which line each AI class picks.** `LINE_ORDER` (`lines.c:12` = guest `0x00624018`),
  the fallback lists `0x00624030` / `0x00624020`, the default AI profile `0x00623dc0` and
  `AI_SETTINGS 0x00623e50` (`ai.c:20`) are **guest static data in the PS2 image, not in the
  `.ad`**; they were not read from the image here. So "line 0/3/7 are empty" is a fact about the
  bytes, not yet a statement about which line is used.
- Per-field meaning of the four 0x80-byte AI rows beyond `ai.c:2174-2213`'s reads
  (which byte is braking, cornering, aggression…).
- Whether the original engine tolerates a 0 block-0 pointer (no oracle read).
- The `s16` at line point `+0x10` (read as the ground id, `ai.c:1309-1316`) and the `+0x28` byte:
  the values were never traced end to end.

---

## 6. HOW THIS WAS MEASURED (reproducible)

```bash
python3 tools/vol_file.py "/mnt/ssd/gt4/Gran Turismo 4 (USA) (v2.00).iso" ls /crs
python3 tools/vol_file.py "/mnt/ssd/gt4/Gran Turismo 4 (USA) (v2.00).iso" cat /crs/yosemite.ad 0 256
```

plus a read-only pass over all 110 `.ad` files with `tools/vol_file.py` imported as a module
(`Vol(...).read_file`), printing: header words, the five offsets, block0's magic and line table,
and each block's first bytes. Numbers quoted above (110/110 headers, 93 `ADLN` / 17 fixed-point,
0x80-byte AI blocks, line counts 8, per-line point counts) are from that pass. No file was
extracted and nothing is committed (LAW 1).
