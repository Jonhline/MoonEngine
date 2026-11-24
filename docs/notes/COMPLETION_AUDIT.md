# Completion audit — 20 October 2026

Current follow-up: [Classic fidelity audit — 3 November 2026](CLASSIC_FIDELITY_AUDIT_2026-11-03.md).
It checks current source and the pinned upstream decomp reference; some missing
items below are historical and have since gained implementations. Consult the
follow-up before reopening an old F item.

This was the approval list delivered on 20 October 2026. The user subsequently
authorized continuation; the first completed follow-up is recorded below.
No missing feature was implemented in the original audit commit. Physics formulas,
parameters and simulation state must remain untouched by presentation work.

## Scope and how to read the estimates

Reviewed: [Fidelity](../FIDELITY.md), the general handoff,
the race handoff, [career status](../GT_MODE_COMPLETION.md),
[physics status](../PHYSICS_STATUS.md), and [editions](../EDITIONS.md), including
linked detailed notes and the current source paths cited below. The handoffs and
resource inventory contain historical failures: later scoped results take priority.
No complete-suite run, full playthrough, new PCSX2 comparison or hardware survey
was performed for this document. Prior test results are attributed, not rerun here.

E/C apply only to the stated comparison, not the entire area. A means approximate
or incompletely verified; F identifies a particular missing original path. P is
an intentional optional enhancement or host adaptation, not original fidelity.
A is not a synonym for an absent mode. No completion percentage is justified.

Effort estimates are engineering days for one developer, including original-code
analysis and focused positive/negative tests: small 1–3, medium 4–8, large 9–20,
very large 20–40+. They are provisional estimates, not delivery dates. Time to
obtain matching original captures, hardware access and resolve unknown format
branches is additional. Rows overlap; adding them is not a reliable total.

## Area inventory

