# Web research: how GT4 content mods ship, patch the game, and what blocks them

Scope: GT4 (PS2) only. No GT5/GT6 conflation except where a source is explicitly cross-title.
Retrieved: 2026-10-15 UTC. All quotes verbatim from the linked primary/community source.
Our measurements referenced here: model file = command program (256-opcode interpreter); Spec II car table 722 -> 744 rows, courses 110 -> 114.

---

## 1 SPEC II AS SHIPPED

### What it is and what it requires of the user

Primary: <https://theadmiester.co.uk/specii> and <https://www.theadmiester.co.uk/specii/install.html>

> "Spec II is a mod which expands upon the core Gran Turismo 4 experience, bringing a variety of fixes and improvements to the game."
> "Spec II is based on the NTSC version of Gran Turismo 4 Online Public Beta and is distributed as an xDelta patch requiring this version."

So: **the base disc is NTSC GT4 Online Public Beta (SCUS-97436), NOT retail GT4 (SCUS-97328).** Shipping format is a binary xDelta patch that the user applies to their own ISO. Never a distributed ISO.

Installation (verbatim, <https://www.theadmiester.co.uk/specii/install.html>):

> "To get started, ensure you have an NTSC copy of Gran Turismo 4 Online Public Beta. Use a file hashing tool to verify that the MD5 hash is 3306538778dda2ded87ceaf52c944a98. Other versions (such as the DNAS Bypass build commonly used to play online) are considered pre-modded and will not patch correctly if used."
> "Using either xdelta or xdelta UI, select your unmodified disc image from Step 1 as the input file, the .xdelta file as the patch, and select a name/path (e.g. Gran Turismo 4 Spec II.iso) for the output disc image."
> "NOTE: Some versions of xdelta may not preserve the .iso extension, meaning your patched disc image may not be recognised by your PS2/PCSX2 setup. Ensure that the output file has a .iso extension before continuing."

Mirror restating the same readme: <https://gt-mod.site/specii.html>. Community install guides confirm the workflow and note the output ISO **grows** (input 2.47 GB -> output ~3.4 GB in one walkthrough: <https://www.youtube.com/watch?v=g7uSAVGmQ2M>).

Requirement summary: Redump-verified Online Public Beta image (exact MD5), a backup, xdelta, and enough disk for a ~3.4 GB output ISO.

### Does it document patching the game's executable?

Yes, admitted in the FAQ, but **not documented in any technical detail**. From the Spec II FAQ (<https://www.theadmiester.co.uk/specii/faq.html>):

> "Q: Why don't my emulator cheats work anymore?
> A: Spec II makes comprehensive changes to the game's disc image and executable, which has resulted in it having a different CRC hash when played in an emulator. Simply rename your cheat file(s) so that they start with the new CRC hash for Spec II: 4CE521F2"

And on the changelog (<https://www.theadmiester.co.uk/specii/changelog.html>, v1.09, 2025-03-14):

> "NEW CRC HASH - please rename any PCSX2 cheats, changing 73FEFB84 to 646B2E29"

Three observed CRCs across versions (4CE521F2, 73FEFB84, 646B2E29) prove the executable is re-patched every release. **There is no public patch manifest, no list of patched addresses, and no description of what in the executable is changed.** That remains closed.

Networking is deliberately removed:

> "Networked functionality has been disabled in Spec II as there is no reliable way to ensure that users attempting to play together are on identical versions of the game." (FAQ, as above)

### Does it document adding cars/tracks?

Yes — as **feature claims**, not as a technical method. From <https://theadmiester.co.uk/specii>:

> "Added Circuit Valencia from Tourist Trophy to Arcade Mode and Event Synthesizer"
> "Added the Mini Cooper 1.3i '98"
> "Added the Mazda MX-5 '05 from the MX-5 demo build"
> "Added Korea-exclusive Hyundais: Click Type-R '04, Tiburon Turbulence '99, and Tuscani CCS '03"
> "Added Korea-exclusive Lexus IS200 GT-1 Rac[e Car]"
> "Added new Driving Missions 35-40"
> "Added 15+ new events to various event halls"
> "Made all cars and tracks available in arcade mode by default, including Citta di Aria in race modes"

Every added car named is **a Polyphony-made asset that already existed in another PD build** (MX-5 demo, Korea-region release). Every added course is a port from another PD title (Tourist Trophy). "Made all cars and tracks available" is data/flag enabling, not new content.

The FAQ also publishes the mod's own source:

> "Yes! You can find the adhoc source code for Spec II [here](https://github.com/TheAdmiester/OpenAdhoc-GT4SpecII), and you can use the [GT Modding Hub](https://nenkai.github.io/gt-modding-hub/ps2/) to get started with modding."

i.e. the **game logic lives in Adhoc scripts (compilable source), and the mod is distributed partly as script source**. That is how "15+ new events" and "Driving Missions 35-40" were added without the community describing executable surgery for them.

### Versioning / current state

Current advertised build is **v1.10 Full** with incremental `.xdelta` updates per version, and per-version MD5s for the output ISO. Every version bump can change the executable CRC (cheat files must be renamed). Saves carry over between updates unless stated otherwise:

> "New saves are not required for updates unless otherwise stated." (<https://www.theadmiester.co.uk/specii/changelog.html>)

---

## 2 THE EXECUTABLE

### Public documentation exists, and it is substantial — but it is reverse-engineering docs, not a spec of the shipped binary

Primary: **Gran Turismo Modding Hub** — <https://nenkai.github.io/gt-modding-hub/ps2/executables/>

> "Starting from GT3, the game is split between two executables:
> - `SCES_517.19` (or any other game code) - Bootstrap executable
> - `CORE.GT3/CORE.GT4/CORE.TT` - Main executable."

Bootstrap resolution order and checks (verbatim from the same page):

> "1. `HostSource` - `HOST` - Using `host`, `host:/tmp/CORE.GT4`
> 2. `CardSource` - `MCARD 0` - Using memory card 1
> 3. `CardSource` - `MCARD 1` - Using memory card 2
> 4. `DiskSource` - `DISK` - Using `CORE.GT4` at the root of the disk"

Boot validation flow (as published, condensed from their flowchart):

> "Attempt to open CORE from various sources -> CRC Matches? -> Is GT4O? (Decrypt CORE) -> Decompress CORE -> Read header -> Computed hash matches with header? -> Defined segments overlap? -> Is valid entrypoint address? -> Ready to go, pass arguments and entry to ExecPS2"

So the executable has, at minimum: **a CRC check on the CORE blob, an optional decrypt step for the GT4 Online build, LZ-style decompression, a header with its own hash, a segment-overlap check, and an entrypoint validation.** That is the concrete "structure/checksum" answer the community has published.

Also: "Some older GT4-engine builds such as GT4 First Preview or Tourist Trophy Demo does not use a bootstrap executable."

Tooling: "PDTools.GT4ElfBuilderTool can process a CORE file to an ELF file for easier use in IDA or Ghidra." (<https://nenkai.github.io/gt-modding-hub/ps2/executables/>)

Independent reimplementation of the same conversion: `tools/core2elf.py` in <https://github.com/gustavocanepa/gt4-decomp> (`orig/README.md`), which publishes expected hashes for the retail US 1.01 build:

| File | SHA-1 (SCUS-97328 v1.01) |
|---|---|
| `SCUS_973.28` (bootstrap) | `1df6f08c348e55f66c3a38e1774a9a989ce7d6ba` |
| `CORE.GT4` (as on disc) | `30031137ce494323816bb707b90d3ec0d07937d2` |
| `CORE.GT4.elf` (from `core2elf.py`) | `eb6e17112038bbeab63b09d445112adcd957c900` |

Disc/image checksums and per-build metadata: <https://nenkai.github.io/gt-modding-hub/builds/gt4/>

- **Retail US, `SCUS-97328` (v1.01)**: Adhoc v5, "Supports up to instruction: `36 - VARIABLE_PUSH`", SpecDB `GT4_US2560`, BuildNumber 415, 4.94 GB, Dual-Layer, ISO CRC32 `D1D25931`.
- **Retail US `SCUS-97328` (v2.00, Oct 18 2006)**: BuildNumber 415, "identical to 1.01 except a few course files", ISO CRC32 `831F9F84`.
- **Online Public Beta, `SCUS-97436` (Jun 6 2006)**: Adhoc v7, "Supports up to instruction: `39 - SOURCE_FILE`", SpecDB `GT4_PREMIUM_US2560`, BuildNumber 76, ISO 2.47 GB, ISO CRC `E8BF4147`. (Note: the hub lists the Beta's ISO CRC as `E8BF4147`, whereas Spec II's readme requires its image MD5 `3306538778dda2ded87ceaf52c944a98` — different algorithms, not a conflict.)
- Other regional builds: `SCAJ-30007` (JP), `SCES-51719` (EU, BuildNumber 1010, ISO CRC32 `E1CA4D84`), `SCKA-30001` (KR, ISO CRC32 `02C25ED0`), `SCCS-60002` (CN). Full table on the builds page.

Note for us: the hub's build metadata is per-game-code + per-build-number, with **"Adhoc version" and "supports up to instruction N"** fields. That is a documented, structured description of the shipped executables — closer to what we need than anything else public.

### What the community knows about hardcoded limits

Two authoritative first-hand statements, both from GTPlanet's "Gran Turismo 4 Modding Discussion" thread (<https://www.gtplanet.net/forum/threads/gran-turismo-4-modding-discussion.413859/>).

TheAdmiester, page 23 (<https://www.gtplanet.net/forum/threads/gran-turismo-4-modding-discussion.413859/page-23>):

> "When I mention limits it's not in the sense of DVD space as I have almost 1GB left to play with, it's referring to the amount of memory the game can allocate which can't feasibly be changed."

pez2k, same page, explaining the mechanism:

> "As far as I'm aware, it's all fixed static memory allocations, generally with everything allocated contiguously to avoid wasting space. That means that every reference to a particular piece of data is a hardcoded memory address in the executable which would need to be manually patched... plus anywhere it's referred to via an offset from something else, or is in an array that gets looped over."
> "A lot of the game data is also stored in a custom database ('specDB') which takes up a big chunk of memory by itself. It's too big to relocate, so expanding its allocation would mean moving everything else out of the way. Nothing is completely impossible, but it's the sort of level of work where it'd be 'easier' to fully decompile the whole game so that a compiler could sort it all out."

That is the community's own model of GT4's hardcoded limits: **static, contiguous, address-hardcoded allocations; no relocation slack; SpecDB is the big immovable block; raising a limit means patching every reference plus every offset-derived access plus every loop bound.** No public list of specific limit values (max cars, max courses, max events) was found — see §6.

Other documented hardcoded structures:

- **Maker IDs are hardcoded in GT4/TT** — the SpecDB `TUNER_LIST` page publishes the full maker-ID table (`acura 2` … `trickstar 92`) and states: "The list of makers is hardcoded in GT4/TT." (<https://nenkai.github.io/gt-modding-hub/concepts/specdb/>)
- **GT5** has a hardcoded horn limit of 280 — **this is GT5, noted only to mark the pattern; do not treat as GT4 data.** (same page, `GENERIC_ITEMS`: "GT5 has a hardcoded limit of 280 horns.")
- **Car model file size limits**: "For GT3, the max car model file size is `688.128` bytes (`672Kb`) for race models, `819.200` bytes (`800Kb`) for menu models." (<https://nenkai.github.io/gt-modding-hub/ps2/models/>) — GT3 numbers; the page gives no numeric GT4 equivalent, only "The main concern is model file size and textures will take most of it."
- **The game logic is scripts, not the executable.** OpenAdhoc README (<https://github.com/Nenkai/OpenAdhoc>): "Scripts operate nearly as the whole of game logic, while the executable mostly serves as the engine and exposes libraries to the script interface." This matters for us: event/menu/flow features are script-side, so a mod loader that swaps scripts reaches far more of the game than executable patching does.

### The model format (relevant to our 256-opcode finding)

Primary: Nenkai, "Lifting the Bonnet on Gran Turismo's Model Format" (<https://nenkai.github.io/gt-modding-hub/blog/2023/11/26/lifting-bonnet-on-gt-models/>):

> "| `MDLS` | ModelSet2 | GT4, TT | VM, Model color patching, Bind matrices (bones) |"

> "ModelSet2 introduced with GT4 went even further and added a virtual machine and scripting language for shape manipulation."

> "It's strobe ... is a float ... 5. VM Bytecode - This is the bytecode/commands for the VM, in one single blob of data for the entire model set."

> "Each model contain VM entrypoints - when to use the VM and where within bytecode they should execute. There are 3 of them: 1. Constructor ... 2. Update ... 3. Render Function - Called when the render command interpreter encounters a `CallVM` command."

> "When a game needs to render a model set's models, it will iterate through all of the models and interpret their list of commands which closely maps PDI-GL functions."

**This independently confirms the shape of our finding**: GT4's car model file (MDLS / ModelSet2) is a **command program interpreted by the game**, and it contains both a render-command list and a VM bytecode blob with (at least) three entrypoints. The community calls it "a virtual machine and scripting language"; we measured 256 opcodes. **No public GT4 opcode table with a count was found** — see §5/§6.

---

## 3 OTHER CONTENT MODS

| Mod / project | Base | Technique | Adds content or replaces? |
|---|---|---|---|
| **GT4 Spec II** (TheAdmiester) | NTSC GT4 Online Public Beta (SCUS-97436) | xDelta over user's own ISO; executable patched (CRC changes per version); Adhoc script source shipped; SpecDB + volume edits | **Adds** — new events, missions 35-40, Valencia course, ported PD car models. All added assets are PD-made, sourced from other PD builds. |
| **GT4Hooks** (Nenkai) | GT4 Online US (SCUS-97436) | Decrypt/decompress `CORE.GT4` -> ELF (`GT4ElfBuilderTool`), then **inject `GT4Hook.elf` into the ELF** with `ps2plugininjector` -> `SCUS_974.36.elf`; PCSX2 boots the ELF with the ISO attached | Replaces executable only; feature host. Ships HostFS (loose files instead of a rebuilt volume), reimplemented `HOutput` debug print, "+three new cameras" (`CameraSys::CameraOnBoard::GetCameraMountIndex`), and `mStorageMC::getFileSize`. (<https://github.com/Nenkai/GT4Hooks>) |
| **Adhoc / OpenAdhoc** (Nenkai) | GT3..GT7 family | **Re-implementation of the Adhoc scripting language + compiler/disassembler**; source-level editing of game-logic scripts | Enables both. Powers GT6 Spec II and GT4 Randomizer/Spec II. **Explicitly mis-usable for us:** it is the mechanism by which GT4 features were added without touching the engine. (<https://github.com/Nenkai/OpenAdhoc>) |
| **OpenAdhoc-GT4SpecII** (TheAdmiester) | GT4 | The Spec II Adhoc **source** (GPL-v3 per OpenAdhoc's license note) | The added events/missions live here. |
| **GT4FS** (Nenkai) | GT4 | Volume (`GT4.VOL`, RoFS) unpacker/packer. Modders extract `GT4.VOL`, edit loose, repack, rebuild ISO | Replacement-by-default tool; the substrate for every content edit. |
| **GTPS2ModelTool** (Nenkai) | GT3/GT4 | Dump/import car models (`GTM1, GTTR, GTTW, GT4/GTTW, CAR4, MDLS, PRM0, PRZ0, GTBR`); builds `.mdl` model sets, wheel/tire/car files from `.yaml` | **The one public path toward authored models.** Docs: "starting from here, you can edit the SpecDB using GTDataSQLiteConverter to edit/add the car." (<https://nenkai.github.io/gt-modding-hub/ps2/models/>) |
| **GT4 Re-Tuned** (community team) | GT4 | UNVERIFIED (see §6). GTPlanet describes it as in progress, "possibly the biggest GT4 mod yet" | UNVERIFIED |
| **HD texture packs / retextures** | Any GT4 version, incl. Spec II | Texture-level replacement. A public example states compatibility: "They work with all GT4 Versions (NTSC, PAL, Spec II Mod, etc.)." (<https://www.reddit.com/r/granturismo/comments/1p8vuwy/i_made_70_custom_retexturesliveries_for_gran/>) | **Replaces only.** No new cars/courses. |
| **Tourist Trophy track ports** | GT4 | Porting TT course data into GT4's volume/SpecDB. Wiki footnote: "Hackers have arranged to produce modified copies of GT4 incorporating this track from Tourist Trophy." (<https://gran-turismo.fandom.com/wiki/List_of_Gran_Turismo_courses>) | **Adds courses** — but only PD-made TT courses. Spec II ships Valencia this way. |
| **GT4 Online (custom servers)** | GT4 Online builds | Community server reimplementations restore the shipped online code; Spec II hides it, other mods re-enable it ("GT4 Online has it hidden in the menu so naturally I've re-enabled it.") | Restores existing functionality; no new content. |
| **Save editors / hybriding** (Nenkai GT4SaveEditor, MyMC, PS2 Save Builder) | GT4 | Save-file crypto/decrypt + editing; car hybridding | Replaces/edits existing data. Not a content mod. |

**Has ANY mod added content rather than replacing?** Yes, but with a hard ceiling. Spec II added cars, a course, events and missions — all from PD-authored assets or via script logic. **No mod, as of the last public discussion I could reach, has added a car model that Polyphony did not author.**

---

## 4 STATED BLOCKERS (verbatim)

The community's own words, in order of how load-bearing they are.

**The core blocker — adding a car that PD never modelled:**

> "It is not currently possible to add new cars into GT4 that weren't already created by PD and in the right file format to enable inclusion. In other words, this isn't possible at present."
> — GTPlanet staff/thread answer, <https://www.gtplanet.net/forum/threads/gran-turismo-4-spec-ii-added-ford-gt90-vector-m12-vector-m12-lm-race-car.435854/>

> "But as confirmed earlier, adding new cars outside of a select few already modelled and compressed to the correct format by Polyphony that weren't included in GT4 at launch isn't possible at this point in time."
> — same thread

> "This is my little wishlist of cars to be added to GT4. ... From my (limited) knowledge, it's pretty hard to mod the PS2 Gran Turismos to the point of adding completely new cars to the game."
> — r/granturismo4, <https://www.reddit.com/r/granturismo4/comments/1k33t35/>

> "iirc theres been recent progress on adding custom 3d models to gt4 but straight up adding cars I don't think so."
> — r/granturismo4, <https://www.reddit.com/r/granturismo4/comments/18w4f87/>

**What is and isn't possible, by category (the single most useful line for us):**

> "A general rule of thumb is that moving cars and *data* around is 100% possible, things like menu flow and adding certain features is sort of 50/50, and anything engine level like physics calculations or car cameras is a no."
> — TheAdmiester, <https://www.gtplanet.net/forum/threads/gran-turismo-4-modding-discussion.413859/page-2>

**The memory-allocation wall:**

> "When I mention limits it's not in the sense of DVD space as I have almost 1GB left to play with, it's referring to the amount of memory the game can allocate which can't feasibly be changed."
> — TheAdmiester, page 23

> "As far as I'm aware, it's all fixed static memory allocations, generally with everything allocated contiguously to avoid wasting space. That means that every reference to a particular piece of data is a hardcoded memory address in the executable which would need to be manually patched... plus anywhere it's referred to via an offset from something else, or is in an array that gets looped over."
> "Nothing is completely impossible, but it's the sort of level of work where it'd be 'easier' to fully decompile the whole game so that a compiler could sort it all out."
> — pez2k, page 23

**Custom models, still not done at time of writing:**

> "When we finally get custom models in the PS2 and PS3 GT titles it will be a good day, but it is not this day."
> — Dave A (GTPlanet moderator), page 25, last edited Apr 2 2026, <https://www.gtplanet.net/forum/threads/gran-turismo-4-modding-discussion.413859/page-25>

**Explicit near-miss, same page — "first custom GT4 vehicle mods are posted":**

> "Also today is the day the first custom GT4 vehicle mods are posted:"
> — a thread comment on page 25 (date not confirmed; the surrounding posts run Dec 2025 – Apr 2026). See §6.

**Save-file decryption limits (relevant if we ever read/write saves):**

> "Not all cars can be decrypted correctly, as the game uses shuffling during decryption of cars which involves non IEEE-754 compliant PS2 floating points that cannot be accurately replicated. Nothing can truly be done about it."
> — GT4SaveEditor README, <https://github.com/Nenkai/GT4SaveEditor>

**Modelling constraints, stated by the mod author:**

> "Bear in mind though that these sounds won't be perfect - I'm working with tiny 100KB per sound PS2-era limits at the end of the day."
> — TheAdmiester, first post of the Modding Discussion thread

---

## 5 CONTRADICTIONS (community claim vs VULCAN 4 measurement)

**1. Our "256-opcode interpreter" for the model file — corroborated in kind, not in number.**
The community independently documents that GT4 car models (`MDLS` / ModelSet2) are **an interpreted command program with a virtual machine, bytecode blob, and three VM entrypoints (Constructor/Update/Render)**, plus a render command list that "closely maps PDI-GL functions" (Nenkai, model-format blog, 2023-11-26). Our 256-opcode count is **not** published anywhere I found, and nothing public contradicts it. Treat the architecture as corroborated; the specific opcode count stays our own measurement.

**2. Our "car table 722 -> 744 rows, courses 110 -> 114" and "adding was done by patching the game executable too."**
The community **does not describe widening tables.** TheAdmiester frames the limit as *memory allocation*, not row count, and pez2k's mechanism (contiguous static blocks, hardcoded addresses) is about relocating/referencing data, not about table size per se. The only public statement that the executable is changed is the FAQ's "Spec II makes comprehensive changes to the game's disc image and executable" (evidenced by changing CRCs). **So: our table-resize finding is more specific than any public claim, and is neither confirmed nor denied by the community. Do not cite community agreement for it.**

**3. "Adding cars is impossible" reads as absolute but is not.**
Spec II demonstrably adds cars (Mini 1.3i '98, MX-5 '05, Korean Hyundais/Lexus, Valencia). What the community means by "impossible" is precisely **"impossible for models that are not PD-authored and already in the right format"** — their own follow-up sentence says exactly that. Reconciliation: the absolute phrasing is loose; the operative constraint is asset provenance + format.

**4. Where new content actually came from — script vs executable.**
Our framing centered on the executable. The community's architecture says the opposite weight: "Scripts operate nearly as the whole of game logic, while the executable mostly serves as the engine" (OpenAdhoc). The added events, missions 35-40, and event halls are **script-side** (Spec II ships Adhoc source). This is a real correction to our emphasis and is directly relevant to the mod loader design.

**5. "Adding features is 50/50, engine-level is a no"** (TheAdmiester). Our plan (second mod container, mod manager, `.kn5` -> drivable car SDK) sits across all three of his buckets: data (fine), features (50/50), engine-level (his explicit "no"). Worth flagging to the team as the honest risk band.

---

## 6 UNVERIFIED

- **Spec II's exact patch internals.** No public manifest of what bytes in `CORE.GT4` or `GT4.VOL` are changed, and no published list of patched addresses. Only the CRC changes and the FAQ's "comprehensive changes to the game's disc image and executable."
- **Whether Spec II resized the car/course tables, and if so how.** Not stated anywhere found.
- **GT4's numeric hardcoded limits** (max cars, max courses, max events, SpecDB row caps). No public value list found — contrary to our measurement of 722/744 and 110/114, which are unseen in public sources.
- **256 opcodes.** Community confirms a VM exists and publishes entrypoints, but no opcode count or table. Our number is unverified against any external source.
- **GT4 car model size limit.** Only GT3 values are published (688.128 KB race / 819.200 KB menu). No GT4 equivalent.
- **"the first custom GT4 vehicle mods are posted"** — GTPlanet comment, page 25. Date, author, content and whether it was a model swap vs a genuinely authored model are all unconfirmed. The next-most-recent statement on the same page (Apr 2 2026) still says custom models are not available.
- **GT4 Re-Tuned** — repeatedly described as in progress and "possibly the biggest GT4 mod yet" (GTPlanet pages 12/21/25, Dec 2024 – Dec 2025). No primary page reachable in this pass; base disc, distribution format, and whether it adds content are all UNVERIFIED.
- **Spec II output ISO size (~3.4 GB)** — from a single YouTube walkthrough, not from the mod's own docs.
- **`SCUS_974.36` and the "-Hook" suffix seen in a Mar 7 2026 screenshot filename** ("Gran Turismo 4 [Online Public Beta] [SCUS_974.36-Hook]_SCUS-97436_20260307022652.webp") — suggests the author now runs under a Hooks/HostFS-style executable during development, but this is inferred from a filename, not stated. UNVERIFIED.
- **Access note:** the GTPlanet Spec II thread (<https://www.gtplanet.net/forum/threads/gt4-spec-ii.420611/>) failed to extract via the extraction backend (CRAWL_UNKNOWN_ERROR). Its content is not represented here; the Modding Discussion thread (413859) and the Spec II website were used instead.

---

## Appendix: primary sources

- Spec II official site — <https://theadmiester.co.uk/specii> · install: `/install.html` · FAQ: `/faq.html` · changelog: `/changelog.html`
- Spec II mirror — <https://gt-mod.site/specii.html>
- Spec II source — <https://github.com/TheAdmiester/OpenAdhoc-GT4SpecII>
- GT Modding Hub — executables: <https://nenkai.github.io/gt-modding-hub/ps2/executables/> · volume & making edits: <https://nenkai.github.io/gt-modding-hub/ps2/gt4/volume> · models: <https://nenkai.github.io/gt-modding-hub/ps2/models/> · SpecDB: <https://nenkai.github.io/gt-modding-hub/concepts/specdb/> · game builds: <https://nenkai.github.io/gt-modding-hub/builds/gt4/> · model format post: <https://nenkai.github.io/gt-modding-hub/blog/2023/11/26/lifting-bonnet-on-gt-models/>
- GT4Hooks — <https://github.com/Nenkai/GT4Hooks> · OpenAdhoc — <https://github.com/Nenkai/OpenAdhoc> · GT4SaveEditor — <https://github.com/Nenkai/GT4SaveEditor>
- gt4-decomp (CORE.GT4 -> ELF, hashes) — <https://github.com/gustavocanepa/gt4-decomp/blob/master/orig/README.md>
- GTPlanet Modding Discussion thread — <https://www.gtplanet.net/forum/threads/gran-turismo-4-modding-discussion.413859/> (pages 1, 2, 23, 25 used)
- GTPlanet car-addition thread — <https://www.gtplanet.net/forum/threads/gran-turismo-4-spec-ii-added-ford-gt90-vector-m12-vector-m12-lm-race-car.435854/>
