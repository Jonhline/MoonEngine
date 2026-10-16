# GT4 community TRACK knowledge — web research

Research date: 2026-10-15. Scope: tools, documented course layout, the `.ad` companion, collision/AI-line/grid handling, and any genuinely ADDED course.
Primary sources preferred. Every claim carries a URL; dates where the page states one. GT4 vs GT5/GT6 kept separate.

---

## 1 TOOL INVENTORY

### GT4-capable (PS2)

| Tool | Author | What it does | Can it WRITE a course? |
|---|---|---|---|
| PDTools | Nenkai (MIT; repo created 2021-06-23) | Library of readers for GT file formats across most GTs. | **No finished writer.** Source has `ToStream` methods, but format coverage is explicitly incomplete (below). |
| GT4FS | xfileFIN, q-k, Echelo, Nenkai | `.VOL` extractor and repacker for GT4. | Not a track tool — but it is the only way to *inject* a course into the disc container. |
| GTSpecDB | Nenkai, xFileFIN | "Tool to view, edit, and export the car and **course** databases for GT4. (Note: multiple table editing may currently break the game)". | Writes the `COURSE` table (course ids/labels) — this is the "add a course" lever, not geometry. |
| GT4Hooks | Nenkai | Code hooks, HostFS support. | Dev-time file loading, not authoring. |
| GT.RText / GTMusicLibraryEditor / Awave / PSound / VagConvSharp | various | Text/audio. | n/a |

- Sources: https://nenkai.github.io/gt-modding-hub/ps2/gt4/tools/ (page footer April 25, 2023 / Aug 4, 2026); https://github.com/Nenkai/PDTools (repo metadata).
- PDTools course/model credit line: **"ddm999 - Course files & other bits"** — https://github.com/Nenkai/PDTools (README Credits). ddm999 is the person who reversed the course files.

### PS3-only (GT5/GT6) — do NOT conflate

- **GTTrackEditor / GTTrackViewer** — ddm999. Listed by the modding hub under **PS3 tools** as: *"GTTrackEditor | ddm999 | Just a track viewer. (unfinished, rough state)"* — https://nenkai.github.io/gt-modding-hub/ps3/tools/. Repo: https://github.com/ddm999/GTTrackViewer (description "very unfinished"; created 2021-09-28; last push 2023-03-17; contributors ddm999 + Nenkai). Solution name inside the repo is `GTTrackEditor.sln`. **No GT4 support is claimed anywhere.** Viewer only even for PS3.
- **GT6 Track Path Editor** — Polyphony's official tablet app (TED file). GT6 only. Community reverse-engineering of the TED "ROAD SECTION" / "DECORATION SECTION" (20-byte road entries, 24-byte decoration entries) exists on GTPlanet (https://www.gtplanet.net/forum/threads/modified-track-path-editor-tracks-discussion.337816/page-38) — **that format is GT6's course-maker format and has nothing to do with GT4's course files.**

### Bottom line on (a)

Nothing publicly available **writes** a GT4 course. The chain is: reverse (PDTools readers, partial) → repack (`GT4FS`) → register (`GTSpecDB`). PDTools' own README states the gaps:
- `Runway` (GT4 `RNW4`/`4WNR`): *"Not fully completed, there may be a KDTree that needs to be reversed."*
- `ModelSet` GT4 (`ModelSet2`): *"GT4 is not completely done."*
- Textures (PS2): *"Not fully finished... We don't save enough space to be able to edit existing models rather than scratch."*
Source: https://github.com/Nenkai/PDTools/blob/master/PDTools.Files/README.md (file dated by footer/repo activity; content current at 2026).

---

## 2 COURSE FORMAT KNOWLEDGE (public)

Modding-hub page **"GT4 Course Data"** — *Applies to: GT4, TT, GTHD · Extension: None · Endian: Little* — https://nenkai.github.io/gt-modding-hub/formats/courses/gt4_coursedata (page footer July 28, 2025 / October 3, 2023).

Verbatim key points:
> "The Course Data file holds all the contents of a track/course. It is split in multiple components making up a track. This file is completely loaded into memory, and used in-place. It should not exceed 12mb in size (which is already 1/3 of the PS2's ram on its own!)"
> "Most offsets can be set to 0 so that a component won't be processed. Vision lists can be set to 0, everything will be rendered for the specified component at the expense of framerate."

Header: fixed `0x100` block. Offsets we care about:

