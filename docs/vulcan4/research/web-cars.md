# GT4 modding community — CAR-side knowledge

Research date: 2026-10-15. Scope: what tools exist for GT4 *car* assets, what they can and cannot do,
what the car model container format is documented to be, how paint/texture variants work, and any
evidence that anyone has ever **added** a car slot rather than replaced an existing car's files.

Platform labels are per source. GT4 = PS2. Nothing here conflates GT4 with GT5/GT6/GTPSP unless stated.

---

## 1. TOOLS INVENTORY

The GT4 car-modding stack is small and almost entirely one author's (Nenkai's) work, plus one
volume tool. Everything is **replace / overwrite**, not add.

| Tool | URL | What it does | Replace or Add? | Evidence |
|---|---|---|---|---|
| **GT4FS** (v3.4.0; xfileFIN, q-k, Echelo, Nenkai; repo owner Razer2015) | https://github.com/Razer2015/GT4FS | Extract and repack `GT4.VOL` (also TT, TT Demo, GTHD, GT4 MX-5 demo, GT4 First Preview). `extract -r`, `pack -r <folder> -g <game>`. | **Replace only.** Repacks an extracted tree back into a VOL; `pack-append` overwrites files at *preserved original paths*. No mechanism to mint a new asset path. | README (last updated 2026-09-07, per repo page) |
| **GTSpecDB** (Nenkai, xFileFIN, q-k) | https://github.com/Nenkai/GTSpecDB | Loads/saves GT4→GT5 SpecDB folders; edit car specs, course info, race events. GUI editor + SQLite converter. Saves custom Huffman-compressed tables "required for GT4" (since 2023-05-17). | **Can add ROWS** to tables — but see §4: a row with no model file behind it is not a car. | README; https://nenkai.github.io/gt-modding-hub/ps2/gt4/tools |
| **GTPS2ModelTool** (Nenkai) | https://github.com/Nenkai/GTPS2ModelTool | Dump GT4/GT3 models+textures to obj/png (works on `Tex1, GTTR, GTTW, GTM1, CAR4, MDLS, PRM0, PRZ0, GTBR, CourseData`). Can *create* custom models (`make-model-set`, `make-car-model`, `make-wheel`, `make-tire`, `make-tex-set`). | **Cannot add a slot.** GT4 model *creation* is explicitly not finished. | Repo README + release notes 1.1.0/1.1.1 (1.1.1 dated 2026-05-30) |
| **PDTools** (Nenkai) | https://github.com/Nenkai/PDTools | Library toolbox. `PDTools.Files/Models/PS2` = the GT3/GT4 model importer+exporter. `PDTools.SpecDB` = SpecDB reader/writer (GT4/5/PSP). `PDTools.GT4ElfBuilderTool` = decrypt/decompress `CORE.GT4` → real ELF. | Library, not a slot tool. This is the reference implementation VULCAN 4 cross-checks against. | README; ResHax, Nenkai 2024-04-19 |
| **GT4Hooks** (Nenkai) | https://github.com/Nenkai/GT4Hooks | Code hooks + HostFS support. The `GT4Hook.elf` plugin is injected into the rebuilt ELF with `ps2plugininjector.exe`. | **Executable patching.** This is the *only* route anyone has used toward changing game behaviour, not asset counts. | https://nenkai.github.io/gt-modding-hub/ps2/gt4/volume |
| **GT4SaveEditor** (Nenkai) | https://github.com/Nenkai/GT4SaveEditor | Save decrypt/encrypt, garage, hybridding (swap a car's parts). | Replace only. | README |
| **GT4 Retexture Mod v3.0.4** | https://www.gtplanet.net/forum/threads/gran-turismo-4-retexture-mod-v3-0-4.408852/ | AI-upscaled + hand-redone textures delivered through **PCSX2's texture dump/replace** feature (`replacements` folder, hash-named PNGs). Not a disc mod. | Runtime replace. Adds nothing to the disc. | Thread (date not captured) |
| **GT4 Mem Tools** (pez2k / AJB-Tech) | https://github.com/pez2k/Gran-Turismo-4-Tools | Memory-edits GT4 NTSC on PCSX2: AI grids, hybridding, camera/FOV. | Runtime replace. | README |
| **GTAdhocToolchain** (Nenkai) | https://github.com/Nenkai/GTAdhocToolchain | Disassemble GT4's Adhoc scripts, unpack `.gpb`. | Script-level; orthogonal to car slots. | hub tools page |
| **GT.RText** (xFileFIN, Nenkai) | https://github.com/Razer2015/GT.RText | Edit `.rtext`/`.rt2` localization. | Replace. | hub tools page |