| Area / current grade | Existing evidence | Remaining work and acceptance evidence | Estimated size |
|---|---|---|---|
| Race — A overall, bounded E/C | `race_run.c`, `race_game.c`, `race_hud.c`; original-code physics/controller/session oracles, race resources, pregrid, split-screen and pit tests in Fidelity §§4.1–4.2. Native startup, races, finish/return, pit crews and replay exist. | Equivalent-frame comparisons of starts, TV/interior/mirror cameras, HUD, results, pit sequence and audio across representative race classes. Preserve solver/cadence. Grade each mismatch separately, with an independent original comparator and a failing negative; do not relabel the whole race E from a solver fixture. | Large 10–20 for a representative validation matrix; discoveries additional. |
| GT Mode / career — A overall; rules E and bounded cycles C | `gtmode_game.c`, `career_home.c`, `career_bspec.c`; 144 series/527 stages/144 prize deliveries and 16 endurance wins documented in Fidelity; profiles were independent. Tuning across 722 cars has bounded setup/curve proofs. | One continuous new-profile progression, championship suspend/resume, unlocks, cumulative stats, all reward dependencies, shopping/GT Auto and display comparison. Isolated completed series do not prove a continuous 100% career. B-Spec experience is implemented: 4096 original arithmetic cases plus a physical Sunday Cup persistence/Retry/replay-exclusion test. Review complete monitor/pit/status presentation, not another experience producer. | Large 10–20; long race execution and original references additional. |
| Licences / missions — A overall; rules E, integration C | All 80 teacher-input licence attempts satisfy gold gates; 64 recorded nonzero times exact, 15 recording times zero, B-6 differs by 34 ms. 34 mission setups and 136 controlled result/persistence cycles exist. Physical wins documented for 1,4,8,9,21,22,24; mission 10 losing flow also tested. See arithmetic and mission records. | Explain B-6 timing from original startup/clock, without tuning physics or loosening strict timing checks. Validate other 27 physical mission victories, prize conditions, full result UI/cameras/audio and complete original construction/trajectory. Controlled injected outcomes are not physical wins. | Large 10–20+, dependent on fixtures and original state. |
| Ghost / demo — A overall; command/state boundaries E/C; PS2 best-lap import F | `disc_replay_arithmetic` covers all 13 title tapes, extended 161862 commands, sharing native initial contexts; frontend plays all tapes. Native v2 ghost roundtrip 5614 steps, measured zero position error with shifted-command negative. | Independent PS2 best-lap fixture/converter (`FileReplayBestLap`/license-family variants must be identified from original formats); test real-card containers and native v1 ghosts in corrected live TT. Fresh-console constructors, final camera/pixel parity remain separate. Historical Bentley wall-sticking notes are superseded by the shared EE arithmetic work. | Medium 4–8 for best-lap import; medium 4–8 for construction/version compatibility proof. |
| Logger UI — A overall; data adapters E/C | `logger_view`, `gtmode_logger_drive`, `race_resources`; frontend analysis/save/load/Track B retains A and reproduces 165 points; synthetic view negative exists. UI and analysis worker are functional. | Recover all CanLogger gates, original lap-clock/cursor/map icon and composition behavior; compare original draw lists and representative real A/B recordings. PC worker duration is a host adaptation; do not invent a console loading delay. | Medium 4–8. |
| Replay / Theater — A overall; imports/commands E, flows C; PC format P | `replay_file`, `replay_types`, `disc_replay_oracle`, frontend demo/Theater checks and arithmetic notes. GT4R v1 retains historical arithmetic, v2 uses corrected shared arithmetic. Replay return routing and two-player playback have bounded checks. | Real memory-card user recordings beyond disc demos, complete cameras/dates/selection/return/playlist comparison, long recordings, pause/seek/resource lifetime and version rejection coverage on supported hosts. No guarantee of every replay constructor or every raster branch. | Medium/large 6–12. |
| Photo Travel / Photo Drive — A overall; data/optics E/C; restricted subpaths F | 15 destination data and CRSS environment oracle; `photo_travel_flow`, `photo_optics`, `photo_motion`, `photo_pause_movie`, `photo_drive_menu`, `race_resources`. Capture, album, slideshow and PNG export work. | Implement original region/polygon/obstruction and can_shoot selection (`003FECB0`, `00371530`, map `003704D0`); current `src/core/photo_mode.c` explicitly uses region 0. Restore PhotoPause camera rejection and obstacle zoom (`003FFC68/003FFD10`) in `src/app/photo_travel.c`. Complete object/particle/pit temporal history. Compare actual destination and slow-shutter images. | Medium 4–8 per missing family; large 12–24 combined with optical comparison. |
| Audio — A for final sound; isolated banks/commands E/C; custom library P | `race_audio`, listener/camera-cut clocks, backfire producers, backend, musiccontrol/Jukebox and playlist persistence tests. Engine, ambience, pit, menu effects and music exist. | Independent timed SPU2 waveform/mix comparisons for engines, Doppler/camera cuts, tyres/surfaces, pit and UI music transitions; verify audible output after silent tests on representative hardware. Review pause against the original source before changing it: current original-behavior note retains the independent music stream. CR-X timbre investigation was deferred by the user, not solved. | Large 10–20 for representative references/comparators; hardware listening additional. |
| Movies / film development — A overall; PS2 scene subpaths F | Opening/course PSS and original still-picture fallback exist; movie tests and 56 course pictures have scoped decode/selection checks. `film_menu/develop`, `ps2_media.c` restore fitted wheel/wing, driver, SMOK/PTCL/ENVP. | Distinguish video playback from saved photographic film. PSS needs full A/V conversion/sync/transition reference coverage. Photographic film still lacks body/LOD CDTL application, GDGT, SMOK clipping plane, full develop far-clip/projection and temporal objects. Verify real personal film fixtures and portrait/aspect/rotation composition. | Medium 4–8 video validation; large 9–20 photographic scene completion. |
| Saves / profiles — C at tested integration boundaries, P host files; real-container breadth A | `save.c`, `save_state.c`, `save_ps2.c`, `profile_storage`, career reward cycles, import/export and native profile transactions. Reopen/CRC/truncation and user-data isolation have scoped tests. | Real-world PS2/GT3 containers, imported nonzero experience/options/media, multiple versions, interrupted-write recovery and compatibility matrix. GT3 inheritance currently has synthetic-source evidence; do not certify real card containers from it. Separate native exports from lossless PS2 writeback promises. | Medium 4–8; region breadth additional. |
| Render Classic — A overall, individual decoded data/parameters E/C | Original-model/shader/material/brake/time-zone/GS-state tests and bounded UI draw tests; `NIGHT_COURSES.md` contains source addresses, 25 native environment captures and lamp-shadow parameter oracle. Screen Setting and favourites already have E/C results. | Equivalent original screenshots and draw lists for skies/fog, day/night material passes, shadows, transparency/blur/heat, UI gradients and layouts. Lamp shadows now exist but ground-contact adapter, primary-pass scheduling and full pixels remain A. Gadget shadow drawing is now restored with bounded C tests and full-image A; backfire tween/colour/ground now has bounded E/C implementation, full-image A (2026-10-20 follow-up). Classic must disable every new option and restore prior output on both APIs. | Very large 20–40+ for a defined representative visual matrix; no all-assets estimate yet. |
| Enhanced — P; integration/visual balance A | Optional material shading, HDR/bloom, shadows, reflection quality, atmosphere and GPU/preset tests. New `graphics.car_lamps` and `graphics.night_sky` are Classic Off / Enhanced On; same shader paths on OpenGL/Vulkan. | Twelve car models now have checked front/rear candidate counts, with a bounded six-car/24-light GPU run and ghost exclusion. Direct atlas/batching has bounded reference checks and a modest measured gain, with higher depth storage; Vulkan/full-race performance remains open. Tagged lit/unlit lens emission and HDR bloom now have scoped GPU checks; inference still needs a broader cars/materials survey; rigid-world/car depth occlusion now has bounded GPU/native checks, but skinned/particle casters, aliasing and full-race performance remain open. Geometry-based directions/cones/range/tints are artistic, not recovered optical data. Night sky adds deterministic 24h sun, moon/stars; authored local radiance is now retained at night instead of being darkened twice. Baked track illumination still prevents full physical relighting. Daily sun/moon visibility, star rotation, actual app clock and paused image now have bounded GPU/integration checks. Broader dusk/night composition, complete replay scene restoration and multi-track GPU/driver stability/performance remain to verify. No whole-frame backend parity or console support claim. | Medium 4–8 lamp survey, large 9–20 occlusion/night integration, hardware coverage additional. |
| Custom cars — P, partial supported workflow; publication gate open | `tools/c/moonmod_addcar`, `moonmod_glbcar`, SDK runbook and Fidelity export/specs/sound/parts/pictures entries. Interactive export of a local Banana package and independent new table/model identities are documented. Vulkan picture shutdown fix supersedes the earlier crash note. | Finish authored paint and wheel-marker contract, complete camera/interior/parts/lamp coverage, audible sound-donor check, invalid-package and uninstall/conflict/reload tests. Demonstrate an independently authored redistributable car from SDK intake through purchase, save/reload and driving with no disc asset redistribution. A mesh alone does not close this gate. | Large 9–20; artwork and author iteration additional. |
| Custom courses — P for label/overlay proof; actual authoring pipeline F | `tools/c/moonmod_addtrack.c` explicitly copies an existing course and `.ad` under a new label. SDK runbook §7 says collision, timing and driving-line authoring are unresolved. `MODS.md` remains a proposal, not a stable schema guarantee. | Author and convert new world/collision/surface/RNW4 search data, driving lines, grid/sectors/finish/pits, cameras, sky/fog/environment and catalog assets. Prove full physical laps, AI, timing, save/replay and uninstall on a newly authored course, plus deliberate broken-collision/finish/line negatives. Cloning a disc course cannot qualify. | Very large 20–40+, high format/authoring uncertainty. |