| Field | Offset | Type |
|---|---|---|
| Relocation Pointer | `0x00` | int |
| Main Model Set | `0x04` | ModelSet2* |
| Main Model Vision List | `0x08` | VisionList* |
| Reflection Model Set | `0x24` | ModelSet2* |
| Skybox / Far Clouds / Fog Clouds Model Sets | `0x54` / `0x58` / `0x5C` | ModelSet2* |
| Environment Parameters | `0x80` | CourseEnvPtr* |
| Minimap | `0x84` | MiniMapSet* |
| Course Sounds | `0x8C` | CourseSound* |
| Replay Camera Data | `0xB0` | GT4ReplayData* |
| Pre-Race Cameras | `0xC0` | GT4ReplayData* |
| **Runway** | **`0xDC`** | **RunwayGT4* — "Runway for the course. Collision tree, checkpoints, sectors, grid, etc."** |

Glossary page **"Basics"** — https://nenkai.github.io/gt-modding-hub/concepts/courses/basics/ (footer July 14, 2025 / April 17, 2023):
> "Course Pack: The course pack contains most of the components that makes up a track. The runway, main model set, model set for the rear-view, camera component, vision list and more."
> "Runway: Runways are the collision component of the course. It contains the checkpoints, boundaries, collision tree, starting grid points and more."
> "VCoord: Represents one meter progression on the track."
> "Vision List: Vision lists (also known as VLS) determine what models should rendered on a precise point on the track."