**Spec II (TheAdmiester)** is not a tool — it is an **xDelta patch over the GT4 Online Public Beta disc**
(US/JP), per its own page and the hub's recommendation to mod that build instead of retail.

---

## 2. CAR FORMAT KNOWLEDGE

### 2.1 Model set (the mesh container) — PRIMARY

Nenkai, *Lifting the Bonnet on Gran Turismo's Model Format*, 2023-11-26:
https://nenkai.github.io/gt-modding-hub/blog/2023/11/26/lifting-bonnet-on-gt-models/

Six known revisions of "Model Set". **The magic string identifies the revision:**

| Revision | Magic | Games | Notes |
|---|---|---|---|
| ModelSet0 | `GTM0` | GT2000 | initial |
| ModelSet1 | `GTM1` | **GT3**, GTC (and GT4P) | structure changes |
| **ModelSet2** | **`MDLS`** | **GT4, Tourist Trophy** | **VM, model color patching, bind matrices (bones)** |
| ModelSet3 | `MDL3` / `3LDM` | GT (PSP), GT5, GT6 | flexible vertices, packed meshes (`PMSH`), streamed data, bundled shaders |

Key structural claims (same blog post, plus the 010 template):

- No parsing at runtime — the file is **re-mapped** into memory (offsets rewritten to addresses).
- Every revision header = **a table of counts + a table of offsets** pointing at four components:
  **Models, Shapes (meshes), Texture Sets, Materials**.
- **Shapes** map close to PS2 hardware: vertex buffers are stored as **VIF commands** (mostly `UNPACK`),
  holding packed vertex data + `GIFtag`s, rendering as **triangle strips**. No triangle indices — the
  vertex-per-strip count is carried inside a VIF code component (Nenkai, ResHax 2024-04-21).