## Specific original paths still missing

These are bounded F items, not whole missing modes. They require original-code
analysis first and an original comparator with a deliberately failing negative.

| Missing item | Source / current boundary | Size estimate |
|---|---|---|
| Photo Travel region collision/visibility and shoot eligibility | `003FECB0/00371530`; `photo_mode.c`, `menu_widgets.inc` ready-only can_shoot | 4–8 days |
| PhotoPause camera partition/rejection and minimum obstacle zoom | `003FFC68/003FFD10`; `photo_travel.c` floor substitute | 4–8 days |
| Slow-shutter particles/objects/pit histories | `photo_motion` covers car/wheel/driver histories, not all scene actors | 4–8 days |
| Independent PS2 best-lap ghost import | Distinct source format and real fixture needed; full GT4M and native GHST are insufficient evidence | 4–8 days |
| Photographic film body/LOD, gadgets, clipping/projection | CDTL/GDGT/SMOK in `ps2_media.c` and film develop path | 9–20 days |
| Backfire frame tween/colour and ground glow | `00456850/004A60E0`, a2=1/model1; producer/audio/basic flame already implemented | 2–4 days |

## Publication and scope decisions

**Keep the project private.** The user requires both custom cars and custom
courses before publication. Acceptance requires the end-to-end authoring evidence
above, stable package/version/conflict rules, clean removal, and a distributable
code/tools package containing no game assets, ISO/ELF, generated original C or
captured memory. Sharing only a new label or inherited course is insufficient.
This audit does not authorize a push, release or implementation of the list.