Block magics (PDTools classes, https://github.com/Nenkai/PDTools/blob/master/PDTools.Files/README.md):
- `RNW4` / `4WNR` — Runway, **"within GT4 course/track files"** (i.e. an interior block, not a separate file). Includes a `CourseV` "meter distance calculator", reversed.
- `Vls0` — VisionList: *"Handles regions to display depending on current position, based on voronoi."*
- `GTCM` — MinimapSet. `CRSS` — CourseSound. `GTFX` — DCourseEffect (flares). `REP4` — GT4ReplayData.

PS2 folder layout, verbatim from https://nenkai.github.io/gt-modding-hub/ps2/gt4/file_structure/ :
```
│   ├─ crs/                     # Course files
│   ├─ <course_label>/          # (3)!
│   ├─ <course_label>.ad        # Autodrive file, for AI
```
Footnote (3): *"From the COURSE table"* — i.e. the label is bound to a `COURSE` SpecDB row.

SpecDB side — https://nenkai.github.io/gt-modding-hub/concepts/specdb/ :
> `COURSE` — "Defines all courses in the game. In GT5, `courselist.xml`, the arcade and freerun xmls refer to it using their labels."
> `COURSE_NAME_*` — "Defines the course names for `COURSE`. (for GT4)"

---

## 3 THE `.ad` FILE  ← strongest new finding

**The `.ad` is the AI "Autodrive"/enemy-line file, and it is a separate file next to the course container — not part of it.**

- Modding hub file structure, verbatim comment: `crs/<course_label>.ad  # Autodrive file, for AI` — https://nenkai.github.io/gt-modding-hub/ps2/gt4/file_structure/
- PDTools README, verbatim: `Autodrive -> .ad files, most GTs.` — https://github.com/Nenkai/PDTools/blob/master/PDTools.Files/README.md

Reverse-engineered structure, from source (PDTools `PDTools.Files/Courses/Autodrive/`, files carry "Copyright (c) 2026 Nenkai", https://github.com/Nenkai/PDTools/tree/master/PDTools.Files/Courses/Autodrive):
- `AutoDriveFile.cs` — header: `uint32 relocPtr` (*"If not 0, memory file most likely"*), `int32 enemyLineHeaderOffset`, then 4 unknown u32 offsets; aligns to `0x80` (`HeaderAlignment`); debug string says *"ADLN expected at 0x40"*.
- `EnemyLine.cs` — **magic `"ADLN"`**; fields: relocPtr, one skipped int32, `adlnFullSize`, `unkCount`, `autoDriveInfoCount`, `autoDriveOffset`; then an int32 offset list → `AutoDriveInfo` records.
- `AutoDriveInfo.cs` — `attackInfoCount` followed by 0x40-aligned `AttackInfo` records.
- `AttackInfo.cs` (1373 bytes, ~heavily attributed at 2026).

So: per-course AI driving line + overtake/"attack" data lives in `/crs/<label>.ad`, `ADLN` container. **No community documentation of `.ad` beyond these class names was found; no writer, no published field-level spec page.** This is the single most useful new fact for us: an imported course needs a matching `.ad`, and the AI-quality complaints in §4 are `.ad`-side, not runway-side.

---

## 4 COLLISION + AI LINE + GRIDS

**What the format says the pieces are** (all inside the `/crs/<label>` container unless noted):
- Collision + checkpoints + sectors + grid → the **Runway** block at header `0xDC` (`RNW4`). Hub: *"Collision tree, checkpoints, sectors, grid, etc."*
- PDTools Runway classes are the concrete field list (https://api.github.com/repos/Nenkai/PDTools/contents/PDTools.Files/Courses/Runway): `RunwayBoundaryVert`, `RunwayCheckpoint` (Left/Middle/Right Vec3 + `TrackV`), `RunwaySector`, `RunwayStartingGridPosition` (**a single `Vec3R Position`** — no yaw column visible in the class), `RunwayRoadVert`, `RunwayRoadTri`, `RunwayRoadVertMap`, `RunwayRayCastData`, `RunwayRayCastNode` (7 KB — the raycast/KD tree), `RunwayLightSet`/`CarLightSet`/`LightVFX`, `RunwayGadgetOld`.
- AI racing line → the **`.ad`** file (`ADLN` / EnemyLine / attack infos), §3.

**What the community actually does** — GTPlanet "Gran Turismo 4 Modding Discussion" thread 413859 (796 comments; started by TheAdmiester, Nov 9 2022). Verbatim:
- TheAdmiester, **Mar 28, 2023** (page 2), on TT layout ports: *"Currently they only use staggered starts with a sizeable gap, and the AI aren't particularly good at them because their paths are designed for bikes, but everything does work so they're at least worthy of inclusion in arcade."* → the AI line is data they inherit, and its quality is a known defect, not a solved problem.
- dylansan: *"the tracks from TT don't have any pitlanes, so the AI breaks on those tracks when their tires/fuel run out"* → TheAdmiester: *"It is as it would be an actual 3D model issue, which we can't edit yet."* → **geometry/collision editing is explicitly the blocker, in his words, as of Mar 2023.**
- No standing grid for imports → "staggered starts with a sizeable gap" (grid data is inherited from the donor).
- Apprentice asked about porting the GT4 Prologue course: TheAdmiester: *"Probably not, file formats changed a lot between games."* (page 2)
- GT3 tracks (Ask on page 19, #545, Oct 16 2024) — TheAdmiester #547: *"1 and 2 aren't possible"* (1 = port GT3 tracks). **GT3→GT4 not done; TT→GT4 works because TT shares the GT4-era format** (hub: "Applies to: GT4, TT, GTHD").
URLs: https://www.gtplanet.net/forum/threads/gran-turismo-4-modding-discussion.413859/page-2 · /page-19

**Verdict on (c):** there is **no documented method** for authoring collision, grid or AI line for an imported/custom course. The community's method is to inherit all three from the donor course; the collision/pitlane issue is publicly acknowledged as unfixable-with-current-tools (Mar 2023) and the AI line is inherited and admitted to be poor (bike-derived). No source describes regenerating a Runway or an `.ad`.

---

## 5 ANYTHING ABOUT ADDING A COURSE

**Historical practice = replacement only.** GTPlanet thread 89457 (Valencia rumour, posts from ~2007/2008): *"you take the GT4 ISO and TT ISO and put them on your PC hard drive, remove the track data, textures.ext from TT and replace a track in GT4 with them, then write a new ISO"*; *"replacing Fuji by Valencia means that you copy the GT4 DVD onto your PC, slice the huge .VOL file ... and replace that part with the data for Valencia"*. URL: https://www.gtplanet.net/forum/threads/valencia-is-not-in-gt4-greatest-hits-is-it.89457/ — pre-GT4FS, disc-level surgery.

**The one documented case of a genuinely standalone added course: Circuit Valencia in Spec II.**
- TheAdmiester, GTPlanet 413859 (page 1, ~Mar 27 2023), verbatim: **"Valencia has been hacked in as a replacement before, but now it's time to do it properly:"** (followed by a SpecDB/COURSE screenshot). URL: https://www.gtplanet.net/forum/threads/gran-turismo-4-modding-discussion.413859/
- Spec II feature list: *"Added Circuit Valencia from Tourist Trophy to Arcade Mode and Event Synthesizer"* — https://www.theadmiester.co.uk/specii/
- Spec II changelog (v1.09, 2025-03-14): *"Removed Valencia from Track Randomizer pool as its rolling start made shorter races difficult (Valencia is still available in Event Synthesizer's pool)"* — https://www.theadmiester.co.uk/specii/changelog2.html → it is a **rolling-start** course (no standing grid; consistent with no pitlane), and it exists as its own entry in track pools.
- Community reaction: r/granturismo, Sep 2024, spec-II on real PS2 "Valencia Gameplay (New Track)" — https://www.reddit.com/r/granturismo/comments/1fn6sqm/

**What is NOT documented anywhere I could read:** whether Spec II's Valencia occupies a **new** `COURSE` row/id or re-uses an existing slot's file folder. The wording ("do it properly" vs "replacement") plus its own arcade/Event-Synthesizer entry implies a standalone course entry, but I found no post stating the id. **UNVERIFIED** — see §7.

**No case of a hand-built / non-PD course being added** was found — every added course is a port of a PD-authored TT course.

**Useful analogy from the same community (cars, Jan 2026):** *"It is not currently possible to add new cars into GT4 that weren't already created by PD and in the right file format to enable inclusion."* — https://www.gtplanet.net/forum/threads/gran-turismo-4-spec-ii-added-ford-gt90-vector-m12-vector-m12-lm-race-car.435854 (post #3, Jan 19 2026). Same shape as the track story: PD-authored assets in the right format can be re-registered; new content cannot be authored.

---

## 6 CONTRADICTIONS WITH OUR MEASUREMENTS

Measured by us: one course = TWO files in `/crs/` — `/crs/<label>` and `/crs/<label>.ad`; retail v2.00 = 110 courses, ids 2..176, 22-byte COURSE row; Spec II = 114.

| Our measurement | Community evidence | Verdict |
|---|---|---|
| Two entries per course: `<label>` and `<label>.ad` | Hub lists exactly `crs/<course_label>/` and `crs/<course_label>.ad` — **and marks the first as a FOLDER (trailing `/`)**, with footnote "From the COURSE table" | **Confirmed**, with the nuance that the non-`.ad` entry is presented as a **directory**, not a single file. If our container reading says one file, we differ on that detail. |
| `.ad` = per-course companion | "Autodrive file, for AI"; magic `ADLN` | **Confirmed** — the hub answers our open question about what `.ad` is. |
| Geometry block-tagged `MDLS` (world model set) | Hub calls the `0x04` component "Main Model Set" / `ModelSet2`; the string `MDLS` is **not** printed on the hub page | No contradiction. Different naming, not evidence against our tag. Mark our `MDLS` reading as ours. |
| `RNW4` = runway with road verts/tris | PDTools README: runoff magic `RNW4`/`4WNR`, "within GT4 course/track files", and the hub puts Runway at a header pointer (`0xDC`) | **Confirmed**, and clarified: RNW4 is an **interior block of the course container**, not a third file. |
| `Vls0` = vision/culling | Hub: "Vision lists... determine what models should rendered", PDTools: `Vls0`, voronoi-based region display | **Confirmed.** |
| 110 courses / ids 2..176 / 22-byte row | No source found that states course counts or row size. Hub only says `COURSE` "defines all courses" and that the crs label comes from it | **Neither confirmed nor contradicted** — external silence, not disagreement. |
| Spec II = 114 courses | Spec II adds Valencia at minimum; changelog also mentions randomizer pools and arcade entries | Consistent, but no count published. **UNVERIFIED.** |

Also note: hub states the course file **should not exceed 12 MB** ("already 1/3 of the PS2's ram"), and that zeroing offsets disables components — i.e. a component (even a vision list) can be dropped. Relevant to any "add" plan.

---

## 7 UNVERIFIED

1. **GT4 support in ddm999's GTTrackViewer/GTTrackEditor** — listed by the hub only under PS3 tools; repo README is not retrievable on branch `canon` (404); description "very unfinished". Assume PS3-only until shown otherwise.
2. **Whether Spec II's Valencia is a new `COURSE` row/id or a reused slot.** The phrase "now it's time to do it properly" strongly implies standalone, but no source gives the id. Our §5 claim stops at "it is selectable as its own course".
3. **`.ad` field-level semantics beyond class names.** `AttackInfo` is unexplained; the two skipped/unknown ints in `EnemyLine` are unexplained. No doc page exists.
4. **Whether `.ad` is mandatory for a course to load.** Not stated anywhere I read.
5. **Save-as/ToStream completeness in PDTools** for a round-trippable Runway — README says the KDTree "needs to be reversed", so a written Runway probably will not load yet.
6. **Whether the 12 MB course-size figure is a hard limit or advisory** — hub wording is "should not exceed".
7. **Retail v2.00 = 110 courses, ids 2..176, 22-byte rows** — no external corroboration found in this pass.
8. **The GT3→GT4 door.** TheAdmiester says not possible; the hub does document GT3 formats separately. Untested by us; treat as community claim.
9. **Any case of a user-authored (non-PD) course being added** — none found. Absence of evidence, not evidence of impossibility.