- **Models** are groups of shapes that also carry a **command list**. The engine *interprets* this list;
  the commands map onto **PDI-GL** (Polyphony's OpenGL-alike). `CallShape` sends a shape for rendering;
  preceding commands set state (alpha test, blend, depth mask, matrices…).
- **Callback commands**: parameterised requests into the game — e.g. parameter `0` = tail lamps on/off,
  selecting shape 14 or 15; parameters 1–4 drive the tweening of wipers/antennas/mudflaps.
- **Virtual machine (new in MDLS/ModelSet2, i.e. GT4):** the header gains *Instance Flags*, *Instance
  Size*, *Host Method Infos*, *Out Registers*, **VM Bytecode blob**, *Instance Offset*. Each model has
  three VM entrypoints: **Constructor, Update, Render Function** (the latter invoked by a `CallVM`
  render command). The worked example is the GT-R Pace Car's rotating strobes — the VM returns
  `builtin_br0` (branch) and `builtin_tw0` (tween) from a `time() % 1.0f` computation.

**GT4 vs GT6, explicitly:** MDLS/ModelSet2 (GT4) → MDL3/ModelSet3 (GTPSP/GT5/GT6) is called a
"direct upgrade". MDL3 *keeps* the bytecode model-setup + VM design and adds flexible vertex
definitions, bundled hardware shaders, packed meshes. So GT4's container is the **first VM-bearing**
revision; GT6's is the same idea plus a modernised vertex/mesh layout.
Spec page: https://nenkai.github.io/gt-modding-hub/formats/models/mdl3_modelset3

### 2.2 GT4-specific container / file layout — PRIMARY

- Binary template: **`GT4_MDLS_ModelSet.bt`** —
  https://github.com/Nenkai/GT-File-Specifications-Documentation/blob/master/Formats/PS2/GT4/GT4_MDLS_ModelSet.bt
  (repo dir: `Formats/PS2/GT4/`, last touched 2026-09-21 on the `GT4_INST_SShd_Jam_Custom.bt` commit).
- `CAR4` = **CarModel GT4** (GT4, TT) — the per-car container that bundles model set + tire + wheel +
  car-info (Nenkai, GTPS2ModelTool 1.1.0 release notes).
- **Folder structure** (https://nenkai.github.io/gt-modding-hub/ps2/gt4/file_structure/):
  `car/lod/<model_code>` (race), `car/menu/<model_code>` (menu), `car/open/<model_code>` (open
  interior), each with a sibling `<model_code>.pat` = **Color Patch**; also `wheel/{lod,menu}`,
  `tire/{lod,menu}`, `wing/{lod,menu}`.
  Model file names come from **`CAR_VARIATION` → `VARIATION`** in the SpecDB.
- `specdb/<name>/CARS/<TABLE_ID/ROW_ID>` — one file per car, e.g. `00000000/00000436` = GENERIC_CAR row 0x436.
  So the car→parts link is a **file path built from the table id and row id**.

### 2.3 Car info / camera / collision — PRIMARY

A "car info" file (`.json` when dumped) holds onboard camera positions, front/night brake-light data,
collision particle positions, tire compound indices, brake textures, exhaust backfire data.
Collision box for GT3 is computed from `RACINGMODIFY` (`ModelFront/ModelRear/ModelWidth/ModelHeight`);
GT4 keeps `RACINGMODIFY` (aero/chassis data).

---

## 3. TEXTURES / PAINT

### 3.1 Colour = a *patch file*, not a palette set — PRIMARY/SECONDARY

- File structure (primary): `car/lod/<model_code>.pat` — "Color Patch".
- SpecDB `VARIATION` table GT4 columns (primary, https://nenkai.github.io/gt-modding-hub/concepts/specdb/):
  `Label`, **`ModelCode`**, `VarOrder`, **`ColorPatchFileName`** (ends `.pat`, resolved against `car/`),
  `Name`, `ModelWidth/Height/Front/Rear/Projection`, `ColorChip(0..4)`.
  GT4 reaches a VARIATION row via **`CAR_VARIATION`** (GT4/TT only).
- pez2k, 2024 (GTPlanet, secondary but from a project contributor): GT4 does **not** ship separate
  palettes per colour; it ships **one base set plus byte-level patch instructions** that generate the
  others. TheAdmiester confirms: the colour data is a patch file that "tells the game how to edit the
  base model… changing the palette pixels applied to various parts of the texture", and notes a colour
  could in principle *disable* a separate modelled shape (licence plate, wing).
  https://www.gtplanet.net/forum/threads/gran-turismo-4-modding-discussion.413859/page-13

### 3.2 Texture set format — PRIMARY

- `Tex1` = **TextureSet1**, shared by GT3P, **GT4**, GT4P, TT. Nenkai (ResHax, 2025-08-29): a Tex1 can
  house **multiple textures all swizzled together in one huge transfer for GS memory** — which is why
  naive tools (imageheat) fail on them.
- Nenkai (GTPS2ModelTool issues): a TextureSet is "a **thin wrapper over the GS registers**"; PDI packs
  textures into the obtuse GS memory layout "such that no block goes unused, or even better, parts of
  the memory get reused for other textures or palettes". His own builder is still **~1.5× above** PDI's
  average GS block consumption — the stated reason custom cars can only be basic.
- **Variants rule (from release notes):** when creating colour variations the textures must be
  **identical except for colour**, and must be pre-quantised to the palette size (16 colours for
  `PSMT4`, 256 for `PSMT8`) — because the game "simply changes the colour palette (and materials)".
- Formats available when building: `SCE_GS_PSMT4` (4bpp, 16-colour, **default**), `PSMT8` (8bpp,
  256-colour), `PSMT32`. Power-of-two dimensions; reuse textures aggressively; 1×1 texture to spread a colour.

### 3.3 Repaint at runtime / off-disc — SECONDARY

- Spec II adds "**Cars can now be repainted at GT Auto**" and "Added new colour options to various cars"
  (author's own feature list, https://www.theadmiester.co.uk/specii/).
- The Retexture Mod replaces textures via PCSX2 hash-named PNGs — an emulator feature, not a disc edit.
- Modders note two ways to get variant textures in a custom ISO: bind a model to the car, or dump+replace
  by hash (GTPlanet Retexture thread).

---

## 4. ANYTHING ABOUT ADDING

**This is the load-bearing section. The community's answer is unambiguous: no one has added a car slot.**

Primary/author statements:

1. **Dave A** (GTMods, 2026-01-19), on a request to add the Ford GT90 / Vector M12 to Spec II:
   > "It is not currently possible to add new cars into GT4 that weren't already created by PD and in
   > the right file format to enable inclusion. In other words, this isn't possible at present."
   https://www.gtplanet.net/forum/threads/gran-turismo-4-spec-ii-added-ford-gt90-vector-m12-vector-m12-lm-race-car.435854/
   Same thread: *"adding new cars outside of a select few already modelled and compressed to the correct
   format by Polyphony that weren't included in GT4 at launch isn't possible at this point in time."*
2. **TheAdmiester** (Spec II author), GTPlanet Modding Discussion, ~2024-06:
   > "adding new manufacturers to the dealers is very easy… but I saw no point in doing it for anything
   > more since **we can't make new cars yet**."
   Asked "would it be possible to add new countries after the secret to adding new cars is found?", he
   answers only "Just as easy as adding another manufacturer." — i.e. **the "secret to adding new cars"
   is treated as unfound**, by the person with the most GT4 car content in existence.
   https://www.gtplanet.net/forum/threads/gran-turismo-4-modding-discussion.413859/page-13
3. **Nenkai**, GTPS2ModelTool README (primary, current):
   - "A tool that allows creating custom models for **Gran Turismo 3**… Other games like **GT4** are not
     yet supported (please contribute!)". Open task list: *"GT4 support + VM/Compiler?"*
   - "**THIS IS NOT USABLE FOR REBUILDING CARS.**" Rebuilding from dumped originals fails because
     repacked textures overflow GS memory; "Building custom models is possible. Building models back
     from original models are not."
   - Open tasks include *"Attempt to better recover render commands for each model while dumping. This
     is needed for rebuilding cars correctly"* — the command-list recovery is acknowledged incomplete.
4. **Hub note** (https://nenkai.github.io/gt-modding-hub/ps2/models/): "It is **not yet possible to
   create cars with more than one color.**"
5. **Cross-generation precedent:** GT4 is *not* compatible with GT3-era content. rikiflsh_99 and
   TheAdmiester (GTPlanet, 2024-06-28): GT4 Prologue / GT Concept / Prius demos share GT3's engine, so
   cars can be *ported into GT3*; "GT4 uses a different engine so, for now at least, porting cars is not
   possible" and GT4 is "only compatible across its own builds".

**What Spec II actually did (this is the community's "adding").** Its own feature list claims cars
*added*: Mini Cooper 1.3i '98, Mazda MX-5 '05 (from the MX-5 demo build), Korea-exclusive Hyundai Click
Type-R '04 / Tiburon Turbulence '99 / Tuscani CCS '03, Korea-exclusive Lexus IS200 GT-1 Race Car '04,
no-stripe FPV GT '04, restored Vauxhall/Toyota Altezza regional equivalents, restored original Esso
Exxon variants. Every one of these is a **PD-authored asset recovered from another GT4-family build's
VOL and re-pointed at by a new SpecDB row** — not a new mesh. That is exactly the "select few already
modelled and compressed to the correct format by Polyphony" caveat above.

**SpecDB row-level adding is real.** GTSpecDB can save tables (with GT4's required Huffman compression,
added 2023-05-17) and the hub documents the per-car file `specdb/<name>/CARS/<TABLE_ID>/<ROW_ID>`,
i.e. rows are addressable by id. The hub warns multiple-table editing *may currently break the game*.
So the wall is not the table — it is that a row needs a **model file, textures, a VARIATION row with a
`.pat`, and a `CAR_VARIATION` link**, and the only sources for those are files PD already shipped.

**Executable patching in practice.** The modders' executable route is: `PDTools.GT4ElfBuilderTool` to
turn `CORE.GT4` into `SCUS_974.36.elf`, then `ps2plugininjector` to inject `GT4Hook.elf` (GT4Hooks).
TheAdmiester used exactly this for camera work: "the additional cameras and the GT3-style chase cam
were implemented using **actual C code plugin hooks to the game's executable**." No one has published a
hook that raises the car count.

---

## 5. CONTRADICTIONS WITH OUR MEASUREMENTS

Our measurements: retail GT4 v2.00 has **722 rows in GENERIC_CAR, ids 21..1077**; file
`/specdb/GT4_US2560/GENERIC_CAR.dbt`; meshes at `/car/{menu,lod,open}/<name>`; wheels `/wheel/…`;
wings `/wing/…`; whole disc inside `GT4.VOL`. Spec II carries **744 car rows and 114 courses**.

- **No contradiction found.** Nothing in the community sources contradicts any of the above. The
  folder paths, the `.dbt` database, the SpecDB-per-car `CARS/<table_id>/<row_id>` layout and the
  `GT4.VOL` container are all corroborated independently.
- **New precision the task brief lacked, worth adopting:** the mesh container magic is **`MDLS`** (not
  "a 256-opcode program"); the per-car bundle is **`CAR4`**; texture set is **`Tex1`**; wheel/tire old
  magics `GTTW`/`GTTR` belong to GT3-era files. Model file names come from **`CAR_VARIATION` →
  `VARIATION.ModelCode`**, and the colour is the sibling **`.pat`** named by
  **`VARIATION.ColorPatchFileName`**.
- **Spec II's 744 vs our retail 722 is consistent, not contradictory:** Spec II is based on the **GT4
  Online Public Beta** (2006-06-06 build), a *different* disc from retail v2.00, and the hub explicitly
  recommends modding the Online build because files can be edited without rebuilding a volume. So the
  two row counts are over two different GENERIC_CAR tables. **Action for us:** re-measure the row count
  on the Online Public Beta disc before quoting 744 as "Spec II grew retail's table".
- **Our "722 fixed-size rows" phrasing is consistent with the community's wall**, but the *stated
  reason differs*: the community's blocker is the **asset side** (no tool can author GT4 meshes /
  GS-legal texture sets, and dumped originals can't be rebuilt), not obviously a hard table limit.
  GTSpecDB demonstrably writes tables. Treat "tables are fixed-size" as **UNCONFIRMED by community
  sources** — it is our hypothesis.

---

## 6. UNVERIFIED

- **The 256-opcode claim.** The hub's public "Model Render Commands" page lists only ~25 named commands
  (AlphaFunction, AlphaFail, BlendFunction, ColorMask, Disable/Enable*, FogColor, DepthBias,
  Push/PopMatrix, MatrixMode, Rotate, Translate, Scale, two `UnkGT3_*`). No public source states the
  total opcode count is 256. Our engine-side reading is not corroborated here — **UNVERIFIED externally**.
- **`MODEL_INFO` platform.** Two Nenkai sources disagree: the hub SpecDB page marks `MODEL_INFO` as
  "(for >=GT5)", while `Gran-Turismo-Modding-Guides/5. SpecDB/SpecDB_Editing.md` says "GT4/5/PSP uses
  their own formats… MODEL_INFO | Defines all the available models for each car. Row ID's are directly
  linked to the car's row in GENERIC_CAR." **Flagged, unresolved.**
- **GT4 model file size limits.** The hub gives GT3 limits (688,128 B race / 819,200 B menu). No GT4
  equivalent is published. **UNVERIFIED for GT4.**
- **`CAR4` internal field layout.** Named and dumped by GTPS2ModelTool, but no public field table was
  found; the 010 template found is `GT4_MDLS_ModelSet.bt`. **UNVERIFIED.**
- **Exact `MDLS` header field table** — the standalone spec page (`formats/models/mdls_modelset2`) is a
  **404 / not crawled** as of 2026-10-15; only the blog description and the 010 template exist.
- **Colour-patch (`.pat`) internal format** — documented as *what it does*, never as a byte layout.
  **UNVERIFIED.**
- **Spec II release date** — the feature page carries no date; the mod is widely reported as released
  in 2023. **UNVERIFIED.**
- **Any published executable patch raising the GT4 car count.** None found. The only documented hooks
  are cameras/chase-cam (GT4Hooks + PS2 Plugin Injector). **UNVERIFIED / apparently absent.**
- **GT4 "car replacer" tools** — searches surfaced none beyond GT4FS (path-preserving overwrite) and
  the PCSX2 runtime texture replacer. Absence of evidence, not proof.

## 7. SOURCE INDEX (primary / secondary)

PRIMARY: Nenkai GT Modding Hub (models, tools, file structure, specdb, model render commands, blog);
PDTools, GTPS2ModelTool, GT4FS, GTSpecDB, GT4SaveEditor, GT4Hooks repos; GT-File-Specifications-
Documentation (`Formats/PS2/GT4/`); theadmiester.co.uk/specii feature list.
SECONDARY: GTPlanet Modding Discussion thread; GTPlanet Spec II car-request thread; ResHax threads
(car models 2024-04, binary model files 2025-08, Tex1 2025-08); GTPlanet Retexture Mod thread.
