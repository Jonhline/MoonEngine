# Classic fidelity audit — 3 November 2026

Classic is playable, but **100% fidelity is not established**. There is no
justified completion percentage. This audit separates missing code from
implemented behavior that lacks an independent original comparison.

## Reference and method

- Moon Engine starting revision: `0b91061`; current changes separately recorded below.
- External reference: [gustavocanepa/gt4-decomp](https://github.com/gustavocanepa/gt4-decomp),
  fetched read-only at `122afce110f76738720f5c0dfdfc410c5d08a62e`.
  No checkout replacement or wholesale source import was performed.
- Reviewed upstream README, `knowledge/architecture.md`, `coverage-map.md`,
  `classes.md`, RacePause loader/init sources and the local original disassembly.
  Knowledge pages carry different snapshot dates: their old percentages are
  not a current runtime-coverage measurement.
- The upstream README reports matching compiled PS2 functions/code bytes, with
  remaining assembly in its reconstructed executable. That demonstrates its
  own binary reconstruction, **not Moon Engine's renderer, sound, adapters or
  runtime completeness**. Its README also distinguishes the separate,
  not-playable native-port experiment. A matching function is useful as an
  oracle, not evidence that our corresponding path is integrated.
- Local review: FIDELITY, PHYSICS_STATUS,
  GT_MODE_COMPLETION, EDITIONS, OPTIONS and the previous COMPLETION_AUDIT;
  current `race_game`, `race_audio`, `race_pause`, `strobe_player`,
  `photo_mode`, `photo_travel`, `ps2_media`, `race_view` and LAN gate source.
- Local disassembly remains the behavioral authority; equivalent PCSX2 output
  is the image/audio reference. Do not reject a supported SCUS solely for its
  displayed version number; compare layouts/addresses when using another binary.

Grades: E exact within a declared fixture; C bounded comparison/integration;
A approximate or unverified; F a specific absent path; P deliberate host
adaptation/enhancement. No row below grades a whole mode E.

## Defects addressed in this session

1. **Split audio, C integration / A final sound.** `split_camera_audio_update`
   supplied only P1. Original `003B5F44..D8` selects the human entry's viewport.
   Both human entries now receive their own listener/target/history. Shared
   music/driver clock remain single. The actual app fixture keeps P2 engine
   requests identical when P1's camera moves 10,000 units; reinstating the old
   route fails. This is not an independent SPU2 recording or listening test.
   Ambience still uses P1; its complete original two-view policy needs review.
2. **Pause residency, C resource lifecycle / A Switch freeze cause.** The app
   previously reopened `/menu/common/pause.strb`, race text and later fonts/
   cursor while entering pause. Loading now prepares a resident pause and one
   font batch per loading step; reopening resets script/input state while
   keeping immutable resources. Original loader `003C31C8` and RacePause init
   are separate boundaries, supporting separation of loading and activation.
   Repeated 1P/2P openings/draws work with the volume closed in GL/Vulkan;
   skipping reset makes the negative fail. This does not reproduce or close
   the reported Switch hang. A console run/log and repeated Start/Continue,
   Screen Settings, Retry and Exit are still required.

## Remaining work, by area

Estimates are rough engineering days for one person for a bounded milestone;
obtaining original fixtures and hardware time are extra. They overlap and
must not be summed into a release date.

| Area / current grade | What remains | Evidence / first concrete closure criterion | Size |
|---|---|---|---|
| Race / physics — E/C per fixture, A full sessions | Fresh original constructors, full-event trajectories/results/AI/pits across race classes and ARM arithmetic coverage. Do not rewrite equations or retune vehicles. | PHYSICS_STATUS and EE_PHYSICS_AUDIT distinguish shared-native initial states from an independently captured PS2 constructor. Compare same inputs/state at declared boundaries, with an altered-state negative. | Large, 10–20+ |
| Split screen — C interfaces, A full original mix/pixels | Listen to both engine banks on hardware; verify ambience, replay cameras, per-player HUD and all pause routes. | New split_audio passes/negative fails. Need equivalent two-player console recording with cars far apart, camera changes and pause/resume. | Medium, 4–8 |
| Classic rendering — A overall | GS material/blend/depth/DATE/alpha, reflections/low-poly highlights, clipping, shadows, fog/skies/water/tunnels, particles and motion blur at equivalent frames. Existing isolated E/C proves inputs or synthetic passes, not the entire picture. | RENDERER/FIDELITY and night-course notes. Build a paired PCSX2/GL/Vulkan matrix; retain masks/tolerances and a deliberately corrupted-frame negative. Reproduce Alfa/Taurus faceting and Hong Kong water reports rather than blanket-smoothing geometry. | Very large, 20–40+ |
| Classic preset boundaries — C bounded options, A complete isolation | Check every registered enhancement, imported/older preferences, launcher/in-game changes and render restoration. Identify unconditional approximations. | OPTIONS and video_settings/preset registry. The GT Auto transition is now baseline, but its host 0.12-second smoothstep remains A; do not call those constants original. | Medium, 4–8 |
| GT Mode/career — C bounded cycles, A continuous completion | One independent new-profile progression with unlocks, cumulative rewards, endurance/championship suspend/resume, shopping and save/reload. | GT_MODE_COMPLETION/FIDELITY: isolated 144-series cycles are not one continuous 100% career. Compare persistent outcomes and original dialogs, including losing/cancel paths. | Large, 10–20+ |
| Licences/missions — E/C bounded rules, A full mode | Explain B-6's documented 34 ms timing difference without physics changes; verify remaining physical mission wins and UI/audio/camera results. | Existing all-80 gold-gate and 34 mission setup checks are not all-34 physical victories. Historical audit documents seven physical wins; remeasure before upgrading this claim. | Large, 10–20+ |
| Ghosts/demos — C/E commands, F independent PS2 best-lap import | Real best-lap save-family fixture and importer; compatibility of native v1 ghosts with corrected live arithmetic; fresh original replay constructors. | Native GHST roundtrip and full GT4M demo playback do not prove PS2 best-lap format support. Positive real-card import, deliberately wrong command/version negative. | Medium, 4–8 each |
| Replay/Theater — C paths, A complete presentation | Long user recordings, camera/seek/pause lifetime, timestamps/playlists and correct return origin on every route. | Existing disc/demo and host roundtrips. Compare a real personal PS2 recording end-to-end, not just disc tapes. | Medium/large, 6–12 |
| Photo Travel/Drive — E/C bounded camera data, A complete presentation | Map scroll/region highlighting (`003704D0`), camera conversions/trig, full warning/focus/help/obstacle behavior and slow-shutter histories for all scene actors. | Current photo_mode has CRPM selection/clipping and photo_travel has car-box/wall rejection. Old notes saying all these functions are absent are superseded. Compare original editor frames and camera movement; moving only the car is insufficient. | Large, 9–20 |
| Saved photographic film — C parsing/adapters, F/A remaining detail | Apply the stored `detail_mode`/LOD where required; verify SMOK clipping/temporal particles and complete develop projection/exposure against real original films. | Current ps2_media parses CDTL/GDGT; photo_travel applies fitted body/wheels/wing and race_view applies gadgets/far clip. Do not list these entire features as absent. `detail_mode` is retained in ps2_media.h but has no consumer in the reviewed renderer/editor. | Medium/large, 6–12 |
| Logger — C data/flows, A complete UI | CanLogger gates, original clocks/cursor/map actors, real A/B lap comparisons and cancel/reopen behavior. | Existing logger_view/drive/resource fixtures. Compare native original data and draw lists on actual two-lap recordings. | Medium, 4–8 |
| Sound/music — E/C banks/commands, A final mix | Timed independent SPU2 output: engine layers/pitch/Doppler/camera cuts, tyre/surface/crowd/pit/UI effects and music transitions, including split. | race_audio_clock/split_audio are transport regressions, not full original-waveform or timbre proofs. Silent tests need representative hardware listening. Do not assume a requested music-pause behavior is original without source. | Large, 10–20 |
| Movies — C bounded decode, A all playback | PSS A/V sync, seek/end/skip/failure/loading transitions on each host. | In-process playback and isolated decode checks do not certify all cinematics or Switch playback. Capture timestamps and compare equivalent source intervals. | Medium, 4–8 |
| Saves/profiles — C transactions, A real-container breadth; P host storage | Region/version/card-container compatibility, inherited nonzero GT3/GT4 data, interrupted writes, media/options/experience and exact export promises. | Existing CRC/roundtrip/synthetic inheritance checks; obtain independent real memory cards. Native host save format is an intentional replacement, not a PS2 memory-card implementation. | Medium/large, 6–12 |
| Native UI/inputs — C bounded pages, A all screens | Exact widget state/layout/focus, original fades/timelines, clipping, navigation reset and all pad/wheel semantics. | Local Adhoc scripts + native widget implementations. Paired original page/draw-list fixtures; a matching script does not prove every host native method. | Large, 10–20 |
| LAN/network, EyeToy, USB printer — F or explicit platform scope | LAN/network remains gated in menu_widgets. Decide whether 100% means complete original peripheral/network support or offline play. Native equivalents need actual integration/protocol/device tests. | Host export is not original USB printer support; an optional new network protocol would not prove original LAN. These exclusions must be named, not hidden under 100%. | Very large / uncertain |
| Switch/platform — P transport, A original behavior on hardware | Reproduce/fix pause hang with logs, memory/GPU lifetime, audio latency, input/focus and long-session stability. | ARM build success and PC GL/Vulkan checks do not certify deko3d or silicon physics. Test application mode and correct SD resources. | Hardware-dependent |

## Classic versus the publication gate

Enhanced realism, custom playlists, new cars/tracks and Tourist Trophy are not
missing original GT4 behaviors. They must not inflate this Classic backlog.
However the user's separate publication condition still requires complete
custom-car and genuinely newly authored-track workflows before public release.
No push or publication is authorized by this audit.

## Next evidence, in order

1. Run the new Switch NRO through repeated pause/reopen/Screen Settings/Retry/
   Exit in 1P and 2P, retain a fault log if it hangs. Listen to separated cars.
2. Create a versioned equivalent-frame/audio reference set for Classic:
   day/night/split, licences, GT Auto, Photo Drive/Travel, replay/results.
3. Close specific F paths (PS2 best-lap import and film detail-mode consumer)
   with original-source comparisons and independent real fixtures.
4. Expand constructor, continuous-career/mission and platform checks. Keep
   approximations A until their stated negative/reference checks exist.

This document is an audit, not authorization to implement every listed system
or a claim that the two reported problems establish whole-game fidelity.
