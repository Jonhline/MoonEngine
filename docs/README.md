# OpenGT4 documentation

Start with [How it works](HOW_IT_WORKS.md), then [Building and testing](BUILDING_AND_TESTING.md).
[Architecture](ARCHITECTURE.md) maps the execution model to source directories.

## Project and validation

- [Editions](EDITIONS.md): the original baseline, the planned improvements
  (on by default, with an Original preset), Tourist Trophy and competitive
  multiplayer.
- [Mods](MODS.md): proposed packages and asset conversion; a design plan,
  not a supported SDK or stable format.
- [Fidelity](FIDELITY.md): per-feature grades, evidence and limitations.
  Each comparison certifies its stated scope, not the entire game.
- [Resource coverage](CLASSIC_RESOURCE_AUDIT.md): functional availability versus verification.
- [GT Mode interface](GT_MODE_UI.md): scripts, career objects and presentation boundaries.
- [Cinematic camera](CINEMATIC_CAMERA.md): controls for the deliberate PC camera feature.
- [Display settings](PC_VIDEO_UI.md): host display choices and their persistence.
- [Development notes](notes/README.md): implementation history, original
  addresses, experiments and positive/negative test evidence.

## Disc formats and menus

- [Volume](VOL_FORMAT.md): GT4.VOL indexing, compression and file lookup.
- [Images](TEX1_FORMAT.md): Tex1 palettes, pixel storage and authored mip levels.
- [UI projects](UI_FORMAT.md): widget trees and image packages.
- [Adhoc](ADHOC.md): compiled menu scripts and their native interpreter.
- [Text](TEXT_FORMAT.md): fonts, localized strings and layout.
- [Bitmap animation](STROBE.md): STRB timelines and supported operations.
- [Navigation](MENU_NAVIGATION.md): script-driven focus and input routing.
- [Presentation](MENU_PRESENTATION.md): actors, transitions and composition.
- [Movies](MOVIES.md): playback and composition with menu layers.
- [HD images](UI_HD.md): optional local cache of upscaled menu images.

## Rendering, racing and persistence

- [Renderer](RENDERER.md): OpenGL backend and native scene submission.
- [Models](MODELS.md), [showroom](SHOWROOM_SCENE.md): car assets and studio scenes.
- [Courses](COURSES.md): geometry, collision data and scene loading.
- [Cars](CAR_CATALOG.md), [course catalog](COURSE_CATALOG.md): disc-backed selection data.
- [Save games](SAVEGAMES.md): PC storage and supported legacy transfer.

## Career, media and detailed coverage

- [Physics status](PHYSICS_STATUS.md): comparison coverage and transitional state.
- [Career lifecycle](CAREER_CYCLE.md): setup, results, awards and persistence.
- [Career logic validation](GT_MODE_LOGIC_VALIDATION.md), [coverage](GT_MODE_COMPLETION.md): bounded comparisons and remaining verification.
- [Home](HOME.md): profile counters and script-cache consistency.
- [Car setup](SETTING.md): installed parts, settings and race parameters.
- [Missions](MISSIONS.md): descriptors, rules and result handling.
- [Race screens](RACE_SCREEN_INVENTORY.md): mode-dependent project routing.
- [Race audio](RACE_AUDIO.md): banks, control commands and waveform limits.
- [Photo modes](PHOTO_TRAVEL.md), [media coverage](PHOTO_MEDIA_COVERAGE.md): photo scenes, imports and separate verification scopes.
- [Roadmap](ROADMAP.md): development stages and historical plans.

## Original executable research

These describe reference tooling, not the runtime of `opengt4`.

- [CORE.GT4](analysis/CORE_FORMAT.md): executable container and unpacking.
- [Symbols](analysis/SYMBOLS.md): library identification and original addresses.
- [Execution start](analysis/EXECUTION_START.md), [IOP](analysis/INIT_IOP.md): boot flow and bounded reference protocols.
- [Threads](analysis/THREADS.md), [thread audit](analysis/THREAD_REFERENCE.md): reference scheduling and differences.
- [Physics](analysis/PHYSICS.md): update order, memory structures and comparisons.
- [Reference capture](analysis/REFERENCE_CAPTURE.md): local PCSX2 evidence collection.