LAN, EyeToy, printing and an online replacement remain explicit scope decisions
in Editions/Fidelity, not silently completed features. Tourist Trophy and future
competitive multiplayer are not missing GT4 implementation. Additional console
and Android hardware defects must be tracked separately; PC OpenGL/Vulkan tests
here do not certify Switch or Android. In particular the Switch GPU fault has an
unresolved root-cause entry; a successful host build cannot close it.

## Reconciled historical notes

- B-Spec experience, favourites and Screen Setting are implemented with bounded
  original/integration tests. Old F summaries in handoffs must not reopen them.
- Bentley/Paris demo failures and the old licence gold counts predate the EE
  arithmetic correction. Current all-13 tape coverage and all-80 gold-gate result
  supersede those counts, with the 34 ms B-6 timing limit retained.
- 144 physical series cycles use independent fixtures; they are not a continuous
  complete career. Seven mission wins are not all 34 physical wins.
- This task restored the lamp shadow pass but did not prove its whole image.
  Night courses with a bright authored sky and zero ground glow are preserved
  from their source data, not fixed by a track-name blacklist.

Supporting detail: [resource audit, historical](CLASSIC_RESOURCE_AUDIT.md),
[EE arithmetic](DISC_REPLAY_ARITHMETIC.md), [adapter/consumer audit](EE_PHYSICS_AUDIT.md),
[native night captures and limits](NIGHT_COURSES.md),
[SDK workflow](../vulcan4/SDK-MAKING-A-CAR.md), [mod design](../MODS.md),
[renderer](../RENDERER.md), [option registry](../OPTIONS.md).

## Follow-up: original gadget shadow pass

Implemented after the user authorized continuation, 20 October 2026. The paired
odd model, source plane projection/alpha and Env0 gate are restored. Original
scalar comparisons and actual Tsukuba race-view GL/Vulkan positive/negative
checks pass; object state is unchanged by drawing. Whole-image comparison stays
A until equivalent PCSX2 captures and further course/view coverage. Details and
initial failed harness/build attempts are in Fidelity, “Original course gadget
shadows”. Remaining estimates above are unchanged; this does not authorize
publication or certify the complete race.

## Follow-up: Enhanced authored night radiance

The excessive darkening is corrected at the shader boundary: baked/local source
radiance survives the optional solar relighting, while procedural sky and new car
lamps stay separate. Both APIs pass source-radiance/Off checks and the 25-course
capture audit. This removes the known double-darkening defect, not the remaining
night image comparisons, optical lamp survey or light-leak/occlusion work.

## Follow-up: daily celestial cycle

The optional24h cycle now has explicit Course/manual-hour selection, visible
sun/moon disc checks, rotating stars and verified pause/live/replay clock coupling
on OpenGL/Vulkan. A fixed-camera 97-frame day closes to identical midnight pixels.
This is intentional presentation P, not monthly lunar phases or original PS2
astronomy. Original lamp/vehicle model gates and the broader audit remain separate.


### Backfire follow-up, 20 October

The previously missing authored tween/RGBA and model-1 ground glow are now
implemented. Callback fraction/branch has an original-code comparator and failing
negative; both native APIs show the effect using the disc's Focus and route5.
The 24 scoped checks and limits are recorded in FIDELITY. Equivalent PCSX2 pixels
and an all-car survey remain outstanding; this does not close Classic rendering.


### Photo camera follow-up, 20 October

CRPM region selection, retained edge/corner movement clipping, angular aiming
window and rejection, ordered/wrapped REP4 point selection, PhotoPause car-box
zoom rejection and original wall-query integration are implemented with bounded
E/C original-code comparators and failing negatives. The writable can_shoot flag
now preserves its original default and warning-dialog flow. Full camera/visibility
pixels, map-car input, arbitrary pose conversion and original-library trig remain
A; see the detailed FIDELITY entry. Photo film body/LOD/GDGT/clipping/far projection
and temporal particles are a separate follow-up, not closed by camera work.
