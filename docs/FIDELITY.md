# Fidelity record

## Supra boost HUD diagnostics (21 October 2026)

C (host integration only): hud_boost tests load the four turbo racing
Supras (Castrol 2000/2001, Denso, au), exercise the existing engine-drive
routine at 8,000 rpm for 120 calls with full throttle and 120 on lift,
and compare the watched-car HUD pressure with car+60C (003446B0,
0039E104 -> 003A94A8). Loaded engines exceed pressure 1 under load and
fall below 1 on lift. Woodone 2003 correctly has no turbo gauge.
All five WILL_FAIL negatives detect a wrong pressure/enable value.
18 focused HUD/oil checks pass, including existing needle-range checks.
The optional HUD capture fixture accepts MOON_BOOST_HUD_VALUE.
A/open: this does not reproduce sixth-gear live driving or prove complete
needle pixels against PS2; no angle, pressure formula or physics fix is claimed.
Original 00407568 sets range 0..2.4 and sweep 0..216; 003BE828 adds 90.
The production executable compiled after adding a missing forward declaration
used by the local launcher include. No launcher behaviour was changed.


## Arcade oil warning cold cache (21 October 2026)

C: 003437B0 calls 00350830 and 00350678 before reading the oil threshold.
No-wear Arcade engines can leave that cache cold; the HUD used zero and
lit the lamp on fresh cars. Both layouts now resolve the displayed limit
from settings +D8 when +CC is clear, without modifying the simulation.
gtmode_oil_warning compares cold/warm thresholds against original code,
including 16 onset samples 0..255; its WILL_FAIL negative is detected.
Oil lamp, watched-car pressure bridge and boost needle tests pass with
negatives (8 focused checks). A/open: live turbo report not reproduced.


## Civic underside report and GT Auto rim audit (20 October 2026)

C (host data invariant only): car_wheel_body checks all 222 GT Auto rim
labels against stock on seven cars, using both /car/lod race and /car/menu
loaders: Civic SiR-II 1991, CR-X SiR 1990, Ka, S2000, Focus RS, Mustang GT
and the Skyline pace car. All 3,108 combinations retain body vertex fields,
material/draw state and referenced RGBA texture bytes. The WILL_FAIL negative
changes one body vertex colour and is detected. This is not a PS2 pixel oracle.
The source selection remains 00454270 +20..48 (wheel/menu versus wheel/lod),
with attachment through 0045314C; no renderer/physics behaviour was changed.

A/open: the user's bright underside seams remain unreproduced. Local low-side
race captures include tilted bodies, wheel blur, stock and brdg001w rims;
the Civic was checked on OpenGL and Vulkan and in its menu model. These
samples do not certify every rim's pixels or the user's saved vehicle.
showroom_probe now accepts MOON_PROBE_WHEEL in the race path, with optional
MOON_PROBE_ORBIT (radians), MOON_PROBE_ROLL and MOON_PROBE_DAYLIGHT diagnostic
fixtures. The affected player's profile/build still needs comparison. Do not
report the white seams fixed or declare other cars unaffected from this audit.


## Manual Save uses the authored dialog flow (20 October 2026)

C: gtmode.ad L4347 asks “Do you save?”, L4350 queues saveAuto;
L253..269 owns the optional progress dialog, and L4341 owns completion.
The native recursive button binder no longer replaces GT Mode Save handlers.
profile_storage checks the real Home handler: No preserves dirty data,
Yes writes the current player, then Trade and Arcade still work. The
save_authored_handler_negative WILL_FAIL test restores the native override
and must fail. storage_access checks animated I/O, failures and recovery.
P: profile files and the hard-disk artwork remain host adaptations; this
check does not claim complete PS2 storage-error-dialog equivalence.


This record grades individual features against the original game. Read each
row together with its evidence and limitations: a grade applies to the
listed fields, inputs and comparison, not automatically to an entire mode.
The detailed record below is maintained in Portuguese.

- **E — Exact:** value-for-value or bit-for-bit agreement in an automatic
  comparison against original code or data.
- **C — Checked:** an automatic comparison passes with an explicitly stated
  tolerance for the output being checked.
- **V — Visual:** side-by-side comparison without an automatic comparator.
- **A — Approximate:** a known difference or estimate remains.
- **F — Missing:** original behavior is not implemented.
- **P — Intentional difference:** a documented PC adaptation or new feature.
- **N — Out of scope:** explicitly not planned.

The policy below counts E and C as verified, with a specific exception for
V in 3D imagery and audio. V elsewhere, A and F remain verification debt;
P and N are excluded from the denominator. Historical rows can retain
superseded findings: follow their scope and supporting evidence, and use
the [development notes](notes/README.md) for the implementation history.
The inventory was reconciled with source and registered tests on 13/10/2026.
Aggregate A grades do not override the narrower E/C proofs below; confirmed
missing subfeatures remain F.

### Current profile for ordinary storage operations (20 October 2026)

P storage adaptation, requested: another-profile selection is now confined to
GT Mode Trade's explicit other-card check (gtmode.ad Home Trade L4297, slot 1).
Implicit checks for GarageRoot L6749, acquisition GetCarRoot's internal return,
garage operations L13283 and pushRidingCar L13923 bind the active player.
Arcade's garage likewise reads the active profile, rather than invoking a
picker. Player Profiles remains the place to switch the active player.
The old manager's stored slot cannot accidentally redirect a later implicit
check to Trade's source. Autosave, access indicator and manual save remain.
This supersedes the earlier row retaining an Arcade profile picker.

profile_storage checks four implicit calls using a manager previously bound
to slot 1, zero picker invocations, current file slot, actual Arcade openGarage
while the picker would cancel, explicit Trade selection/cancel/purchase with
source unchanged. operation_current_profile_negative deliberately records an
unwanted picker call and fails (WILL_FAIL); owned-car negative still fails.
Scope: native storage routing plus actual Trade/Arcade integration, not a
complete new/used dealership purchase oracle. An attempted old special-purchase
harness failed at GetCarRoot.onInitialize (owned-part data) before its check;
it is not registered as evidence for this fix. No game economy/physics changes.

### Page-close loading flash regression (20 October 2026)

C: startPage no longer writes RootWindow.visible=0 between its two presented
waits. Original 0022F6A0 calls Sync(2) at 0022F6E0/0022F79C; 00233D18 sets
RootWindow+E8 bit 3, a pending-close flag, and finalization is after the second
barrier. It does not clear visibility. Colour/direct page transitions could
therefore show an unintended black frame in the native implementation.
menu_tests checks visibility in both waits with no game assets; WILL_FAIL
page_close_visible_negative hides it deliberately. This covers the adapter
regression, not every loading route's visual parity. Hidden live loading smoke
still fails its cold-preparation responsiveness threshold (741 ms pump gap),
although fade alpha completes; that independent latency remains open.
Quick Settings/QuickTunePopup is direct startPage in quick-arcade.ad L386,
L618/L623; no original panOutIn/loading screen is present on that path.

### Owned-car profile picker regression (20 October 2026)

P correction to native storage adaptation: gtmode.ad GarageRoot L6749 calls
checkGameData with implicit slot zero, matching memorycard.ad L167..172.
It now binds the current player without asking to choose another save. Explicit
other-card Trade and Arcade garage loads retain the picker/cancel flow.
profile_storage verifies no picker invocation for owned-car selection, active
slot 1, independent source slot 2, Trade purchase and cancel. WILL_FAIL
owned_car_picker_negative deliberately records an unexpected picker call.
No autosave event or original acquisition/calendar rule was removed.

### Photo Drive Select boundary (20 October 2026)

C integration: replay Select press (0xFFFF) enters the existing resident
PhotoPause editor; release/Start/live driving do not. Select exits the editor
to that replay without also toggling replay hold. F5 retains the existing PC
shortcut. Retail instruction manual pp24/25/28 describes drive -> finish ->
Start Replay -> Select, not a new entry inside common/pause.strb (whose callback
set contains no photo action). Original caller 0010CE14 reaches 003C3CE8.
https://db.hfsplay.fr/files/2019/12/02/Gran_Turismo_4_USA_teBOv76.pdf
photo_drive_menu now obtains the actual authored getPhotoDriveInfoByCourse
record and checks init_entry_item makes its button visible. race_resources
covers Select/release/wrong-key/live-driving gates, frozen world/cursor while
photographing and the replay return; photo_drive_select_negative deliberately
suppresses Select and fails. Headless platform test, not hardware pad QA.

### Cappuccino save-editor report (20 October 2026)

P external tooling: errors from Apply power / mass are now displayed beside
the controls as well as in Activity. Exact reported targets 160 cv / 7 kg
were reproduced for both catalogue Cappuccinos through the actual frontend
button and native helper: EA11R 1991 achieves 160.04 cv / 7 kg (253%,1%);
EA21R 1995 achieves 159.80 cv / 6 kg (248%,1%). Integer percentage storage
explains the latter quantization. save_editor_cappuccino writes/reloads the
native profile through menu_save_refresh, compares selected current body,
native physical stats and getCurrentPower/getCurrentWeight. WILL_FAIL expects
1 kg. Frontend also checks an impossible target is rejected visibly and the
document stays unchanged. User's unchanged-file report remains unreproduced
without the specific edited file/path; no claim that this is its root cause.

### Private native save editor (20 October 2026)

Native vehicle / parameter follow-up, P: Garage can add a searched real model
with factory paint, remove it and select the current car. These use native
addNewCar/delCar/changeRidingCar and the seeded vehicle factory, not partial
invented objects. Advanced targets metric cv/PS and kg through the actual
vehicle-stats calculation. Serialized sheet +0x163 (power), +0x162 (mass) are
original percentages read by physics/record.c and physics/setup.c; 0 means
100%. Targets use the nearest 1..255 whole percent, show achieved results and
reject out-of-range values. Compact units/flags and selected/imported caches
are synchronized. No game physics source was modified.

Advanced exposes setup groups (career_setting.c setters, 0043E2F0..0043F248,
live offsets minus 8), paired ASM, signed ballast position, 64-bit equipment
keys and typed access to all 0x4b8 body bytes. Unverified units remain original
group/index labels. Arbitrary parts combinations are not proven valid. The
separate helper rejects invalid records without applying changes; only its
garage result is merged back. License grant/remove creates missing records.
Generic Advanced adds/removes typed fields, array/map entries and allowed save
sections. Save-copy protection and undo remain in effect.

Tests: save_editor_vehicle uses real native operations behind mocked headless
frontend IPC. It adds RX-7 Type RS, selects it, tunes targets 400 cv / 1000 kg
to representable 399.56 cv / 998 kg, changes TCS, removes the selected vehicle
and restores by undo. Checks include current selection, unchanged calendar and
credits, missing license records, grant/remove, typed endian/sign/overflow,
invalid model/target rejection and generic field add/remove. WILL_FAIL negative
expects the deliberately wrong mass 1 kg. Local capture:
`build/save-editor-vehicle.png`. Packaged WebView/dialogs still need a manual
pass; this remains external debug tooling, not original UI.

Guided-editor follow-up, P: Garage now has named owned vehicles, actual factory
paint names/chips, kilometres, active setup and TCS/ASM/brake forms; Licenses
has B/A/IB/IA/S tabs, individual medals/times and a confirmed bulk operation per
16 exams. Basic categories use forms; raw types/JSON stay in Advanced. Review
shows human-readable vehicle/exam changes before collapsible stored details.
The read-only `moon_save_catalog` helper is embedded in the standalone binary;
it reads the user's US disc and caches metadata in the editor's private data
folder. No game data is committed/distributed. Current catalogue: 722 vehicle
labels and 80 exams. The native-vehicle follow-up above adds model creation;
guided parts installation and a named event catalogue remain outside this pass.

Sources: CAR_NAME_american +0 / StrDB and menu_car.inc `car_paints` sorting;
00430E58 / career_race_lookup.c for authoritative record ordinals; 004323D0 /
career_license.c for ranking ordinals; 00447670/B0/F0 for medal targets;
gtmode_game.c getOdometer reads wear +0x498 and converts metres to kilometres;
career_setting.c original setter offsets and paired ASM bytes; career_blob.c
base64 array storage. Paint changes all three setup bytes and compact flag
bits 2..8; mileage updates raw wear, full body, compact unit and selected-car
caches. Initially power/weight summaries were read-only; the native-vehicle
follow-up above adds physical edits. Aid/brake values are u8 debug
settings without vehicle-specific tuning range validation. License editing
updates authoritative words, existing named caches and ranked times together,
preserves upper record/reward bits, and refuses non-US imported license maps.
It neither advances the calendar nor awards prize cars/credits.

Tests: the guided fixture uses the actual authored GT Mode project and native
stock-car factory. Headless Edge edits paint/mileage/setup/TCS and all 80 exams;
the copied file is reloaded through the native reader and getters: all five
licenses passed, gold fields/best rankings, all three paint bytes, compact
variation, full/current body equality, metre value and TCS. WILL_FAIL negative
expects 8 instead of 7 TCS. UI/model checks also cover undo/redo, invalid paint,
times that contradict medals, clearing/downgrading, unchanged calendar/credits,
cached record/high-bit preservation and non-US refusal. The original six Rust
tests and native interop positive/negative still pass. Local screenshots:
`build/save-editor-guided-garage.png` and `...-licenses.png`. Windows dialogs
and the packaged WebView still need a manual pass; UI IPC is mocked in the
headless test, with final output independently loaded by the engine.

During development the first form rejected the valid stock ASM value 11:
the invented 0..10 limit was removed in favour of the stored byte range.
The native fixture also exposed base64-wrapped bodies instead of byte arrays;
the editor now preserves that encoding. A harness called a nonexistent result
method and was corrected to read the actual native `result` property. No game
code/physics was changed for these failures. These are tooling checks, not a
claim that arbitrary career/ghost/reward edits are semantically consistent.

Initial preview (before the guided follow-up): P, standalone Windows tooling in `tools/save_editor`, using the existing Moon
SDK Tauri/CSS shell. No game/physics changes and no player setting: this is an
explicit external editing operation. English categories, searchable saved
paths/values, scalar editing, typed JSON for collections, undo/redo, review,
source-change detection, original backups and new-file-only output. Existing
profiles cannot be overwritten. Internal IDs/opaque import arrays are editable
as stored; the first preview did not have guided car/license controls or a named
event catalogue. Not a PS2 memory-card or complete-package archive editor.

Format evidence: `src/core/save.c` 32-byte OGT4SAVE v1 envelope, lengths, payload
and header CRC32, timestamp; `save_state.c` tags 0..7, ordered members/maps,
depth/count/string limits and root validation. No-op native writer -> Rust codec
-> native reader preserves the entire envelope byte-for-byte. A changed credits
value above JavaScript's exact integer range loads through `gt4_state_replace`
with every other payload byte unchanged. Test fixture covers every stored tag,
negative zero, UTF-8, an unknown nested field and integer map keys. This proves
the fixture's storage interoperability, not semantic validity of arbitrary edits.

Focused tests: six Rust unit tests (all tags, exact integers, invalid CRC/types,
names/duplicates/non-finite values/overflow, new-file protection, exact backup,
external modification refusal); `save_editor_interop` and its WILL_FAIL negative
(mutates one expected native payload byte); headless Edge frontend test covers
validation errors, edit/undo/redo, review/copy and safe search, with a local
capture at `build/save-editor.png`. Frontend uses a mocked dialog/IPC boundary
and real native fixture; packaged Windows dialogs/WebView interactions still
need a manual pass. Initial test launch lacked a bundled Chromium browser and
was changed to installed Edge; opening ES modules over file:// also required
the test's temporary loopback server. Neither is a production dependency.

### Optional full-day procedural sky (20 October 2026)

P: `graphics.night_sky` is individual, Classic Off / Enhanced On, in PC Settings
> Light & Shadow > Night Sky and the shared launcher Quality API. It requires
Daylight; turning it on enables Daylight. Off retains the legacy daytime cycle
and the authored-night guard. No physics or recorded control format changes.

Solar Time now has all 24 hours; saved indices 0..10 still mean 07:00..17:00,
then 11..23 mean 18:00..06:00. With the default 12:00 setting, nonzero authored
Env0 time zones start at 22:00; explicit other hours override that default.
Fixed, Real Time (1/3600 hour per second) and 60x (1/60) use the existing recorded
frame clock, wrapping at 24. No wall clock or RNG. Lamp activation continues to
follow original night/safety-car and brake flags, independently of solar time.

The same shader on OpenGL/Vulkan adds a moon opposite the sun and a deterministic
integer-hash star field, blending with the existing daytime atmosphere over
sun elevation -0.12..0.08. Direct sunlight is zero below the horizon; solar
shadow-map casters are skipped there. Night ambient is artistic, not recovered
PS2 data. Course illumination is still baked into textures; this is not a fully
relightable nighttime world. No lunar ephemeris, moonlight shadow pass or weather.
Native Route5 captures are very dark away from the car lamps; visual balance
and more tracks still need review. Whole-frame API equivalence is not proved.

Tests: night_sky GL/Vulkan and their deliberate noon-as-night negatives;
2400 deterministic clock/ambient samples, repeat-frame byte equality, dark-night
and star output, default course time, wrap and legacy clamp. Initial tests failed
because no star samples survived the float-hash/threshold combination; the
integer hash and revised density pass. launcher_quality verifies persistence
and Classic restoration. No equivalent PCSX2 capture: original parity remains A.
Local captures: build/night-sky-<api>-enhanced-night.png; logs:
build/night-sky-final-tests.log and build/night-sky-regression-tests.log.

### Optional car lamps (20 October 2026)

P: graphics.car_lamps, independent Classic Off / Enhanced On. World-space cones
and attenuation illuminate course/walls/cars; nearby lens fragments emit into
HDR/bloom. Sources are inferred from night-versus-day geometry/material changes
and unlit pressed brake branches, area centroids and normals; no per-car positions.
Night and safety-car gates follow the existing original selection, brakes use
per-entry presentation. Shared GLSL on both APIs. car_lamps GPU fixtures with
disabled-source negatives pass; launcher_quality verifies persistence/presets.
Real Focus/Route5 has four emitters and changed pixels on both APIs. Initial
geometry check found zero emitters because unlit normals are absent; face-normal
fallback and comparison of lit night surfaces repaired the classification.
A/P limits: inferred optical axis and generic cone/range/tint; no full 600-car
survey, whole-frame API parity or PS2 image claim. The later car-lamp occlusion
entry below supersedes the original missing-shadow limitation.
[Parameters and evidence](RENDERER.md#optional-world-space-car-lamps-20-october-2026).

### Live and replay brake-lamp presentation (20 October 2026)

C (integration): the existing per-tick driver presentation copies each entry's
CAR_BRAKE_PEDAL and publishes callback-0 pressed state. race_brake_lamps checks
six cars on every tick for 2400 live ticks and 2400 replay ticks, with real
keyboard brake/release commands and IA pedals; every entry has transitions.
The deliberately inverted-state negative fails. No physics writes by the test
or renderer adapter; normal simulation advances from controls. Existing
car_brake_branch/negative compare authored geometry/attributes and
brake_material/focus/negative retain independent material/texture selection.
Seven focused cases pass. No brake-state functionality needed changing.
A: whole-frame PS2 parity and all car/time-zone combinations still unproved;
probe diagnostic getter is read-only and does not certify final pixels per tick.
Local image build/race-brake-brake-lamps.png is available for comparison.

### Classic night shadow parameters and sky guard (20 October 2026)

C: 0038E7F8 -> 0038F168 shadow direction/tint/alpha uploads compared for 192
cases at 2e-6, including directional/point/spot, missing surface and global paths.
Dedicated alpha negative and lighting negative fail. Initial positive failed
on omitted point-light alpha attenuation and was repaired. OpenGL/Vulkan compare
identical RGB with the day-only sky disabled versus blocked on authored night;
cleared-zone negatives fail. No physics formulas/parameters/state edited.

A: model-1 per-lamp pass integrated, up to four primary/two other, skipping fewer
than two selected lamps; retained contact and ground glow. Ground query remains
an adapter, original scene scheduler and complete pixels not proved. 25 course
variants captured locally; original PCSX2 equivalents still needed. This
supersedes the F pass absence, not the A full-image grade. Details, exceptions,
source and evidence: [night audit](notes/NIGHT_COURSES.md).

### Jukebox Menu/Replay folders, Library groups and song line (19 October 2026)

**P:** `audio.jukebox_folders` adds Menu and Replay rows under Racing and
Slideshow in MusicRoot's folder column. They are clones of the authored
PlayList2/Append2 run by the authored playlist code: music_list/music_map gain
modes 3 and 4, and append_music_list(mode), list_box.append_playlist(mode),
remove_playlist and set_mode work unchanged. Only set_mode_icons (three
mode_table_list rows, L353..375) and mselect_callback (L400..402) are wrapped:
modes 0..2 run as authored, the new modes use the playlist-folder row. Pictures
are the disc's: gtmode HomeRoot `option.png`, labomode `folder_replay.png` and
`to_list_mov(_over).png`; the Menu add button is `to_list_slide_bgm(_over).png`
with its note replaced by four squares in the button's own two colours. With the
folders the sidebar steps 68 px (was 80) and VisualEffect_library moves to the
sixth button place. Empty Menu keeps the original menu music; empty Replay uses
the Racing list, as the PS2 replay does. Classic sets the option to 0 and the
page is the authored one. `custom_music_folders` (+negative) adds one song to
both folders, shoots Library/Menu/Replay, re-enters and checks the saved lists
and contexts.

**P:** `audio.jukebox_groups` orders the Library as disc songs, then each Music
subfolder's files by name, with the folder shown where the artist goes.
init_music_list (L327..329) reads that order from default_library_playlist.

**A:** with the mouse the help line was written after onFocusEnter, so the
folders' "Racing playlist/No. of songs:" + count (slide.ad L1421 "%s%d") lost
its count on hover, and a click on the focused folder restored the bare text.
Hover now writes it before onFocusEnter, as setFocus does, and refocus keeps a
line the script extended. The new folders and buttons show the authored Racing
texts with the folder's name ("Add to Menu playlist", "Replay playlist/No. of
songs:1"); `custom_music_folders` checks them by mouse hover and click.

**A:** Jukebox preview played local songs fast: the stream was installed again
every frame, each time reading its preroll from the song. It is installed once;
`custom_music_ui` bounds about a second of play under 96000 frames (was ~260000).

**A:** a ListBox moved by the pad did not call its focus_callback, so the
Jukebox song line (list_box.focus_callback -> set_music_tooltip, L1181..1184)
stayed on the first song. mListBox's selection change (002B29C0, virtual 69)
dispatches it; both ListBox key paths now do, as hover already did. Only
arcade.ad and slide.ad register one. arcade_return and the menu navigation
tests pass.

### One Jukebox page, Array.sort and MusicRoot exit (19 October 2026)

**P:** the second slide/MusicRoot page the extension opened over the original
(its own loop, input and audio; leaving it fell into the original underneath) is
removed. Local songs are bound into the page GT Mode opens, before entry; its
sound.startStreamEx row plays through the library, its own Exit leaves.
Opening and leaving no longer enables `audio.custom_music`; filenames show
without extension. `custom_music_ui` drives the page the game opened.

**A (EE oracle pending):** Array.sort(less) was ignoring the script's comparator,
so musiccontrol.h load_playlist (`list.sort(sortfunc_playlist_dispno)`) showed
Racing/Slideshow in Library order instead of saved dispno order. Now 002EE884 ->
002ED7A0 -> 002F03C8: SGI std::sort over 4-byte values (introsort 005EC8D0,
threshold 16, depth 2*lg n, heap 005ECE38, median of three, unguarded partition
005ECCE8, final insertion 005ECB28). `adhoc_array_move` covers rows by dispno and
descending order for n 0..300, with a negative control. slide.adc is the only
script calling sort.

**A:** the authored Exit never ran MusicRoot.onFinalize (apply_bgm_playlist /
apply_slide_playlist, slide.ad L245..252) because the slide project stays cached;
edits were applied only on the next visit. Leaving the project now finalizes its
page stack once, as the PS2 project destruction does, before the playlists are
stored. outside_menu, album_pcm_bridge, playlist_cache and slideshow_playlist pass.

### Jukebox folder integration and map cursor (19 October 2026)

**P:** supersedes the management UI described below. The original Jukebox Library
scans Music beside the active options file. Racing/Slideshow folders and native
list/player/selection/removal/shuffle remain visible; no added Playlists or tools
screen and no music F9 entry. Editing mixed lists opts in; Classic
restores original routing. Registry and source files are retained. `custom_music_ui`
exercises two entries, nested Unicode files, both native append controls, native
remove/readd/play/shuffle/Exit, PCM, no duplicated tracks or undefined methods,
and absence of management widgets. Windows tested; mobile scanner unvalidated.

**P:** GT Mode map edge cursor reuses the Arcade pointer spring, points in eight
directions only along axes that can pan, resets at limits/disabled pan. Existing
edge-pan/smoothing options apply. `desktop_pointer` checks all eight angles,
bounds and disabled pan; negative control and map navigation regressions retained.
No claim that mouse controls existed in PS2 GT4.

### Unified original Jukebox and mixed playlists (18 October 2026)

**P:** the natural GT Mode -> Jukebox project entry opens the authored
slide/MusicRoot, with one Library for disc and user songs and the original Racing
and Slideshow folders/player/list controls. No F9 entry, launcher editor, Disc/Local
split or separate user-song browser. Playlists extends the sidebar with import and
named-list management. Lists can mix both sources and share songs independently.
Off/Classic restores original audio routing; extended IDs never enter PS2 blobs.

Evidence: custom_music_ui enters the original project without F9, imports Unicode
files/folder, uses native Append1/Append2, Trash, Play and Shuffle, verifies mixed
M3U output and slideshow membership, persistence, actual PCM and source preservation.
No undefined/call-of-nil script errors are allowed. Screenshot reviewed locally.
custom_music_mixed verifies local WAV -> disc-song transition, 8192 frames byte-exact
against gt4_pcm_music_render, plus mixed M3U roundtrip. custom_music (+ negative),
formats, launcher_audio (+ negative), album_pcm_bridge (+ negative) and menu_navigation
cover adjacent paths. These are extension/integration tests, not a global PS2 grade.

**A (native method translation, EE oracle pending):** missing Adhoc Array.move and
Array.erase supplied from local disassembly: registration 002EE834 -> wrapper
002ED8C0 -> 002F0138 (retain value, directional shifts, insert), and 002EE984 ->
002EE0C8 -> 002F02E8 (erase one in-range index, close gap; invalid index no-op).
adhoc_array_move tests every source/destination at sizes 1..20 and erase boundaries;
negative control rejects swap semantics. The authored Jukebox shuffle/removal now run
without missing-method errors. Automatic EE instruction replay remains unproved.

Windows validated. The extension RNG state is independent; no matching PS2 shuffle
sequence is claimed. See [Custom music](notes/CUSTOM_MUSIC.md) for limits.

### Optional external music library (18 October 2026)

**P:** independent, default-off PC extension: local playlists per menu/race/replay/
slideshow/launcher context, edit/order/shuffle/preview, M3U import/export. The initial standalone screen was superseded by the
authored Jukebox adaptation above. References preserve source files; no player save
changes. Classic/Off restores original audio routing. Streaming decode is outside
audio callbacks; race pause silences without consuming the external PCM; original
race cue timing continues. Original jingles remain authored sources.

Evidence: custom_music (+ negative), custom_music_formats (MP3/FLAC/OGG/M4A),
custom_music_ui (real editor events, Unicode paths, folder import, PCM preview,
export and source preservation); launcher_audio, album_pcm_bridge, race_music_pause,
launcher_quality and desktop_options_store with negative controls. Editor screenshot
reviewed locally. This is not a PS2 fidelity claim. Windows tested; other platform
pickers, embedded tags, media bundling and seeking remain pending. See
[implementation and limits](notes/CUSTOM_MUSIC.md).

### Post-disc guide and graphics comparison (18 October 2026)

**P:** optional first-visit guide after disc selection, redone in the launcher's
composition (OPTIONS panel, dealer metal band, disc OptionRoot icons, font and
cursor). Preset, per-option graphics comparisons and menu options are chosen as a
draft and written only by DONE/OPTIONS through the Options' setters; details in
OPTIONS. The Gran Turismo Skyline GT-R on the Tsukuba grid through the race
renderer, no race started, no player car replaced.

`tests/ui/launcher_guide.py` drives every page with real SDL key events from a
script (MOONSTUDIO_GUIDE_SCRIPT) in the hidden fixture: five disc icons, course
installed, screenshots of each page and of eleven comparisons, full-screen Tab
flips that change the scene (preset and Lighting), Escape leaving option bytes and
player files unchanged, and DONE writing exactly texture_filtering=2 and
aspect_ratio=1 (two changes, untouched options kept). OpenGL and Vulkan pass.
Art is subject to user review; this is not an original-game fidelity grade. The
comparison covers this scene, not every course or race.
Visible run with the user's settings (1920x1080, SSAA 200%, HDR, volumetric rays):
navigating the Graphics rows keeps frames under 40 ms except HDR target rebuilds
(up to 51 ms); before, each comparison frame took 65-115 ms.

Jukebox with Custom Music off (19 October 2026, P fix): custom_music_ui runs the
Jukebox with `audio.custom_music=0` beside a Music folder with songs and expects the
native disc-only entry, no folder scan. Racing/Slideshow repaired to their named
lists checked on a copy of a player file whose races pointed at an empty list
(Racing 36 songs, Slideshow 15). SOUND rows: screenshot reviewed. Not verified: the
real GT Mode entry with the player's save.

Options column (19 October 2026, P): `tests/ui/launcher_options_guide.py` opens
QUALITY on Lighting in the hidden fixture: "comparison Lighting" logged, the course in
the column's picture, option bytes unchanged; the negative (VIDEO, its own pictures)
fails. Screenshots of every tab's rows reviewed locally (the clip at its first and last
frames through MOONSTUDIO_CLIP_FRAME); launcher, launcher_guide_ui and
pc_settings_section tests pass. Not verified: the divider
dragged with a real mouse, and frame times in a visible window.

Launcher Scene (19 October 2026, P): `tests/ui/launcher_scene.py` starts the launcher
with scene=tsukuba in launcher.ini: "studio: scene tsukuba" logged and the course in the
window left of the car; the negative (scene empty, the room) fails. Screenshots of the car
standing in Gion, Times Square, Shibuya, Grand Canyon, San Marco, Tsukuba and Suzuka
reviewed locally (home framing). Not verified: every other place on the list, the
cinematic shots and dock framings in each place, the free rig, and the Vulkan renderer.
Theme and Light Colour renders, the preset buttons and a 640x400 window: screenshots
reviewed locally. After the road-following fix, the home framing in all 35 scenes reviewed
locally (Laguna Seca, Monte Carlo and Seattle no longer in a wall). Not verified: the car
tilt against the game's own physics pose on a slope.

Launcher prints (19 October 2026, P): `tests/ui/launcher_prints.py` focuses INTERFACE >
Desktop Input: both prints taken by hidden runs of the launcher's own executable, the
column's box full of the game's menu, `desktop-input-0/1.png` and `version` in
launcher-prints/, and a second run reads them from the cache instead of printing again;
the negative (Map Edge Pan, no print) fails. Screenshots of the six print rows reviewed
locally. Not verified: Linux (fork/exec path) and a disc path with non-ASCII characters.

PC Settings section (19 October 2026, P): `tests/ui/pc_settings_section.py` opens
each of the five tiles from the game's Options with pad input and expects its page
("video settings: page Music", ...), the Screen tile on Screen Settings, and back
from Light & Shadow focused on that tile (the script itself refocuses Screen); the
negative (Music expected as Screen) fails. quality_settings, personal_presets_ui
open their pages through MOON_SETTINGS_PAGE. Screenshots of the grid and of every
page reviewed locally. Not verified: the mouse on the new tiles, Android and Switch
(unchanged by design), the Japanese/European discs' labels.

Controller Tiles (19 October 2026, P): pc_settings_section walks to the Controllers row
with pad input; By Player ends the row at Steering (right goes on to Exit) and
Controller 1P opens the button config page; Classic keeps the disc's ten tiles.
Screenshots of both reviewed. Not verified: a real wheel on either port, and the
wheel models' own button pages (on the PC they show no keys; hidden in By Player).

Graphics Swap Key (P): a real F7 event injected by MOON_TEST_GRAPHICS_SWAP_AFTER in a
Tsukuba drive logs "graphics swap: classic" and the capture shows the Classic picture
against the unswapped run at the same time; leaving the race restores the player's
graphics. Keyboard only; no pad binding yet.

### Launcher audio styling (18 October 2026)

**P:** Audio/Music quick controls use the studio header typography, secondary
shortcut labels and thin state rules instead of pale rectangular tiles. The
expanded mixer uses the OPTIONS charcoal surface and top rule in both themes.
Existing hit areas, F8/M shortcuts, volume and mute behavior are unchanged.
Focused launcher_mixer and launcher_mixer_negative pass. Visual captures:
build/mixer-style.png and build/mixer-style-dark.png. Local candidate:
build/Release/MoonEngine-audio-style.exe; main executable was open during link.

### Complete player package with shared media (18 October 2026)

**P:** the save screen now exports one `.moonplayer` package: selected native
player plus all shared photos, replays/ghosts, films, their lists, auxiliary
files and extracted legacy-import data. This supersedes the earlier profile-only
export limitation. Existing native/PS2 imports and the independent transfer-tools
option remain available. Relative media names and bytes are retained; no host
paths or disc assets are included.

Focused automatic tests compare the complete serialized profile payload and all
fixture media, then read back the replay, ghost and replay playlist. Corrupt
members/profiles, unsupported manifests, traversal/alias/duplicate names and
existing-library conflicts fail before changing the player. Both initial-write
and activation-write failures roll back newly promoted media; the previous
player remains active. save_package_negative deliberately corrupts a photo and
fails. profile_storage regressions also pass. A real save-screen Export/Import
fixture validates the package from another save directory, compares its complete
member set and media bytes, and checks that original scripts still run.
Evidence: build/save-package-ui-full and tests/ui/save_package*. Native file
picker interaction is injected in the hidden-window UI test, not automated.
Full multi-file crash recovery and on-device file-picking remain unverified.
The final build replaced build/Release/MoonEngine.exe after it became available.
The UI fixture also covers accented media/package filenames.

### Optional native player-profile transfer (18 October 2026)

**P:** native `.ogt4` profile export/import in the existing save screen on
Windows. Independently switchable `interface.save_transfer_tools`; Classic
restores the previous footer. The native format is unchanged. Imports validate
the selected envelope/state/name before creating an empty slot; occupied or
corrupted imports preserve the current player. Export preserves active/dirty
state and rejects destinations under the live Windows save directory.
The existing PS2 import route remains available. Separate photos/replays/films
are not bundled, and the UI labels that limitation.

Focused profile_storage passes a byte-identical payload roundtrip with garage
and career state, occupied/corrupt-file rejection and export state/destination
checks. save_transfer_negative changes one byte and fails as intended; existing
profile_storage_negative and launcher_quality cases pass. Captures verify the
new and restored footer (build/save-transfer-proof). After adding the settings
row, explicit key navigation fixes a diagonal jump from Save Preset to OK;
the real preset UI Cancel/Apply/export fixture passes again on the candidate
build (build/save-transfer-presets-ui-fixed). Native picker automation and
on-device file selection remain pending. The currently open main exe prevents
replacement; candidate build/Release/MoonEngine-save-transfer.exe is linked.

### Optional personal preset files (18 October 2026)

**P:** shared named presentation/input-menu preset files for the original
Screen Settings extension and official studio launcher. `interface.preset_tools`
is independently switchable; Classic hides the Load/Save actions. File export
omits host display/API, disc/save paths and physical-device bindings. Protected
built-in names, bounded versioned parsing and transactional draft imports keep
invalid files and Cancel from changing active preferences. Personal names and
modified state persist. The existing tab labels are inset and separated; this
is a PC layout adjustment, not a PS2 fidelity claim.

Six focused store/launcher/preset cases pass, including deliberate negatives.
Real ScreenRoot tests check Load/Save, Cancel, Apply and exported values with an
injected host-file selection. Captures were checked for legibility and tab gaps.
Native picker automation, dedicated library management and Switch/Android file
selection remain pending; the broader settings, music, saves and input redesign
is not claimed complete. The positive UI fixture passes and its invalid-value
negative fails as required. Evidence: build/personal-presets-proof,
build/personal-presets-ui-final and build/personal-presets-ui-negative-final.
The validated build also replaced build/Release/MoonEngine.exe once unlocked.

### Directional menu comparison (18 October 2026)

**C, comparator and projection:** menu_navigation_oracle executes 0026F2B8,
0026F0A8, 0026F630 and original 0057D008/0057D118 angle code. Forty thousand
candidate-pair comparisons and forty thousand edge-midpoint projections
match for four directions, clip angles 0/20/45/50, front/back origins,
wrap on/off, group priorities and current-item/tie cases. The negative
inverts the first comparison and fails. No original math is stubbed.

**A, collection/integration:** RootWindow defaults from 00232F0C..54 and
properties read by 00233E38 now select that comparison in native directional
focus. Authored source/target rectangles stay distinct; wrap uses the
original second-pass shift of 10000 (0026F5A0). SelectBox retains its separate
ordered-list handlers. The disc-screen test checks quick-arcade default
wrap and option/OptionRoot explicit zero; its negative fails. Existing
menu_navigation passes with a corrected fixture specifying both source and
target regions. Hierarchical collection is checked separately below against
0026F9E0; class defaults and exact heap-address ties remain audit debt.
The whole navigator is not declared exact.

Before integration, options_lifecycle failed option_pending==index;
gtmode_setting_race and gtmode_setting_catalog_menu timed out at 60 seconds
(the catalog also logged focus_index==lo+1). Local baseline:
build/ui-audit-baseline.log. Seven focused tests pass after collection integration.

### World-space sun volume and ambient depth (18 October 2026)

**P:** optional 32-step world-space scattering through the existing local light
map, independent screen-space ambient occlusion, hemispherical sky fill and
near-caster occlusion of reflection overlays. Opaque course geometry blocks
full sunlight. New controls persist in shared settings/preset files; selecting
them enables HDR/Daylight. Classic retains its route. Artistic parameters and
local-map, transparency, off-screen, weather and unchanged-mesh limitations are
[documented](RENDERER.md#world-space-sun-volume-and-ambient-depth-18-october-2026).

GPU/store/shadow/material regression: 34 focused cases pass, including explicit
volume/AO/hemisphere negatives. Two Options cases also pass. Controlled volume
lit/blocked is 155/148 (disabled negative 149/149); AO corner darkens 512 pixels
(disabled zero). Real-disc AO/volume pairs preserve all 33,554,432 simulation bytes; Classic
images change zero pixels on both APIs in the F-150/Tsukuba fixture.
Source geometry/silhouettes remain unchanged. Evidence:
build/volume-proof. No full-suite run, commit or push.

### Edge AA and solar post correction (18 October 2026)

**P:** optional Edge AA after tone mapping on both PC APIs; Enhanced default,
Classic unchanged. Independent shared preference and personal-preset roundtrip.
The original HDR route's single sampling and a saved FSR reduction explain
strong aliasing without a post filter. Depth writes are restored before scene
clear; mirrors no longer overwrite the main solar projection, stale menu
projection is cleared, and split-screen shafts remain disabled pending separate
viewport buffers. The radial integration uses artistic 48/5/0.3 parameters.

Sixteen focused GPU/store/menu cases pass, including explicit disabled negatives:
476 transition pixels versus zero; flat interior 255/255; open shafts 199 versus
blocked/off 149. Real-disc AA/rays pairs preserve every simulation byte;
Classic images have zero changed pixels in the F-150/Tsukuba fixture on both
APIs. Temporal AA, full volumetric transport, off-screen occlusion and weather
remain pending. [Evidence and limits](RENDERER.md#scene-edge-filtering-and-solar-post-fixes-18-october-2026),
build/aa-rays-proof.

### Daylight contact shadow correction (18 October 2026)

**P:** opaque daylight casters no longer inherit the old blended-shadow alpha.
Filtered near-caster occlusion also reduces sky fill, which previously washed
out contact after the hard authored mesh was removed. Two-metre decay/0.8
strength are intentional presentation parameters; this directional-map
approximation is not full AO. Six focused GPU cases pass, including separate
contact negatives: 38 darkened fixture pixels, minimum brightness 51.4%, versus
zero/100% when disabled. Classic/non-daylight attenuation remains unchanged.
F-150/Tsukuba front-grid crop mean RGB: 42.53 -> 32.05 on both APIs.
Classic capture differences: zero pixels; paired simulation states match in
all 33,554,432 bytes. Twelve related lighting/daylight cases also pass.
Evidence and limits: [renderer](RENDERER.md#daylight-contact-regression-18-october-2026),
build/contact-shadow-proof.

### Optional daytime atmosphere and HDR scene (18 October 2026)

**P:** Daylight/HDR/Sun Rays and solar hour/cycle preferences on both PC APIs.
Enhanced has a procedural clear-day sky shared with reflection faces, solar
illumination, nearby course shadow casting, a float main scene and GPU
screen-space depth-occluded shafts. Classic retains the authored path.
The hard authored contact blot is suppressed only under active depth maps;
comparison interpolation replaces snapped penumbra taps. Reflection targets
retain their encoded LDR convention and avoid accidental HDR conversion.

Solar hours 07..17 use a fixed 35-degree/equinox presentation scenario;
Fixed/Real Time/60x read the existing simulation frame count. The eight/four
sample atmosphere is reduced single scattering. Numerical atmosphere,
lighting and post-processing parameters are intentional PC presentation
choices, not recovered PS2 values. Clouds/rain/night, whole-course cascades,
multiple scattering, HDR10, full HDR reflection maps and removal of baked
track lighting remain pending. Split/mirror shaft placement is unverified.

Focused validation passes 34 renderer/store cases and two options-menu cases,
including deliberate negatives. HDR-to-RGBA8 snapshot conversion reads
64/128/255 for float input 0.25/0.5/2.

Ford GT/Tsukuba real-disc regression: Original has zero changed pixels
against the pre-change renderer on both APIs. Daylight time and replay pairs
retain all 33,554,432 simulation bytes, while their rendered images change.
Evidence: build/daylight-proof/comparison.json; this is scoped PC regression
evidence, not an E grade for PS2 imagery.

GPU fixtures on each API retain float headroom (2/4 -> 246/252), isolate
nested LDR targets, measure real depth occlusion (open shafts 185 versus
blocked/off 149), check procedural zenith/time response and exact Original
restoration, with dedicated disabled negatives. Continuous penumbra fixtures
resolve 19/19/16 intermediate levels for 512/1024/2048 and retain the prior
flat/sloped/cutout regressions. The historical local-shadow description below
is superseded for covered mapped cars. See [scope and current evidence](RENDERER.md#optional-daytime-atmosphere-and-hdr-scene).

### Optional local shadow maps and modern lighting (18 October 2026)

**P:** independent graphics.shadow_maps (Off/512/1024/2048) and
graphics.modern_lighting (Original/Modern) on both PC renderers. Classic
keeps both original paths; Enhanced uses 1024 and Modern. Local car body,
fitted-wing and wheel depth shadows receive on course geometry and the car,
with native-alpha cutouts and sixteen weighted taps. Flat/sloped self-surface
regressions catch triangular boundary artifacts; the repaired derivative order
reduces the reproducer from 20 false-shadow pixels to zero. Glass is excluded
from opaque casting; reflection layers do not cast twice.

Modern uses GGX, separate sky ambient fill, elevation-dependent daylight colour
and filmic SDR highlights. Roughness/F0 conversion and atmospheric optical
parameters are deliberate approximations, documented in the renderer scope.
This does not replace baked track lighting or implement full-scene HDR, global
illumination, a new time of day, world/driver casting or glass transmission.

Each API passes 120 independent CPU/GPU BRDF cases (maximum error 1 byte/channel,
allowed 2), opacity/unlit invariance, exact Original restoration and deliberate
negative checks. Shadow cases cover three resolutions, soft edges, moving/union
casting, raised/sloped receivers, self-shadow rejection and alpha-cutout energy.
All 28 distinct focused render/store/menu tests pass, including deliberate
negatives. Previous-build Original race images have zero changed pixels on
both APIs; all 33554432 simulation bytes agree in race/replay comparisons.
Race/replay image and complete 32-MiB state comparisons are scoped
in build/shadow-map-proof/comparison.json; timing is a local sample, not an FPS
promise. See [shadow implementation](RENDERER.md#local-car-depth-shadows-on-pc)
and [lighting limits](RENDERER.md#optional-modern-race-lighting).

### A-Spec tyre-condition HUD (18 October 2026)

**C, scoped integration:** the live wheel-condition colours now reach both
A-Spec panels, including the split player bridge, instead of B-Spec only.
The original simple/onboard draw gates (003A7968 / 003A88F4) use driving
state; anchors come from 003A7F24 (58,464), 003A92A4 (26,465), and
003A7CE4 for split (32,464). Wheel-layout mapping and signed condition
colours retain 003446D8 / 003677F8; inactive heat/wear returns black.
The physics still chooses heat over wear for the encoded gauge when both
are active; this change does not alter simulation. B-Spec AI-watch black
indicators remain unchanged.

Focused bridge checks cover ten signed-colour boundaries, four wheel
layouts, both players and inactive/wear/heat flags. Draw checks require
four tyre blocks at the original simple/onboard coordinates and none
outside driving state. Four existing focused tests pass; two additional
omission negatives fail as required (build/aspec-tires-*-negative.log).
PC build: build/Release/MoonEngine-aspec-tires.exe, because the running
main executable prevented replacement. Full PS2 image comparison and
on-device Switch/Android validation remain pending.

### Optional generated reflection quality (18 October 2026)

**P:** graphics.reflection_quality selects Original 128, High 256 or Ultra
512 on OpenGL and Vulkan, independently of other PC effects. Classic keeps
128; Enhanced selects 256. The race/replay uses the existing six views,
position, cadence and remap at the selected resolution. Generated studio
maps also resize; their source faces remain 448 pixels. High/Ultra studio
composition uses the GPU when resident reflections are enabled, with CPU
fallback. Source environment textures, materials and physics are unchanged.

The synthetic angular-stripe fixture measures spatial standard deviation
0.000 / 62.052 / 112.105 for 128 / 256 / 512 on each API. It checks all
sizes, invalid-mode fallback and byte-exact Original restoration; deliberately
using 128 for Ultra makes its negative fail. Positive/negative shared-store
and real-menu checks cover persistence/presets; seven Rendering rows retain
28-pixel spacing. Existing material/shadow/filter cases are area regressions; 24 focused
tests passed including the two real-menu cases.

Race Original images match the pre-change build with zero changed pixels
on both APIs. Within-instance race and studio cycles 128->256->512->128
restore Original exactly. Same-time studio comparison also keeps the upper
wall region unchanged in both larger modes; separate-process studio captures
can differ in their asynchronous-load timeline, so do not treat their raw
pixel difference as a quality metric. The panel camera basis is preserved
across rebuilds. Race and replay comparisons retain all 33554432 simulation
bytes between levels. The forced CPU High path loads and renders on both APIs. Studio CPU
quality cycles also restore Original exactly on each API.
Evidence: build/reflection-quality-proof/comparison.json, timing-replay.json,
*-studio-cycle logs/images and GPU fixture output; menu: build/pc-quality-settings.

In the warmed 180-frame Vulkan sample, GPU time is 5.716 / 5.736 / 6.066 ms
and total frame time 16.362 / 16.777 / 17.210 ms for Original / High / Ultra.
OpenGL total is 14.546 / 14.581 / 14.530 ms, without GPU timestamps. These
are local samples, not an FPS gain claim. All levels keep 216757 draws /
7912611 triangles and zero readbacks with resident reflections; the forced
CPU High sample intentionally reads back six faces per frame. Larger targets
cost storage/fill work. No roughness filtering, new reflected objects,
refraction or new claim of full PS2 reflection-image equivalence is included.

### Optional per-pixel material shading (18 October 2026)

**P:** graphics.material_shading selects Original / Per-Pixel on OpenGL
and Vulkan. Classic retains vertex lighting and interpolated environment
coordinates. Per-Pixel evaluates the same authored coefficients with
interpolated normals and a fragment view ray; reflection colour, pass
separation, alpha tests and masks remain in use. No inferred paint/glass/
rubber profiles, new lights or physical material parameters are introduced.

Original source checked: func_0049EB90 +0x84..0x94 isolates specular colours;
func_0049E918 +0x100..0x150 selects the race's exponent program. The existing
model reader retains MDLS ambient/diffuse/specular/shine fields. Direct
read-only disc measurements find 14/18 material rows (menu/race) on Ka,
8/10 on Ford GT and 7/8 on M5, with different authored coefficients and
powers. Local numbers/provenance: build/material-shading-proof/disc-materials.json.

Sixty analytical surface cases per API have maximum measured byte error 0
(allowance 2). Gradient-reflection/strength, zero-specular, separate-pass,
flat/unlit invariance, opacity and byte-exact restoration checks pass.
The curved-normal fixture changes its centre from 0 to 127; deliberately
retaining Original makes the negative fail. Twenty focused tests pass:
material shading and negatives, existing materials/shadows/filtering,
shared presets/store and real-menu persistence/cancellation on both APIs.
Six Rendering rows keep the 28-pixel spacing and four-page layout.

Old-build versus current Original Tsukuba captures have zero changed pixels
on each API. Per-Pixel changes 22406 pixels (OpenGL) / 22402 (Vulkan).
Ka/Ford GT/M5, recorded replay and warmed timing probes preserve all
33554432 simulation bytes between modes on both APIs. Captures were
inspected locally. GPU readbacks, draw and triangle counts do not increase
in the warmed probe. Vulkan's 360-frame GPU sample is 7.885 ms Original /
8.095 ms Per-Pixel; total frame time is 20.910/20.979 ms. OpenGL total is
14.005/13.120 ms, without GPU timestamps. These are local samples, not an
FPS improvement claim. Evidence: build/material-shading-proof/comparison.json,
scenarios.json and the corresponding logs; UI: build/pc-quality-settings.

Limits: low-resolution environments and authored normals/geometry remain;
no roughness-filtered reflections, refraction or complete physically based
material model. Complete car-image equivalence to PS2 remains unproved;
this P feature does not upgrade the older aggregate material fidelity grade.

### Optional geometric car shadows (18 October 2026)

**P:** Original / Projected car-shadow choice on both PC APIs, independent
of texture sampling. Classic retains the existing cast volume. Projected
unions current body/wing and posed wheel geometry on the GPU, softens its
edge, depth-tests the local ground receiver and retains the contact blot.
Authored cast alpha and existing course/pose inputs determine opacity,
tint and direction. No simulation changes.

Four focused renderer cases include a deliberate omitted-pass negative;
positives measure 180 projected and 116 soft-edge pixels on each API and
verify union, zero opacity, moving geometry, foreground occlusion and exact
restoration. Shared settings and positive/negative real-menu tests cover
persistence and presets. Prior-build versus current Original Tsukuba images
have zero changed pixels per API; the enabled image changes 4590 pixels.
Race and recorded-replay state comparisons match all 33554432 bytes per API.
Evidence: build/car-shadow-proof/comparison.json and timing-replay.json.
The 360-frame warmed timing sample measured total frame time 13.338/13.191 ms
(OpenGL Original/Projected) and 16.410/17.235 ms (Vulkan). These are local
samples, not a universal speed claim; the effect adds GPU work.

Limits: local receiver plane only; no self-shadowing, wall receivers,
complete curved-terrain conformity, glass transmission or alpha cutouts.
These are future shadow-map/material work, not E/C claims. Desktop video
settings are separated into four pages to avoid overlapping rows; layout
is checked visually, while option behaviour has automatic coverage.

### Optional PC texture filtering (18 October 2026)

**P:** independent Original / Trilinear / Anisotropic 4x, 8x and 16x
selection on OpenGL and Vulkan. Classic selects Original; Enhanced selects
8x. The hardware limit caps anisotropy; unavailable anisotropy falls back
to trilinear. GS region clamp/repeat retains the existing manual sampling.
No physics, colour grading or lighting changes are included.

The elongated-footprint fixture measures contrast 0 for trilinear and
238 (OpenGL) / 223 (Vulkan) for 16x. It checks byte-exact restoration of
Original within one process and unchanged region-clamp pixels. Its negative
substitutes Original for anisotropic sampling and fails. Four renderer and
two settings-store cases pass; positive/negative UI checks cover both APIs.
On the Tsukuba probe, old-build versus new Original captures have zero
changed channels on each API. Enhanced and Original simulation dumps are
byte-identical (32 MiB per backend). These are scoped PC regression proofs,
not a full PS2-image fidelity claim or a measured frame-rate improvement.
Evidence: build/texture-filter-proof and build/pc-quality-settings.

### Functional mod-car export (18 October 2026)

**P:** SDK exposes authored rim/tyre GLBs, per-mesh released/pressed brake
branches and per-label engine/exhaust WAV banks. The native CAR4 writer
retains original callback-0 lamp tags and callback-11..14 wheel frames;
KHR_materials_unlit stays an explicit material choice. Functional mesh
roundtrip and GLB export have deliberate corruption/missing/duplicate
negatives; original brake-branch/material regression cases pass.
Ten focused metadata/export/material/branch tests and two original audio-clock
regressions pass (each area includes a negative). Five rig and two sound Rust
cases pass; Node specs/functions/pictures and syntax checks pass.
Own camera inheritance is independent of wheel inheritance. Native assembly
checks four authored wheels/1024 triangles plus 35561 body triangles.
Custom JSON banks compile to the existing ENGN/SPU decoder/controller;
a 2800-frame sine fixture measures 56.32 dB SNR against the source WAV.
Silent Vulkan racing and OpenGL replay load both per-car banks (1/3/1
engine/road/exhaust layers), while donor asset rows and sound-number files
are untouched. Evidence: build/sdk-functions-proof; SDK Rust/Node focused
tests cover persistence, import, invalid WAV/loop/ranges and rig binding.
Total custom-car independence is unfinished: donor-free specification
bundles, event sounds, steering/pilot/wing and arbitrary animations remain
outside this change. See the SDK runbook for format and exact scope.

### User-operated SDK export verified (18 October 2026)

**P** authoring workflow: user completed export in moonsdk.exe. Resulting
moon_banana.zip (40087118 bytes) passes CRC validation, contains both Tex1
images (6100/64100 bytes), and all three models match the project build
(25 cameras at 0x488, only rig slot 6 authored, cabin Drop, compression .35).
Images match an independent repeated conversion of the fresh project PNGs.
This closes the pending interactive export artifact check; no automated UI
click coverage or original-game image equivalence is claimed.

### Vulkan preview shutdown (18 October 2026)

**P:** Context destruction now calls close while all members remain alive.
Command-line preview returns without explicit video shutdown; previously
reverse member destruction destroyed target_cache before texture image
deleters consulted it, producing STATUS_HEAP_CORRUPTION (0xC0000374).
Explicit shutdown remains valid because close clears the instance handle.
**C** rendering regression: explicit/implicit close GL/Vulkan fixtures pass
with the existing two-channel-level tolerance; deliberately corrupted
fixtures fail under WILL_FAIL. Four focused CTest cases pass. Three real
Banana Vulkan picture captures exit zero and produce identical PNG pairs;
white pixels match the pre-fix capture exactly. SDK capture again uses
the selected renderer; the temporary OpenGL restriction is removed.
Evidence: build/sdk-picture-proof/render-vk-fixed-{0,1,2}.log and PNGs.

### SDK automatic package pictures (18 October 2026)

**P:** Export renders missing catalogue PNG inputs before packing, reusing
Make Pictures; failed rendering/conversion stops export. Offline capture
uses the selected renderer after the shutdown correction above. Initial OpenGL proof:
OpenGL exit 0, both PNGs, Tex1 outputs 6100/64100 bytes (50x30/160x100).
Focused JavaScript tests cover automatic/manual/mixed sources and renderer
and converter failures. This is pipeline validation, not original-image E.
The initially failing Vulkan capture (0xC0000374) is corrected and verified
in the shutdown entry above. Evidence: build/sdk-picture-proof.

### Authored Banana roof camera and 35% export preset (18 October 2026)

**P:** Custom model camera 6 is authored in the rig, not patched into game
camera logic. The 35% gltfpack export (`-si .35 -sv -kn -noq`) removes 59
interior parts with `--no-interior`: 35561 body triangles, 7232 donor wheel
triangles, 42793 total; assembly reports zero unsupported/nonfinite parts.
The measured source bounds, facing/centering/scale and axle shift put the
roof at 1.459313 m. The authored eye is 1.559313 m, with 0.1-m clearance
chosen from the existing renderer near plane (race_game.c camera path).
This is an intentional custom-car placement, not an original-camera E claim.
**C** geometry check: eye/transform agreement within 1e-5 m, and an explicit
old-height negative fails clearance. **E** retained-data scope: the other
24 camera rows remain byte-identical in all three slots (25 @ 0x488).
A silent 720-frame race passes; before/after OpenGL images were inspected
(**V**, custom framing only). Local evidence: build/sdk-interior-proof.
SDK `inherit: true` camera slots emit no rig line; editing marks them
explicit again. Four focused Rust rig tests and both camera/sound CTest
cases pass, including invalid-pose and deliberate corruption cases; Node
serialization/syntax checks and cargo build pass. Prepared SDK project:
Banana-camera-check.moonproj, using the full source with cabin Drop and
compression .35. Interactive export through moonsdk.exe still needs the
manual UI confirmation: this execution session has no app-control tool.

### Independent mod vehicle specifications (18 October 2026)

**P:** Optional weight, torque/RPM/redline, drivetrain, gear count/final/ratios
author new mod rows; no physics source changes. Data **E** scope: mass at
CHASSIS +6/+12, ENGINE curve/RPM/catalogue fields, DRIVETRAIN +0x10 and GEAR
ratios/count/final are measured through both native readers and pristine
Ka/M5/NSX/BTR/Lancer rows. Stock four-part rows and assembly surveys remain
byte-identical with/without the package. Native integration **C**: example
150.002 PS / 200.018 Nm (tolerances .5 PS/.099 Nm from table quantization),
4500-RPM torque peak, 7500 redline, 1100 kg; the real Arcade CarRoot emits
150PS/7300rpm and 200.02NM/4500rpm. A local OpenGL UI capture was inspected;
full showroom rendering is outside that harness. Five drivetrain choices
complete 1200 race ticks; FR/six-gear example reads final 3.900 in runtime
parameters, with 1170-kg mass after the driver addition. Four focused CTest
cases pass, including camera/mass corruption WILL_FAIL negatives and an
explicit missing-UI-payload rejection. SDK serializer/restore/flags test,
node syntax check and cargo build pass. A joint PS/torque override authors
a documented two-segment curve (P); no claim of real engine curve fidelity.
Interactive SDK export and custom interior-camera framing remain pending.
Installed folder/adjacent ZIP camera headers are checked as 25 @ 0x488;
default loader trace selects the folder named in mods.ini. See
[package runbook](vulcan4/SDK-MAKING-A-CAR.md).

### Mod camera and independent sound authoring (18 October 2026)

**P:** SDK rig camera overrides and independent sound-donor selection are mod
authoring features; retail physics is unchanged. Narrow **E** data proof:
`mod_car_rig_sound` compares all 800 Ka camera-table bytes (25 @ `0x488`),
checks that sound changes only ENGINE `+0x18..19` (37701 to 52601), and compares
the untouched source ENGINE rows and Ka survey with/without the package.
No-donor/no-rig output matches the previous converter SHA-256 exactly.
Authored rotation matrices pass an independent synthetic comparison within
`2e-5`; this is a transform contract, not an original-game oracle. The
deliberate camera corruption fails under the registered WILL_FAIL negative.
Both focused CTest cases and three SDK rig tests pass. A silent PC race probe
renders 720 frames switching modes 1/0/6 and opens sound bank 52601.
All-camera framing, audible equivalence and interactive SDK export remain
unverified; unchanged donor interior eyes can intersect a different body.

### Championship pre-race score identity (18 October 2026)

**C:** Pre-race championship summaries now map persistent driver scores into
actual grid slots, as the car/name rows already do. Both initial/resumed
championship entry and the next-round path use this mapping. Evidence:
00119918 constructs grid-ordered quick-work rows; 003EEFE0/003EF1E8 retain
entry identities for championship scores. Previously a player in slot six
received another driver's displayed score, while the player's score appeared
on the first car. This changes presentation association, not scoring or saves.
`championship_grid_points` checks all six rows over three cumulative rounds
with the player in slot six; its negative restores the old raw copy and fails.
Full championship screen raster equivalence is not claimed.

### Empty Home garage preview (18 October 2026)

**C:** Re-entering the cached GT Mode Home without a selected car clears
its stale CarFace model before the authored initialization. gtmode.ad
HomeRoot L3946..3965 gates car loading on hasRidingCar, so the native
cached widget previously retained the sold car when that branch was skipped.
The focused Home garage test exercises populated Home, deletion of all cars,
empty Home, and purchase/selection of another model in the same project.
The negative reinstates the stale model and must fail. Ownership, sale prices
and save format are unchanged; full PS2 raster equivalence is not claimed.

### Playback controller motors (18 October 2026)

**C (host-output regression):** SDL controller rumble now shares the existing
force-feedback playback/pause/photo gate, with an explicit demonstration gate.
Disabled output sends zero to cancel the previous pulse; race shutdown clears
both controllers. The underlying 00353EE8 output bytes and simulation are
unchanged. `replay_rumble` intercepts the actual host adapter and checks P1/P2
live amplitudes, replay/demo/pause/photo silence, unrelated-car rejection and
shutdown cancellation; the negative injects leaked replay vibration and fails.
This proves host output policy, not physical controller timing or a complete
comparison with the original game's motor commands.

### First-run welcome dialogue (18 October 2026)

**P (host profile setup):** The four welcome cards are replaced by successive
Yes/No questions for profile creation, PS2 save import, existing profiles and
guest play. The frame and button hierarchy come from the player's authored
cursor/ConfirmDialog project, retained by the destination UI with its GPB
resources; wording and profile flow remain host adaptations. No returns to
the next question, declining guest play returns to creation, and leaving
profile selection returns to the welcome dialogue. Name entry cancellation,
creation, existing-player boot and all question routes pass the 11-case
first-run application smoke test with disposable saves. Welcome and guest
screens were visually inspected. The import parser is unchanged; an actual
file-picker/import transaction is outside this smoke test. No console screen
or exact original onboarding-flow equivalence is claimed.

### Visible native profile transactions and restart persistence (18 October 2026)

**C, scoped file-I/O integration:** the game's bootstrap now probes only the
active index before video initialization. It reads and validates the actual
profile on the authored boot LoadRoot with the load face active, following
boot.ad L190..198. The temporary access menu is not ticked, so its queued
boot script cannot launch another load. The host launcher/legal sequence
and native profile files remain adaptations; this is not a claim of complete
PS2 boot equivalence.

Create, rename and GT4-import commits now use the same presented immutable
writer as Auto Save. Profile activation-index writes are included in those
transactions and in presented loads. Legacy inheritance uses the presented
save path too. Final shutdown persistence uses a separate access screen,
without drawing already released race/photo resources. The original menu
scripts still choose their save boundaries (memorycard.ad L607..625;
gtmode.ad checkAutoSave/doAutoSave L230..269); Auto Save Off and guest play
retain their original no-automatic-write gates. No per-input writes or fake
storage delay were added. Capture failure now returns IO_ERROR instead of
being reported as successful by the native autoSave adapter.

**C, native persistence fix:** withdrawn/withdrawnGT4p, the one-time legacy
transfer flags (menu_legacy_import.inc, 001D49C8/001D4698), were absent from
the serialized root whitelist. They now persist as strictly validated
boolean/integer 0/1 values. Existing containers and the other progress fields
are unchanged; older profiles without these fields still load. A failed
new-profile activation also retains the previous profile's dirty flag.

storage_access checks live blinking on creation, rename, load and save;
failed data/index writes preserve the prior player and unsaved state;
Auto Save Off makes no write; successful Auto Save clears dirty only after
writing; startup reload restores money and both transfer flags into a menu
whose old flags were explicitly cleared. Invalid flag value 2 is rejected.
The existing blink negative, profile-storage, legacy-transfer, purchase,
Home and welcome pairs are area regressions. Real PS2 import fixtures and
console visual/timing comparison remain outside this test's proof.

### First-run import guidance and HD indicator alignment (18 October 2026)

**P:** the host welcome retains the full GT4 save import route (No to create,
then Yes to import) and now names that route in the first question's help.
After the three Auto Save notices, optional legacy-transfer instructions
restore the purpose of boot.ad ExplainRoot L789..800. The US edition's
001D4500/001D4598 both select US GT3 data: credits up to 100,000 and B/A
completion respectively. Instructions lead to GT Mode Status/START and the
existing extracted-file boundary documented in SAVEGAMES.md; they do not
claim to import actual GT4 Prologue saves or other regions.

The example arrow aligns its authored bg (x8, width22) with the visible HD
body (x4.5..20.5), rather than centering their composite rectangles.
`first_run_dialog` checks those centres, the GT4 import question, all legacy
instruction stages, accept/decline and icon cleanup; its negative restores
the incorrect rectangle alignment. Application smoke routes use disposable
profiles. Existing storage/profile and legacy-transfer tests are regressions.
Auto Save timing remains scoped to the previously documented native I/O
integration: no claim of complete original startup or save-trigger parity.

### Profile access indicators and first-run Auto Save explanation (18 October 2026)

**C (native integration):** Memorycard save/load faces are no longer suppressed
by the renderer. The menu adapter follows share/memorycard.ad L26..50: only the
active face is visible, with MBlinkActor period 0.10000000149011612; both actors
stop and both faces hide at the end. Native file reads and writes run on an
SDL worker while the main thread presents the existing menu and actors.
Serialization, validation and session changes remain on the caller; bytes,
atomic replacement and error handling are unchanged. No artificial device
wait or post-operation busy state is added. Headless callers retain direct I/O.

`storage_access` exercises delayed real file I/O, visible/hidden blink phases,
load/save face selection, cleanup, failed write preserving the previous file
and dirty flag, and failed load preserving the active player. Its negative
rejects a non-animated indicator. Profile storage and Home regression pairs
and original dialog-background regression pairs pass (10 focused tests total). Full device/controller/GPU coverage is not claimed.

**P (host profile setup):** First-run profile completion restores the three
ExplainRoot L780..785 notice stages before the opening: Auto Save, its animated
icon/arrow example, and the storage safety warning. Frame/buttons and the
indicator layout/arrow come from the player's original cursor/boot projects.
Both access faces reuse the native hard-disk artwork already used by Replay
Theater, with the example arrow centered below it; wording
refers to native files rather than a console memory card. Guest play skips
this explanation. The 14-route application smoke test uses temporary saves;
welcome and explanation screenshots were inspected. Automatic profile probing
before video initialization remains a host bootstrap operation, without a
rendered access animation. No complete PS2 startup-flow equivalence is claimed.

### Secondary launcher quick audio mixer (18 October 2026)

**P:** Launcher-only Audio controls provide independent music/effects sliders
and a separate music mute that preserves the selected music level and effect
volume. M toggles music mute; F8 opens the panel; sliders support drag/wheel,
with arrow keys and Shift for fine adjustment. The shared options store keeps
three registered audio.launcher_* values; no new configuration file or game
volume mapping is introduced. The previous launcher levels (78/128 music,
128/128 effects) remain the defaults. This launcher-only access was explicitly
requested; it does not change the in-game sound settings or official launcher.

Four focused tests pass: mixer input/persistence and its mute-leak negative,
plus existing native launcher audio waveform/mute tests and their negative.
They cover restoring music, independent effects, clamping, drag cancellation,
keyboard repeats and unrelated shared preferences. Light/dark launcher captures
were inspected. Physical controller navigation of this quick panel is not
implemented; mouse and keyboard are the supported controls.

### Switch setup host interface (2026-10-17)

**P:** The separate Windows Switch installer follows the desktop first-run
wizard's generated white room, Barlow face, centered disc/connection steps
and graphite action/help bar. Setup messages and packaged instructions are
in English. This changes no game behavior or desktop launcher code. Host
Shell/config and local FTP replacement/backup/negative tests pass; both
setup layouts were visually inspected. Full ISO installation through real
DBI hardware remains unvalidated.

### Android host/input foundation (17 October 2026)

**P:** ARM64 APK bootstrap selects a player-owned document with a persistent
read grant and retains a seekable descriptor. Android uses the shared native
Vulkan/game code, touch menu adapter and optional mobile pad overlay. Controls
are individual shared-store options with Classic Off; ordinary pad bindings
and replay commands remain the input boundary. Background/foreground cancels
contacts, pauses active driving and resets presentation timing. Host mobile
control/negative tests pass; real phone, GPU, lifecycle and end-to-end race
validation remain pending. Videos/IPU decoding and additional mobile features
are explicitly incomplete; see ANDROID.md. No Android performance grade is
claimed and no PC/Switch physics was changed.

### PC scene quality (17 October 2026)

**P:** Optional SSAA 150%/200%, reduced 3D scale and model texture smoothing
apply only at PC presentation boundaries, through the existing Options store.
Original retains the previous backend-specific scene path; smoothing Off keeps
authored shader sampling. Physics/replay commands are unchanged. SSAA increases
GPU work and is not a performance improvement. Switch/Android are excluded.
Synthetic OpenGL/Vulkan tests check fractional edge coverage, target dimensions
and switching back to the retained path. Actual course/material coverage and
visual quality across all GPUs remain unverified. Alt+Enter is a desktop window
shortcut; it consumes the confirmation key and leaves game actions untouched.
Focused validation: 13 tests passed, covering materials and their negatives
on both backends, scene sampling, desktop events, quality persistence, original
settings/presets and cancellation. The desktop shortcut pair passed again after
the final platform guard. No complete-suite or multi-vendor GPU claim is made.

### Suggested gear and launcher quality (17 October 2026)

**C (HUD integration):** `0039E150/0039E1BC` read the inline driver model's
signed suggested gear and flash flag, already computed by `00469E58`.
The native race HUD now forwards both for the watched car, including P2.
`003A6FA8/003A7048/003A7108` supply the font, red alpha fade, retained digit
and blink parameters; `003A7FCC/003A9274/003A7C1C` supply chase, onboard and
split anchors. No gear selection, driver model or physics calculation changes.
Tests check signed -1/0/positive hints for two cars, 0.2-second fade (1e-6
host tolerance), flash range, layout translations (1e-5), B-Spec suppression,
course reset and negative controls. Absolute chase text anchors are checked
against 003A7FCC/003A7108 and the current gear: left in chase, above onboard.
HUD/font-cache regressions pass; the new
HUD pair also passes on Vulkan. Native HUD and secondary-launcher captures
were inspected. Full original-code animation/pixel comparison remains pending;
this is not an exact PS2 raster or whole-physics grade.

**P (secondary launcher):** Options > Quality exposes the same optional PC
AA, resolution scale and texture smoothing as Screen Settings. The shared
store/API, mutual exclusions and Classic/Enhanced values are tested for both
renderer choices, including persistence and a negative control. Values apply
at Start; the official launcher is unchanged. Fifteen focused tests passed,
including in-game quality, cancellation, presets and both renderer settings.
See the OPTIONS registry.

### Optional event-ticket hover (17 October 2026)

**P:** The existing Hover Feedback option also recognises EventHallRoot ticket
cards, including EventItem and SimpleEventItem. The complete ticket composition
uses the existing eased 4.5% enlargement for mouse hover and retained touch
preview, without added colours or text. Classic/feedback Off retains the authored
geometry immediately. The used_car_stamp fixture checks both ticket templates,
intermediate animation, text/art scaling, unchanged neighbouring cards, restored
authored geometry and touch preview. It fails without the ticket recognition;
three focused tests pass with the change, including existing negative controls.
The touch path is checked on the host; no new Switch hardware validation is claimed.

### Licence menu cone dimensions (17 October 2026)

**C (UI integration):** LicenseDetailRoot cone ImageFaces now draw in their
inherited authored 66 x 82.5 client area, following 002A2608 +0x7C..0xC0
(00231420/00231440). Drawing the 88 x 110 source texture at native size enlarged
the cones and clipped the bottom row. The gtmode_license_cycle fixture checks
all 80 cones across B/A/IB/IA/S; it fails before the fix and passes afterwards.
Three existing negative controls pass. Native before/after captures were
inspected: the lower cones are fully visible. This is a geometry/integration
check, not an exact PS2 pixel comparison.

### Event and championship licence admission (17 October 2026)

**C (profile integration):** Restored RaceEventProject, ChampionshipProject and
MissionProject contexts now bind the indexed race-record resolver, as GT Mode
already does. Admission 00110498 -> 004312B0 must query the authoritative saved
licence records; an unbound quick project instead created empty per-code units
and rejected completed licences. The extended gtmode_license_cycle fixture
failed before this fix and passes afterwards. It checks Japan Championship
blocked after A-1..A-15 and allowed after A-16, then 7,090 admission queries
across the loaded race catalogue, both event/championship contexts and all five
completed licence categories. Three existing negative controls also pass.
No medals are granted or entry requirements relaxed; full visual/hardware
coverage is not claimed.

### Shared units in prepared races (17 October 2026)

**C (presentation integration):** Race setup now reads the published unit
preferences for every shared start path, including prepared license, mission
and career races. Previously these paths retained the last race's presentation
units, while Arcade explicitly refreshed them. This follows MOption.apply's
global MUnit publication (004365B8 +4C..68); no physics or replay commands change.
The disc-backed race_options_units fixture checks both the Options project and
the published preferences over seven power units and alternating speed units.
Four focused unit/lifecycle tests pass, including negative controls. Complete
visual coverage of every race mode remains pending.

### Options return: race units and cursor (17 October 2026)

**C (presentation integration):** UnitRoot.apply publishes unit_velocity
(KMPH/MPH, 005A6978) and unit_power. Returning to an existing race now refreshes
both through the same adapter as initial Arcade setup. The race is not rebuilt
and physics inputs are unchanged. The disc-backed race_options_units test checks
seven power units and alternating speed units over repeated applies; its negative
omits the refresh and fails. Existing Options lifecycle checks pass.

**C (cursor context):** A departing context's fade freeze must not overwrite the
destination's own transition state (0022F6A0). ui_cursor_transfer preserves its
position/device but keeps the destination freeze. The regression fixture fails
with the old copy and passes with the fix, including movement after cached-page
return. Six focused menu/navigation/unit tests passed. Full PS2 pixel comparison
and all modal/project combinations remain unverified.

## 1. As notas

Cada item do jogo recebe exatamente uma nota:

| Nota | Significado | Exige |
|---|---|---|
| **E** Exato | Idêntico ao original, valor a valor ou bit a bit | Teste automático que compara com o original e passa |
| **C** Confere | Mesmos dados e fórmulas do original; a saída (lista de desenho, imagem, som) bate dentro de uma tolerância declarada | Teste automático com a tolerância escrita |
| **V** Visual | Feito a partir do código e dos dados do original, comparado lado a lado com o PCSX2 por comparação humana, sem diferença percebida | Print de comparação guardado; **ainda não conta como fiel** |
| **A** Aproximado | Parecido, com diferença conhecida ou valor estimado | A diferença descrita no registro |
| **F** Falta | O original tem, o port ainda não | Nada |
| **P** De propósito | Diferente porque é melhor assim (resolução, carregamento) | Justificativa de uma linha |
| **N** Não será feito | Fora do escopo ou impossível | Justificativa de uma linha |

**Fidelidade = (E + C + V da imagem 3D e do áudio) / (todos os itens − P − N)**

V, A e F contam contra. V existe para não perder o trabalho feito, mas é
dívida: todo V precisa virar E ou C por um teste, ou ser rebaixado a A.

**Exceção (política de validação, 02/10/2026):** na imagem 3D (pistas, céu,
luz, reflexos, efeitos) e no áudio, V é a nota final e conta como fiel.
Comparar no olho com o PCSX2 basta, porque um comparador automático ali
seria caro demais. A evidência continua vindo do código e dos dados do
jogo (regras 2 e 3): V não autoriza ajustar no olho.

## 2. As regras

1. **Nada é "fiel" por declaração.** Só E e C contam, e só com teste que
   passa. "Parece igual" é V.
2. **A evidência vem do jogo.** Número, posição, cor, tempo e fórmula saem
   do código desmontado (`splat/asm`), de um arquivo do disco ou de uma
   captura de memória. Citar a função (`func_00xxxxxx`) e o offset no
   comentário do código.
3. **Proibido ajustar "até parecer".** Se um valor foi medido em print ou
   escolhido sem evidência, o item é A, o comentário no código diz isso e o
   registro descreve a estimativa.
4. **Todo teste precisa de um caso que falha.** Antes de confiar num
   comparador, alterar um valor de propósito e ver o teste acusar. Teste
   que nunca falha não prova nada.
5. **O que nenhuma captura exercita não é portado no olho.** Ou se força o
   caso (estado sorteado, valor injetado) e se compara com o original, ou o
   item fica A.
6. **Registrar na hora.** Ao terminar um item, a atualização inclui o
   registro deste documento, no mesmo commit ou no seguinte. Não deixar
   para depois.
7. **Rebaixar sem pena.** Se um teste passa a falhar, ou alguém aponta uma
   diferença, a nota cai imediatamente. Subir de volta só com a correção e
   o teste.
8. **Referências não entram no repositório.** Capturas de memória, prints
   do PCSX2, GS dumps e arquivos do disco ficam locais (`captures/`,
   `extracted/`). O teste gera ou lê a referência na máquina de quem roda.
9. **Relatar com honestidade.** Toda resposta ao usuário diz: o que mudou,
   como foi conferido (qual nota), o que ficou estimado ou pendente. Nunca
   dizer "igual" para um item V ou A.

## 3. Tipos de prova

| Área | Prova | Estado da ferramenta |
|---|---|---|
| Física, IA, montagem da corrida | Memória idêntica ao original, passo a passo (`tests/physics/compare.sh`, `PHYS_AB=`) | Existe |
| Dados dos carros | Parâmetros idênticos ao original | Existe |
| Regras da carreira | Retorno e memória idênticos ao original (alvos `gtmode_*_oracle`) | Existe, por regra isolada |
| HUD, telas da corrida, menus | **Lista de desenho**: rodar a função de desenho original no oráculo com as primitivas (imagem, texto, cor, posição) trocadas por um gravador; comparar com a lista que o port produz | **Não existe. Prioridade.** |
| Câmeras | Matriz de vista e projeção iguais às do original (captura ou oráculo) | Feito à mão em duas câmeras; automatizar |
| Imagem 3D | Diferença de imagem com tolerância contra referência do PCSX2 | Não existe |
| Áudio | Comparação de forma de onda com tolerância | Não existe |

A prova por lista de desenho é a que falta para tirar o HUD, a tela de
resultados e os menus da nota V. Ela não depende do PCSX2: usa o mesmo
esquema A/B da física, comparando ordens de desenho em vez de memória.

## 4. Registro

| Switch menu touch | P | Optional `controls.touch_menu` (Classic Off, Enhanced On), registered in Options. Uses existing authored menu hit bounds/actions and list/map limits; confirms once on matching release, with single-contact capture, cancellation and synthetic mouse suppression. Eleven focused host checks include Classic/Enhanced persistence, button/scroll/map/dialog behavior and negatives. Native Release builds; hardware touch/docking validation pending. No physics, race controls or camera gestures changed. |


Legenda de quem conferiu: **(c)** conferido diretamente ao registrar esta
linha; **(d)** copiado dos documentos da área, ainda precisa de
confirmação ou correção.

### 4.1 Simulação

| Item | Nota | Prova ou observação |
|---|---|---|
| Shared scalar EE arithmetic: original/native step state | E | All live modes build the same native C with integer ADD/SUB/MUL/DIV/SQRT results, preserving operand order. `disc_replay_arithmetic` compares RAM outside oracle stack 01E18000..01EF0000 after 1,200 steps on each of 13 recordings; extended runs cover all 161862 commands. Changed-player-byte negative fails. Both sides share the arithmetic model and native initial state: no independent silicon/FCR31 or fresh-console-constructor claim. Original 00359568/00359830/003598F8 curve order kept distinct; 00353800 slip-fold output sequenced before use. [Scope](notes/DISC_REPLAY_ARITHMETIC.md). |
| Original teacher commands: all 80 licence gold gates | C | `gtmode_license_gold_catalog` runs current DC=0 native examinations with original teacher controls/options: 80 successful states and times at or below original RACE gold targets; impossible-target negative fails. No player save edited. Separate strict timing audit: 64 exact recorded times, 15 zero metadata fields, B-6 22715 ms versus recorded 22749 ms (gold 22800). Gold reachability is checked; complete licence trajectory/constructor/presentation remains A. |
| Original Time Trial replay entries and lap events | E | 003DDA08 retains GT4M +958 entry count, without adding an unrecorded ghost. `disc_replay_lap_event` compares RAM/queue against 003F5B80 -> 0035E550 at a forced completed lap; changed queue byte fails. 0035ABA8 checkpoint oracle now uses EE arithmetic. Source metadata and bounded event/restoration proof; complete original session startup remains A. |
| Surface waves: EE initializer and historical captures | E | `physics_surface_waves` retains 408 historical captured words with the legacy backend and separately compares 408 EE words against original 0035F808 on the same 17 captured inputs. Changed-bit negative fails. The historical emulator multiplier is not independent hardware evidence: ar_02 surface 11 is 417B53D1 historically, 417B53D3 with EE arithmetic. |
| Native recording arithmetic version | P | New GT4R v2 uses corrected shared physics; v1 full playback selects historical arithmetic automatically, and re-save retains v1. `replay_file` checks both versions and rejects unknown versions; `physics_math_modes` compares all 32 MiB after 600 commands with direct historical execution, with negative. Compatibility is selected by file format, never a live physics setting. v1 ghosts mixed into new live Time Trial remain uncertified. |
| EE physics oracle arithmetic audit | E | Twenty-seven previously host-backed adapters now directly link the integer EE result object; executables inheriting moon_recomp_physics do too. Logger sampling, engine/gear chart operations and 0035DDB0 +14 mileage subtraction now retain EE arithmetic at application boundaries. Existing original numerical checks and deliberate negatives retained. Numeric-model sharing and approximate FCR31 bookkeeping do not certify silicon. [Inventory and limits](notes/EE_PHYSICS_AUDIT.md). |
| Mission timed-start dispatch and timing-record eligibility | E | 003F6760 +48..64 calls virtual +A0 including RaceMission's 003F6E50 override; 00358000 +78..B8 checks virtual +30 before the new-record flag. Original/native RAM outside oracle stack agrees for all 1282/11400 steps of winning mission 9/22 pad fixtures, including changed-byte negatives. Both defects predated 27e2397. Shared native construction, not full-console startup parity. |
| Disc command consumers: frontend return, guide and Logger flow | C | Eight positive/negative end-to-end checks cover idle Arcade attract through 9996 commands, 80 licence demonstrations with 12 moving RaceTraining guides and no awards, PSU-imported Rdemo0/9 full playback with source final poses bit exact, and 165 Logger points identical after repeat analysis, native save/load and Load Track B with A preserved. Separate complete Theater check covers all 13 tapes. Full visual/console-initial-state comparison and real memory-card recordings remain pending. [Scope](notes/EE_PHYSICS_AUDIT.md). |
| PS2 best-lap ghost import | F | No independent FileReplayBestLap fixture or separate certified converter. Full GT4M replay import and native saved ghosts are not proof of this path. Native v1 ghosts mixed into corrected live physics also remain uncertified. |
| EE arithmetic fast paths | E | physics_float_fast compares 14583683 full result/flag words with unchanged full recurrences, with changed-result negative. Integer shortcuts for zero Booth correction and exact SRT quotients; no arithmetic toggle or host-float substitution. PC six-car medians fall 779.159 -> 715.465 us (Tsukuba), 867.902 -> 806.548 us (Nurburgring). Cortex-A57 compile only; Switch timing unmeasured. [Method](notes/EE_PHYSICS_AUDIT.md). |
| Motor, turbo, câmbios, trações, diferenciais | E | `compare.sh arcade`, `PHYS_POKE` nos 722 carros (d) |
| Pneus (aderência, desgaste, calor) | E | idem (d) |
| Suspensão, carroceria, colisão com pista e paredes | E | idem (d) |
| Auxílios (ASM, TCS), controle de guinada | E | idem (d) |
| IA dos pilotos, catch-up | E | capturas `ar_*`, `nur_*` (d) |
| Colisão carro contra carro | E | idem (d) |
| Voltas, relógio, posições | E | idem (d) |
| Pit stop com troca de pneus | E | idem (d) |
| Montagem da corrida a partir do disco (Time Trial, Arcade) | E | `compare.sh built` (d) |
| Parâmetros dos 722 carros | E | (d) |
| Pontos A-spec da corrida | E | `PHYS_AB=aspec:4000`, zero diferenças (d) |
| Sinal do carro (FULL THROTTLE etc.), a lógica | E | parte do passo da física (d) |
| Física de duas rodas (moto) | N | Não pertence ao GT4; investigação do Tourist Trophy é outro projeto, não recurso ausente deste jogo. |
| Sessões do GT Mode (campeonato, endurance, licença, missão, B-Spec) | A | 13/10: implementadas em session.c, gtmode_game.c e main_race_flow.inc; regras E e integração C delimitadas nas linhas abaixo. Testes gtmode_mission_flow/exit/cycle, license_menu_demo/disc_demo_playback, bspec_tests e ciclos de eventos/endurance. Evolução persistente da experiência B-Spec ainda F; comparação integral dos modos A. |
| Eventos, ghost e recordes do Time Trial | A | 13/10: session_best_update, fita GHST e course_ranking ligados. tt_game_probe --saved reproduz 5661 passos a 0 m; negativo desvia 16,2 m. course_ranking testa top10, empate, exclusão e save/load, com negativo. Logger A/B e replay disponíveis; ghost importado do PS2 ainda F, apresentação integral A. |

### 4.2 Corrida do Arcade: apresentação

| Item | Nota | Prova ou observação |
|---|---|---|
| Race HUD contour kerning | C | FC_48 pair advances compared automatically with original 00492978 by hud_font_original, with deliberate changed-step negative. hud_kerning records START/FINISH/TIME UP glyph positions and measurement against those metrics (1e-4 pixel tolerance); its negative drops the T/A adjustment. Existing numeric cell policy retained; full GS raster remains outside this proof. |
| PC audio camera-cut transport | C | TV first-shot and Dive director boundaries reach the sound listener; changes between driving/external views are discontinuities. 003B5E80 +0x58..0xBC restarts car voices and skips displacement Doppler; 00398B70 +0x90..0xB0 restores ambience depth 0FFF. race_audio_camera_cuts injects a 1000-unit jump: restart and neutral Doppler, registers/PCM equal at 1/2/4 ticks per draw for CR-X and Mustang; continuous-motion negative fails. TV oracle retains matrix/operator comparisons and checks first-shot publication. Final SPU2 waveform/cut smoothness remains A. |
| PC menu fade lifetime and loading continuity | A | Outgoing MIDI producer retained until the device has consumed the authored fade, including pending PCM handoff. Same-group menu music survives internal loading overlays. audio_backend verifies 200 ms fade lifetime, active after 30 ms, finished after completion; deliberate premature-stop negative fails. Original track crossfades are not invented; complete menu-transition waveform comparison is pending. Normal race pause retains the independent music stream while stopping car/ambient/pit effects (0038B428 -> 0033C3E8). |
| PC pause Circle confirmation | P | Requested Circle/B confirmation added alongside Cross/A only in the race pause poller, including player 2. pause_buttons drives real virtual SDL pads into the original pause movie: Continue on both buttons/ports; wrong-button negative fails. Result and pit-menu Circle behavior retained. |
| Arcade course-card still-picture fallback | C | arcade.ad loadIpic("course") L30 and original 001FD528 +0x3C..0x68, 002771C8 +0x0C..0x20, 00277228 +0x38..0x70: original archive names/order/offsets restored. movie_ipic decodes all 56 disc pictures (including the two 640x512 entries) and compares batched output byte for byte with independent single-picture decodes. Actual draw_movie stays ready for 112 forward/backward changes with the PSS decoder absent and uploads the selected picture; missing-archive negative fails. The previous readiness gate blocked input until video decoding/blending completed. PC cache preloads asynchronously once; only one GPU poster texture is retained. C covers readiness and picture selection, not complete PS2 RGB conversion or universal frame-time guarantees. |
| PC car material pass separation | A | Original func_0049EB90 +0x84..0x94 selects specular colours alone; VU1 0x215 handles environment colour independently of surface lights. Shared OpenGL/Vulkan shader no longer erases specular-only race layers after vertex saturation or lights reflection geometry as a diffuse surface. car_material_opengl/vulkan check 21 GPU samples each against the isolated formula (1/255 tolerance), with deliberately wrong material negatives; previous shader failed both paths. Eight focused material/frame/disc checks pass, including negatives; no full suite. Complete glass/paint image and PS2 comparison remain pending, so no full material fidelity claim. [Scope](RENDERER.md#car-material-lighting). |
| PC per-pixel material shading | P | Independent Original/Per-Pixel setting on both APIs; 60 analytical cases per API (maximum measured byte error 0), reflection/pass/opacity/invariance checks and deliberate disabled negatives. Retained Original Tsukuba images match the prior PC build exactly; race/replay state matches all 32 MiB between modes. Full PBR, refraction and roughness filtering remain outside scope. [Implementation and limits](RENDERER.md#optional-per-pixel-material-shading). |
| PC generated reflection quality | P | Independent 128/256/512 generated targets on both APIs; angular-detail positive/negative checks, persisted levels, live race/studio Original restoration, previous-build Original images and 32 MiB race/replay state comparisons. Local Vulkan GPU sample 5.716/5.736/6.066 ms; no claimed FPS gain. [Scope and limits](RENDERER.md#generated-reflection-quality-on-pc). |
| PC local car depth shadow maps | P | Optional 512/1024/2048 maps on both APIs, native-alpha cutouts, soft kernel, raised/sloped receivers and exact self-surface/Off checks with deliberate disabled negatives. Local rigid cars cast; world/driver casters and transmission remain pending. [Scope](RENDERER.md#local-car-depth-shadows-on-pc). |
| World-space sun volume and ambient depth | P | Local-map ray integration, independent screen-space AO, hemispherical fill and reflection contact. Focused positive/negative GPU and Options tests. Unchanged source geometry, local-map and off-screen limits remain. [Evidence](RENDERER.md#world-space-sun-volume-and-ambient-depth-18-october-2026). |
| HDR sheen and daylight reflection balance | P | Encoded course sheen enters HDR as linear light; coverage/source passes avoid double relighting. Daylight car overlays use bounded dielectric Fresnel with authored tint. 38 focused renderer tests pass, including deliberate negatives. Peugeot 206/Tsukuba measurements, zero changed Classic pixels on both APIs and identical 32 MiB simulation states. Baked track lighting, roughness-prefiltering and metallic classification limits remain. [Evidence](RENDERER.md#hdr-sheen-and-daylight-reflection-balance-18-october-2026). |
| Shared daylight irradiance for cars and courses | P | One direct/sky calibration replaces separate vehicle/course intensities. GPU neutral-surface comparison: sky 200/200 and sun 204/204 on both APIs, analytic tolerance two bytes; dedicated negative fails at 102/200. All 26 focused modern-lighting/daylight cases pass. Artistic coefficients and baked course lighting remain limitations; full visual integration is not claimed. [Evidence](RENDERER.md#shared-daylight-irradiance-for-cars-and-courses-18-october-2026). |
| Bounded paint overlays and wheel contact | P | Daylight additive reflection capped at artistic 0.16, controlled grazing 195 to 80/255; original weighting retained. Existing optional car shadows add four soft tire footprints from visible wheel pose and loaded dimensions, with height fade and receiver depth rejection. Four-contact positive/negative GPU fixtures and 22 focused renderer tests pass on both APIs. Real Ka contact toggle changes 526 pixels per API. Planar approximation and detached-pit-wheel limits remain. [Evidence](RENDERER.md#bounded-paint-overlays-and-wheel-contact-18-october-2026). |
| Daylight paint albedo and ambient body footprint | P | Daylight sky uses diffuse paint albedo; textured car colours normalize GS 128-white before linear decoding. Controlled texture 97 versus analytic 96.62, coloured sky-only paint 45/63/169, deliberate negatives fail. Broad soft body footprint supplements directional shadow and tire contacts; centre 79 versus 128 without it, raised receiver rejects it. All 28 focused renderer cases pass on both APIs. Artistic planar/composited AO, texture clipping and material-calibration limits remain. [Evidence](RENDERER.md#daylight-paint-albedo-and-ambient-body-footprint-18-october-2026). |
| Rough road reflection composition | P | Optional daylight sheen uses rough dielectric angular weighting after HDR decoding; original mask/source/blur and Classic route retained. Controlled original/normal/grazing 153/10/17, zero-mask and deliberate bypass negative. All 18 focused daylight cases pass on both APIs. Peugeot/Tsukuba comparison, zero changed Classic pixels and identical 32 MiB simulation state. Artistic roughness, horizontal ground and no inferred wetness remain limitations. [Evidence](RENDERER.md#rough-road-reflection-composition-18-october-2026). |
| Scene Edge AA and solar post | P | LDR/HDR edge filtering, mirror-safe main projection, depth clear and expiring shafts. GPU/store positives and disabled negatives; real-disc Classic/state regression pairs on both APIs. [Evidence](RENDERER.md#scene-edge-filtering-and-solar-post-fixes-18-october-2026). |
| Daylight contact occlusion | P | Full opaque solar occlusion plus filtered near-caster ambient attenuation; six focused GPU cases including contact negatives. Artistic 2 m/0.8 parameters; directional approximation. [Evidence](RENDERER.md#daylight-contact-regression-18-october-2026). |
| PC daylight atmosphere, HDR scene and shafts | P | Independent shared preferences; procedural clear-day sky/reflection source, solar hour/cycle, nearby course casting, float main scene and depth-occluded screen-space shafts on both APIs. Positive/disabled negative GPU fixtures; source/scenario parameters and clouds/rain/night/cascade/HDR10 limitations documented. [Scope](RENDERER.md#optional-daytime-atmosphere-and-hdr-scene). |
| PC modern race material lighting | P | Independent Original/Modern option; GGX/direct daylight/sky fill/filmic SDR response, 120 CPU/GPU cases per API (maximum error 1 byte), unchanged opacity and Original restoration, deliberate disabled negatives. PS2 material conversions and daylight parameters are PC approximations; baked tracks/full HDR/GI remain outside this implementation. [Scope](RENDERER.md#optional-modern-race-lighting). |
| Screen Setting: original race/replay option selection and parameters | E | `screen_setting_oracle` runs original `003EFEA8/003EFEE8 -> 003EFDF0 -> 003EFC10`, selecting the independent four-value option arrays and comparing all six float parameters bit for bit. Negative replaces contrast with identity. Existing `gtmode_license_image` compares original CLUT/mix generation with deliberate negatives. |
| Screen Setting: application before race/replay HUD | C | `screen_setting` invokes the real `race_game_draw_hud` presentation boundary on clean race and replay frames, compares GPU output with original-verified LUT/mix within one byte per channel in 4:3 and 16:9, and verifies unmodified overlay colors, SETVAL/DEFAULTVAL, preservation of saturation/temperature, independent preferences and profile reload. Negative changes brightness before rendering. Complete PS2 scene rasterization is outside this proof. ResultLicense retains its separate saturation pass without double correction. |
| Nintendo Switch: bounded GPU loading submissions | A | Hardware log14 passes composition/alpha probes but driver kickoff is rejected without a GPU error notification after 592 uploads / 60157548 bytes and 372 draws. Switch-only partial Submit/Flush after 32 transfers or 4 MiB (P work budgets, not driver limits), preserving FIFO lists/barriers and frame-fenced storage; no ordinary CPU idle/readback. Raw libnx Result wrapper observes failures without retry. Shared helper host test checks 674 transfers, exact order/uniqueness, byte cap and no wait; three deliberate negatives, 32/32 focused deko checks pass. Synthetic 592-image/60 MB native boot probe and actual race entry await hardware; root driver limit/reason and crash resolution unproved. See SWITCH.md. |
| Nintendo Switch: race-entry GPU failure diagnosis | A | Latest Atmosphere module ID exactly matches retained e8e9976 ELF; ARM64 symbolication identifies main -> plat_video_end_frame -> nd_present -> dkQueuePresentImage -> Queue::flush -> RaiseError(Fail), result 0x367. Optional diagnostic SDK archive retains NvError messages erased by release SDK, logs three frame contexts only on failure; no extra GPU wait. Native vertex extent guard and failed-fence reclamation guard added; range positive/overflow/truncation cases and deliberate negative pass, 28/28 focused deko tests. Root GPU fault and console resolution still pending diagnostic log; no physics/OOM cause claimed. Full-image composition repair included independently. See SWITCH.md. |
| Nintendo Switch: nonblocking profiler output | P | Hardware trace isolates 305-427 ms in swp_ready after drawing, with presentation 0.26-0.35 ms. Switch-only bounded diagnostic queue moves stdio to a libnx worker; overflow drops records rather than blocking the frame. Host FIFO/wrap/full/empty/oversized checks and deliberate negative pass; ARM64 NRO builds. Hardware confirmation pending; no general loading, race-FPS or PS2 fidelity claim. See SWITCH.md. |
| Nintendo Switch: specular-only vertex lighting | C | 0049EB90 +0x84..0x94 selects the specular block before saturation. Native shader previously cleared this contribution before selecting it. deko_materials now passes 2160 vertex and 3072 fragment cases against desktop GLSL, zero byte/channel difference; six deliberate negatives pass. ARM64 shader/NRO compile. Scope is this shared lighting rule and host GPU comparison; full console/PS2 material parity remains unproved. |
| Nintendo Switch: immutable pipeline binding reuse | P | Backend command optimization only: shader pair and mesh/UI vertex layout cached until frame reset; mutable render state and draw order retained. 8000-draw SDK-command fixture checks effective state and 80/40 binds, plus shader/layout/reset negatives; 18 focused checks pass. Laguna opening window CPU submission 1.770 to 1.540 ms, FPS unchanged at 39.20. Full 60 FPS and GPU pixel parity not claimed. See SWITCH.md. |
| Nintendo Switch: native capture shutdown | C | First hardware PNG saved then dkDeviceDestroy rejected leaked renderer targets. Native teardown now releases seven renderer-owned targets before device destruction, preserving borrowed handles. Repeated 10-second Laguna/Ka capture saves PNG and exits normally in live SDK diagnostics. Hardware integration scope only; all exit states not covered. See SWITCH.md. |
| Nintendo Switch: widescreen rank clip | C | Console PNG has empty position box while matched desktop has digit. 003A3B58 clips width 640 and 003AFD18 shifts rank left by 96; native scissor lacked desktop full-content expansion. Shared native helper tested at three scales with rank coverage, local/panel preservation and central-only negative. Math/clip integration scope; updated console image pending. See SWITCH.md. |
| Nintendo Switch: dynamic course reflection producer | A | Native path no longer skips course_reflection or returns null environment handles. Reusable six-face atlas and native sphere-map shader restore the existing PC adaptation around 0044E528/0044EEA8 without ordinary CPU readback. Three asymmetric camera poses match the CPU formula within 1 byte/channel; reversed-basis negative and 21 focused checks pass. Console Laguna/Ka PNG shows body reflections and rank, exits normally. Complete matched-tick PC/PS2 pixels unproved; GPU cost remains, no 60 FPS claim. See SWITCH.md. |
| Nintendo Switch: bounded DATE transfer span | C | `deko_snapshot`/negative retain 100,000 projected float points in both axes across 1,000 boxes, rotation/mirror/jitter and near/skin fallback; 23 focused checks pass. Laguna/Ka console scene/car regions have zero changed RGB channels versus the full-copy build. Same shader, draws, physics and GPU barriers; transfer width only. Portable windows 34.61/44.71 → 60.36/59.56 FPS; later 44.97, not constant 60. Two-axis capture retains zero scene RGB differences; Laguna windows 60.00/59.98, later 47.26 FPS. Cropped-transfer delayed-engine and wrong-offset negative pass. Tsukuba 46.57–60.55 ordinary windows, with larger outliers. Full-frame/all-content parity unproved; separate HUD pixels differ. |
| Nintendo Switch: fragment-engine DATE snapshots | C | Shipped shader matches full/cropped RGBA8 host framebuffers byte-for-byte; wrong-scissor negative fails. Native boot checks 40 dependent ping-pong snapshots plus one crop, all alpha bytes, exact after readback. Same authored draws, saved alpha and resolution; only GPU copy engine changes. Tsukuba ordinary windows roughly 52.69–60.73 FPS; START preparation outliers treated separately. Native racing capture checked visually; matched-tick full-scene equality and constant 60 unproved. |
| Nintendo Switch: resident START/opening HUD fonts | C | FC_48 first-use measured 787.604 ms on console; staged/shared preloading puts atlas creation before readiness. Fixed original 003A4AE8/003A4E00 fonts/metrics/colors retained; no physics or presentation-clock change. Warmup verifies twelve cached styles, incremental loading and atlas alias ownership; exact fresh/used replay/pause framebuffer comparison and negatives pass. 30 area checks pass; native repeat removes observed ~800/460 ms START/catch-up spikes, early steady maxima 17.010/16.969/18.792 ms. Later 85.813 ms outlier remains; loading animation, split/docked/all-track and constant 60 coverage pending. Windows lazy path retained at that revision; superseded by the PC loading fix below. |
| PC: prepare START fonts during race loading | C | Race readiness now waits for the same twelve incremental font preparations used on Switch, before the presentation clock starts. Original 003A1488 START duration and 003A4AE8 FC_48 rendering remain unchanged. Headless runs skip GPU preparation; split-screen preparation remains outside this fix. `race_hud_warmup`, its negative, `replay_hud` and its negative compare fresh/resident START plus replay/pause images exactly at 4:3 and 16:9; START adds no new cached face. Six focused HUD/license checks pass; a Release PC Ka/Tsukuba drive crosses START, captures at race time 2.966 s and exits normally. End-to-end PC frame timing still pending; this does not prove every START stall is gone. |

| Nintendo Switch: scalar DATE masks | C | Destination alpha retained at the same 8-bit precision in R8_UNORM instead of RGBA8; same draw order, threshold and physics. Host full/cropped GPU checks cover all 256 values and untouched pixels; wrong-scissor/channel negatives fail. PC-reference material comparisons pass 3,072 fragment and 2,160 vertex cases; 34 focused tests pass. Native full/crop alpha256 probe passes; racing capture visually checked. Matched Tsukuba later windows 54.71/51.61/54.09/57.23 become 56.48/53.33/55.84/58.84 FPS; constant 60 and all-track coverage remain pending. |
| Nintendo Switch: lossless indexed geometry | C | Complete 108-byte vertex keys reconstruct 6,000 authored vertices byte-for-byte; wrong-index negative fails, UV/normal/skin/signed-zero/NaN distinctions retained. Static meshes share vertex records without changing triangle/group order; transient/update paths keep expanded arrays. Native boot indexed-vs-original full RGBA8/depth comparison exact; racing capture visually checked. Tsukuba later windows 56.48/53.33/55.84/58.84 become 59.95/57.71/60.38/60.44 FPS. Constant 60 and broader tracks/docked coverage pending. |
| Nintendo Switch: full-screen viewport bounds | C | Native viewport now rounds fitted content bounds as video_gl.c instead of truncating them. Tests cover widths immediately below 960/1280/1920, existing local/panel/HUD clips, and deliberate truncation negative; 39 focused tests pass. Native racing capture confirms rightmost columns now contain pavement (50,49,49), not background leakage. The earlier fragment-scaler experiment left the line unchanged and was removed; original 2D scene scaling restored. No cropping/resolution/physics change. Constant 60 still unproved. |
| Nintendo Switch: native build boundary | P | 13/10: devkitPro ARM64 NRO build with SDL2/Mesa/EGL/glad, project NACP/icon, SD configuration, positional Nintendo controls and unavailable movie/FSR backends. Windows paths retained. ARM FPCR rounding/FZ startup check and independent positive/negative probe are built but not yet executed on hardware; no claim of ARM physics fidelity, menu/race playback or measured performance. See SWITCH.md. |
| Nintendo Switch: widescreen and performance diagnostics | P | 13/10: requested fixed 16:9 startup and settings boundary, including old aspect_ratio=0 configs. ARM64 NRO rebuilt; 7/7 host video/renderer checks passed with existing negatives. Passive frame/present, car decoding, volume read/inflate, mesh upload and shader logs use wall-clock timers without forced GPU synchronization. User reports menu/race load with severe 3D slowness; no measured console FPS or performance fix established yet. See SWITCH.md. |
| Nintendo Switch: stage timing baseline | P | Passive armGetSystemTick spans for physics, scene/GL submission, swap, audio, disc/inflate and asset loading; two-second aggregates, context visits and nested load markers. No forced GPU completion; CPU submission is not GPU execution time. Host switch_profile checks nested exclusive accounting, pending CarFaces, frame statistics and concurrent workers; deliberate negative changes expected disc time. ARM64 NRO and Windows Release built; 11/11 focused checks passed with negatives. Reference rendering/build flags retained. Actual handheld/docked menu, Tsukuba/heavy-course FPS and load timings require console logs; no measured optimization gain claimed. See SWITCH.md. |
| Nintendo Switch: recoverable SD save replacement | P | fsdev non-overwriting rename requires a Switch-only destination rollback/install/commit helper, shared by profile and config writes. Read recovery restores an interrupted rollback name; failed installs keep complete old/new files. System-tick temp names preserve older pending saves from unavailable getpid=-1. Host switch_save models EEXIST and tests real profile reopen/update, backup rotation, install/commit/rollback faults and recovery; negative rejects rollback move. Six focused storage checks passed; Windows Release/NRO built. Save format unchanged; power-loss atomicity and recovery of the user's pending progress are not claimed. See SWITCH.md. |
| Nintendo Switch: GPU reflection/composition work | P | Handheld baseline supplied: initial main menu 59.74 FPS, Camaro showroom 15.39 FPS, Tsukuba physics-active windows 6.00 FPS (explicit >=500 ms window exclusion, details in SWITCH.md). Six race environment readbacks replaced by GPU atlas/remap; showroom room/panel weighted composition stays resident on GPU. Geometry/native mips retained; unused mesh textures skipped with missing shared-child fallback. Model/new shader boot warm-up; specialized warm-up/cache/CPU trials pending. Host switch_render compares remap/composition/reflective model channels within one byte and tests uploads; corrupt-channel negative. Eight focused checks pass. Windows paths retained. Actual console gain and docked/heavy-course samples pending; not a PS2 pixel proof. |
| Nintendo Switch: 3D budget and shader warm-up | P | Second handheld Camaro showroom sample: 15.39 to 20.03 FPS; Tsukuba still 6.75 FPS with a different opponent grid, so no controlled race gain. New Switch-only race/full showroom caps 960x540/1280x720 with single-sample storage and native UI; existing service layers capped, ordinary direct CarFaces pending. Eleven enabled programs warmed at boot on temporary targets; driver-internal variants and persistent caches remain pending. Host switch_render checks warm-up reuse, actual sample count, scene/layer dimensions, page composition and native viewport restoration; deliberate width negative. Fifteen focused checks passed; Windows/NRO built. Actual console improvement of this budget remains pending. Save activation/readable slots confirmed by the new log and user. See SWITCH.md. |
| Nintendo Switch: menu preference persistence | P | Second console log reports menu option writes failing although profiles activate. menu_options.inc still used non-overwriting fsdev rename; Switch capture/read now use the existing recoverable replace/recovery helper for config.ini.game_options. Format/path and Windows branch retained. Host Switch-helper fault tests and desktop option lifecycle/profile regressions cover their respective paths, including negatives; console preference persistence still requires restart validation. |
| Nintendo Switch: Arcade 2D preparation | P | Third console log isolates GameRoot first visit 1365.125 ms, including hn_14 preparation 1201.492 ms. Switch project load now warms GameRoot/CategoryRoot/static CourseRoot fonts/images and hidden states/cursors, using the actual tooltip fallback, before TopRoot interaction. No script entry, actor advance or 3D/video preparation. Real-disc arcade_prepare verifies widget bytes, no new resource cache entries in 40 GameRoot frames and identical final RGBA; skipped-preparation negative fails. Synthetic hidden-state/resource-read test and negative also pass (4/4 focused checks). Host timing 232.841 to 0.091 ms first submission, with preparation paid earlier; not a console timing claim. Switch confirmation pending. |
| Nintendo Switch: adapted GL submission | P | Switch-only per-mesh texture/uniform reuse and consecutive visible opaque VBO batching preserve primitive order and model-list bounds; blend/alpha-fail/DATE/active skin/ghost passes remain separate. PC-generated mip chains/8x anisotropy omitted, authored levels retained. Host GPU fixture compares full RGBA for cached/uncached texture-null, native-mip, wheel and DATE cases (12 to 3 texture-state updates), and 300-triangle batching (3 to 1 draws) with culling gaps, blending, alpha-fail, DATE, ghosts and model-list exclusions. Separate corrupt-pixel negatives; upload/filter assertions. Seventeen focused checks passed; Windows Release and NRO built. Console gain, direct CarFace targets, persistent caches and deko3d implementation pending. Not a PS2 pixel proof. See SWITCH.md. |
| Nintendo Switch: shader work reduction | P | Supplied handheld log confirms Arcade GameRoot first readiness 1360.377 to 15.001 ms with 4151.567 ms boot preparation; remaining Ka/Focus showrooms 44.52/44.44 FPS, Ka Tsukuba 17.47 FPS (physics 2.021 ms), mixed GT map 43.40 FPS with final idle windows 60 FPS. Switch-only shader skips overwritten lighting/unused reflection math and uses hardware base-level reflection filtering. Host full-RGBA comparison: 240 material/light/filter/view cases, max difference 1 byte/channel, nonblack guard and deliberate corrupt-byte negative. Eighteen focused checks passed; Windows Release/NRO built. Actual gain of this shader iteration, first-use room preparation, direct CarFace targets and deko3d remain pending. Not a PS2 pixel proof; see SWITCH.md. |
| Nintendo Switch: draw-state reuse | P | Next handheld log confirms shader_fast=1 and Arcade readiness 14.515 ms; Ka/Laguna 24.04 FPS, Escort/Tsukuba six-car brief sample 20.28 FPS and TT 28.02 FPS. Different grids/routes prevent a controlled shader gain claim; no GT map sample. Switch-only per-submission material/blend/depth/mask reuse, invalidated after alpha-fail and bypassed for ghosts; DATE snapshots remain per group, binding queries reused. Host full RGBA and float depth exact across ten state/texture/DATE/ghost cases with nested read-FBO restoration; repeated-state case 12 to 1 applications with all 12 draws, 76 fixture cache hits. Independent pixel/depth negatives; twenty focused checks passed, Windows/NRO built. Actual console gain, first-use room preparation and deko3d pending. See SWITCH.md. |
| Nintendo Switch: native deko3d backend | P | User requested Switch-specific graphics and preservation of PC. Integrated native device/swapchain, fenced streaming/deferred resources, resident meshes/textures, UI, skin lighting and studio composition; five build-time UAM modules. Dynamic six-face race reflection and road-sheen passes replaced by static environment; forward material masks/blending, base filtering and simplified studio shadows/accumulation are intentional adaptations. ARM64 NRO and Windows Release built; 22 host checks pass including negatives, native artifact audit rejects injected GL reference, desktop source branches preserved. Boot GPU upload/quad/mesh/depth selftests added but not run on hardware. Console startup, images/controls/FPS and full visual coverage remain pending; no 60 FPS or PS2 equivalence claim. See SWITCH.md for missing native mip/region/DATE/alpha-fail/morph and photo coverage. |
| Nintendo Switch: native shader startup ownership | P | First console attempt closed before renderer-ready; supplied log confirms FPCR and three readable saves, not the faulting instruction. Native DKSH initialization previously passed read-only embedded control to dkShaderInitialize, which patches entrypoint/constbuf offsets. Now uses an owned writable copy and reserves SDK code-tail padding. Shared CPU fixture covers copy identity/mutation/reinitialization/truncation and an alias negative; 3/3 focused tests pass, ARM64 NRO builds and native artifact audit passes with injected-GL negative. Flushed boot stages and SDK callback added. Console opening and GPU selftests still pending; not an E/C PS2 claim. See SWITCH.md. |
| Nintendo Switch: exclusive native display ownership | P | Next console log stops at swapchain before shader initialization. Installed SDL SWITCH_CreateWindow unconditionally calls SDL_EGL_CreateSurface, taking the same NWindow needed by deko3d; libnx rejects configured buffers. Native backend now keeps SDL event/audio/controller initialization without an SDL window, enables background pad events, and checks/logs NWindow validity/buffer mask before swapchain creation. Five host checks pass including an actual windowless virtual-pad event and discarded-event negative; native import audit rejects injected SDL-window and GL calls; ARM64 NRO builds. PC and optional Mesa window paths unchanged. Console startup/GPU tests and external keyboard/text focus remain pending. See SWITCH.md. |
| Nintendo Switch: 16:9 GPU boot probe | P | Next hardware log confirms empty native window, completed swapchain/shader initialization and first acquire, then the combined 2D comparator fails. Fixture used a 16x16 target despite always fitting 16:9; bottom sample centers lay at the excluded draw edge. Shared fixture now uses 32x18 and corner-quarter sample centers. Upload remains exact and corner tolerance remains 1 byte/channel; failure still stops startup. Six focused host checks pass including a new old-square-geometry negative; ARM64 NRO builds and native artifact audit passes. Per-upload and per-corner actual/expected logging added. Console upload/draw correctness and main menu still pending; PC unchanged. See SWITCH.md. |
| Nintendo Switch: native material and image-copy repair | P | User confirms probe-fix NRO opens; reported 60 FPS is not a controlled measurement. Native-only Usage2DEngine image layouts and boot blit comparator, preserved UI destination alpha, GPU DATE snapshots/AFAIL masks (00455708), cropped TEX1 per-tap addressing, GS alpha quantization, desktop light/specular/fog rules (0049EA18/0049EB90), independent environment clamp and black fallback. Actual native GLSL versus desktop shaders: 2048 fragment and 324 vertex-output cases, tolerance 1 byte/channel; four deliberate pixel/DATE/address/lighting negatives. Eleven focused host checks and ARM64/UAM build pass; artifact audit rejects injected GL/window imports. Console copy/encoder/full-scene validation and removal of reported checker bands/paint errors remain pending. PC rendering and game logic unchanged; native static-reflection/mip/morph/shadow limits remain. See SWITCH.md. |
| Nintendo Switch: material regression / transfer ordering | A | User reports flickering cars and a Replay Theater crash after the material-fix NRO. Audit finds missing cross-engine pre/post transfer completion, incomplete target-copy invalidation, zero borrowed-sprite dimensions and omitted shadow depth_bias. Native-only fixes use shared GPU-ordering helpers, resident/clamped particle extents and desktop shadow bias. Sixteen host checks pass: 3072 fragment / 432 vertex outputs (zero byte difference, tolerance 1), 120 delayed transfer schedules with independent ordering/cache negatives; ARM64/UAM build and artifact audit pass. New boot DATE-reuse/particle probes await console execution. Flicker removal and crash cause remain unconfirmed; no new NRO package released. See SWITCH.md. |
| Nintendo Switch: native blend-state mapping | P | Switch-only 1+C coefficient fallback corrected from C to the existing desktop One fallback; no claim of exact GS blending. Shared native SDK helper tested on a host GPU against desktop set_blend for 8192 RGBA cases (all 256 equations, enable/disable, fixed and source/destination alpha combinations): zero byte/channel difference, tolerance 1. Negative restores the old mapping and fails (equation 04). Eighteen focused deko checks pass; ARM64 NRO and native artifact audit pass. GPU boot material-blend probe added; console visuals/flicker and Replay Theater crash remain unconfirmed. PC renderer unchanged; see SWITCH.md. |
| Nintendo Switch: scene reset and partial-surface flicker | A | User confirms graphics-audit package still flickers on roof/window/body sections. Console log10: upload/image-blit/reused-DATE/particle PASS, material-blend and mesh/depth FAIL; old probe lacks failing RGBA. Native r3d_begin omitted desktop destination-alpha reset; shared clears now explicitly set write masks and reset depth/DATE alpha while preserving RGB. SDK-header fixture covers 96 prior-state combinations with depth/mask/alpha negatives; 22 focused deko checks pass. New GPU clear RGBA/depth, blend RGBA and twelve-frame/three-slot capture probes await console execution. Root cause of flicker and removal remain unconfirmed; no PC renderer or physics changes in this fix. See SWITCH.md. |
| Nintendo Switch: independent-alpha command encoding | A | Log11 still flickers: clear-after-particle and twelve-frame ring PASS, material-blend actual 128,0,0,255 versus expected 128,0,0,128. Installed deko3d 0.5 encoder omits per-target separate-alpha enable and repeats RGB destination factor in alpha field (source and binary checked against NVIDIA clb197 methods). Native-only helper now sends complete target-0 state through replay API, shared by 2D/3D, with independent alpha factors. Test now decodes emitted GPU packets: 8192 host GPU RGBA cases match desktop with max error 0 (tolerance 1), new missing-enable/wrong-destination negatives fail; 24 focused deko tests pass. Boot checks modes 08/44/48 separately. Hardware log12 disproves this as a repair: modes 08/44/48 still fail (alpha 255, expected 128; 44/48 RGB 128, expected 64), and the user confirms flicker and checker bands persist. A separate 98-case same-console comparison now crosses exact first-build/current shaders, state encoders, fractional-alpha UI/meshes and full-size copies; ARM64 diagnostic builds, hardware results and the actual correction remain pending; no PC renderer changes. See SWITCH.md. |
| Nintendo Switch: hardware-isolated vertex alpha regression | A | GPU Compare on the console completes 98 cases: all 36 current-vertex combinations fail fractional alpha even without blending, while 24 first-vertex/current-or-direct-state combinations pass with either fragment shader. UI fixed state and all eight full-size upload/copy checks pass; nine original SDK blended-alpha cases fail independently. Production vertex alpha now uses an arithmetic mix retaining desktop shadow clamp; UAM TGSI no longer uses the conditional alpha merge. 2160 host vertex cases cover .25/.5/.75/1.2/1.5; forced-alpha-one negative fails; 25 focused tests pass. Game boot separates three unblended vertex-alpha probes from its blend probes. ARM64 game/GPU Compare v2 build; Log13 confirms arithmetic-alpha marker and all three fractional-alpha plus three blend probes PASS on the Switch (one-byte tolerance); user confirms showroom car visibility. Whole-scene image/reflections/checker bands and race-entry crash remain unresolved. PC renderer/physics unchanged. See SWITCH.md. |
| Nintendo Switch: full-image composition bounds | A | nv_draw_mix incorrectly reused 16:9 UI fitting on 640x448 studio and 256x256 environment targets, leaving 44/56 untouched rows at each vertical edge; display itself is already 1280x720. Native-only shared clip-space quad now fills internal targets independently of display layout. deko_composition executes production UI/mix shaders across 15 full-RGBA images at five sizes through 1920x1080, tolerance 1; old-fit negative fails at first pixel. All 27 focused tests pass. New hardware composition-full probes await execution. Actual band/reflection removal and Nürburgring race crash remain pending; no desktop renderer/physics change. See SWITCH.md. |
| Disc replay: surface wave number initialization | E | Original `0035F808 +0x1B8`: native SRT division replaces host division toward zero. `physics_surface_waves` compares all 24 wave numbers against 17 independent RAM captures (408 exact words); the negative changes one bit and fails. E is limited to this table and these fixtures. The full Bentley recording still diverges later and remains A. [Evidence](notes/DISC_REPLAY_SETUP.md#surface-divider-correction-2026-10-14). |
| Disc replay: serialized entry restoration | E | Original `003DDAD8 +0x00`: setup, wear, grid and driver options restored without overwriting runtime links. `disc_replay_setup` compares all 0x3490 destination bytes against original code for 53 entries from the 13 authored US demos and 128 varied records. Negative flips a grid byte and fails. E covers entry restoration only. [Evidence](notes/DISC_REPLAY_SETUP.md). |
| Disc replay: settings and recorded-mode integration | C | Original `003DD838 +0x30/+0x44`, `0034D418 +0x14`, `00387CF0`, `0010BBB0 +0x1AC`: preserve RACE settings independently of world class, RaceInfo options and recorded mode. `disc_replay_bspec_runtime`: all four B-Spec demos, 1200 headless ticks each, driver kind, command count, finite pose and movement; negative drops B-Spec and fails. Eleven focused replay tests pass. Complete trajectories remain A; Bentley / paris_rally still becomes stuck. |
| Disc attract replay: long Rdemo0 trajectory | A | User log identifies Bentley / paris_rally. Existing desktop replay path reproduces near-stationary motion after initial travel over 5400 fixed ticks, while tape and solver counters both reach 5400. Optional disc_demo_probe --theater-long prints poses; finite-pose/command checks are not a trajectory oracle. Switch log retains about 60 solver calls/second, drawing 30.45–54.43 FPS in sustained windows. Original-state comparison and exact divergence cause remain pending; no physics or tape-control patch applied. Existing six focused playback/checkpoint tests pass within their limited scope. See SWITCH.md. |
| Disc replay: restore recorded B-Spec mode | C | 00388E38 +6C..70 stores world+CC4 at GT4M+C; 00387CF0 retains the recorded A/B-Spec choice. Import previously discarded it, leaving four zero-pedal B-Spec demos in A-Spec (including El Capitan/Nissan Rdemo1). Disc-only metadata now selects the existing model-driver path; native replay format and physics unchanged. disc_bspec_playback checks all four authored B-Spec demos, complete zero-pedal tapes, exact imported/running modes, finite poses and movement over 1200 ticks each; negative discards the mode and fails. 29 focused tests pass including native replay round trips, command/checkpoint oracles and graphics checks; Windows/NRO built. C restricted to mode import/driving integration; complete trajectories, separate A-Spec early turns and console crash remain unproved. See SWITCH.md. |

| Item | Nota | Prova ou observação |
|---|---|---|
| Demonstração: isolamento da coleta de telemetria | C | 13/10: race_game_start_impl conservava license_log_collecting da tentativa ao reter a fita para replay. Agora limpa a flag antes de habilitar uma sessão de coleta viva. 0011D1B8 +28..38 isola a tentativa retida na Scene; 00122FA8 +9C..100 é o caminho separado do Logger. disc_demo_probe verifica ausência de concourse/coleta, 1471 comandos B-1 exatos, ausência de prêmio, retorno/cancelamento e preservação da fita, miniatura e Logger anteriores; license_teacher_gold compara estado ouro e tempo com o metadado do disco. Negativos alteram comando, medalha esperada e reativam coleta. C restrito à integração testada, não certifica trajetória/pixels nem todos os demos de abertura. |


| DualShock right-stick default pedal bindings | E | `func_0043B808 +0x28/+0x30`; `controller_defaults_oracle` executes the original initializer and compares six axis bindings on both ports with zero tolerance. Negative removes RYF acceleration and fails. Grade covers bindings, not the raw stick response curve. |
| Older PC profiles: restore missing right-stick pedals | P | The input boundary restores RYF acceleration and RYL braking only when both bindings are unassigned; assigned actions take precedence. `controller_input` checks legacy repair, custom reverse and explicit single-axis unassignment, plus neutral/half/full travel through virtual SDL input. `controller_right_stick_negative` deliberately rejects acceleration. Raw PS2 response-curve comparison remains pending. |

| Item | Nota | Prova ou observação |
|---|---|---|
| Arcade: abrir Options depois de sair da corrida | C | (c) 12/10: 00109D00 +44..58 limpa next_sequence (game+3A368) antes de carregar um menu; o cache nativo conservava single_race. TopRoot.Option do arcade.ad L647..649 só troca next_menu_project, disparando outra corrida sem carro. menu_set_origin reproduz a limpeza na entrada do projeto. options_lifecycle executa esse callback original após o pedido/retorno da corrida, confere rota option sem novo pedido e permite uma nova Time Trial depois. Caso antes da correção falhou; negativo injeta novo pedido e falha. C restrito à fronteira/cache, sem oracle EE integral da sessão. |
| Options: aplicar e persistir unidades | C | (c) 12/10: faltava MOption.apply (004365B8 +4C..68 publica velocidade/potência/torque). UnitRoot.apply do option.ad L4131..4138 agora termina; valores atravessam projetos e perfil. KW authored deixa de ser confundido com label kW; MUnit lê option da VM atual, também usado por catálogo/Logger/replay. Captura de menu sem edições pendentes restaura só opções compartilhadas antes de salvar, evitando sobrescrever escolhas com cache antigo, sem substituir garagem/registros em andamento. options_lifecycle e profile usam os dropdowns/OK/Cancel originais, sete potências, três torques, duas velocidades, reentrada, arquivo standalone e save/load de perfil isolado; tolerância zero para valores, índices e conversões exercitadas. Negativo troca preferência e falha. 16/16 testes selecionados incluindo controles, Setting e ida/volta real do app ao pré-race passaram; Release atualizado. build/options-units.png inspecionada (km/kW/NM), sem upscale. C de integração; comparação visual integral PCSX2 pendente. Física não editada. |
| Câmera cinematográfica manual PC (F9) | P | (c) 12/10: câmera PC para filmagem. Livre, Seguir/órbita/mira e GoPro presa à posição/rotação do carro, troca de alvo, mouse/WASD/QE, suavização, zoom/roll, HUD limpo, hold, foco automático/manual e desfoque opcional com oito amostras GPU da lente Photo Travel. Escolhas PC documentadas em CINEMATIC_CAMERA.md, sem alegar câmera original. race_cinematic confere seed, isolamento das teclas de direção, movimento, órbita/aim, troca do alvo, montagem sem salto/eixos ortonormais, zoom/foco, hold/reset/cleanup, pixels com desfoque e projeção do plano focal (<1e-6); 32 MiB da física byte a byte inalterados antes/depois da manipulação e desenho. Negativo troca byte e falha (2/2); controles de menu/relógio de áudio e negativos passaram (4/4). Capturas sharp/focus locais inspecionadas; Release atualizado. Restrito a corrida/replay 1P; oito passes podem reduzir desempenho. Nenhuma fórmula/parâmetro da física editado. |
| Premiação de evento: troféu e início dos créditos | A | 12/10: corrigido ModelFace sem env_image_path e sem luzes l1..l3; PrizeRoot.model do quick-event.mproject fornece env0.png e segunda luz (90/90/90). ui_prepare_root prepara modelo/reflexo antes de retirar o loader. Retorno da corrida e troca de projeto deixam MScriptEvent doAction (quick-event.ad L899/L920) para menu_advance após loading_screen_active=0, em vez de menu_tick com overlay ativo. prize_model_loading compara RGBA do reflexo com GPB do disco, tolerância zero, e retém evento durante 60 frames de apresentação; negativos sem reflexo/com dispatch antecipado falham. 8/8 testes de premiação/menu/retorno real Options passaram; Release atualizado. Captura local prize-fixed.png confirma desenho, mas entrada direta tem contexto de evento vazio e não é referência de carreira. A para pixels e sequência integral: sem captura equivalente do PCSX2; não certifica HUD de corrida inteira. |
| ModelFace: troféu comprimido horizontalmente | A | 12/10: causa encontrada em video_viewport: o frustum aumentado (-960,-720,2560,1920) era reduzido só em X a 640; largura do mesh ficava 1/4, altura preservada. Restrito ajuste content bounds ao viewport exatamente fullscreen; r3d_begin_ui_environment usa projeção authored sem compensação de aspecto da corrida. 00217058 +2B0..3A8/004A59B8 preservam ambos os eixos do frustum. model_viewport testa extensão x4 e centro nos dois eixos, negativo repõe clamp X e falha; 5/5 com testes menu_projection/scissor. Release atualizado; captura local prize-width-fixed.png confirma deformação removida. A para imagem integral, sem referência PCSX2 equivalente. |
| Replay: textos após preencher o cache da corrida | C | (c) 12/10: limite local de 12 estilos em race_hud.c descartava a fonte dos avisos ReplayMode* e da pausa; a faixa laranja permanecia vazia. Cache agora guarda nós estáveis por estilo, resolvendo as fontes solicitadas como 0044D920 → 0044D9B8, sem invalidar a face ao carregar Gaiji/%F. replay_hud carrega 12 estilos usados por painéis/abertura/música e compara com HUD nova os sete avisos do rtext e a pausa em 4:3/16:9: 14 quadros, zero canais RGBA diferentes; vinte repetições não aumentam o cache. Antes da correção, primeiro quadro falhou com 5022 canais diferentes e faixa vazia. Negativo suprime os glifos e falha. C restrito à integração/cache; coordenadas, métricas, animação e composição integral do replay permanecem A na linha de reprodução. Evidências locais build/replay-hud-{before,after}.png e logs. Nenhuma física alterada. |
| Hodômetro: quilometragem inicial e avanço no Arcade/Time Trial | C | 12/10: split_owned usa corpo original de usado com 85.574.576 m e catálogo com zero; 900 frames por corrida, Arcade/TT e Retry. HUD +6DC preserva origem e cresce com a distância +6D4 (erro <1,01 m pela fração +6D8); resultados usado +31/+228 m, novo +32/+239 m. split_owned_odometer_negative congela leitura e falha. 3/3 testes. Código já funcionava: nenhuma mudança de física/HUD. 003434C8 e 0039E234..27C mostram só inteiros (m/100/10 em km, m/160/10 em milhas), %06d em 003A4158. C restrito à integração dos dados; raster/fonte e execução relatada pelo usuário ainda não comparados. |
| Licenças: assistências efetivas da B-1 | E | (c) 12/10: license_aids_reference compara os enables da entrada +3450/+3454, máscara do carro +5E9 e 52 bytes do banco params+1098 com eeMemory.bin do PCSX2 slot02, tolerância zero; negativo troca byte do nível TCS e falha. O Demio preparado tem ASM/TCS efetivos zero apesar dos enables 1/1. Prova restrita a esses campos da B-1; não certifica pixels do HUD nem trajetórias. Nenhuma fórmula/parâmetro alterado. |
| Licenças: enables de assistência nas 80 provas | C | (c) 12/10: license_aids monta os 80 descritores e carros e confere entrada +3450/+3454=1/1 (003B4B38; treino 0010F880 → 003C0D78/003C0D98); negativo desliga TCS e falha. Níveis continuam vindo das peças/setups do disco: B-3 tem TCS 3, B-1 zero. Integração nativa, sem comparação EE de cada uma das outras 79 provas. |
| Licenças: controle no Retry/Exit | C | (c) 12/10: pause_input entrega D-pad/Cross ao result_license.strb original (003DF978 +D4 → 003EBF40); o aplicativo descartava os eventos SDL esperando essa ponte, que faltava. license_result_input usa pad virtual e confere callbacks Retry/Exit nos resultados fail/ouro/prata/bronze, botão mantido e reconexão. Negativo suprime a borda Cross e falha. Tolerância zero no callback; foco STRB e composição integral continuam A. |
| Licenças guiadas: entrada B-5 e resumo do aluno | C | (c) 12/10: quick-license.ad L225..278 mostra só o aluno; race_game_quick_grid usava a posição do grid físico como índice desse resumo de uma linha, rejeitando B-5. Resumo agora usa índice zero sem alterar o grid da física. license_load verifica carregamento assíncrono, menu e início da tentativa nas 12 provas guiadas (5/10/15 de B/A/IB/IA); negativo zera participantes e falha. App real navegou GT Mode → B-5, captura/log locais em extracted/handoff/license-fixes-20261012. Falhas sem mensagem recebem texto de diagnóstico PC. Não certifica a corrida guiada inteira nem imagem PCSX2. |
| Licenças: rolagem de Information | A | (c) 12/10: corrigida a alocação da altura: 00245950 +38..4C escreve tamanho preferido, não geometria atual; TextFace relativo segue Info (0025B578 +C0..D0), aumentado por quick-license.ad L255..264. Antes, range do texto e barra divergiam. license_information executa os 80 parágrafos do disco, confere última linha inteira no fim, barra no fundo e três ciclos cima/baixo, tolerância 0,001 pixel; negativo suprime deslocamento e falha. Alocação relativa da B-1 também comparada com slot05 PCSX2 (TextFace 01E34740/Info 01E34670), tolerância zero. C restrito à integração/alocação testada. L413..443 move 24 px por cima/baixo; setScrollActor ratio 0,2, repeat desligado no código 002C0568 +94 e na captura. Usuário escolheu preservar esse comportamento; nenhum ciclo automático acrescentado. App real: B-1 recebeu vinte comandos Down e exibiu a última frase inteira; captura information-bottom.png e logs locais em extracted/handoff/license-scroll-20261012. Quebra/equalized, medida integral e pixels continuam A, sem comparação diferencial do layout completo. |
| Licenças: reentrada da barra de Information | C | (c) 12/10: cache reinitializava sem quick-license.ad onFinalize L312, deixando Info.h aumentado; L256..262 ocultava a barra ao reabrir ou trocar para texto menor. menu_quick_event_context/menu_enter_quick_start agora finalizam páginas LicenseProject antes da nova inicialização, conforme recarga 0010FBD8..0010FCA0/0010FE40. setScrollActor (quick-share/icon.ad L46) substitui space.actor; o ator anterior é parado no port para não sobrescrever a nova posição enquanto aguarda GC. license_information confere três reentradas nas 80 provas, reabertura com/sem context e alternância IB-1/B-8/B-1/IB-1, barra visível, topo restaurado e scroll operante (0,001 px); negativo oculta a barra e falha. Antes da correção, o caso positivo falhou na primeira reentrada. C restrito ao ciclo nativo executando scripts originais; pixels/métrica integral continuam A na linha acima. Física preservada. |
| Ranking: algoritmo do passcode | E | (c) 12/10: ranking_code compara 1.024 entradas com 0044B258 recompilada: todos os 32 dígitos idênticos; também confere chave lida do CORE do usuário, schedule 0044B680, bloco 0044B900 e digest 0044ACD8. Negativo troca um dígito e falha. Não embute a chave nem arquivos do jogo. |
| Ranking: geração e persistência do passcode | A | (c) 12/10: curso/Power & Speed deixam de gravar vazio; course_ranking verifica quatro partes de oito dígitos, inserção e save/load com tolerância zero; negativo altera o código esperado e falha. A: campo de um bit não inicializado na pilha do caller 0015B7A0/0015B948 é explicitamente zero; relógio do PC P. Algoritmo E separado acima, caller integral ainda sem oracle. |
| Volante: entrada e saída de force feedback no PC | A | (c) 12/10: SDL detecta volante mesmo sem GameController, publica comando tipo 4 e força existente de 00353EE8/out[2], sem editar fórmulas/parâmetros. wheel_input confere eixos/pedais/botões/hats, 256 bytes de torque, zero/NaN e desconexão por volante virtual e driver gravador; negativo inverte força e falha. C restrito à fronteira testada. P: força constante SDL normalizada, duração 100 ms, eixos raw 0/1/2; pausa/foto/saída param a força. A: sinal, calibração, disposição física dos botões e G29 sem ensaio real. |
| Battle2P: seleção sequencial e estado dos jogadores | E | (a) 11/10: `split_select` executa os callbacks do `arcade.ad` original: P1/P2Select → MakerRoot → CarRoot → pintura → SettingPopup.OK. Confere foco em P2 após P1 e em RaceStart após ambos, cancelamento, troca de P1, mesmo modelo com pinturas distintas e opções separadas. Três negativos alteram código, pintura e disponibilidade e falham. Corrigidos `SliderBar.range` (002DFA10) e foco solicitado antes de `RaceStart.visible` em `SplitRoot.onInitialize`. Nota restrita ao fluxo/estado, sem prova dos pixels. |
| Battle2P: duas prévias e catálogo normal | A | 11/10: cada CarFace retém pintura e showroom residente próprio; renderer do SplitRoot agora desenha os dois modelos e a rotação do script. Captura local `extracted/handoff/split-select-20261011/selected-final.png`, sem comparação integral com PCSX2. Catálogo usa os 240 carros de `boot/US_carlist.adc`, chaves importadas e override de disponibilidade (0042EAC8 → 0042EA80); ponte de perfil ainda sem oracle EE integral. Iluminação/reflexo/apresentação continuam A; garagem 2P integrada, prova separada abaixo. Release e 20/20 testes de seleção, controles e split-screen passaram. Física preservada. |
| Battle2P: carros da garagem na corrida e Retry | C | 11/10: `menu_split_owned` resolve car_garage/car_garage_2p quando car_code está vazio (arcade.ad GarageRoot.onActivate), entrega corpo 0x4B8 ao construtor existente e lê pintura do setup ativo (00441248, +18). `split_select` executa seleção original e confere dois corpos/pinturas; `split_owned` inicia P1 próprio/P2 catálogo, P1 catálogo/P2 próprio e ambos próprios, compara os setups inteiros 0x178, corpo/desgaste 0x4B8 e Retry; tolerância zero. Negativos trocam o carro de P2 e removem a entrada própria de P1, ambos falham. Não certifica a imagem nem montagem EE ponta a ponta; comparação visual PCSX2 pendente, sem savestate disponível. Nenhuma fórmula/parâmetro da física editado. |
| Arcade 1P/Time Trial: carro da garagem | C | 11/10: GameRoot.doGame do arcade.ad também escreve car_code na seleção 1P; car_garage continua sendo a escolha própria e tem precedência na ponte. `race_game_set_arcade_owned` copia corpo 0x4B8; construção e Retry recebem o setup ativo, peças e desgaste, pintura final lida da entrada. `split_select` confere corpo próprio com car_code preenchido; `split_owned` inicia Single Race e Time Trial e repete ambos, compara setup 0x178/corpo inteiro com tolerância zero. Negativo compartilhado remove corpo na fronteira e falha. Integração C; apresentação integral e oráculo EE de ponta a ponta pendentes. Não altera física. |
| Battle2P: boost, handicap e desgaste | E | (c) 11/10: `split_battle_oracle` compara retorno e bloco inteiro com EE original 003471A8, 00347188/00354EB8 e 00347108/0034D048; 28 casos, ambos os índices, negativo altera byte de boost e falha. Apenas integração de sessão; fórmulas/parâmetros da dinâmica preservados. |
| Battle2P: dados do HUD horizontal | E | (c) `split_layout`: 13 posições, dimensões e alinhamentos diretamente contra D_006A0DE0; altura/transformação inferior 240 contra instrução 003A23C0 (0039F698 +20..30). Negativo desloca cronômetro e falha. Prova dos dados, não do desenho completo. |
| Battle2P: retângulos dos espelhos | E | 12/10: `split_layout` compara dez constantes diretamente às instruções de 0032FCC8: P1 (200,16,240,40,2), P2 (200,8,240,40,2), em coordenadas de cada metade. Negativo altera Y e falha. Nota restrita aos dados. |
| Battle2P: imagem dos espelhos | A | 12/10: bumper/roof desenham cena traseira refletida no recorte de cada jogador, escondendo apenas o próprio carro e restaurando câmera/culling. `split_replay` verifica alterações dentro dos dois retângulos e zero pixels alterados fora; negativo corrompe um pixel externo e falha. Captura local build/split-mirror.png. Pose, projeção e condição de exibição A; comparação PCSX2 pendente. |
| Battle2P: replay de duas fitas e carros próprios | C | 12/10: `split_replay` grava 1200 passos com comandos independentes, empacota/lê duas fitas e compara 2400 poses e velocidades byte a byte na reprodução; conserva dois corpos de garagem 0x4B8 e setups 0x178 separados. Negativo troca as fitas e falha. Prova da integração nativa, sem oracle PS2 da sessão inteira. Extensão SP2R e atalho F6 P; apresentação/ativação/relógio A. Replay Theater lê o arquivo; nenhum solver/parâmetro de física alterado. |
| Battle2P: corrida local, câmeras e apresentação | A | (c) 11/10: Arcade/SplitRoot original inicia dois carros humanos; configuração 2P de pneus, câmbio, assistências, boost/handicap/desgaste. Uma simulação, dois passes 3D, câmeras/comandos/HUD próprios. `split_battle` exercita entradas isoladas, dois fins, reinício e contagem inválida; `split_input` usa dois pads SDL virtuais, desconexão, remapeamento e bordas de câmera; `split_menu` executa RaceStart original, pinturas/pista, resultado e pausa Retry/Exit; `split_viewport` confere pixels, recorte dos dígitos e restauração da tela. Todos com negativos; 17/17 CTest selecionados incluindo regressão áudio/controle. App real: Ka/Today em Tsukuba, captura compact2, Retry e retorno ao Arcade, em extracted/handoff/split-20261011. A: lente herdada (sx 1.73205, fator vertical 1.30122, near 0.1), câmera/animação inicial e composição integral, timing dos resultados esperando ambos os flags, reflexos centrados em P1, áudio com listener P1, montagem do grid/intro herdada. 12/10: espelhos e replay de duas fitas implementados, com provas separadas abaixo. Teclado P: P1 setas/Space/V/PageUp/PageDown; P2 WASD/LeftCtrl/C/Q/E; Escape pausa. Nenhum solver de física alterado. |
| Câmera de trás (posição e orientação) | V | olho bate com a captura em 1e-4, conferido à mão (c); automatizar para E |
| Câmera do teto | V | idem, captura `roof` (c) |
| Câmera de para-choque | V | sem o tremor da câmera: essa parte é A (c) |
| Projeção das câmeras | V | valores da captura 0x01CE9860 (c) |
| HUD: posição, volta, tempos, cronômetro | V | posições da memória capturada; desenho lido do código (c) |
| HUD: painel simples (velocidade, giros, marcha, pedais, G, sinais, combustível) | V | (c) |
| HUD: painel de bordo (velocímetro e conta-giros redondos, faixa vermelha) | V | (c) |
| HUD: mini mapa | V | (c) |
| HUD: passo fixo dos dígitos | A | deduzido do desenho da fonte e do print; falta ler no motor de texto do jogo (c) |
| HUD: unidade de velocidade | A | segue a opção `unit_velocity` (KMPH/MPH, nomes em 0x005A6978 da classe GranTurismo4Option); imagens e escalas km/h já lidas do código. Falta comparar a tela em km/h com o PCSX2 (c) |
| Mensagens START, FINISH | V | (c) |
| Mensagens do sinal (FULL THROTTLE, GET READY TO DRIVE, GO!) | V | posição e fonte do código; box e contramão não vistos em tela (c) |
| Mensagem de penalidade ("%d sec. penalty") | A | `func_0039D6D8`: enquanto o carro não terminou e +0x712 (`func_0035FC60`) tem quadros, a segunda mensagem mostra o rtext "Penalty" (índice 0x49 da tabela, +0x124) com os segundos arredondados para cima, em 0x800000C8, entrando em 0,2 s, no lugar do sinal. Visto com uma penalidade forçada; falta ver uma penalidade real (opção de penalidade do Arcade) lado a lado com o PCSX2 (c) |
| Largada: câmera alta e baixa (movimentos da pista) | V | (c) |
| Largada: campo de visão dessa câmera | A | estimado a olho; investigação em andamento (c) |
| Largada: HUD entrando aos 4,5 s | V | (d) |
| Largada: textos A-spec, logo da pista, números 3-2-1 | V | (d) |
| Largada lançada: textos | V | (d) |
| Largada lançada: câmeras antes da linha | A | câmeras de TV da pista (REP4 +0xB0, `course_tvcam.c`, de `func_0037C138` com as molas do operador `func_00384E58`/`00384EE8`) enquanto cond+0xCBE4 está ligado. O seletor `func_00372DF8` -> `func_003744F0` escolhe o tipo 1 (ShotManager, tabela 0x0069F9C0 -> `func_003FD980`) antes de race+0xBC; o momento exato da volta para a câmera de condução não foi lido. Falta comparar com o PCSX2 |
| Replay: reprodução | A | como o jogo (`func_003F59A0`, método 0xC8 da sessão: o comando de 0x20 bytes do carro do jogador vai ao gravador do mundo, vtable +0x240 -> `func_00426C50`), a corrida é refeita pela física com esses comandos; com direção variando, posição do jogador idêntica ao vivo e no replay em 1.500 passos (`MOON_RACE_REPLAY_LOG`). Abertura: a câmera da largada (`func_0037D620`) com o banco Mot1 +0xD4 e a tabela `D_00621290` (5 movimentos: <1 s o 5º ... <11 s o 1º), sem espelho nem quadro do grid (+0x1D0); depois as câmeras de TV. Sem HUD, como o padrão de `replay_display_enable` (o que ele mostra ligado não foi lido, P). Pausa: Select segura o replay com a faixa de `func_003C3B08` (SimplePauseMessage "Press %Bb to resume", hn_14 a 107%, cor A5A7A5, faixa preta a partir de 438/480; %B<c> na fonte Gaiji como `func_004940E8`), lado a lado com o savestate rp_03: texto e posição iguais; O nome da música (RaceMusicDisplay, RaceDisplay +0x3EEC, `func_003A22A0`/`func_003AE350`): a cada faixa nova, com o display aceso e sem mensagem, depois de 0,5 s por 10 s, entrando e saindo em 1 s; a nota (Gaiji n) em (25, 432), o título em hm_16 a 90% em (41, 432), o artista em hc_20 a 70% 18 abaixo, cor C8C8C8, título e artista do registro do ads.inf (+8, +0xC); lado a lado com rp_04: posição, tamanho e largura iguais (A: `func_0044DFA0(2)` tomado como espaço de 0,7 entre letras, o traço do PCSX2 um pouco mais grosso). Start sai. Depois dos resultados o replay começa sozinho da abertura e termina no menu (usuário, PCSX2). Botões (mapa do usuário no PCSX2): Quadrado cicla Normal -> Dive -> Display (o HUD da corrida, painel simples sob a TV), Círculo a música, X a câmera do carro, L3 o avanço (2x: o player do replay sobe a velocidade de 1 em 1 até 3, `func_003D9FB8`; A: o tratador do L3 não lido), cima/baixo o carro seguido. Placa `replay_plate` laranja 228,132,0 com o rtext ReplayMode*, logo `JP/gt_logo_mini` fora do Display: lado a lado com rv_02..04, posição e tamanho iguais ; a placa entra em 0,25 s, fica 2,75 s e sai em 0,25 s (envelope +0x54 de RaceReplayModeDisplay em rv_01..06). Dive (tipo 8, `func_00376638`): diretor com as tabelas D_006210B8/E0/108 pelo tempo até a próxima marca da música (`func_00400A18` lê o objeto em corrida +0x84 +0x74, `func_0033D170`: a 1ª lista do `.adm` da faixa, "BMRK", posições em amostras, lida com o `.ads` por `func_0045FFF8`; até 8 s copia, sem marca conta 8 s e corta; passar uma marca corta; o diretor só roda num corte, `func_00376AF0`, e a tomada dura até o próximo). Conferido em rp_04: a faixa é race_35 (101 e 104 marcas), cursor 7, próxima em 1,67 s = timer +0x18, e a câmera de ponto (+0x170, `func_0037E4D8`/`func_0037EA78`) nos pontos do REP4 +0x30 (0x30 bytes: posição, trecho), escolhidos e pontuados como `func_00375F58`/`func_003FF3E0`/`func_003FF1B0` (melhores três 70/25/5); lente por tamanho sorteado na distância (modos de ponta a ponta ou até o ponto), mola de razão 0,91..1,03, operadores, inclinação e soltura no fim do trecho. Conferido com rv_04/rp_04 (Laguna Seca): ponto, trecho, meio do trecho, tabelas de lente e duração batem. Tipo 0 (`func_00376238`): câmera presa ao carro num ponto do modelo (30% o 0, 30% o 5, senão um dos que o modelo tem, sem repetir), lente 60..80 nos pontos 0 e 5, pelo tempo pedido; 40% das vezes uma rolagem de ±20° até outro ângulo pela curva (`func_00379FA8`, lida em `func_0037A458`), fixa nos pontos 0, 5 e 14..17 (A: os pontos do modelo do jogador para qualquer carro; o +0x269 só tem o sorteio consumido). Tremor de mão (+0x265, 30% nos pontos 0 e 5, `func_0037A458`/`func_00384A70`): dois desvios com alvo sorteado em 30% dos quadros, força pela velocidade de 10 a 80 km/h, 0,9° e 0,3° no ponto 0 (`func_0037B020`), 0,7° no 5 (A: a condição de `func_0037B580` e o gerador próprio +0x1B4 não lidos). Tipos 1..3 a órbita (+0x180, `func_00380E48`/`func_003814A0`), 4..6 as fixas (+0x174: à frente no caminho, ao lado da pista, junto ao carro; `func_00381D88`/`003821F0`/`00382708`), 7 o helicóptero (+0x188, `func_003834A0`/`003837C0`). Paredes (`func_003FE7B0`, com as paredes da física): do ponto de partida até o olho em passos de 4 m (até 10), a primeira parede cruzada de frente puxa o olho para trás pela margem; tipo 4 a partir da linha da pista na distância do próprio olho (`func_003955E8`, margem 0,5), tipo 5 da linha da pista (1,2, com `func_003FCB20`), tipo 6, órbita e helicóptero a partir do carro (0,3); os tipos 4 a 6 sorteiam os ângulos mesmo quando falham, como o jogo. Filtro de cor (display +0xBC0, `func_004007D8`/`func_004004A8`): a cada corte sorteia nenhum, um dos 21 tons de D_00622700 ou um dos 12 tingimentos de D_006228F8 (chances pelo estado atual: de nenhum 0/35/15%, de tom 25/25/15%, de tingimento 25/15/5%; o resto mantém), 20% das vezes entra aos poucos (1,5..5,5% por quadro), senão de vez. rp_04 tem D_006228F8[1] (1; 0,6; 0,35) ligado e a pista sai nessa proporção (cerca 0,65/0,36) com os carros sem filtro; o nosso dá 0,60/0,36 na pista e o carro azul intacto. A: o passe que aplica o filtro não foi achado (aplicado na pista e no céu, não nos carros); saturação, deslocamento e mistura dos 3 últimos valores deduzidos das tabelas (sépia, preto e branco); tipo 6 no giro inteiro do carro (`func_003FCEA8` com flags 7), órbita e helicóptero só na direção (flags 2); o valor 1,4 que a órbita e o helicóptero guardam ao bater numa parede (+0x22C/+0x30C) não usado; o triângulo do chão do carro (+0x5AC) procurado de novo; as tomadas forçadas no pit (`func_003FD620` 1 e 3) não feitas; distância própria do ponto = meio do trecho; a hora da música é a do nosso fluxo (A: o atraso do fluxo do PS2 não lido). Triângulo: zoom 4 no +0x14C da câmera de TV (4,0 em rv_01, 1,0 nos outros), usado como em `func_0037C138` (0x0037C1A4 só nas tomadas no carro; enquadramento max(1, lente/zoom) e lente dividida em 0x0037C888). Volumes `replay_bgm/se_volume_raw`. Salvar o replay não feito (P) |
| Replay: zoom do Triângulo | E | O Triângulo liga e desliga a bandeira da vista (+0x1E); a cada quadro `func_003751F0` passa ao +0x14C 1 + 3 x posição/20 entre 1 e 4, com a posição da mola `func_003852A0` (alvo 20 ligado, 0 desligado; entrando, rigidez 40, amortecimento -10, /60, velocidade entre -900 e 900; saindo, x0,8 por quadro até o alvo) lida por `func_00385370`. Antes o zoom pulava de 1 para 4 (relato de teste: no jogo a aproximação é rápida e suave). `tvcam_oracle`: 400 passos ligando e desligando, estado e zoom iguais bit a bit ao original recompilado; o negativo muda a velocidade e falha (c) |
| Corrida: sons na pausa | A | Abrir a pausa (`func_0038B428`) chama o vt+0x2D0 do mundo (`0033C3E8`): `0038A010` passa cada carro por `00392FC8` (vozes desligadas) e `00399300` para o ambiente com o reverb em 0FFFh; continuar (`0038B4B8`) chama o vt+0x2C8 (`0033C3B0`): `00389F90` reinicia os carros (`003924D0`) e `00398B58` marca o ambiente. Conferido pelo traço: com a pausa aberta nenhuma voz toca; ao continuar as 46 voltam. A: o efeito +0x98E8 do mundo (`003D33C0`/`003D3358`, efeito 0x46) não tem voz aqui; sem comparação sonora com o PCSX2 (c) |
| Arcade: cached return destination after the quick menu | E | `arcade_return` compares the destination with authored GameRoot.SingleRace and ArcadeProject.onLoad (arcade.ad L1493..1512, L43..69): normal CategoryRoot, favorites-only CourseRoot, retained menuRef. Cached CarRoot no longer overrides major_menu_page. Negative injects CarRoot. Proof covers routing, not complete reload presentation or background dimming. |
| Corrida: luz de freio acesa | A | O ramo 1 do callback 0 do carro (`00453698`: car+0x38, pedal +0x590 acima de 0) desenha a lanterna acesa (no Ka a textura 48, vermelho 255 com o brilho no alfa, blend 0x44). No savestate do usuário (PCSX2, Ka freando em Laguna Seca, `extracted/handoff/brake-20261011`) ela sai em 255,8,10; com a luz da cena sairia em ~85, como saía aqui (relato: freio não parecia acender). Agora o ramo aceso é desenhado sem a luz da cena: 197..250,7,8 no mesmo quadro (`MOON_RACE_SNAPSHOT`), lanterna solta e carroceria inalteradas; adversários acendem pelo mesmo caminho. A: o código original que tira a luz desses triângulos não foi achado (material igual ao resto, sem cor por vértice, nenhum comando no ramo) (c) |
| Corrida: START e o som da largada (lançada e parada) | A | `func_0039E9B0` (RaceDisplay, a cada quadro) lê cada evento 4 da fila (+0xF4, `003B77E0`) do carro observado (+0x2F) ou de nenhum, com a contagem dele já em 0 (`0034C1C8`): chama o START (vt+0x140 = `003A1488`: RaceStart por 1 s, saída em 0,15, só com +0x20 abaixo de 2, isto é, na corrida) e liga D_00621515, que `0039DA80` toca como count_s. Antes o START e o count_s saíam da contagem chegando a 0, que a largada lançada não tem (race+0xBC já 0): nenhum dos dois aparecia (relato com print do PCSX2, START a 4,7 s do cronômetro). O evento 4 vem de `00347928` quando o carro 0 cruza a largada saindo da linha de pit. Conferido: Nürburgring e Tsukuba (lançadas) com um count_s e START na tela a ~6 s; Grand Valley East (parada) com três count_d e o count_s. A: nossos eventos postados com 0 ficam na fila até serem tirados; quem os tira no PS2 não foi lido, por isso um evento novo é contado pelo aumento (c) |
| Corrida: nome da música só no replay | E | `func_003A22A0` sai logo quando `003A1628` dá RaceDisplay +0x20 abaixo de 2: 0 em todas as capturas de corrida (ar_*, s0*, o savestate do usuário em Laguna Seca) e 2 nas de replay (rp_*, rv_*). A corrida não mostra o título (nem apaga o logo); o replay segue como estava. Relato do usuário: no original o nome não aparece correndo (c) |
| Corrida: chama do backfire | A | `func_00452AC8` (passe da corrida, a2 0): cada saída do escapamento (CARINFO +0x38 contagem, +0x3C posição e direção, 24 bytes cada; `gt4_car_exhaust_points`) toma a partícula k & 3 de `003D6D30` (tempo, tamanho, giro); acesa (tempo acima de 0), na posição da saída, Ry(atan2(dx, dz)) Rx(-asin dy) Rz(giro), escala D_008465D8 (1,0 no savestate do usuário) x tamanho, o modelo 0 de `race/after_fire.mdl` (cone de fogo somado, blend 0x48, sem luz) no quadro que o `AfterFireCallback` (`00452F28`) escolhe: (int)(tempo x 6) dos 6 ramos do callback 0 (`gt4_model_decode_branch`). Conferido forçando a chama no savestate do usuário (Escort WRC, Laguna Seca, slot 2): sai do cano esquerdo traseiro; os quadros acendem, explodem e somem. A: a transição entre quadros (`00456850`, a fração), a cor de `004A60E0` (D_008465C8 0,52/0,47/0,45) e o passe a2 1 (modelo 1, brilho no chão) não feitos; sem print do PCSX2 com a chama (c) |
| Corrida: HUD em 16:9 | E | `func_003AFD18`: com D_006184F0 (tela larga) e o bit 4 no alinhamento do elemento, a posição anda 96 unidades para a esquerda, ou para a direita com o bit 8. Alinhamentos tirados do RaceDisplay das capturas ar_02 e rp_04 (byte +1 de cada elemento): posição e volta (e seus rótulos) 0x06, Best Lap e Last Lap 0x0E, mapa 0x04, cronômetro 0x02; painel simples: velocidade, conta-giros, marcha, luz de troca e o painel da direita 0x0D, combustível e pedais 0x04, avisos 0x06, medidor G 0x02; o painel de cockpit sem bits (fica no meio). A tela larga é a do app (proporção 16:9 das opções de vídeo). Conferido em 16:9 nas duas câmeras (prints): os elementos vão para as laterais e o resto fica no centro, como o original. A: os elementos de B-Spec, licença, replay e tela dividida não revisados (c) |
| Replay: abertura (carro, potência, pneus, modo, logo da pista) | A | `func_0039BEC8`/`func_0039C028` (RaceDisplay vtable +0xF0/+0x100) chamam +0x168 = `func_0039C838`: logo da pista (+0x11B0) e duas mensagens, 0,1 s de atraso, 6 s, entrada e saída em 1 s; +0x267C o nome do carro (entrada +0x348C) em hb_24 em (320, 220), +0x27CC em hm_16 em (320, 260) o RaceInfoFormat "%s  %s  %s": potência "%d%s" (`func_00364868` monta o registro do acerto com D_0069F780, pico de `func_0034F3F8` + `func_00356EA8`, lido em +0x1700 por `func_00364E78`; unidade por `func_004729A0` truncada; "--" se GENERIC +0x12 bit 3, `func_004489A0`), pneus "%s/%s" de TireShort_%02d (`func_004462E0`/`func_00446320`: linha +0x60 campo 0x3A, +0x68 campo 0x38; "-" sem pneu ou acima de 12, `func_003B03B8`), modo pelo rtext (ARCADE_SINGLE_RACE, ARCADE_TIME_TRIAL, GTMODE_EVENT_RACE). Logo GT4 (+0x1170): cada faixa nova esperando o título o apaga (`func_003A22A0` -> `func_003A3AA8`: some em 0,5 s, 11 s, volta em 3 s; desenho `func_003A3A00` = 0,5 x luz x (1 - isso)), então na abertura só o logo da pista aparece. Conferido com o print do usuário (Time Trial, "299HP  RH/RH  Time Trial") pela composição; Ford Ka '01 no Arcade dá "60HP  SM/SM  Single Race"; replay idêntico ao vivo nos 64 pontos de `MOON_RACE_REPLAY_LOG` com o cálculo no começo. A: quem chama +0xF0 e quando (tomado como o começo da fita); os modos +0x1C 8 e 9 que pulam o texto e o número do modo (licenças) não usados; outros modos da sessão (vtable +0x308) não lidos |
| Replays salvos: disco do PC no lugar do memory card | P | Camada de PC (docs/EDITIONS.md). Mesma tela e mesmo script do labomode; o gerenciador dá `free`/`realFree` com o espaço livre real (KB) do disco da pasta de saves (`plat_disk_free`), nunca 8000 (o "não formatado" do script); conectado e formatado sempre. Textos trocados em `ui_translate` com os mesmos argumentos: `n_kbfree_slot_n` "%K free : Hard Disk" (ex.: "17.3GB free : Hard Disk"), os `not_enough_space_slot_to_create_*` e `create_*_need_n_byte_confirm` de replay, film, foto, playlist e slideshow "on the hard disk"; o `format` aceita `%K` (KB escrito em KB/MB/GB/TB, uma casa) e `%_` (argumento descartado: o slot). Ícone `image/labo/memory_header.png` trocado por um HD desenhado pelo código, 30x30, corpo branco tingido pelo widget (azul normal, vermelho falta/gravação) sobre sombra preta suave como a do cartão, com prato, eixo e braço recortados. Linha "MEMORY CARD slot" (Composite McSlot) das opções do labomode escondida; o painel empilhado fecha o espaço. Os diálogos do labomode (OptionReplayDialog...) têm o onLoad rodado com a primeira página, porque o open() move o actor do Pane (setMoveActor) antes do pushPage; antes a janela Replay Settings não abria. A: o ícone é provisório; quando o jogo roda o onLoad dos diálogos não foi lido; o diálogo do álbum põe o foco na linha escondida (nota histórica: álbum/fotos já implementados; foco/composição integral A) |
| Replay: fim da gravação e lista do Replay Theater | A | A fita para na chegada do jogador: `func_003BA260` no estado 4 chama `func_003BA3E8`, que fecha os gravadores (`func_00346498` liga +0x38 e `func_00346138` não grava mais); antes a fita seguia com a tela de resultados e o replay mostrava o carro rodando sem fim. Teste: fita até o passo 12600 (antes 19200), replay idêntico ao vivo nos 42 pontos. Lista: textos sem fonte (data, pista, carro, tamanho) na fonte padrão do motor hn_14 (006991A8), lado a lado com o print do PCSX2: tamanhos iguais e a data termina antes do ícone ListCheck; carro pelo nome do CAR_NAME ("Ford Ka `01"), não o rótulo; miniatura capturada só da área do jogo, sem as faixas da janela. A: o estado 4 tomado como a chegada do jogador; título "Arcade" de uma corrida avulsa não lido |
| Replay: tela Save Replay e Enter Filename | A | SaveReplayRoot do labomode lado a lado com os prints do PCSX2: painel New Replay, "Create new file" no meio do item (alinhamento padrão 4 do construtor 00246F58, centro/meio, agora também no labomode), moldura e "Create new file"/"Overwrite" só no item sob o cursor (`visible_condition 3` sem `visible_compare`: o estado inteiro, ativo 1 + item focado 2). Título pelo modo: o arquivo dá `racemode`/`racesubmode` e o título é o rtext RaceModeName do modo (ARCADE_SINGLE_RACE "Single Race", ARCADE_TIME_TRIAL "Time Trial", GTMODE_EVENT_RACE, GTMODE_CHAMPIONSHIP), ou o nome que o jogador deu. EntryRoot: InputTextFace com `value`, `putString`, `backspace`, `left`, `right` e `max_input_length` 16, texto na cor do modelo e cursor de texto da altura do campo; o nome vai para o arquivo (bloco "TITL" depois da miniatura; arquivos antigos seguem válidos, `tests/ui/test_replay_file.c`) e Overwrite grava sobre o arquivo escolhido. Fluxo testado: Create new file -> teclado com "Single Race" -> enter -> confirmação "...created on the hard disk" -> "Save successful" -> item novo na lista. A: o cursor de texto fixo (piscar e largura não lidos); o submodo das licenças não usado; a posição da seta do cursor no teclado difere um pouco do print |
| Tune Shop: compra e instalação de peças pelo menu | A | (c) 09/10, Lancer Evolution do perfil da Sunday Cup, vindo da Mitsubishi (pilha "japan,mitsubishi"): a loja Ralliart abre com logotipo e imagem (getSubTuner), as categorias acesas por `canTune` (00140D68: preparadora do carro GENERIC +0x10 ou TUNER_LIST, nomes pelo enum 00448DC8, desconhecido -1); Exhaust lista Sports/Semi-Racing/Racing com preço; a janela mostra antes/depois (250 -> 262 HP), Cr. 1.550; Buy compra e instala ("These parts have been purchased", "Parts have been equipped"); reabrindo, saldo 99.668 e a peça marcada, "You have already purchased these parts". Corrigido no motor: `onLoad` dos widgets filhos chamado pelo motor ao carregar a página (TuningRoot::Pane define `parts_types` ali e o script só chama o `onInitialize`), `isReversibilityType` (0044A218, tabela jtbl_006A7AB0), `interpolateY` (0025C0A0), `movetoLast`, e o `MCarGarage().copyFrom` amarrado como carro da garagem (a prévia da peça é feita numa cópia). A: a potência antes/depois não foi comparada com o original; "?Price" com um caractere especial não desenhado; a instalação dos outros tipos de peça e o efeito na física ainda sem oráculo |
| Tune Shop: janela de compra (potência antes/depois, preço, saldo, rótulos das peças) | V | 09/10, lado a lado com o print do usuário no PCSX2: Honda CR-X SiR '90 usado, HKS, Exhaust & Air Filter: Sports, 144 -> 147 HP, Cr. 1,500, "You have Cr. N,NNN", chips SP/SR/RC. No port, CR-X usado (MOON_TEST_CAR=crx_sir_90:20000000, makeUsed + addUsedCar) dá os mesmos números; novo dá 160 -> 163. Corrigido: (1) `gt4_career_vehicle_stats` deixava os ajustes da carreira (D_0069F780) em 0x620CD8 durante 00361908, e o fator de desgaste (00350868) lia limites inválidos e dava 1; o original (00146648 -> 00364868 com car+0x4A0) só os passa ao 003F3B68. Desgaste máximo: 0,95 x 0,95, 160 -> 144. (2) `main::game.money` lido pelo script é a string de 0042E6B8 (atributo 00163708; gtmode.ad L302 "%s".format, L7787 "Cr. " + money), via `ah_host.read`; sai o remendo que agrupava dígitos só no cabeçalho. (3) `parent.movetoLast(child)` (L7444 Spec.movetoLast(Spec.label)) movia o pai: rótulos SP/SR/RC ficavam sob o chip. Sonda: `gtmode_tune_power_probe ISO label --mileage N...`. O "●" de "●Price" (U+25CF): códigos de 0x300 vão à fonte KNJI /font/jp_menu.kf (00108D80 região "us"), subfonte e célula pelos bytes +0x10..+0x16 da face (00492748/00492198); diâmetro, linha de base e borda esquerda a até 1 px do print (docs/TEXT_FORMAT.md) |
| Tune Shop: compra pela tela em todas as categorias, NA Tuning e layout | V | 09/10, segundo print do usuário no PCSX2 (mesmo CR-X usado, HKS, NA Tuning: Stage 1, saldo 4.586): 144 -> 162 HP, Cr. 4,600, "You have" em laranja (money_red), "●Price", chips 1/2/3; o port dá os mesmos valores (MOON_TEST_MONEY=-5414). Logo da preparadora: o .img de 378x135 desenhado no cliente da caixa Logo (340x121,43, 002A2608), a até 2% do print (antes 11% maior). Imagens das peças: hidden::pict anexado herda o tamanho da moldura (270x127), como pedem pict.w*índice*2 e a rolagem PartsPict.w*i*2 (gtmode.ad L7344/L7452); as três de escape inteiras. Compra: Exhaust, Brakes, Engine, Drivetrain, Turbo, Suspension, Tires e Other compram e instalam sem erro de script; NA Tuning (tipo 13) zera turbo e supercharger (L7521) pelo buffer do contexto (001403A0 +0x30), que agora começa sem células escritas em vez de falhar. Janela "In progress": FBox empacotada trata Composite como caixa (is_face só aparece como 0 nos modelos) e centra quem expande sem preencher (Bar 280 em 12, OK 140 em 82, de 304). Texto do GT Mode no espaçamento de 00492978 (pares, bearing só depois de glifo): título "Tune Shop" (tr_30) 121 px como no print (era 127), preço 103 (era 108); largura medida pelo alinhamento (00491418 tira meio espaço da caneta só do alinhado à esquerda; centro e direita pela caneta inteira, 00491220): preços e potências a até 1 px dos prints, cabeçalho (ícone Cr e nome do carro, gtmode.ad L335..L341) idêntico. Barra "In progress": ProgressFace desenha a fração (agora cheia) e a imagem ocupa os 280x24 (002A2608). "Cannot equip/apply": isExistParts pelo seletor 0044A048 (E no gtmode_parts_oracle), recusas vindas dos dados do carro. "Power: ----" do carro completo: o script (L12868) mostra "----" com potência 0, que GENERIC_CAR +0x12 bit 3 dá. Prints de 09/10 também conferidos: Turbine Kit Stage 1 144 -> 171 por Cr. 4,500; Transmission Close 144 -> 144 por Cr. 5,500; Suspension Sports 144 -> 144 por Cr. 3,000; ícones das listas (bloqueados inclusive) e números cinza quando a potência não muda iguais. A: partes pretas no modelo do carro completo (renderizador) |
| Resultados: linhas, ordem, tempos, diferenças | V | lógica de `func_003DF3B8`/`func_003E0030` (c) |
| Resultados: texto (escala, espaçamento, margem) | A | medido contra print (c) |
| Resultados: ícones dos carros | A | Vista lateral de `func_00393D20` (ramo ortográfico): luz 1 sozinha de `func_00394A10` (ambiente 0,8, difusa 1,5, especular 1, direção (1; 0,5; −10,5) passada pela modelview do ícone), reflexo com a imagem em degradê 128x128 (oito faixas brancas→pretas na metade de cima) e a cor Fresnel da pista sem a duplicação do quadro. O halo borrado (9 toques) só existe no ramo em perspectiva, não nos resultados; a malha de sombra (modelo 2) fica de perfil e não aparece. Lado a lado com u30: Ka quase igual, S500 um pouco mais claro. Não feito: render a 2x com redução e os passes de alfa; a direção renormalizada depois da escala é suposição (c) |
| Resultados: liberar o botão de pular | A | momento escolhido sem evidência (c) |
| Resultados: som de confirmação | A | result.strb chama play_sound("ok"); toca a sequência 6 do gt4sys.ins (GTSE "ok") pelo sequenciador de efeitos do driver no nível de SE da corrida. O callback original de play_sound não foi localizado no CORE (o nome não está nas strings), então volume e caminho são suposição (c) |
| Pilotos nos carros, animação | V | (d) |
| Fechamento dos dedos: controle escalar isolado | E | `004104B8`: `driver_pose_oracle`, 17.920 casos bit a bit; estados de mão/marcha, fases e passos de 60/120 Hz. Mutação `--finger-negative` falha como esperado (c) |
| Fechamento dos dedos: desenho das mãos | A | Duas poses e expressão de grasp lidas dos MDLS; GPU interpola posição/normal/UV. 12 mãos dos seis pilotos comuns/clássicos auditadas; teste GL com controle negativo. Normalização/rasterização OpenGL não equivalem automaticamente ao VU/GS; comparação de imagem com PCSX2 ainda pendente (c) |
| Freios, rodas, conversíveis | V | (d) |
| Carros: UVs da pintura nos modelos comprimidos | A | `0049D328`, pacotes da forma +0x10: V2-32/16/8 assinados em 8040+n, STROW/STMOD; C040 V2-8 é reinício de strips. `model_stream_tests` verifica os três formatos, modos e offsets, com `--negative` que falha; captura nativa do Ka com capô roxo. Falta comparação frontal com PCSX2 (c) |
| GT Auto: roda/spoiler instalados chegam aos modelos da corrida e foto | C | (c) 12/10: `fitted_purchase` compra brdg001w/p001 pelo script original e confere rótulos antes/depois de Auto Save/reload. `fitted_parts` carrega assets originais: geometria/UV com tolerância absoluta 1e-5, contagens exatas; roda muda o aro e wing muda a carroceria. P1/P2 próprios ou stock, Retry, Arcade/TT ghost, replay local e serializado/reimportado, Photo Drive, sessão GT preparada e Photo Travel duas entradas no editor cacheado + reset catálogo. Corrigidas peças ausentes no P2/ghost e fonte errada no Photo Travel cacheado. Negativos retiram peças esperadas do P2, usam editor stale, removem corpo do replay e rótulo da compra. Body +468 / setup ativo +100 (+488); 00390200/00390340, 00454270/00454378, CARINFO +d0/+dc em 004533A8. Integração C; aparência integral A, nenhum valor ajustado visualmente; PCSX2 equivalente e todos os carros/peças ainda pendentes. |
| Corrida: pintura dos carros | A | Os modelos da corrida (/car/lod, /car/open) vêm sem tabela de pinturas (CAR4 +0x1C zero); `func_003B5988` carrega o caminho mais ".pat" (0x003B59D0), uma Pat0 igual à do modelo do menu (ford0011.pat: 10 cores, 88 trechos). Lida e aplicada com o índice da cor escolhida, também nos adversários. Conferido nas nossas imagens (Mustang amarelo, prata; antes sempre vermelho); sem comparação lado a lado com o PCSX2 (c) |
| Modelos: descarte de faces | A | Dois controles do contexto de desenho, como `00455708`: comandos 45/46 ligam e desligam (00456170/00456180 -> 004A1638/004AB040 com 0Fh), 78/79 escolhem o lado (00456190/004561A0 -> 004A19B0 grava 2 ou 1 em 70002000h +0x2C8), 64 troca o lado sem argumento (00456288). Antes 78 ligava o descarte: o Integra Type R '04 (hond0036) escolhe lados sem ligar e perdia a traseira. Conferido nas nossas imagens; 807 modelos com a mesma contagem de triângulos. Sem comparação com PCSX2; lado padrão (1) e estado ligado no início de cada modelo (desligado) não lidos (c) |
| Pistas: camadas de brilho (tipo 4) | A | As camadas só especulares das pistas (tipo 4, mistura 0x58 Cs * Ad + Cd: Tsukuba texturas 73-80, 427-431, 468 e uma sem textura) não têm cor própria (`0049D1A0` -> `0049EB90` carrega direções de luz e especular, sem o bloco ambiente/difuso); a pista é desenhada sem luz e elas usavam a cor de vértice 1,0 dobrada, o que deixava brancas as marcas de pneu do grid de Tsukuba (relato do usuário, PCSX2 mostra escuras). Agora somam zero. Quais luzes o jogo dá a elas não foi lido: se houver brilho de sol nas zebras e guard-rails ele sumiu junto. Conferido em `race_inspect_probe` com PROBE_DIST=2000 (c) |
| Corrida: freios atrás dos aros | A | Na sequência do modelo cada freio vem antes da sua roda (callbacks 11, 15, 12, 16, 13, 17, 14, 18 em subr0013, ford0011, fdeu0006); `draw_car` desenhava os quatro freios depois das rodas, e os texels transparentes do aro, que gravam profundidade, os escondiam (Legacy B4 Blitzen, relato do usuário). Agora freio e roda por roda, nessa ordem. As camadas misturadas do freio (brilho aditivo 0x48 e tinta 0x44) passam pelo multiplicador de cor do callback (`00453798` -> `004A60E0`, alfa no bloco da roda +0x28) e somem com ele em 0: desenhadas frias. Conferido em `race_inspect_probe` (PROBE_CAR) no Legacy, Mustang e Integra; quem escreve o +0x28 (calor do freio) não achado; sem comparação com PCSX2 (c) |
| Carros: aros dos modelos de corrida | A | WheelContext `00453E68`: ramo 0 e banco em slot externo 2. Recortes na borda corrigidos pela ordem pneu → aro de `00453028`: o aro transparente gravava profundidade antes do pneu. Prancha `extracted/renderer/ka-wheel-order-comparison.png` compara antes/depois e close original enviado pelo usuário, em outro ângulo; borda contínua observada. Banco ctx+8=1 e acumulação ctx+8>1 implementados. Capturas `ka-wheel-accum-{sharp,blurred,cached}.png`: aro borrado, pneu preservado; comparação com câmera/luz equivalentes no PCSX2 pendente. Geometria leve original mantida; montagem não prova aparência de todos os carros (c)  18 October audit: 20 focused cases pass; Ka/Legacy B4/Esprit sharp-vs-exposure fixtures on both APIs, cached images exact and all 32 MiB simulation bytes unchanged. Registered exposure/accumulation oracles and independent CPU/GPU pixel comparator (4/255). Overall image remains A without matched PS2 capture. [Audit](RENDERER.md#wheel-rim-and-brake-visibility-audit-18-october-2026). |
| Captação PC: clipes 1080p/60 separados, sem música ou upscale da UI | P | (c) 12/10: pedido final substitui montagem de 90 s/trilha única. Exportador opt-in grava RGBA do FBO final 1920x1080 antes de reduzir apenas a prévia no secundário. Sete corridas usam relógio original, sem CAPTURE_FIXED_STEP: PTS/DTS reconstruídos do CSV, depois CFR60 com repetição/descarte de quadros captados se a cadência varia, sem interpolação de movimento; não é prova de desempenho constante de 60 fps. UI_HD=0, FSR off, filtro original e text_sharpness=0. CAPTURE_NO_MUSIC força só bgm_option=-1; race_capture_stems compara controles/pitch/PCM de CR-X/Mustang em batches 1/2/4 com mistura sem BGM; negativo deixa música ligada e falha. native_video_capture confere seis cores, FBO independente da janela, ordem/timestamps/gate/reset e negativo de 1 canal. 6/6 com race_audio_clock e negativos. Entrega local clipes-sem-musica e ZIP: 14 MP4, sete corridas com SFX AAC estéreo 48 kHz e sete tomadas silenciosas UI/foto (exportação offline). A captura desktop estava coberta por outra janela e foi retirada; refilmagem cancelada pelo usuário para filmar manualmente com F9. Validação local decodifica todos os arquivos, verifica 1920x1080/60, compara AAC ao PCM (>0,98; negativo com 0,5 s <0,9), confere filtros/clock nos CSVs e preserva manifestos/PID/módulos/hash. Executável filmado moonengine-capture.exe, alvo nativo com nome temporário, PE AMD64 sem moon_ee; prova da proveniência, não certificação integral de fidelidade/arquitetura por vídeo. Nenhuma fórmula/parâmetro da física alterado. |
| Captação PC: vídeo nativo (câmeras, PCM e monitor) | P | Adaptação PC: abertura com ângulos novos, várias pistas/carros/modos e revelação do port. MOON_RACE_FILM (low/orbit/crane) só lê poses e publica câmera; probe confere matrizes finitas/ortonormais e os 32 MiB de física inalterados em nove poses, negativo altera um byte e falha. Segundo motorista automático 2P é seleção opt-in do controlador original 5, sem fórmulas/parâmetros novos. Shutter automático usa o mesmo caminho do F5 no Photo Travel; enquadramento opt-in aponta à carroceria (+0,65 m, escolha P do fotógrafo) e solicita AF nativo antes do disparo. Gion/NSX e Brooklyn/Viper refeitos e inspecionados com carro em foco. MoonEngine-native-90s.mp4 tem 2700 quadros 1080p/30, áudio estéreo 48 kHz, decodificação integral conferida e 22 s finais contínuos da janela real; manifestos registram PID/módulos/hash/comando e o link Release sem moon_ee. MOON_WINDOW_POSITION coloca a janela no secundário sem ativá-la; PCM capturado após a mistura SDL exclui os outros programas. native_audio_capture compara música + voz exatas (tolerância 0), timestamp e estéreo S16; negativo de 1 unidade falha. Capturas/edição locais em captures/native-showcase-20261012, nunca no Git. Não comprova imagem/câmera original: trajetórias, lente e montagem são escolhas para filmagem. |
| Rodas: montagem determinística para upload | C | (c) 12/10: `append` inicializa `source_model=0` na montagem PC; `decode` continua atribuindo o índice MDLS authored após cada stream (00455598), sem mudar Vls0/ordem/pixels dos modelos do disco. A falta de inicialização deixava bytes do allocator nas rodas anexadas e `r3d_mesh_create` rejeitava índices inválidos, causando rodas ausentes em carros diferentes a cada execução. `model_attach_upload`: 32 buffers pré-preenchidos, oito triângulos por montagem, quatro instâncias visíveis por imagem (1/255); negativo reintroduz índice 0xA5A5 e falha no upload. `render3d_vision` e negativo mantêm 32 quadros/listas authored passando. `race_inspect_probe`, PROBE_ALL_ENTRIES/PROBE_STEPS=1400/PROBE_DRAW_STEPS=1: 24 meshes reais aceitos e 24 capturas de CR-X, Ka, Act, Today, Midget e S500, nos dois lados, frente e trás. Evidência local captures/native-showcase-20261012/fixed-wheels-*.png; Today traseira e Midget dianteira inspecionados, sem roda ausente. C só da fronteira PC; imagem integral/borrão continuam A, sem nova prova GS/PCSX2. Física preservada. |
| Rodas: sequência de acumulação do borrão | E | `wheel_accum_oracle extracted/core.bin`: 2.816 casos contra `00453BB0`, larguras 1..1024, 2..32 amostras, 32 ângulos; dimensões, reduções, offsets float 0xBECCCCCC/+0,5, divisão angular, filtros e SCANMSK registrados. `--negative` adultera offset e falha. Quad de `00453A58` confere com tolerância 0,00002 usando a mesma fronteira libm para seno/cosseno; isso não prova pixels GS (c) |
| Rodas: imagem e integração do borrão por acumulação | A | Passe GPU residente, saída min(TEX0.TW,64), scratch com 3 vezes a altura. `00340950 +564/+59C`: carro da câmera 16 amostras, demais banco 1. `004522D8`: atenuação exp(-20 × deslocamento); entrada XY usa projeção PC, ainda sem equivalência à vf24..27, e exp/seno/cosseno usam libm. SCANMSK usa paridade 0; subpixels/filtragem GL e limpeza do scratch ainda não comparados ao GS. `wheel_accum_probe`: 48 imagens sintéticas contra rasterização CPU independente, erro máximo 4/255 por canal; negativo falha. Teste Ka com seis carros: 15.803 pixels alterados, 48 passes GPU iniciais, 0 passes no redesenho idêntico, draws/triângulos da cena constantes e sem erros GL. Não há upload/leitura CPU por quadro. Release e 22 CTest passaram; DATE/DATM mantém 8 casos 1x/4x passando. Comparação visual final PCSX2 e survey de imagem dos outros carros pendentes (c) |
| Rodas: ordem de montagem pneu → aro | E | `wheel_order_oracle extracted/core.bin disco.iso`: executa `00453028 +16C/+198` com consultas de recursos/desenho como fronteiras; compara ordem, tipos, UVs e índices dos bancos de 24 rodas Ka/S2000 no showroom e corridas de 1/6 carros. `--negative` inverte a ordem esperada e falha. Não é oráculo de pixels/GS. Catálogo race=6: 722/722 sem falhas, rodas ausentes ou valores não finitos; Release e 22 CTest passaram (c) |
| Rodas: intervalo angular e banco nítido/borrado | E | `wheel_motion_oracle extracted/core.bin`: 50 mil intervalos bit a bit de `003D73A0`/`00344E48` e bancos contra `00453E68`, incluindo fronteira de 5 graus, sinal e exposição zero. `--negative` acusa banco adulterado. Env0+EA0 fornece exposição; texturas residentes, sem upload por quadro (c) |
| Rodas: deformação de posições do pneu | C | `00458AB8 +64..+18C`, limites de todas as formas de `004588C0 +B0..+FC`. 50 mil amostras contra função EE original: tolerância absoluta 0,000001; `--geometry-negative` acusa Y adulterado. 132 vértices do Ka contra snapshot `b_reta`: erro máximo <0,0000002 em unidades locais. Aro/pneu: escala uniforme do raio nominal (`0045314C`/`00453848`). 722 carros montados em showroom/corrida de 1 e 6 sem falhas, rodas ausentes ou valores não finitos (c) |
| Car Selection: contadores abaixo dos botões | C | (c) 12/10: print do usuário mostrou contador sobre Manufacturer/Era. Causa: regra de centralização da tinta de botões também atingia os filhos info. MakerRoot/HistoryRoot originais têm label y=3 e info y=20; centralização exclui o filho suplementar info. `car_selection_count_layout` compara coordenadas dos glifos de seis faces com a referência do retângulo authored, tolerância zero; negativo renomeia info para label e detecta a regressão. Captura nativa `extracted/handoff/car-selection-count-fixed.png` inspecionada; composição integral sem nova comparação PCSX2. |
| Menus: continuidade da saída da montadora e autodemo | C | (c) 12/10: MCrossTransition.panOutIn era chamado uma vez, mas estágio 0/1 realimentava framebuffer preto da página oculta durante os barriers de startPage. Guarda superfície de saída até estágio 2 (0022F6A0 +40..b8; 0028A5B8 +34..48). `menu_navigation` confere 60 campos com root oculto, uma superfície retida, nenhuma recaptura, e retorno à sequência original de feedback .5 no estágio 2; negativo `transition_retained_hold_negative` falha. `attract_next` também apagava o preto concluído por arcade.ad _start_page L52..55 antes de trocar projeto: removido reset; teste panOut/syncOut/dispatch Boot:MovieRoot conserva alfa255, negativo `attract_outgoing_black_negative`. `title_exit_fade` executa TopRoot.onCancel original com intervalo1, compara alfa monotônico na saída, dispatch MovieRoot e 60 campos em alfa255; negativo limpa preto e falha. `outside_menu` conserva scheduler oráculo original. Log da app DealerRoot -> NationRoot em `extracted/handoff/dealer-fade-trace.txt` comprova uma saída authored. Dados/ordem C; imagem integral/timing equivalente PCSX2 pendentes. |
| Carregamento: calculo dos fades | E | Implementado em ui_motion.c/loading_screen.c: func_0010AE40, 001042C0/00104748/001047E0. Auditoria registrada em docs/MENU_PRESENTATION.md (01/10): 258.300 atualizacoes de duracao/opacidade/retorno iguais ao original; cobertura dos limites de desenho separada da prova numerica (c) |
| Carregamento: apresentacao completa | A | Implementado: dois logos e indicador animado dos assets originais, entrada/saida de GT Mode e corrida. run_loading_smoke.py: cinco rotas registradas como aprovadas, incluindo ambos os logos e musica depois do fade. Falta comparacao completa de imagem/tempo com PCSX2; calculo dos fades tem prova E na linha acima (c) |
| Replay (gravação, câmeras de TV, HUD do replay) | A | 13/10: replay.c/replay_file.c, course_tvcam.c e main_race_flow.inc implementam gravação, re-simulação e diretor; famílias orbit/fixed/heli/car-point existem. replay_file/replay_types (48 fixtures), disc_replay_oracle (93 arquivos/556738 comandos), replay_hud e negativos delimitam E/C. Trajetória, sincronismo, câmera e composição completos continuam A. |
| Replay Theater (labomode ReplayTheaterRoot) | C | Os scripts originais do labomode rodam a página: lista vertical preenchida pelo `visible_callback`/`update_callback` do script com os replays salvos (título, pista pelo CourseNameCaption, carro, tamanho, data, miniatura), seleção por `onListActivate` (R1 + X soma à seleção, L1 + R1 + X seleciona tudo: os bits 256 e 2048 que a ajuda do labomode chama de L1 e R1, por `getPadButtonState`), `onKeyPress` da lista levando aos comandos (direita) ou às pastas (esquerda), Play carrega o arquivo (`handleLoadSaveResult` espera "OK") com o ProgressDialog do cursor.ad ("Please wait... Loading data" durante o `sync(0.5)` do script) e o replay toca por cima do Theater, voltando a ele no fim. Playlist: `MMemoryCardPlayList` (search/append/insert/remove/rotate e `size` como membro) guardada num arquivo da pasta de replays no lugar do "replaylist" do cartão; a pasta Playlist mostra a lista na ordem; o Play dela (`commandPlayList`, menu_result "list") toca os replays em sequência, em ordem ou embaralhados (`replay_play_mode`), parando se um for abandonado. Ícones dos botões na linha de ajuda pelo `%B` de `func_004940E8` (fonte Gaiji, cores de `D_006AE738`). Conferido em janela curta com `MOON_LABO_ARG=ReplayTheaterRoot` e `--input` ("r1+accept" segura R1). P: L1/R1 no teclado são Q/E; controles modernos têm ponte C em controller_input; a barra do "carregando" aparece cheia (leitura imediata); a miniatura é o quadro em que o jogador cruza a chegada, ou o de 10 s numa corrida largada antes, 128x96 (A: o momento e o tamanho do PS2 não lidos); `onKeyPress`/pad ligados nas páginas suportadas; provas de remapeamento separadas. A: pasta Demo agora lê o catálogo US de config-main.adc (13 arquivos), decifra Base e reproduz comandos; provas separadas abaixo. Miniaturas originais do disco E em prova posterior; datas/composição integral A; trajetória/câmera completa não comparadas. |
| Pré-race: curvas, duração e molas das câmeras | E | `course_pregrid.c`: curso +0xC0, REP4, modo 5 (`003747B8 +0x220`, `0037BF00`). `pregrid_oracle` compara `003DDBC8`, `003FC9A0` e `00385098`: 100.763 valores de curvas e 7.751 valores de tempo nas pistas Grand Valley, Grand Valley East, Tsukuba e Nürburgring; 4.000 estados de mola bit a bit, dois ciclos por pista. Passo de produção restaura o arredondamento do chamador. `--negative` adultera uma coordenada e falha (c) |
| Pré-race: projeção e orientação final | A | Fórmula e constantes de `003794A8` conferidas em 999 entradas no oráculo, com a mesma fronteira `tanf`; isso não compara a biblioteca trigonométrica PS2. Orientação world-target expressa por produtos vetoriais e roll; matriz VU final não comparada. Plano próximo 0,1 herdado do renderer de corrida, sem prova própria; viewport nativo 640x480, escalas GS 320/224 (c) |
| Pré-race: inicialização, ícones, legenda e comandos | C | `pregrid_menu disco.iso`: scripts quick-arcade originais, mproject e RT03 US; ícones condicionais centrados (72, intervalo 4), conforme canReplay/IsBSpec, chaves `TOOLTIP\|icon_text_*`, Start/Option, abrir/cancelar QuickTune e SceneViewFace desenhado antes da grade. Comparação exata dos campos conferidos; negativo troca a legenda esperada e falha. `startStreamEx` deixou de abortar a inicialização. Teste atualizado em 02/10 para quatro/seis ícones com IsBSpec=0, incluindo projeto em cache. Não prova a rasterização das fontes nem a aparência de todos os widgets (c) |
| Pre-corrida: chips de pintura e indices rejeitados | E | 001109D4 -> 00445880 rejeita indice >= numero de variacoes e conserva pintura inicial; 004458D0 seleciona order == indice+1; 00445AE8/00445B28 copiam VARIATION +0x24. pregrid_paint_oracle compara os 16 bytes contra EE original em 115 casos (5 variantes 3000GT, A110, Ford GT, Ka) e confere getColorChipInfo -> ColorChipFace; --negative altera flag e falha. Prova da selecao/dados; nao classifica a aparencia renderizada (c) |
| Time Trial: projeto e estados pre/pos | C | quick-tt:TimeTrialRoot selecionado pelo next_sequence original; single-car e retorno de Option preservam o projeto. pregrid_menu roda scripts/disco e testa canReplay antes/depois; --tt-negative falha. Build Release e captura local build/time-trial-pregrid.png. Ghost PC, ranking, Logger e replay implementados; provas delimitadas abaixo, comparação integral A (c) |
| Time Trial: catalogo superior | A | ENGINE +0x1A/+0x1C e UnistrDB +0x10/+0x14, conversoes 004729A0/00472A08 e formatos 00127C48/00127D20. Ano/categoria/drive do SpecDB. Falta oraculo do formatter/arredondamento. Registro vazio usa sentinel 00112EE8; recorde atualizado por session_best_update/course_ranking, testes de persistência C; formatter integral A (c) |
| Time Trial: a corrida com a sessão e o mundo do TT | A | `00112620` (sequência time_trial) cria o mundo `003B9A18` (vt 0x680140, sem o objeto de resultados `003DEE70` do Arcade); a sessão vt 0x685890 (`003B9BD8`). As capturas de TT (a_parado, b_reta, c_curva, d_reta1, e_reta2, nur_02/03) têm essas tabelas, o modo do mundo 2 (ARCADE_TIME_TRIAL, 0069FE58) e cond+0xF864 0, que `race_run` agora escreve com `time_trial`. A sessão responde 0 voltas (0x128): sem última volta nem tabela Results; o HUD mostra a volta sem total. Sai-se pela pausa (objeto igual ao do Arcade nas capturas: sem retry, com ajuste de tela) de volta ao TimeTrialRoot com o replay. Conferido nas nossas imagens; o carro fantasma (segunda entrada das capturas, o mesmo carro) e a largada de cada pista não feitos; o piloto automático de teste não anda no TT (a sessão não deixa o modelo dirigir) (c) |
| Time Trial: Session Record | A | `00112620`: depois da corrida, `func_0035E3F0` (car+0x71C, a melhor volta do jogador) baixa o BestTime (+0x498) do MQuickWork quando é menor; `00112E78` abre a sessão com 0x157529FF. Mostrado como 1'03.183 com um valor injetado em teste; sem volta completa automatizada (c) |
| Time Trial: largada lançada | A | Sem contagem: race+0xB4 3 nas sete capturas de TT (caso 4, "nenhuma", de `003C0208`; quem escreve não achado). O carro sai pela largada lançada da sessão (`003F5670` -> `start_rolling`): a tabela de IA da pista (course+0xA0: Tsukuba +0x19 78, +0x1A 10, isto é 780 m antes da linha a 50 km/h). `tt_build` mostra 13,89 m/s no primeiro quadro e sem parar; antes a contagem do Arcade (10 s) segurava o carro. Posição de largada não comparada com o PCSX2 (as capturas são do meio da volta) (c) |
| Time Trial: carro fantasma | A | Dois carros como nas capturas (o mesmo carro, grade 0, perfil de jogador; o fantasma espera parado: tipo 6 e depois 8, `003455C8`). Eventos de largada e volta do carro 0 (`003F59F8` / `003F5B80`): volta melhor que a do fantasma vira o fantasma (`00426CA0`), o fantasma recomeça com o jogador (`003F58B0` + `0035ABA8`: cópia do carro de 0x10A0 bytes guardando as cinco primeiras palavras e +0x440/0x441, `00463930`, `003455F0`, relógio da volta por `003584A0`, contador +0x4AC, fração do passo em +0x554 e um passo só dele quando ela fica negativa, `0034DB30` com 1) e a gravação recomeça (`003F5820`); o controle do carro 1 lê os comandos gravados. `tt_build <iso> --ghost tsukuba`: duas voltas com um motorista fixo, o fantasma refaz a primeira a 0,0000 m em 5.661 passos; `--ghost-negative` (30 passos fora) falha. Pelo caminho do jogo (`tt_game_probe`: o race_game, seu leitor de controle e quadros), Escort WRC em Laguna Seca: exato com direção analógica, digital, câmbio manual e TCS/ASM; com TCS ligado o fantasma saía 852 m da volta porque só a ficha do jogador recebia câmbio e auxílios (+0x3448..+0x3454): agora a do fantasma recebe os mesmos (A: nas capturas nur_02/03 a ficha 1 tem 1, 1 com o jogador em 0, 0; não lido o que isso atinge no carro). `--negative` falha (29 m). A: bloco de IA da gravação (`0045CDC8`/`0045CF18`) não restaurado; checagens do gravador (`00426C38`/`00426C78`) não lidas; desenho translúcido não achado (as passadas de carro `00341A30`/`00341BA0`/`00341710` pulam car+0x545 diferente de 0, e o fantasma tem cópia própria do modelo, só com outros endereços de textura): translúcido medido num quadro do PCSX2 (slot do usuário, Escort em Laguna Seca, carro e fantasma com a mesma pintura): a traseira do fantasma (64, 66, 72) entre a do jogador (13, 13, 36) e o asfalto ao lado (125, 124, 122) dá 0,53 a 0,58 por canal; desenhado a 0,55: primeiro só a profundidade das partes opacas, depois a superfície mais próxima misturada (e as camadas misturadas do próprio carro com a parte da cor pela alfa da fonte escalada), sem sombra (o quadro não mostra nenhuma). O código que mistura não foi achado (as passadas `00341A30`/`00341BA0`/`00341710` o pulam; nada em `0038FEB0`/`003901D0`, nas chamadas de `004A60E0`/`004A2808` nem no objeto de desenho +0x2880, que só difere em +0x2C/+0x40/+0x44/+0x48); no PCSX2 ele parece mais largo e suave, não explicado. Comparado lado a lado com `MOON_RACE_SNAPSHOT` e `--time-trial` (c) |
| Time Trial: salvar replay, salvar e carregar o fantasma | A | Salvar replay (Arcade e Time Trial) caía ao abrir a tela de salvar: logo depois de `menu_open_labo` a página ainda não entrou e `menu_active_root` dá -1, que `ui_root_name` usava como índice; agora dá "" fora da faixa. Conferido: TT em Tsukuba, sair pela pausa, SaveReplay, título pelo teclado (EntryRoot), "Save successful" e o arquivo gravado. Fantasma: os botões SaveGhost/LoadGhost do quick-tt.ad (`COMMAND_SAVE_GHOST`/`COMMAND_LOAD_GHOST`) abrem as telas do labomode nos modos `MODE_SAVE_BESTLAP_REPLAY` (SaveReplayRoot) e `MODE_LOAD_BESTLAP_REPLAY` (LoadReplayRoot), que o labomode.ad L32..L36 já distingue; `canSaveGhost` (L72) agora vem da sessão. O arquivo de melhor volta (bestlap_*, objectName FileReplayLicense) guarda o que o fantasma precisa: o carro do jogador no início da volta (0x10A0 bytes, `003F8540`), o resto e a fração do passo da largada da volta, o tempo e os comandos da volta (bloco GHST após os comandos; arquivos antigos seguem lidos). A sessão do TT guarda o melhor fantasma entre largadas (como o jogo mantém a gravação enquanto o TT continua) e o carregado entra no lugar dele, só para a mesma pista e o mesmo carro (o carro copiado inteiro traz os endereços da montagem). `tt_game_probe <iso> tsukuba ka_01 --saved`: volta 1 salva (empacotada e lida de volta), fantasma da sessão esquecido, o do arquivo carregado, nova largada: o fantasma refaz a volta salva em 5.661 passos a 0,0000 m; `--negative` falha (16,2 m). Fluxo pelo menu conferido: LoadGhost, lista, "result OK", de volta ao TimeTrialRoot com o fantasma carregado; SaveGhost grava bestlap_* ("Save successful"). A: o formato e o conteúdo do arquivo de melhor volta do PS2 não lidos (o do PC é outro, docs/EDITIONS.md); o jogo pode aceitar fantasma de outro carro, aqui recusado com aviso; Ranking e Logger A/B implementados (course_ranking/logger_view/race_resources); apresentação integral A |
| Time Trial: Ranking (recordes por pista) | C | `MCourseRecord`/`MCourseRecordUnit` (`0015BCF0`, dados `CourseRecordT<128>`: unidades de 0x238 bytes pela chave da pista `00447C78`, dez entradas de 0x30: tempo, data, carro, passcode; best_score em +0x230). Inserção `00433178`+`00433028`: antes da primeira entrada mais lenta ou vazia (tempo igual vai depois), as seguintes descem e a décima sai; contagem `00433470`; apagar `00433430` (sobem, a última limpa como `00432E40`); getBestTime `0042E478`, getDate `0042E660`, getCarName o nome curto do CAR_NAME (`004481B8`, linha +8). Quem grava: `00113320` (Single Race) e `00117898` (Time Trial), quando a melhor volta baixa o recorde da sessão; aqui no mesmo ponto (`session_best_update`), fora de carreira, tela dividida e replay. Guardado em game.course_record.ranking; o records_ps2 de um save do PS2 importado é lido por pista. `course_ranking <iso> <dir>`: ordem, empate, limite de dez, apagar, nome e data, salvar e recarregar. RankingRoot do quick-tt aberto pelo botão Ranking (antes bloqueado). Passcode gerado e persistido, algoritmo E e integração A nas provas separadas acima; data pelo relógio do PC (`00548918`); sem print do PCSX2 lado a lado |
| Pausa: confirmação ao sair do evento/campeonato | C | O RacePause lê +0x2C em CONFIRMEXIT (`003C3744`); os mundos do GT Mode `003EDE50` (evento) e `003EF498` (campeonato) o põem em 1 quando `D_00620208` é 0 (só a demonstração `0010CA50` o liga); os outros mundos (Arcade, TT, licença, treino, missão) deixam o 0 de `003C2D80`. `race_game` passa o 1 quando a corrida é do GT Mode (linha RACE e sem licença/missão). O pause.strb mostra "Are you sure?" com CONTINUE/EXIT. `pause_confirm <iso>`: sem a opção EXIT sai direto; com ela o primeiro EXIT abre a pergunta e só o segundo sai. Captura conferida (forçada no Arcade só para a imagem). Sem print do PCSX2 |
| GT Mode: Photo Lab da home | C | HomeRoot Labo (gtmode.ad L4202..4206) chama `start_sequence("photo_labo")`, que ninguém tratava; agora abre o labomode com next_menu_arg "" (labomode.ad L37: PhotoRoot). A saída do PhotoRoot (L1451..1456, com a pergunta "exit ok?") volta a major_menu_project/major_menu_page; como cada projeto tem a sua VM, o contexto é copiado ao abrir (como o do setting). Conferido: home, Labo, Photo Lab (Film/Photo/Slideshow/Print), Exit, sim, de volta à home com o cursor no Labo. Sem print do PCSX2 |
| Corrida: linhas de tempo do display (setor, volta, diferença e voltas atrás com ícone) | A | A lista de textos do RaceDisplay (+0x17BC, 8 linhas, `003A50D0` acrescenta, `003A5060`/`003A4AE8` desenham) recebe da fila de eventos da corrida (+0xF4) os eventos do jogador no passo: setor 0xE (`003A0C30`: "T" do WordOfSectionTime, número e tempo do evento 0x10), volta 0xD (`003A0A78`, só o tempo) e diferença 0x11 (`003A04B0` -> `003A1098`: sinal e tempo, cor pelo campo e sinal, deslocada 32 px quando há ícone; o carro comparado por `003A1030`/`0039F0C0`, o 2º colocado quando à frente e o 1º quando atrás) com o desenho do carro 96x64 numa caixa 64x32 (+0x2D14, `003A6678`). Linhas a (320+dx, 139+dy), 3 s, fonte hn_20R, entrada e saída de 0,15 s. A fila agora envelhece uma vez por quadro do mundo (`00389758` -> `003B75D8`: contador -1, evento removido no 0). cond+0xF839 posto em -1 (sem variante de traçado, como em todas as capturas; `00395220` copia para course+0xB0): com 0 nenhum setor disparava; Laguna Seca tem setores a 963,2, 1940,5 e 2844,2 m. `hud_lines_probe <iso> laguna`: corrida Arcade com cinco adversários e o carro do jogador seguindo o traçado; meio segundo depois da primeira diferença a linha está na tela (319 pixels de cor); `--negative` (eventos não entregues) falha com 2. Voltas atrás (0x12, `003A0308` -> `003A0E40`): o jogador atrás recebe as suas ("+1 LAP" em vermelho, ícone do líder, 82 px à esquerda); liderando, as do 2º em negativo, em azul, com o ícone dele; texto de `003B0320` (sinal e "%d %s" com WordOfLap/WordOfLaps, na tabela D_00621870 +0x138/+0x13C da captura ar_02). Diferença e voltas atrás só com o display aceso por inteiro (+0xBC < 1 descarta). `hud_lines_probe <iso> tsukuba --lapped` (cinco voltas, jogador a 1/4 do acelerador): o líder dá a volta e "+1 LAP" aparece (192 pixels vermelhos); com `--negative` falha (0). A: posições e cores conferidas no código, sem print do PCSX2 lado a lado; o caso liderando só lido no código |
| Inventario de menus pre/pos e premios | C | 10 projetos quick, 29 RootWindow comparados ao disco por pregrid_menu; 10 principais +3 AfterRoot +3 PrizeRoot, 13 layouts pre/pos. Dez ramos dos seletores originais evento/missao/campeonato executados. --inventory-negative/--selector-negative falham. Variantes nativas de batalha/rede ainda fora da contagem; docs/RACE_SCREEN_INVENTORY.md (c) |
| Pré-race: pista ao fundo e retorno da corrida | A | SceneViewFace `0012BC18`, modo 5: cenário sem carros/reflexos deles, conforme `001119A0 +0x24..+0x80`. Comparação visual com referências u26/u32 e print de Grand Valley East enviado em 02/10; diferenças de fonte/renderer e fade interno original pendentes. `pregrid_probe` confirma 1.201 ticks sem alterar nenhum dos 32 MB da física; câmera move, volta à primeira tomada e reinício repõe contagem. Negativo altera snapshot e falha. Smoke abre Opções e retorna ao mesmo grid com 77 widgets desenhados, zero omitidos (c) |
| Pré-race: abreviação de nomes longos | E | `00245658 +0x7C`, `0048F790` e `0048F708`: `abbreviate()` separado de `adjustScale()`, buffer 0x104 e reticências originais; igualdade também abrevia. Produção usa arredondamento EE e restaura modo PC, conferido em todos os casos. `pregrid_text_oracle` compara 20.480 resultados/flags com o EE, na fronteira de medição compartilhada; a largura tem a prova C abaixo. Negativo adultera um avanço e falha. `pregrid_menu` executa o script do disco com CELICA/ST165 e Karmann Ghia compridos; escala .875 preservada e `...` presente (c) |
| Pré-race: métricas e kerning FT01 | C | Fonte padrão hn_14 (`00246F58 +0x38`), RGB padrão 212 (+0x0/+0x8), align 4 (+0xC4), sombra preta alpha 255 (+0xB0), gap extra zero (+0xD4); construtor executado no oráculo com allocator de string como fronteira; contornos de `00492978 +0x1EC..+0x344`, largura final `00491418 +0x1A0..+0x1CC` e `00492978 +0x478`. Oráculo: 32.841 passos em hm_16/hn_14/hb_24/FC26, tolerância 0,00001; 32 larguras de strings, tolerância 0,00002. Negativo +1 pixel falha. Escopo: textos simples usados no pré-race, sem controles/CJK; rasterização, sombra e posição vertical ainda A. Outros menus mantêm suas configurações anteriores (c) |
| Pré-race: retângulos dos ícones e corte de texto | C | `002A2608 +0x7C..+0xC0`, getters `00231420/00231440`: quad usa área do cliente; ImageFace herda 72x68 do botão no mproject. `00247448 +0xD8..+0x114`: scissor do TextFace. Probe do script confere quatro quads 72x68 em (170+76*i,360) antes da corrida, seis ícones em 94+76*i depois, sem usar tamanho natural da textura. Captura local com nome CELICA longo confirma limite antes de HP. Negativo do probe troca legenda e falha; amostragem UV/imagem final não comparada pixel a pixel (c) |
| Pré-race: duas músicas PCM | A | `006238C0 +0x18` grupo 3, `00460220`, `0045FFF8 +0x48/+0x98`: /bgm/us/ads.inf v2 seleciona gt4qm-01.ads ou gt4qm-02.ads; não pertencem aos grupos MIDI. Teste lê ambas: 9.440.256 e 9.547.776 frames stereo a 44100 Hz, callback 48000 Hz não silencioso, rewind determinístico, rejeição de cabeçalho inválido e interpolação sem overflow. Negativo espera três faixas e falha. RNG `00579330` passa 10.000 estados EE exatos; relógio PC substitui timer PS2, ADPCM/interpolação linear, loop e fade ainda sem comparação de waveform SPU2. Ponte PCM em main.c usa worker separado do MIDI, pedido startStreamEx/stopStream e volume raw /128. pregrid_audio_probe compara callback com ambas as fontes do disco, volume 120, mute, saída, reentrada, cancelamento de pedido em carga e preservação do callback da tela de destino; negativo de volume falha. Worker testado com os próprios trechos isolados; build local e smoke com --with-music publicam default_quick no backend real. Build limpa do app bloqueada por dependência anterior de career_vehicle.h ainda fora do git; sem comparação SPU2 (c) |
| Pré-race: efeitos do ajuste rápido na física | C | (c) 10/10: o QuickTune do Arcade chega à física pelo caminho do original. Campos da quick work (+0x3AC peso, +0x3B0 potência, +0x3B4..+0x3B6 câmbio, +0x3B8/+0x3BC pneus, +0x3C0 transmissão, +0x3C4 auxílios, +0x488 IsQuickTuned) lidos pelos getters 005C4408..005C47C8. 00111B50 prepara a cada corrida (potência/peso 0, câmbio pela linha TRANSMISSION +0x25..+0x27 e setup +0x12A, transmissão e auxílios das opções, como quick_tune_popup.ad L370..404). No Start, 00111DC0 grava na ficha do jogador: setup +0x163/+0x162 = 100 + nível, +0x160/+0x161 pneus, +0x12A câmbio com relações automáticas (00443240), +0x165; a corrida já montada para o grid é remontada com isso. O mundo do Arcade (vtable 0067BAD8 +0x2E0 = 00387348) encaixa os pneus por eixo (terra/neve forçam 11/12) e apaga +0x165. A física já aplicava +0x163/+0x162 (setup.c, motor e massa). arcade_quick_tune: CR-X em Tsukuba, +10% potência e -10% peso: 160,0 -> 176,0 PS (x1,100), massa x0,912 (piloto fixo), pneus 4/4 -> 3/3; arcade_quick_tune_gearbox: Impreza WRC 03, câmbio 26 -> 42 com relações refeitas; negativo (pneu inexistente) falha. Pela tela: Quick Tune-up +5% e Start reconstroem a corrida ("quick tune: tuned 1"). A: trajetória com o ajuste não comparada ao original; ordem relativa entre a opção de pneus do Arcade (tire_idx) e o 00387348 não lida |
| Pré/pós-race: visibilidade dos ícones | C | Pedido de correção em 02/10 substitui a exceção P anterior: setter visible volta ao script original. quick-arcade.ad L73..90 e L201..232; canReplay=0/1, IsBSpec=0 do caller nativo: quatro antes / seis depois, posições 170+76*i / 94+76*i. pregrid_menu executa o script real, testa ida e volta no mesmo projeto em cache, legendas e comandos; negativo troca legenda e falha. Não prova disponibilidade original do B-spec em cada sessão nem implementa replay (c) |
| Pregrid pós-corrida: B-spec/Replay/Save | A | 13/10: ações encaminhadas por main_race_flow.inc e race_game_controls.inc; B-Spec reconstrói a sessão com piloto, Replay reproduz e Save abre labomode. pregrid_menu testa disponibilidade antes/depois, tt_game_probe --saved testa a fita e bspec_tests testa piloto/ordens. Integrações C/E delimitadas nas provas específicas; ciclo visual de todos os quick-* A. |
| Som da corrida por carro (motor, rodagem, escape, turbina, vento, NOS, 11 sons de pneu por superfície, batidas, b_off, on_bound, heartbeat) | E | race_car_sound.c porta func_003B5E80 (quadro) e func_00392640 (pedidos). race_car_sound_oracle roda os originais recompilados sobre 20 capturas PCSX2: 92 quadros de carro e 2.392 pedidos ao driver (ENGN +0x3C/+0x40/+0x48/+0x4C/+0x4E, 00462A88 por handle, 00392EB8) iguais bit a bit; --negative falha. Escopo: lógica do jogo até o driver; camera nativa entra pelo listener (tipo vt+0xA8: para-choque 0, atrás 2, teto 1, largada/livre 3). Não cobre: segunda viewport (tela dividida), corte de câmera (+0x16B fixo em 1), o pitch de um slot mudo (lixo de pilha no original) (c) |
| Áudio: bancos ENGN da CRX del Sol SiR 1992 | E | 10/10/2026: `race_engine_bank_capture disco.iso eeMemory.bin SPU2.bin` compara o loader nativo com as tabelas EE de `00559470 +0x30/+0x34` e a RAM SPU2 real. Duas capturas locais PCSX2 `.05/.06`: engine/normal0 47801 (`003923D0 +0x48/+0x94`) e roadnoiz, 17 camadas e 255.616 bytes ADPCM idênticos por captura, inclusive flags/loops. `--negative` adultera um byte e falha. Escopo: dados carregados deste carro; não prova waveform, sincronização das vozes nem timbre final. Usuário relata som incorreto; reprodução permanece A (c) |
| Áudio: controlador ENGN isolado | E | 00558FD8/00558DB0/00558E28: 100.000 estados contra EE, escolha de camada, pitch e ganhos inteiros; race_sound_oracle extracted/core.bin disco.iso, --negative falha na camada adulterada. Agora com o slew só para slot que manteve a voz (+0x148..+0x190) (c) |
| Áudio: ambiente da pista (ar, cidade, torcida, aplauso, mar, túneis, helicóptero, profundidade do reverb) | E | func_00398B70 / 00457F60 / 00457918 / 00457738 portados (race_car_sound.c). race_car_sound_oracle: grupos L/R/pitch e níveis suavizados dos alto-falantes iguais ao original em 20 capturas x 3 quadros seguidos; --negative-ambient falha. Túnel pelo raio 003E5A30 5 acima do olho (kind & 0x18), reverb 0,125 -> 0,25/0,5 por 0x3CE38E3A/quadro. tunnel2 sai mudo como no original (pans cruzados). Helicóptero só com câmera de replay (vt+0x130), que não existe ainda (c) |
| Áudio: registradores do driver (VOLL/VOLR, PITCH de 0055D890) | E | gt4_sd_volume / gt4_sd_pitch contra as vozes reais das capturas: 886 volumes e 492 pitches iguais (race_car_sound_oracle; --negative-voice falha) (c) |
| Áudio: conclusão atrasada de voz reutilizada | A | 10/10/2026: race_audio.c vincula OFF informado pelo callback ao serial KON observado, impedindo liberar novo ocupante com status antigo. `0055D890 +0x2C..0x90` lê status atual no original; transporte PC é assíncrono. race_audio_probe mantém voz ENGN real a 3000 RPM por 600 ticks sem callbacks, conserva hardware/serial sob OFF antigo e libera sob OFF atual; --completion-negative injeta status velho e falha. Oracle ENGN de 100000 estados continua E. Ainda pendente reproduzir e comparar sonoramente a ocorrência relatada na licença B-3; não certifica waveform SPU2 (c) |
| Áudio: ordem das posições da câmera no Doppler | C | (c) 12/10: listener nativo invertia o getter f12=0 (00373718 → 00379448 +00..14, câmera+10 atual) e f12=1 (00373850 +3C..58, câmera+7C anterior). Terceira pessoa do jogador baixava o fator a 0,824; corrigida a montagem sem mudar 003B5E80/física/samples. race_audio_clock ampliado para 1200 ticks, CR-X/Mustang e 1/2/4 passos por desenho; atravessa largada até chase, campos de origem exatos, fator 1 com tolerância 1e-4 e registradores/PCM idênticos entre cadências. Negativo troca as posições e falha. 9/9 testes de áudio/câmbio selecionados; oracle em ar_05/ar_05b continua com 12 frames/288 pedidos/37 pitches exatos. C restrito à ponte das posições; timbre/waveform integral PS2 e todas as câmeras continuam A. Logs engine-camera-{diagnostic,fixed,oracle}.log locais. |
| GT Mode: transmissão das configurações | C | (c) 12/10: main lia game.manual do Arcade (ausente no GT Mode), ignorando option.automatic_gear. menu_driving_manual segue option.ad RaceRoot.reflect/apply L2092/L2106 (1-index) e 003D6350 +18..20. Eventos, campeonatos/próxima etapa, licenças/próxima prova, missões, Free Run/Practice e Photo Drive usam essa opção. Start após mudar Options atualiza a descrição preparada e pede rebuild; Retry conserva a escolha. gtmode_transmission executa apply original AT/MT/AT, perfil isolado, Sunday Cup preparada, Retry e troca após o grid; entry+3448 e controlador vivo car+546 comparados com a escolha, tolerância zero. Negativo restaura leitura de game.manual e falha. C restrito à integração testada; não certifica todas as transmissões/eventos nem a UI integral PS2. Nenhuma fórmula da física alterada; Release atualizado. |
| Arcade: transmissão escolhida até o pré-corrida | C | (c) 12/10: apply_setting (arcade.ad L4675/L4689..4694) muda automatic_gear sem setNeedSave; a VM separada do quick-arcade restaurava preferência anterior, e MQuickWork podia nascer antes de restaurá-la. Pedido de corrida 1P publica opções atuais; menu_open restaura main.game antes do bytecode, como o objeto compartilhado do original. split_select executa apply_setting e quatro alternâncias AT/MT com preferência anterior oposta, reusando quick-arcade e seu Start; transmissão chega 0/1, tolerância zero. Negativo repõe snapshot automático e falha. arcade_manual_driver roda CR-X/Mustang 1200 ticks em 1/2/4 passos por desenho e confere entry+3448 e car+546=1; negativo pede automático e falha. 9/9 focados e 10/10 regressões de menus/retorno Options. C integração; não prova todas as transmissões/carros ou trajetória PS2 integral. Release atualizado; nenhuma fórmula da física alterada. |
| Áudio: câmera por passo e Doppler em RPM constante | A | 10/10/2026: `race_game_update` atualizava som por passo mas câmera só após todos os passos do quadro; `003B5E80 +0x1FC..+0x37C` exige posições de câmera consecutivas. Câmera e pedidos agora publicados antes de cada tick de áudio. `race_audio_clock disco.iso`: CRX Si 1990 e Mustang GT 2005, 240 ticks a 3500 RPM, um/dois/quatro passos por desenho: posição, controladores ENGN, pitches e 192000 frames PCM por execução idênticos bit a bit. `--negative` segura câmera anterior e falha no tick 1. `race_car_sound_oracle --negative-listener` detecta Doppler errado (+50: original 3F800000, câmera atrasada 3F748B9C) na captura .05; normal continua E nas duas capturas. A da integração: regressão do relógio PC não certifica waveform/timbre final contra PS2 nem todas as câmeras (c) |
| Áudio: alocação de vozes e objetos de efeito | A | sound_driver.c: 48 objetos de efeito (0055A5F0), 46 vozes de hardware por prioridade (0055D0B0/0055D070/0055D028), slots ENGN (00558FD8 +0x148..+0x310), tons (00559EB0), notas de sequência (0055AED8 -> 00611AA0 -> 0055A8B0). Sem oráculo de um tick inteiro do driver. A: voz roubada de um loop deixa o ponteiro velho no original, aqui o loop pede outra; voz liberada quando o envelope do PC chega a zero, no lugar do status do IOP (0055E6A8) (c) |
| Áudio: voz SPU2 (ADPCM com laço, gaussiana, ADSR, volume fixo) | A | spu.c pelo comportamento documentado do hardware (psx-spx); tabela gaussiana de 512 entradas do hardware. Ordens KON/KOFF preservadas entre ticks e callbacks (0055D688 +0x10..+0x18; 0055DF70 +0x10C..+0x11C); seriais globais distinguem parar um loop de reutilizar sua voz. Usuário relata timbre incorreto da CRX em 10/10/2026; causa ainda não isolada. Dados do banco da del Sol conferidos separadamente, sem prova de waveform (c) |
| Audio: loop de derrapagem encerrado entre callbacks | A | Corrigido transporte em race_audio.c/sound_driver.c: KON seguido de KOFF nao renasce como loop; KOFF seguido de KON conserva voz nova, inclusive wrap do serial. race_audio_probe com PROBE_CAR=ford_gt_05: 40 s, Ford jogador e Ford adversario pilotado pela IA, 46 vozes ocupadas; regressao positiva passa e --key-negative falha. A: callback agrega ticks, nao reproduz atraso de envio IOP de 0055DF70; falta comparacao de ouvido com PCSX2 e repetir a situacao manual relatada pelo usuario. 25 amostras saturadas em 3.840.000 (abaixo do limite 0,01% do probe); nenhum ganho ajustado (c) |
| Áudio: reverb SPU2 | A | Preset do modo 4 (Studio C) lido do /IRX/LIBSD.IRX do disco, EVOL 0FFFh (bloco de atributos do driver nas capturas), um por core. A: os filtros de meia taxa do hardware substituídos por média de pares e interpolação linear (c) |
| Áudio: contagem da largada (count_d 3/2/1, count_s no GO, chime) | A | Condições de func_0039E9B0 (resto 180/120/60 quadros, evento 4 do jogador); sequências 0, 1, 9 do gt4count.ins pelo sequenciador do driver, 240 ticks/s. Disparo testado em Grand Valley East (traço e voz audível no probe). A: velocidade da nota pela tabela do banco (identidade neste disco) e volume do banco 127 lidos da captura, não do código; chime só para largadas individuais atrasadas (c) |
| Box: estágios, clocks e marcas da equipe | E | (c) 11/10: pit_motion_oracle executa 0035BBD8/0035BEB0, 003CF6F0/730/780 e o Mot1 original; agora ambos /pit/pit_001 e pit_002: 383.400 passos/seleções, 3.450.600 marcas inclusivas, intervalos e histórico. dt 1/120, 1/60, 1/30; blend -2..2. Negativo altera jack_up e falha. Prova dos clocks, não da apresentação integral. |
| Box: controlador de serviços, câmeras e modelos | E | (c) 11/10: pit_controller executa 003CFA98, 003CBB28, 003CC010, 003CC498, 003CF7C8, 003C9FC8/CA038/CA0C8 e setters 003D3208/3228/3248. 313.600 ticks (dois pacotes, quatro layouts, dez ordens, quatro disponibilidades, 980 ticks cada); clocks, readiness/histórico, som, springs, câmera/tomada/origem e canais de modelo bit a bit. Negativos independentes de clock, câmera, ownership e modelo falham. Posicionamento/graphics/walkers são fronteiras desta prova; não confere o mundo todo nem os pixels. |
| Box: carro levantado, freio e pneu no mesmo cubo | C | (c) defeito apontado pelo usuário em 11/10: o pneu carregado seguia a referência atual car+888; carroceria/freio ainda usavam pose de chão e travel inicial do renderer. Agora corpo desfaz lift/tilt fixo do showroom e recebe pai vivo 004595C8/00459610 -> 003D6FE0/003D6F40; centro do freio/roda montada por 003D72A0. pit_transform: 1.000 pais e 4.000 centros, 13 layouts, ângulos/posições e cinco interpolações, tolerância 2e-5. Negativos específicos deslocam corpo/cubo 0,125 e falham. Também 2.600 matrizes de pneus antigos/novos contra 003D1488 e 1.040 coordenadas de câmera contra 003CB2C0, tolerância 2e-5. Close corrigido wheel-final.png. Física não editada. Materiais, luz, raster e imagem completa continuam A. |
| Box: cabos flexíveis da pistola | C | (c) 11/10: 003CF938 spring em 10.000 entradas bit a bit; 003CA698/003CAAC8/003CA168 em 400 formas/110.400 vértices, erro máximo 1,9e-6, tolerância 2e-5. Negativo desloca vértice e falha. Duas tiras cruzadas, 24 amostras, ALPHA 44/alpha 0,8; buffer GPU residente atualizado sem tesselação/alocação por frame. Raster/iluminação fora da prova. |
| Box: medidor durante abastecimento | C | (c) 11/10: RaceRefuelMeter 003A59D8 e flag 0039E790..E7C8; refuel_bg/bar/scale originais, hn_20R, posição (320,58) do slot 5. pit_controller compara 10.000 draw lists com tanques parciais/cheios/excedidos e arredondamento: três retângulos tolerância 5e-5; coordenadas/cor/label bit a bit. Negativo desloca barra e falha. Textura inteira esticada (003AF4F8), sem corte UV. Não certifica raster GS/fonte do HUD. |
| Box: poses do banco original | C | (c) 11/10: pit_pose_oracle compara 14.910 streams dos dois pacotes com 0042B440, quaternions 0048B888/0048A970 e blends. Câmera e canais escalares bit a bit; matrizes dos atores erro máximo 7,1526e-7, tolerância 2e-5 nas fronteiras host sin/cos/pilha. Negativos independentes deslocam PELVIS e alteram scalar, ambos falham. Modelos/texturas residentes; não certifica pixels GL. |
| Box: raízes laterais das quatro rodas | E | (c) 11/10: pit_motion_oracle compara 003CD620 com getters originais: 1.404 casos, todos os 13 layouts, lados e interpolação, bit a bit. --negative-wheel desloca raiz e falha. Atualização/fixação/freeze durante dois serviços tem prova C separada abaixo. |
| Box: equipe, ferramentas, iluminação e sombras na corrida | A | (c) 11/10: todos os caminhos residentes integrados: skin/head/gun/fuel/lollipop, BAKE_FUEL/LOLI/GUN/GUN2 e BC_WALK/BC_SQUAT; dois pacotes COURSE +14/+15, seis boxes. Pneus tire0/tire1, rodas montadas por fase, cabos, abastecimento, macaco, lollipop e câmera authored. Corrigidos fallback 003D2C48 e material ambient 1.2..1.5 cortado em 1 (0049E228/0049F038); nenhum valor ajustado no olho. Capturas final4.png, walk-material5.png, final6.png em extracted/handoff/pit-complete-20261011; slots 4..6 PCSX2. 980 ticks por captura preservam 32 MB RAM, negativo de memória falha. Provas E/C de lógica/matrizes abaixo. A: composição/projeção/raster/luz e sombras do renderer compartilhado sem comparação integral; imagem do slot 5 precede RAM em dois frames e cruza corte de câmera. Não certificar pixels pelo teste da matriz; não declarar fechamento visual integral. |
| Box: mecânicos distantes, caminhada e retorno | C | (c) 11/10: 003D4C38/003D50C8/003D3E80 nativos; 468 inicializações, 43.200 ticks com os 20 estados forçados e 24.000 ticks de duas paradas sucessivas por entrada/pacote. Inteiros, estados e RNG E; floats C, tol 5e-5, erro máximo 1.19209289e-7. Dois carros, layouts, espaçamentos, obstáculo e pedidos; negativos de posição/estado falham. 10.000 interpolações de luz 003D4AA8 C, tol 5e-6, negativo próprio. VM/model binding e libm trig são fronteiras. |
| Box: BC_WALK/BC_SQUAT e VM de animação | E | (c) 11/10: ambos os pacotes, quatro ramos WALK e um SQUAT residentes. 40.004 amostras de fase/ramo/tween bit a bit contra 004569F0; negativo altera tween e falha. Decoder valida host time, expressão, posições/UV em dois endpoints e cores iguais do caminho VU1 tipo 25h. Sem modelo/textura criado e sem upload por frame; não prova rasterização GS. |
| Box: matriz completa do pneu e raiz durante serviço | C | (c) 11/10: pit_transform_oracle compara 26.000 matrizes 003D2C48: ativo/inativo, cinco LODs, width adaptation, rotação/camber live/fade, 13 layouts e cinco fases, tol 2e-5. Mais 31.200 ticks de 003D0DF0, duas paradas por layout: root X/Z, old root/hubs, congelamento Euler e reset. Corrigidos hubs ausentes do layout 8 para zero, como getters originais. Negativos draw/root falham. Car getters originais executados, actor-placement 003CDB20/003CE930/003CDFD8/003CE4A0 e graphics/trig são fronteiras explícitas. Diferença de ~1 cm da recomposição fresca do slot 5 não prova erro de uma raiz congelada num frame anterior; comparação do ciclo passa. |
| Materiais MDLS: cores acima de 1 | E | (c) 11/10: 0049E228 +14..20/+9C..AC carrega e preserva três vetores float, 0049F038 multiplica pelas luzes antes da saturação. Contrato de instruções conferido em core.bin; 100 materiais sintéticos, 300 vértices preservam ambient/diffuse/specular bit a bit, inclusive negativos e >1. Negativo repõe clamp indevido e falha. Prova dos dados decodificados, sem executar VU ou certificar iluminação/pixels. |
| Box: posse dos controles no B-Spec | C | (c) 11/10: 003D9998 força vista live nas fases 1..5; 003D8C48 bloqueia todos os handlers de monitor, não apenas setas com menu visível. 18 gates E contra EE original, negativo próprio. pit_monitor_probe percorre dois serviços com racemonitor.strb do disco: força live, rejeita seleção/ordens/velocidade e libera após sair, tolerância zero nos estados; negativo de modo falha. Integração C, fade/composição integral ainda A. |
| Áudio: box (wheelgun, dropnut, droptire, jackup, jackdown, oiltank, refueling) | A | (c) 11/10: ordem de marcas/one-shots e flag do fluxo bit a bit contra 003CFA98 nos 313.600 ticks de pit_controller, seis enums 43/3F/40/42/41/45; loop 46 conforme 003D3358. race_pit_audio confere oito waves do disco, handle persistente, pausa/retorno/stop, negativo troca pistola e falha. Capture de 980 ticks: 11 gun, 2 nut, 8 tire, 2 jackup, 1 jackdown, 511 frames de fluxo e 2 change; RAM física imutável. A: waveform, espacialização e sincronismo do mixer PS2. Oiltank 44 está disponível; nenhuma marca sonora de disparo comprovada (004A2808 com 44 é ALPHA gráfico). Timbre CRX fica pendente como política PC deliberada. |
| Áudio: banco da CRX Si 1990 do slot 7 | E | (c) 11/10: race_engine_bank_capture compara tabelas 00559470 +30/+34 e SPU2 do estado fornecido: engine/normal0 48306 e roadnoiz, 11 camadas, 176.192 bytes ADPCM idênticos; --negative adultera bloco e falha. Timbre final e causa restante do relato continuam A, sem ajuste por ouvido. |
| Áudio: backfire / backfire2 | C | (c) 11/10: produtor nativo 003D6D30, sinal somente lido de 003F33A8 (car+6C4 e parâmetro+10DC), quatro partículas e RNG próprio 00579330/00578598, semente 003D6C18. `race_backfire` compara 16800 frames, todos os bytes +10..6B e o pulso, sete taxas incluindo bordas, três dt; física permanece intacta. Getter de saídas CAR4/CARINFO 00390898/00450FB0 confere nove contagens e arquivo truncado; 16809 casos, tolerância zero, negativo altera pulso e falha. Algoritmo E; integração C: clock de áudio posta +2890+64 e 003B5E80 consome, condicionado às saídas residentes como 003B6B78. Release e quatro testes de backfire/clock passaram. Waveform final e sorteio global de backfire/backfire2 continuam A, sem comparação PCSX2; chama básica desenhada por draw_afterfire; tween/cor authored e passe de brilho no chão ainda F (00456850/004A60E0), imagem A. |
| Áudio: playlist de emergência da corrida (faixas permitidas) | E | `race_sound_oracle`: `00437B10`, chamado por `00336DE8 +0x24`, constrói as mesmas 50 entradas a partir do ADS racing de 65; hash `0044FEC0` e shuffle executados no original. Prefixo `photo_` (`006A5E00`) exclui 15 Photo Travel. Lista independente do replay; a opção americana inicial é outra lista, abaixo. `--playlist-negative` e `--jingle-negative` falham (c) |
| Áudio: seleção inicial americana da jukebox | E | `race_sound_oracle`: todos os 0x810 bytes iguais ao construtor `004379F0` mais a seleção e ordem de 36 faixas do `/config/config-main.adc`, `bgmplaylist.h init_raceplaylist/apply`. O port executa essas duas funções originais; teste independente lê a ramificação US do bytecode e compara IDs, flags e ordinais completos. Mapeamento `0019E900 +0x154..0x168`, header `00438150`. `--defaults-negative` altera ordinal e falha (c) |
| Áudio: embaralhamento e avanço da jukebox | E | `race_sound_oracle`: 10.000 objetos com 0..128 entradas, flags e sementes variadas contra `00437E98/00437F58` e `00438170`; todos os 0x810 bytes e escolhas iguais durante três ciclos por caso. RNG `00578598/00579330` e semente u16 preservados; fim de ciclo limpa consumo sem novo embaralhamento. `--order-negative` altera semente e falha (c) |
| Áudio: preferências nativas salvas da música | E | `race_sound_oracle`: snapshot completo de 0x810 bytes, flags race_bgm/replay_bgm e volumes diferentes sobrevivem ao arquivo, encerramento da sessão e recarga; 108 escolhas retomadas iguais ao original. Cópias e condições de corrida/replay/override comparadas com `0033C180/0033C278`. Replay padrão mantém fila independente e não grava avanço na corrida. Cached menu preserva blob atualizado; fechamento do áudio precede save final. `--preferences-negative` corrompe cursor e falha. `race_audio_probe` verifica open/close reais, isolamento do replay e recarga de perfil; `--persistence-negative` suprime writeback e falha. Prova de persistência nativa; importação de opções de memory card PS2 não coberta (c) |
| Áudio: curva de fade da corrida e ganho do stream | E | `race_sound_oracle`: 640.000 ganhos/ticks bit a bit contra `003C0F40/003C04E8/003C0FB0`, evento 1 em cada cruzamento de zero, 10.000 inícios/overlaps, 131.073 ganhos Q15 contra RPC `00548060`; 7.380 casos do disparo no último segundo do replay contra `00389B08`, incluindo limite 60 frames, modos, transição ativa, timer e loop. `--fade-negative`, `--fade-event-negative`, `--gain-negative`, `--replay-fade-negative` falham. `race_audio_probe` confere produtor real em 15.860 ticks, mute, jingle sem volume da corrida, enum12 com volume /128 e fade; `--music-negative` falha. Não certifica waveform, latência da decodificação ou fades globais de carregamento/menu (c) |
| Áudio: ganho final e gate BGM de corrida/replay | E | `race_sound_oracle`: 40.000 casos bit a bit contra `0033D4B8/0033D510`, cinco modos, todas as combinações de race_bgm/replay_bgm/toggle, ganho base e externo variados, inclusive zero com sinal. `--stream-negative` troca o mute por ganho 1 e falha. `race_audio.c` usa `gt4_music_stream_gain`; produtor real continua passando seus 15.860 ticks. Escopo: gate e multiplicação final; não certifica waveform nem fades globais (c) |
| Áudio: política aleatória da corrida/replay solicitada pelo usuário | P | Pedido de 04/10/2026: replay toca enum12 desde a abertura, copia os IDs/flags habilitados da corrida e embaralha sem gravar avanço sobre ela; perfil sequencial passa a shuffle, persistido quando há perfil ativo. Difere do fallback de replay de `0033C180` e do default US sequencial. `race_audio_probe` confere os 65 IDs/flags, rejeita pedido autodemo enum26 e comprova ordens diferentes com clocks injetados; `--replay-negative` e `--seed-negative` falham. Algoritmo isolado continua E; fonte de entropia PC é A (c) |
| Áudio: saída do vídeo ao sair da tela | A | Decoder nativo em cache podia conservar a saída de música depois do MovieFace desaparecer. main.c desliga decoder/previous quando movie.seen é zero; referência de teardown original `0023E078 -> 0023F890`. Build e smoke de corrida para replay passaram, ambos publicam PCM/racing. Ainda falta comparação sonora e reprodução completa da passagem do vídeo do menu para corrida com PCSX2 (c) |
| Áudio: música da corrida (waveform e transporte completo) | A | ADS v2, fila original acima; opção US inicial de 36 e fallback original de 50; replay nativo copia as faixas habilitadas da corrida e embaralha como política PC deliberada (P), sem jingles start/goal. SDVL coluna0, opção /128 somente enum12, gate BGM e fade/Q15 originais; troca para stream novo para o anterior sem crossfade inventado. Ainda A: interpolação linear 44100->48000, waveform SPU2 não comparada e latência do worker/IO PC; timer PC nos sorteios de grupos não certifica GetSystemTime do PS2. Fades globais de menus/carregamento e importação de preferências de memory card não incluídos nesta prova (c) |
| Áudio: sorteio das músicas entre corridas e replays | A | Semente do jukebox do mundo: `00336DE8` usa D_006464DC (12345678h só se zero), que nas capturas muda a cada boot (8C507570h, 8C579D70h); o escritor não foi achado, o relógio do PC faz o papel uma vez por execução. O jukebox do replay segue de um replay ao outro na sessão. A posição gravada pela corrida (0033C278) não é mais sobrescrita pela cópia velha das opções de um menu: cada captura de menu mantém a mais nova e a passa ao menu. Teste avulso: cursor 11 -> corrida 12 -> captura do menu 12; três corridas seguidas tocaram as entradas 21, 22, 23. Ordem padrão da corrida sem embaralhar (do script do disco) (c) |
| Menus: encerramento das sequencias MAP | A | Removido corte prematuro no CC99/30: conserva outro authored ate FF2F. Fonte 0055B528 +0x298..0x2D8 (loop), 0055B948 +0x58..0x64 (fim). Cinco faixas MAP com dados depois do marcador. sound_tests positivo/disco passam, dez trocas/dois ciclos; GT4_MUSIC_OUTRO_NEGATIVE corta em 24000 em vez de 48000 frames e falha. A: single-pass, contador de loops, cauda SPU2 apos FF2F e fades de paginas sem comparacao original. Sem fade arbitrario (c) |
| Menus: proporcao das capturas durante transicao 16:9 | A | 11/10: video_gl marca capturas da largura completa; quads ampliados e blocos mapeiam o framebuffer inteiro, como 002182E0 +0xA4..0xF0 (dimensoes do buffer). Antes somente o quad exatamente 0..640 expandia, comprimindo os quadros ampliados para 4:3. ui_blend_probe confere pixels internos/externos em 4:3 e 16:9 e 129 blends; GT4_CAPTURE_ASPECT_NEGATIVE usa textura comum e falha. A: adaptador GL testado, sem comparacao visual PS2 da animacao completa (c) |
| Menus: projecao 2D em 16:9 | A | 11/10: 00215FE0 +0x30..0xAC escala a area logica 640x480 ao framebuffer completo; menus nao possuem a compensacao do HUD. video_set_menu_projection separa isso do pacing e do HUD da corrida. Removidas margens extras dos widgets de menu; recorte e captura usam a mesma escala. ui_blend_probe positivo/GT4_CAPTURE_ASPECT_NEGATIVE e captura real MakerRoot 1280x720 passam. A: sem comparacao de pixels GS/PCSX2 (c) |
| Menus: cursor da selecao P1/P2 depois de popup | A | 11/10: set_focus libera retencao do adaptador modal quando o widget realmente muda. arcade.ad SplitRoot.onInitialize L3300 escolhe P1.Select/P2.Select/RaceStart; antigo hold podia impedir cursor_target apesar de foco correto. split_select exercita esquerda/direita/Enter, mede seta desenhada, injeta hold antigo; --cursor-negative falha. A: reproduz estado do adaptador, sem captura original do defeito ou timing completo PS2 (c) |
| PC: perfis locais para garages e Trade, Save imediato | P | Adaptação PC 11/10: chooser in-game com os oito perfis substitui card slots; leitura independente com gt4_save_read_profile preserva jogador ativo/dirty. Contrato original loadGameDataForTrade retorna [status, game]; Home.Trade e Arcade.openGarage continuam executando scripts do disco. load troca perfil somente no pedido de carga explicito; Save escreve perfil ativo e confirma, ManageSaves conserva browser. Labels/dialogs descrevem storage local. profile_storage_test confere Home -> Trade, cancel, Arcade -> GarageRoot, corpo completo 0x4b8, origem/ativo separados, load e Save persistido; --negative altera saldo da origem e falha. Captura/probe real do seletor escolhe perfil2. Layout/armazenamento PC sao mudanca intencional; regras de compra originais nao alteradas (c) |
| PC: todos os modos de armazenamento e textos de dispositivo | P | Adaptação PC 11/10: 17 modos encontrados nos ADC de 34 projetos usam perfis/replays/PNG locais, ignorando slots do console. storage_audit_test verifica 10.370 strings US, com 308 referencias adaptadas inclusive quebra de linha; textos dinamicos e dialogos passam pela mesma adaptacao. Linhas McSlot e icone antigo ocultos/substituidos. PNG saveNew e exportacao so retornam OK apos escrita real; filmes PS2 são preservados em .ogt4film e revelados pelo Photo Lab, com saída PNG nativa. Teste salva/le/deleta PNG e replay, verifica falha de exportacao e perfil ativo7; --negative reintroduz texto antigo e reprova. profile_storage, replay_file_tests, split_select e photo_travel_probe passam. Nomes internos dos scripts sao compatibilidade, sem I/O de dispositivo. Nao certifica apresentacao PS2 nem implementa impressora fisica. |
| Importação PS2 de replay/foto: dados e contêineres | C | 12/10: save_ps2 visitor/ps2_media leem PSU e PS2 com páginas 512/528 ou diretório extraído, sem escrever na origem. P####: 001D0CC0/001D4E60, keys 3F2E4F76/3ECED916; JPEG convertido para PNG na plataforma. Replay PS2R conserva bytes encriptados e usa Base/GT4M. ps2_media_fixture.py: comandos Rdemo0 do disco iguais byte a byte, JPEG autoral contra PNG de Pillow com tolerância3/canal, FAT/ECC/ciclo/CRC/truncamento. Negativo muda pixel e reprova. C restrito aos casos testados, sem mídia pessoal PS2 real; classes não suportadas são skipped. Armazenamento local P. |
| Replay PS2: classes e múltiplos recorders | C | 12/10: replay_types_test cobre 12 classes/48 fixtures, count0..3, dois recorders independentes (003303F0/00346DA0), RACE key independente do nome, setup, eventos, CRC/truncamento; negativo altera steps e falha. disc_replay_oracle: 93 arquivos reais e 556738 comandos completos de 32 bytes bit a bit; negativo altera steering. E restrito a comandos; adaptador C, trajetória integral A. Ghost PS2 permanece F; LAN/Net N no escopo vigente. Classes internas sem produtor público confirmado não demonstram recurso ausente; detalhes em docs/notes/PHOTO_MEDIA_COVERAGE.md. |
| Importação de filmes PS2: leitura do snapshot e cor | E | 12/10: photo_film_format compara 256 FILM escritos por 00334FC8, 256 PCAM escritos por 0033ADE8, 256 bancos CPRM lidos por 0045A4A0 (v0..3), 256 esqueletos DRVR (0041BCC0/0041C0B8), 256 interpolações de 20 ossos contra 0041B048, 256 registros SMOK/PTCL (003E93D0/0040B690), 256 ângulos de roda (004594D0), 256 limites de suspensão (004514A0) e 256 coeficientes/filtros de 00334B28/003EFC10/003EFD80, bit a bit. PMDI9 sem alinhamento, dimensões s16, tiles/amostras s8, booleans byte e frame count s32. Truncamento rejeitado sem alterar a saída; negativos alteram largura, osso, interpolação, ângulo, limite e folha e falham. Soma VU0 da interpolação substituída por quatro somas com arredondamento original; restante executa código do jogo. Traversal RHDR/SFRM e animação DRVR v1 usam fixtures autorais; não certifica filme real, dedos/bloco inteiro DRVR nem pixels. |
| Importação de filmes PS2: revelação e Photo Lab | A | 12/10, autorizado implementar e comparar depois: Import Media preserva .ogt4film, Film/commandDevelop do labomode.ad L1574..1595 despacha PhotoSave PreviewRoot; cena, carros/cor, pose corporal, PCAM, foco/abertura/focal, amostras/exposição e coeficientes de filtro recuperados. ENVP 0x17f4 de 0033A150/0033A838 restaura luz/fog/sheen/cor/sombra na cena residente, sem escrever na pista original. film_menu confere lista, confirmação, load, consumo único e volta; film_develop reconstrói Tsukuba e Brooklyn com Ka/BEAT, prova pixels do carro, preview congelado, PNG e fonte intacta. Cada teste tem negativo que falha. Integração nativa C; imagem A sem filme real/PCSX2. CDTL wheel/wing aplicado a todos os carros; CPRM restaura hubs, spin, steer, camber, delta e dimensões dos quatro pneus (00450F50/00458A30). DRVR restaura/interpola esqueleto e grasp v1; SMOK/PTCL restauram fumaça e folhas sem ticks físicos. film_develop retira cada detalhe separadamente e confirma diferença de pixels em duas pistas; integração C, imagem A. Pendentes: body/LOD CDTL, plano de recorte SMOK, partículas entre snapshots, GDGT, develop ambiental, far clip, retrato e tiles GS. PNG na resolução do viewport e acumulação GL são P. Sem alteração da física. |
| Photo Drive: PhotoPause e PhotoSave | A | 12/10: /menu/common/racephoto.strb (003C4640 +30..40), RaceDisplay de raceUS.rt2, callbacks 003C40C0, preview no projeto original photo_save. photo_pause_test positivo/negativo verifica getter/blink -1/movie/save binding. race_resources_test verifica preview/captura/retorno à ShootRoot e mundo32MiB/cursor congelados; --photo-negative libera mundo e reprova. Gaiji %B de 004940E8; fonte embutida 00479CA0 +138..188 (altura/1024 e baseline do arquivo). A: oito segmentos por curva, GL/lente/pad/câmera e decimal EE sem comparação integral. P: teclas PC e atalhos de controle. Banco de até 64 câmeras com pontos REP4 e posições ao redor do carro (003FFC68/003FFD10); alterações conservadas por câmera. Zoom com rampa de 18 quadros (00380100), exposição e branco lidos de D_00620F40/D_00620F98, foco central/lock, ajuda e roll. race_resources também prova troca/persistência de câmera e aspecto da revelação; negativos camera/film falham. A: particionamento/sorteio, colisão/limites por preset, pad integral e autofocus GS ainda sem reprodução integral. Não é pausa completa E. |

| Menus: startPage com várias páginas | A | `0022BC20`: `0022F6A0` finaliza toda a pilha (vt+0x2D8 na cópia, `00233D18`) e cada argumento é empilhado em ordem (`0022F890`). O cancelar do CategoryRoot do Arcade (startPage(TopRoot, GameRoot)) volta ao GameRoot com o TopRoot embaixo; antes caía no TopRoot com a pilha errada. Conferido com `--input` (GameRoot -> CategoryRoot -> voltar -> GameRoot -> voltar -> TopRoot). startPage de uma página ainda troca só o topo (o original também limpa a pilha; não mudado sem caso que mostre a diferença) (c) |
| Menus: início da música do GT Mode após loading | A | 12/10/2026: preparação começa no pedido authored (RootWindow.bgm/keepMusic/startStreamEx), antes de onInitialize/panIn/Sync terminar. MIDI publica prefixos PCM de 512 frames; callback só copia frames publicados atomicamente, sem síntese/IO; buffer inicial PC de 8192 frames (alinhado ao preroll do backend SDL) (P). Grupos com várias sequências preparam em rodízio; MAP continua avançando na playlist e os demais preservam o loop da sequência. Publicação no backend continua bloqueada até o fade do loading acabar. native_sound compara cada prefixo com o PCM preparado inteiro, byte a byte, e o negativo altera um sample e falha; menu_navigation confere pedido e keep. Ponte PCM testa preparar durante loading, silêncio até a liberação, publicação no primeiro update e PCM do disco; negativo publica antes e falha. A: síntese/ADPCM existentes ainda sem waveform/latência SPU2 ou comparação auditiva PCSX2 (c) |
| Menus: continuidade MIDI durante Sync e publicação do buffer | A | `0023E3D8 +0xAC -> 004616E8`: keepMusic inicia no pedido, sem aguardar panIn; grupo já tocando conserva a sequência (+0x30..+0x58). App agora compartilha o transporte entre loop externo e pump de apresentação; antes só PCM era atualizado dentro de Sync. Preroll pronto alinhado aos 8192 frames já consumidos pelo SDL ao anexar; publicar só4096 podia enfileirar4096 zeros. `menu_music_transport` usa MIDI do disco: publicação sem update externo, pedidos repetidos/keep sem restart e PCM contínuo, loading, fade até o fim, prefixos simples/playlist. Negativos restauram publicação adiada e prefixo curto e falham. Rotas reais GT Mode Home/Garage e captura isolada dos dois menus executadas. A: sem captura auditiva/temporal PS2 e sem garantir cobertura de todo corte intermitente relatado; nenhum crossfade/duração novo inventado. |
| Menus: música ao trocar de projeto | A | O `mSound` do projeto para a música no destrutor (`0023E078` -> `0023F890` -> `002C3BA0`/`00460B90`): ao trocar de projeto o pedido anterior é esquecido, e o projeto option (sem bgm nas 20 páginas) fica sem música, como o usuário viu no PCSX2. Sem teste automático (c) |
| Opções: gravadas ao fechar o jogo | A | Opções marcadas pelo `setNeedSave` dos scripts e ainda não aplicadas por uma troca de página são capturadas e gravadas ao sair (antes só iam se o save já estivesse sujo). Quais opções o usuário perdeu não confirmado; sem teste automático (c) |
| Fumaça dos pneus | A | Objeto do mundo +0x31D0 portado em `race_smoke.c`: 256 partículas de 64 bytes; emissão por roda a cada passo (`003667A8` -> `00395C70` -> `003E75E8`, o bloco de 0x3C bytes da física agora entregue por `gt4_wheel_smoke` também no modo nativo), quantidade = roda +0xBC (`gt4_car_surface_feel`, curvas 0x843A40+0x420 iguais às da captura ar_02), classe pelo piso (D_006A3A00), parâmetros das 6 classes do bloco Env0 da pista (cabeçalho +0x80, +0x4D0 + classe * 0x44: cor, limiar, tamanho, crescimento, subida, giro, esmaecimento, gravidade; conferidos iguais na memória da captura de Tsukuba), sorteios pelo registrador R do VU0 (vrinit 0.1 / vrnext), tabelas D_006A3A60/3A80; atualização `003E7A58` (gira, cresce, esmaece, sobe e cai; some sem intensidade ou abaixo do chão); desenho `003E7F08`/`003E8610`/`003E8BA8`: quadrado de meio lado = tamanho, girado, voltado para a câmera, um dos 4 quadros do Tex1 da pista (cabeçalho +0x68, D_006A3AB0), alfa = intensidade * parcela * classe +0xC * 255 (ou pela distância com Env0 +0x10 bit 0x40000: 153,1 + 102 * min(120/z, 1)), no máximo 128; ordenado de longe para perto e misturado (Cs-Cd)*As+Cd (Env0 +0x918 = 0x44 nas quatro pistas lidas), cor = textura * cor da classe pela luz do chão (MODULATE). `smoke_probe <iso> tsukuba`: Ford GT derrapando com freio de mão após a largada lançada, 43 partículas e 1.085 pixels alterados; `--negative` (blocos não entregues) falha com 0. A: sem print do PCSX2 lado a lado; giro tomado em graus (`004A7438` é microprograma VU0 não lido); a sequência aleatória diverge da do jogo (o R do VU0 é usado por outras rotinas); partículas pequenas não são cortadas no plano do chão como em `003E8790` (o teste de profundidade contra a pista faz o papel); renderização em resolução reduzida (Env0 +0x914/+0x91C, 1 nas pistas lidas) não feita; névoa sobre a fumaça não aplicada; fantasma do Time Trial sem fumaça (não lido) |
| Folhas levantadas pelas rodas | A | O segundo efeito por roda (`003667A8` -> `00395C98` -> `003C2160`, objeto do mundo +0x3238), só em triângulos de colisão com o bit 0x20 (byte +7): no disco, só Autumn Ring e a Mini (e as invertidas), 1.312 e 372 triângulos nas margens (pisos 1 e 2). Escorregamento (+0x38 - +0x34) vezes 0,5..0,95 de 2 m/s ou mais lança uma `LeafParticle` (vtable D_006863A0): para trás da roda a 15 graus do rumo, 65 a 85 graus do chão, para o lado do sinal (roda travada: para dentro do chão, some na hora; patinando: sobe), 85% da velocidade do carro somada, giros e ângulos sorteados (R do VU0); 8 tipos de 1 g (`003C1EC0` -> `0040AB98`: 12, 13, 14 e cinco de 10 cm, inércias de placa, arrasto -2wh na normal e -0,01(w+h) no plano, amortecimento do giro). Voo `0040AFB0`: eixos por ângulos de Euler (`0040AD10`), arrasto no referencial da folha, gravidade, torques giroscópicos, giro * 0,999; some ao passar do plano do chão mais a diagonal da folha. Desenho `0040B4E0`: quadrado plano girado com a folha, imagem 1..8 do Tex1 da pista (cabeçalho +0x9C; a imagem 0, da faísca, é PSMCT24, agora lida pelo `tex1.c`), cor `0040B3C8` pela luz da pista (Env0 +0xD0 direção, +0xE0 ambiente * luz do chão, +0xF0 difusa, / +0x38), lado visto decidindo o sinal. `smoke_probe <iso> autumn --leaves`: o carro levado à margem com folhas, patinando; 5 folhas no ar, 56 pixels (formato de bordo conferido ampliado); `--negative` falha com 0. A: sem print do PCSX2; teste de alfa das folhas (nível) e ordem de desenho não lidos; folha desenhada sem escrever profundidade; sorteios divergem dos do jogo; seno/cosseno da libm em vez de `0057D1F0`/`0057D8A0` |
| Faíscas | C | Existem no código mas vêm desligadas: `00390980` (callback do modelo do carro, pontos de raspar o assoalho) só as solta (`003C24A8`, `SparkParticle`) com D_00621B50 +0xA8 diferente de 0, e `003C2B20` o põe em 0 sem outra escrita no código; 0 nas 40 capturas. Não desenhadas, como no jogo |
| Marcas de pneu no chão | C | Não há marca dinâmica no código: por roda só a fumaça (`00395C70`) e as folhas (`00395C98`); as marcas vistas nas pistas são da textura do cenário |
| Pausa, reiniciar, sair | A | O menu original roda: `/menu/common/pause.strb` com os callbacks de `func_003C3260` (EXIT_MENU continue/retry/exit, RETRYENABLE, SCREENSETTINGENABLE, CONFIRMEXIT, NAVIGATEVERTICAL/ALL, PLAY_SOUND, SETVAL/DEFAULTVAL). Opções do Arcade como todas as capturas guardam o RacePause: reiniciar 0, ajuste de tela 1, confirmar 0 (menu CONTINUE/EXIT/Screen Setting; reiniciar não aparece no Arcade). Corrida parada, HUD congelado; EXIT volta ao menu. Textos pelas chaves do raceUS.rt2, página RaceDisplay ("Screen Setting" -> "Screen Settings", como o PCSX2). Painel: os contornos de lados retos do movie preenchidos de verdade (os cantos 4x4 recortados para os bitmaps redondos), não pelo retângulo que os contém. Fontes sem "#" (hm_16): `func_00490290` só muda a paleta do motor de texto (D_00624980 +0x2C, zerada em `func_0048FAA8`) no "#", então valem a deixada pelos textos anteriores; na corrida, só tinta (rótulos pretos sem contorno no print do PCSX2, como a mensagem hn_14 do replay). Sons do movie (PLAY_SOUND): "ok", "cursor", "cancel" do gt4sys.ins (GTSE banco 0, sequências 6, 2, 1) no driver da corrida, que segue rodando na pausa. Aproximado: tecla de pausa (Esc/Start abre e fecha) e regra do botão mais próximo para o cursor não lidas no jogo; brilho e contraste aplicados e persistidos; provas E/C separadas na tabela; volume dos sons do menu = opção SE x coluna de efeitos do sdvol (o do callback não achado); o que o jogo faz com o som dos carros na pausa não lido (as vozes ficam como estavam); qual texto deixa a paleta 1 não rastreado |
| Box: animação da equipe (mecânicos, macaco, troca de pneus e abastecimento na tela) | A | Ciclo funcional integrado, inclusive mecânicos distantes, espera/serviço/retorno e duas paradas consecutivas. Lógica/poses/matrizes E/C nas provas acima; apresentação integral do renderer compartilhado segue A. A física dos boxes permanece E e não foi alterada. |
| Pista: cor (texel × cor / 128; sem textura, a cor; tudo dobrado) | V | lado a lado com os prints dos savestates `s03` (Laguna Seca), `b_reta` (Tsukuba) e `roof` (Nürburgring); céu de Laguna 2,00x em cada canal, céu de Tsukuba 71 → 142. Onde o jogo dobra: `func_0033E828` → `func_00342580`, um retângulo sobre o quadro inteiro (carros incluídos) com ALPHA 0xA9 (Cd × FIX) e FIX = Env0 +0x38 × 128 limitado a 255, isto é, ×255/128 = 1,992 e não 2,0; o código ainda usa 2. Diferença em aberto: fora do asfalto e do céu o mundo sai ~10 a 15 abaixo do PS2 (grama, arquibancada, prédios) nas mesmas capturas (c) |
| Pista: céu (+0x54) atrás de tudo e fundo distante (+0x58) | V | a serra do horizonte nas mesmas capturas; como o jogo põe o céu atrás não foi lido (c) |
| Pista: conjuntos que não são da vista (+0x14 ambiente, +0x34 máscara) | V | o +0x14 desenhado deixava manchas que o PS2 não tem (`s03`) (c) |
| Pista: brilho/reflexo no asfalto (máscara +0x34, reflexo +0x24, céu do reflexo +0xC8) | V | `func_003411D0` (slot +0x378 da cena, depois do mundo): alfa do quadro zerado, máscara desenhada só no alfa, reflexo numa imagem de Env0 +0xC24 texels, borrado +0xC2C vezes e somado com ALPHA 0x58 (Cs·Ad + Cd), cor +0xC10 × +0xC1C, antes da duplicação do quadro. Lado a lado com os prints de `b_reta`, `roof`, `s03`, `s05`, `s06` (`extracted/renderer/sheen_sheet.png`): asfalto perto da câmera 90/87/105 contra 95/91/104 do PS2; no meio da pista fica ~12 abaixo. Não feito: passes extras das flags 0x10000/0x20000 de Env0 +0x10 (slots +0x388/+0x390; nenhuma das quatro pistas vistas usa); imagem só quadrada; o conjunto +0x24 de Grand Valley East decodifica com um comando não suportado (c) |
| Pista: névoa | V | Env0 +0x60 cor, +0x70 distância, +0x74 piso do F (`func_00397DA0`, `func_004A40B0`, `func_004A3F98`); fórmula por vértice lida do microcódigo VU1 de um savestate (F = max·255 + 0,1 + (255 − max·255)·min(w, d)/w; desmontador em `extracted/handoff/tools/vudis.py`); FGE ligado nas tags de vértice das capturas; céu sem névoa (`func_00396B80`); comandos 33-35 dão a cor própria do desenho (formas aditivas vão para preto). Lado a lado em `extracted/renderer/fog_sheet.png`. Mistura feita com F/255 (o GS real pode usar /256); o cubo de reflexo dos carros não leva névoa (c) |
| Carros: sombra na corrida | V | `func_00340950` -> `func_003907A0` -> `func_00451D18`: duas camadas no quadro da sombra (ponto de referência +0x894, rumo +0x674, inclinação do chão +0x678/+0x67C; subida de 0,02 e 0,008 mais perto na profundidade, `func_00452418`): o modelo 1 do carro achatado no chão na direção do sol (`func_004524B8`; sol de Env0 +0xA70/+0xA74 × +0xA78, `func_00398758`) e a mancha do modelo 2 (`func_00452660`). Cor Env0 +0xA60 / +0x38, alfa +0xA6C; o do modelo 1 vezes (sol × (+0xAB0 − +0xAB8) + +0xAB8) × nível, dos dois valores do chão sob as rodas (`func_003D7460`, `func_003983E0`). O modelo da corrida (`/car/lod`) traz no modelo 1 o alfa por vértice caindo com a altura (77 de 128 no chão até 0 no teto), por isso a sombra clareia longe do carro; o programa VU1 0x2A1 faz cor × escala + deslocamento e limita o alfa a 128. Conferido com a lista de desenho que ficou na memória do PS2 nas capturas `s03` e `s06` (escala de alfa 1,44 e 1,35 no modelo 1, 0,8 e 0,9 no modelo 2, deslocamento azul 0,64 e 1,28, ALPHA 0x44, FBMSK 0xFF000000, sem escrita de profundidade): os mesmos números. Lado a lado em `extracted/renderer/shadow_sheet.png`: forma, lado, tamanho e degradê batem; em `s06` a sombra sai 0,95 a 0,30 do asfalto contra 0,87 a 0,40 no PS2 nos mesmos pontos (os quadros não estão alinhados pixel a pixel). Não feito: pistas com tabela de lâmpadas (RNW4 +0x40, uma sombra por lâmpada, `func_0038F168`); nenhuma das quatro vistas tem (c) |
| Carros: luzes da pista (Env0 +0xD0, `func_00398210`) | A | A passagem dos carros (`func_0038EFF8`) usa as luzes de Env0 +0xD0 (0x80 cada, bit i de +0x10), não as de +0xAD0 (`func_00398058`, que eram as usadas antes e deixavam o carro escuro e arroxeado). Cada cor passa por `func_00397FF8` com os dois valores do chão sob o carro (sol, nível) e os pares de Env0 +0xAB4..+0xACC. A soma por vértice é a dos programas VU1 0x23/0x52/0x83 (ambiente + difusa × N·L + especular × (N·H)^4, ^8 ou ^16 pelo brilho do material, limitada a 255, tudo multiplicado pelo texel). Conferido com o bloco de luzes que o PS2 mandou ao VU1 nas capturas `s03` e `s06` (Laguna: ambiente 0,30 0,30 0,32, difusa 1,12 1,12 1,11, especular 1 1 0,99, direção igual). Lado a lado em `extracted/renderer/light_sheet.png`: o tom bate; a traseira da BMW em `s03` fica com média 44 contra 52 do PS2 (desvio 35 contra 28), a mesma ordem da diferença geral de brilho já registrada na pista. Diferenças conhecidas: o vetor H é calculado por vértice com a direção da câmera (o jogo manda um H por objeto); a segunda luz e o segundo especular do bloco não foram vistos em uso (c) |
| Carros: luzes locais noturnas e ambiente global | C | (c) 12/10: race_lights_sample lê lista RNW4 +40/+50/+68/+6C pelo índice de luz do primeiro vértice do triângulo (003E6C60), calcula até nove candidatas e seleciona quatro estavelmente como 0038E7F8; inclui Env0 +EF0 independente das luzes/fades, upload 0038EFF8. race_lights_oracle compara 192 contextos direcionais/pontuais/spots, lista ausente e ambiente isolado com os uploads EE reais, tolerância 0,000002; negativo altera ambiente e falha. Curso imutável byte a byte; ENVP de filme preserva precedência sem sobrescrever o curso. Auditoria de Rdemo0 identifica paris_rally, 215 registros locais; 477 amostras válidas em Paris, Gion, Brooklyn, Tsukuba e Nürburgring. 12/12 testes de luz/curso/render/regressão dos fiscais e revelação de filmes passaram; Release atualizado. C restrito aos valores de iluminação. Imagem integral A: seleção do triângulo no render usa a malha imutável sob a pose desenhada com teto +5 m, sem usar/interpolar o cache de contato original; trig/pow/sqrt libm, shader e reflexos continuam sem comparação integral PCSX2. Capturas locais nightbefore-13.png/nightfixed-13.png mostram Bentley verde e detalhes recuperados sem alterar brilho da pista. Física não editada. |
| Carros: reflexo da pista (ambiente +0x14, céu +0x5C, `func_0044E660`) | A | conteúdo e câmera do jogo; as 3 vistas de 120° + malha `D_006A7D00` aproximadas por cubo → mapa esférico, como no showroom; cada texel do mapa lê as faces com filtro bilinear (antes o pixel mais próximo dava serrilhado no vidro traseiro do BMW, que reflete o zênite). A coordenada no mapa é calculada por vértice e interpolada, como o STQ do VU1 0x215 (c) |
| Carros: cor do reflexo (Fresnel) | A | VU1 0x215 lido do microcódigo de `s03`: cor da camada de reflexo = cor do vértice × (205 + 206·min(I·N, 0)/\|I\|), sem luzes; 205/206 = ctx +0xBB0/+0xBC0 × +0xBA0 (`func_0049F170`), postos a cada modelo (`00455464`) a partir de Env0 +0x34 e +0x34·+0x30 (`func_00397CE0`; Laguna 1 e 0,8, como na RAM de `s01`/`s03`); comandos 86/87 ligam/desligam; ×2 do quadro (+0x38). Traseira do BMW em `s03`: média 69 contra 75 do PCSX2 (antes 64). Showroom sem mudança (c) |
| Texturas: filtro (TEX1 MMAG/MMIN) | A | Descritor Tex1 +8 é o TEX1 que vai ao GS; com MMAG e MMIN 0 a textura é amostrada sem filtro (ponto). Fiat Coupé (`fiat0004`, 14 de 145 texturas assim): silhueta dos bancos (tex 10) tem texels transparentes vermelhos que vazavam pelo filtro bilinear como traços vermelhos no vidro traseiro; com o ponto somem, como no PCSX2 (`s06`, lado a lado). Mip/LOD (MMIN 2..5, K/L) ainda não seguidos (c) |
| Carros: luz de freio | A | CarModel::Callback `00453698` entrada 0 lê o +0x38 do contexto, posto a cada quadro por `00390C90` -> `00451370` a partir de `003446B8` (pedal de freio +0x590 acima de 0). Ramo 0 = lanternas apagadas, ramo 1 = acesas; os dois são lidos e o desenho escolhe pelo pedal de cada carro. Só os modelos que têm o callback 0 mudam (Fiat Coupé sim; Ka e BMW 320i não têm, no PS2 também). O callback 7 é outra coisa: o modo das luzes fixado na montagem da corrida (`00387FB0`, vtable +0x158). Falta conferir o freio aceso lado a lado com o PCSX2 (c) |
| Carros: retângulos claros nas lanternas do Ka | V | máscara DATE/DATM dos comandos 29/31 de `00455708` ficava antiga: cópia GL inválida no framebuffer MSAA. Agora resolvida na GPU; `model_mask_probe` passa 8 casos em 1x/4x e `--negative` falha. Prancha local `extracted/renderer/ka-lamps-comparison.png` (antes/depois e PCSX2 u29); o retângulo sumiu. Luz/reflexo completo e bordas MSAA continuam com as limitações A acima (c) |

### 4.3 Menus e GT Mode

Preencher ao conferir a área. O que os documentos da área dizem hoje:

| Item | Nota | Prova ou observação |
|---|---|---|
| Arcade car specifications and rebuilt-list focus | C | `arcade_return` verifies drivetrain/power/torque/RPM text on initial CarRoot entry, compares subsequent automatically updated text to authored set_car_info (arcade.ad L4116..4125), then changes maker after selecting several cars and checks index zero with a populated list. focus_callback follows 002B29C0 +0x9CC..0xA58; setItemCount bounds follow 002B4A00 +0x2D0 -> 002B3AF8 +0x38..0x84. Negative cases corrupt the text or restore index 2. Tolerance zero for text/state; full PS2 raster and all formatter outputs remain outside this proof. Existing preserved pre-race focus is not globally reset. |
| Arcade reload: cursor and showroom state | C | `arcade_return` preserves game data and live menu stack across a fresh project load, checks the outgoing hidden cursor and stale showroom state are discarded, then executes CategoryRoot → CourseRoot → MakerRoot → CarRoot and checks focus and a populated car list (zero tolerance). `arcade_return_cursor_negative` hides the cursor; `arcade_return_list_negative` empties the list. Source: ArcadeProject.onLoad L27..32/L40..69 and SettingPopup.OK L4980. The previous destination-only proof did not cover cursor/widget lifetime. Complete PS2 transition imagery is not compared. |
| Arcade pre-race: L1/R1 and native root key fallback | E | `pregrid_shoulders_oracle` runs original `00235680` with an inert script boundary, compares L1/R1 keys from `D_006192E4`, and checks native authored quick-arcade focus on every available icon before/after completion. Original fallback returns zero without navigation; consumed script events retain precedence. Negative injects Right and detects the changed focus. Scope is these keys and this page; other scripted shoulder actions and full draw output are not covered. |
| Car and course favorites: list algorithms and persistence | E | `favorites_oracle` executes original `00430320/00430360/004303B0/004303F8/00430460/00430528/00430570`, comparing the limit, insertion order, duplicate/full rejection, stable removal and all 264 serialized bytes after every operation; negative changes capacity to 33. `favorites_menu` executes authored Option CourseRoot Item toggle and Arcade buildFavoriteCourse, original SpecDB labels, exit capture with no options edits, dirty/unchanged mutation checks, save/reload and cached project transfer; negative clears the course list before reload. All eight MFavorite methods are bound after profile restore. PC save container is P; original list fields/keys/order are preserved. |
| Certificado e carro-prêmio: sequência de apresentação | C | (c) 12/10: gtmode.ad L14712..14736 espera 3 s, move Paper de x640 a x150 com MMoveActor ratio .2, espera 5 s e libera OK/cursor; CarPane L14542..14640 usa Sync 4/1/3 s e move_car L14663..71 (tz8.5, ry180, motion1, posInterpolateSpeed .025). Corrigidos dispatch sob loading, posição que saltava ao alvo e ausência de atualização motion2 sem Mot1. gtmode_award_presentation compara 480 frames por cerimônia com a sequência authored e mantém OK oculto durante os Syncs; negativo suprime animação. gtmode_award_app exercita ambas no executável com perfil descartável e captura movimento. Tolerância zero para sequência/estado; imagem integral A, sem comparação equivalente PCSX2. |
| CarModel: interpolação de posição da premiação | E | (c) 12/10: func_00153BB8, trecho 00153CAC..00153D1C, position += (target-position)*model+58. gtmode_service_placement compara 46.080 frames bit a bit com código original recompilado, nas taxas .025/.1; negativo altera posição e reprova. Escopo: integrador isolado, não certifica pixels da cerimônia. |
| OK do certificado e do carro-prêmio: centralização | A | (c) 12/10: TextFace passa pela centralização de tinta já usada nos botões arredondados; medida usa gt4_font_original_step. gtmode_award_presentation verifica centro da tinta até .51 px para os dois OK; negativo desloca referência 4 px. Centralização é política de apresentação do port, ainda sem oráculo de desenho do botão original. |
| Som de conquista durante apresentação | C | (c) 12/10: get_car é solicitado no início de get_paper/get_car (gtmode.ad L14712/L14542). present_wait publica PCM pronto durante Sync, respeita loader e retira MIDI antigo antes da publicação; liberação de MIDI usa dono para não apagar PCM novo. gtmode_award_app verifica duas publicações PCM/get_car entre início/fim do primeiro Sync de cada cerimônia; negativo exige publicação tardia e reprova. C restrito à ordem observada; teste usa áudio dummy, forma de onda e comparação auditiva PS2 continuam A. 10/10 testes selecionados passaram, incluindo retorno real de Options positivo/negativo. |
| Comparação de profundidade das camadas 3D | C | (c) 12/10: comando25 00456000 chama 004A26F0; +00..+1C lê D_00629DD8 (0,1,3,2). API2 é estrito, API3 inclui igualdade, independente de ZMSK. Port trocava 2/3 e acrescentava offset estimado para leitura sem escrita. Corrigidos ambos. render3d_depth_test lê a tabela do core.bin e verifica 32 frames (2 modos ×2 escritas ×8 posições), RGB tolerância1; negativo inverte igualdade e falha. film_develop passa em Tsukuba/Brooklyn. C restrito ao comparador; precisão GS, composição integral e cintilação relatada na licença8 ainda sem comparação visual do original. |
| MIRA TR-XX Avanzato R 1997: carroceria ao frear | C | 12/10: CAR4 daih0002 de car/lod, MDLS +7C1 callback0: solto +7CB contém callback7, que pula para +7FD por opcode6, atravessando o ramo pressionado +7EB. O decoder só reconhecia a junção via op8/9; marcava 2.937 triângulos comuns como exclusivos de freio solto e não carregava o ramo pressionado. Agora reconhece a junção em todo desvio antes de etiquetar formas. Fonte 00455708/00453698. car_brake_branch compara cada estado com seu caminho autorado selecionado independentemente na tabela do disco: 3.680 triângulos comuns e mais80 ao pressionar; vértices, atributos e estado com tolerância zero. Negativo marca carroceria como só solto e falha. model_tests/model_stream_tests passam. Imagem completa PCSX2 ainda A; demais veículos não certificados por este teste. |
| Seleção da lista de modelos da pista Vls0 | E | 12/10: 00458008 +30..CC e 004580F0 +00..F8; cabeçalho +08/+0C/+10 separa vista principal, traseira e 2P. course_vision_oracle compara índice e bytes da lista em 10.017 posições de seis pistas, três passes; negativo altera índice e falha. 00396F00 +34..78 submete somente os IDs selecionados na ordem gravada. Antes todos os modelos MDLS eram desenhados juntos; Tsukuba tinha 3.691 pares opacos coincidentes com texturas distintas. GPU: 32 frames de alternativas, ordem inversa e lista vazia, RGB tolerância1, negativo desliga lista e falha (C da integração). Imagem integral A: ainda há sobreposições autoradas nas listas e precisão/LOD de textura GS pendentes; não afirmar ausência de toda cintilação. |
| Status: Start para Transfer game data | P | 12/10: ajuda US StatusRoot mostra Start, mas gtmode.ad L14926..14934 escuta Triangle0xFF8D; D_006192D8 +50..54 dá Start0xFF13. Alias restrito a StatusRoot permite Start preservando Triangle e outras páginas. gtmode_status_start compara textos e número de diálogos com ação autorada, tolerância zero; antes falhava, negativo de tecla incorreta falha. C integração; save GT3 real pendente. Trata de herança de créditos/licenças GT3, não exportação GT4. |
| Home: atualização de Complete e A-spec Points após corrida | C | 12/10: gtmode.ad L701..706 mantém cached_achievement/cached_aspec_score; L712..767 conta vitórias de eventos/missões e L775 soma melhores pontos. Licenças não entram no denominador. A VM nativa cacheada sobrevivia à saída do projeto, retendo o zero inicial; menu_save_refresh agora invalida esses dois campos após restaurar os registros. gtmode_home_progress compara os dois valores com uma VM recém-carregada executando o script original sobre o mesmo perfil, depois de cada uma das três vitórias Sunday Cup; tolerância zero. Antes falhava; negativo omite atualização e falha. Não altera os registros existentes nem inventa pontos para licenças. Cobertura de tela/percentual integral PCSX2 e outros tipos de evento permanece pendente. |
| Fundo sob ConfirmDialog nas regras de evento | A | 12/10: present_dialog ignorava as camadas inferiores desenhadas por draw_pages; recompõe fundo herdado/página sob Popup e página ativa sem foco antes do diálogo. cursor.adc ConfirmDialog.open L123..232 mantém o contexto do chamador. menu_dialog_background positivo confere que a camada MovieFace continua sendo desenhada; negativo omite recomposição e falha. Comparação integral do popup sobre regras com PCSX2 pendente. menu_tests completo acusa expectativa antiga de CarModel motion2 (não certificado nesta tarefa). |
| Mipmaps de textura gravados no disco / Tsukuba | A | (c) 12/10: Tex1 +08 MXL e +10/+18 MIPTBP1/2 preservados; upload dos níveis originais e REGION_CLAMP por nível. tex1_mips_test confere bytes de três níveis distintos, truncamentos e MXL inválido; negativo altera texel e falha (C do adaptador, tolerância zero). Disco Tsukuba: 32 texturas/96 níveis carregados. render3d_mips_test confere RGB em oito movimentos, tolerância 1; negativo desliga MXL e falha (C da integração). film_develop passa em Tsukuba/Brooklyn, captura inspecionada. Seleção de nível usa derivadas do host: GS Q/L/K ainda pendente; cintilação integral, LOD geométrico e profundidade não certificados. Sem ajuste de bias ou geometria no olho. |
| Arcade: vídeo ao retornar dos courses | C | (c) 12/10: menu_movie_layer_draw escolhe o MovieFace ainda presente na pilha e chama draw_movie a cada frame. Antes, o nome solicitado bastava para parar a promoção pending/completed, deixando o decoder dos courses atrás de GameRoot. menu_navigation verifica decoder atrasado sintético, seis frames, página com vídeo próprio, face invisível e pilha sem camada; negativo interrompe o pump e falha. Duas idas/voltas nativas capturadas em build/ui-return/from-courses.png. Integração C; timing/GS não certificados: dissolução PC existente de 180 ms continua A. Camada herdada continua sem áudio. |
| Menus: Cross/Circle aceitam, Triangle/Square recusam | C | (c) 12/10: padrão solicitado pelo usuário, preservando keysyms de D_006192E4 (00252458..002525B8) e precedência de onKeyPress. menu_navigation confere duas aceitações/duas recusas e consumo dos três keysyms; menu_buttons_negative altera contagem e falha. controller_input usa pad SDL virtual real, press/release e Start/Select, com negativos. Scripts arcade.ad: circle até CourseRoot, square até CategoryRoot, triangle até GameRoot; captura build/ui-return/buttons.png. C restrito aos menus Adhoc e diálogos PC; comandos próprios de corrida/replay não modificados. |
| Menus: projeção e recorte em 16:9 | C | (c) 12/10: conferida a projeção de menu já implementada com base em 00215FE0 (X = largura/640, Y = altura/480). ui_blend_probe testa capturas 4:3/16:9 e widget/scissor em 960x540, separado da projeção de HUD; recortes RGB com tolerância zero, blend até 1 byte. Dois negativos desligam projeção e falham. Capturas GameRoot/CourseRoot/MakerRoot inspecionadas em build/ui-return. Recortes authored e rolagem permanecem; GS e comparação integral PCSX2 pendentes. |
| Playlist: captura de menus em cache | C | (c) 12/10: playlist_cache compara os dois blobs 0x810 (bgm/slide) byte a byte após escrita mais nova e duas capturas de uma VM antiga; tolerância zero. Negativo altera um byte esperado e falha. Versionamento de armazenamento PC P; não prova imagem nem mixagem. |
| Tela inicial: seleção e sequência de demonstrações | E | (c) 10/10: `00434770 +18..90/+144..168`, `004345D8 +44..6C`, catálogo US de `config-main.adc::setNextDemo`: 13 fitas Rdemo. `outside_menu` executa instruções originais: 200 escolhas com intervalos 0..4, índices, contador, movies e wrap exatos; negativo altera contador e falha. Fronteira AH limitada à escolha do catálogo autoral; não certifica Scene/render. |
| Tela inicial: corrida automática e retorno | A | (c) 10/10: app carrega fitas do disco pelo runner existente, cancela por botão e volta após até quatro apresentações (`0010BE20 +230..238`), com orçamento de 90 s (`00434590 +10`). `outside_menu` confere quatro términos/cancelamento; execução real carregou Rdemo0 e dois carros no Test Course. Não concede prêmio. Câmera/diretor, sincronismo integral e pixels continuam A; espera inicial de 30 s herdada do port não comprovada. Física não editada. |
| Álbum: estados e embaralhamento do slideshow | E | (c) 10/10: `001C5F80`, saída `001C60B0`, shuffle `001C6BE8` (troca com índice aleatório na lista inteira), RNG `00579330`/`00578598`. `outside_menu`: 17.600 estados de 0x54 bytes iguais e 400 capacidades/ordens/estado RNG exatos; negativos de fase e ordem falham. Opções 1/8/4 de `004363A0`; clamp original em `001AAFD0`. Não certifica memória de imagens/renderer. |
| Álbum: enquadramento e alpha do slideshow | C | (c) 10/10: `001C6160`/`001C60D8`, 17.600 retângulos contra instruções originais, tolerância 0,001 pixel; alpha exato. Negativo aumenta largura 1 pixel e falha. Geometria/alpha, não pixels, filtragem, cores ou composição GS. |
| Álbum: seleção, playlist, repetição e música | C | (c) 10/10: scripts `labomode.adc::PhotoRoot/SlideRoot` (L1687..1737, L2274..2311) e `slide/musiccontrol.h`. `outside_menu` seleciona pelo pad, salva/reabre playlist, toca dois arquivos, repete por 2.000 ticks, verifica EOF da música e saídas Return/Cancel; negativo suprime transições e falha. `album_pcm_bridge` confere PCM/racing track 8 até EOF/reinício contra decoder existente, amostras com volume 120/128, negativo falha. App PhotoRoot → Slide Show → List → Play verificado com duas fotos e música do disco. Tolerância zero no fluxo; imagem/som completos continuam A. |
| Álbum: PNG e armazenamento no PC | P | (c) 10/10: `<save>/photos/*.png`, playlist `slideshow.list`; até 400 fotos (`001C6BE8`), nomes em registros de 64 bytes. PNG RGB/RGBA substitui arquivo do cartão; metadata de crop/foco PS2 indisponível, imagem inteira/foco central. Seed inicial do RNG pelo relógio PC. Teste rejeita CRC inválido e caminho externo. Miniaturas cacheadas e dois layers residentes. |
| Controle: configuração DUALSHOCK 2 (1P/2P) | C | (c) 10/10: option.adc KeyconfigRoot::DS2 L17..88, métodos 00191078/00191988 (search), 00190D38/00191648 (set), 00190B50/00191460 (unset), nomes 004382B0/004382F8. controller_menu executa o script original: defaults/reflect, conflito, Cancel, troca Cross/Square, isolamento até Apply, persistência/reabertura e precedência do perfil carregado; tolerância zero. controller_input usa controle SDL virtual na ponte real: direção e acelerador de botão simultâneos, ré, nitro, marchas, gatilho remapeado, troca de câmera/ghost por borda, navegação e desconexão/reconexão. Negativos alteram escolha esperada e removem acelerador; 4/4 CTest passam. Escopo C: escolhas e fluxo autorais; não compara bytes internos do pad EE, rasterização nem controle físico. Física não editada. |
| Controle: gatilhos, analógico nos menus e lista suspensa | A | 11/10. Gatilhos L2/R2 do PC: o bit do botão (marchas, visões, menus) só a partir de metade do curso, como o aperto digital do DUALSHOCK 2; a pressão (eixo) só para as funções que o jogo lê como pressão (acelerador, freio, ré, freio de mão: flags de 00349F48). Antes, 5% do gatilho já trocava marcha. Analógico nos menus: passo a partir de metade do curso, solto abaixo de 3/10 (antes 8% sem histerese: cada tremida perto do centro virava outro passo). controller_input: toque leve no gatilho sem troca, metade troca, pedal proporcional; tremidas e empurrão parcial sem passos; negativo exige troca no toque leve. Lista suspensa (KeyconfigRoot): clearWindow (00205330, vtable +0x310) mantém as faces da janela, então o "bg" do vbox (191,191,191,210) volta atrás das linhas, translúcido como no print do PCSX2 (os valores de trás aparecem de leve); appendChild não empilha face sem retângulo; a lista abre inteira (14 opções), não mais 8. A: o critério de clearWindow pelo tipo (Face) e o limiar de metade do curso, não lidos do jogo. V pendente: lado a lado com o PCSX2 |
| Menus: teclas do controle entregues aos scripts | A | 11/10, do código: o motor (00252404) converte cada bit do controle (func_00214AC0: direções 1/2/4/8, L1 0x100, L2 0x200, L3 0x400, R1 0x800, R2 0x1000, R3 0x2000, Triângulo 0x4000, Quadrado 0x8000, X 0x10000, O 0x20000, Select 0x40000, Start 0x80000) na keysym de D_006192E4 (Return, Escape, KP_Enter, Insert, F11/F12/F21/F22, Delete, Pause; diagonais FEE4..FEE7 de D_006192A0) e chama onKeyPress/onKeyRelease do widget focado subindo até a página e o projeto; só sem resposta verdadeira vira navegação/ativação. Antes: só 7 entradas e onKeyPress de setas apenas no labomode; L1/L2/R1/R2/Quadrado/Select nunca chegavam (photo_shoot, gtmode, opções, listas, slide, memcard, eyetoy) e nenhum onKeyRelease. Agora: menu_send_key (pressionar/soltar, event.port 0), setas/X/O/Triângulo passam primeiro pelos scripts, soltar envia onKeyRelease (Photo Travel agarra a ferramenta com X segurado e larga ao soltar), getPadButtonState com todos os bits. Photo Travel: analógico esquerdo move (x, z) e o direito gira, como 003FAFE8 (botões +-1 mais 2x o analógico). controller_menu: Select chega ao MemcardProject (deleteKeyPress liga e desliga). A: Select/Start pelos keysyms (Delete/Pause), bits 0x10..0x80 e diagonais do analógico não entregues, analógico do Photo Travel sem os bits de botão de 003FAFE8; teclado: Q/E = L1/R1, Insert = Quadrado, Delete = Select. V pendente |
| Menus: barras SliderBar e ScaleBar (abrir com X, setas só abertas) | A | 16/10, do código: tratador +0x244 do mSliderBar (002DF848): com o atributo popup do layout (todas as barras do disco menos PosHSlider/PosVSlider do option2/3, conferido nos 33 .mproject) X abre (002DF738, som cursor, valor guardado), as setas da direção mudam o valor só aberta (002DF640, som param, onValueChanged), qualquer outra tecla fica com a barra, X fecha (002DF7B0) e Círculo volta o valor guardado; fechada, as setas movem o foco (base 00263D28). Sem popup as setas mudam direto (002DF540). mScaleBar (00238D10): mesmo ciclo, Esquerda subtrai e Direita soma step dentro de min/max (00239098/00239048), Esc volta o valor; popup legível pelo script. MKeyPressEvent empurrado para uma barra chega ao tratador nativo dela pela fila de eventos: o FocalLen do Photo Travel (R1+R2 segurados, photo_shoot.ad L316..322 e L399..409) agora abre e fecha. OptionMenu (002C95E8) já não trocava valor fechado. Teste menu_navigation cobre barra direta e com popup (setas fechada, Cima ignorado aberta, cancelar). A: repetição de tecla segurada e posição do cursor dentro do popup não lidas; sem comparação com o PCSX2. |
| Menus: Start/Select distintos e seletores fechados | C | (c) 12/10: Start deixa de virar Return/X no adaptador SDL; press/release usam Pause FF13, Select usa Delete FFFF (pares de D_006192D8 +44..54). Teclado Pause/Delete entrega os mesmos eventos. OptionMenu fechado nao altera valor pelas setas; abre com X, altera pendente e confirma com X, cancela com O. controller_input/controller_menu/menu_navigation verificam eventos, draft, indisponiveis e cancelamento; negativos START_KEY/CLOSED_OPTION falham. Sliders inteiros de Setting conservam ajuste direto; controles numericos com popup exigem abertura. Tolerancia zero nestas rotas; nao certifica todos os modos, L3/R3 ou repeticao/diagonais analogicas. |
| Photo Travel: ativacao da ferramenta por X | C | (c) 12/10: removido atalho que escrevia operate_mode pelo nome do widget focado. photo_shoot.ad L258..346/L350..442 agora e o unico dono de grab/release. photo_travel_flow confere foco Pan sem ativar, X inicia pan, release volta a none, FocalLen altera lente e export/retorno funciona; PHOTO_HOLD negativo falha. Setas/D-pad entregues aos eixos X/Z; right X subtraido como 003FAFE8 +1D4..1E4. C apenas estado dos scripts/focal; transformacao, sinais absolutos do controle e camera integral continuam A, sem oracle pad/matriz PS2. |
| Switch: imagem de pista antes do PSS | C | 16/10. Backend IPU nativo: 56 imagens do disco, lote/single identicos e 112 trocas prontas sem PSS; negativo remove poster e falha. 12 testes da area passam. Log do console confirma preload das 56; fluidez visual e igualdade com IPU PS2 pendentes. docs/SWITCH.md. |
| Switch: decoder nativo PSS reintegrado | C | 16/10. Backend e selecao de fontes restaurados; 8 switch_movie_* passam contra RGBA de FFmpeg CLI (tolerancia 1 byte/canal), audio, loops e negativos. Nao prova igualdade com IPU PS2; frontend/console ainda em validacao. docs/SWITCH.md. |
| Vídeos do menu: faixa de cor do IPU | A | 11/10. O IPU do PS2 converte YCbCr em RGB sem expandir a faixa: o Y aparece como está (16..235), o que dá os pretos acinzentados e brancos suaves do PCSX2. Print do usuário (Arcade, Mode Selection): percentis 5/95/99 do fundo 32/229/238; o nosso com a expansão padrão do FFmpeg 8/253/253, o que deixava o vídeo contrastado e as placas Hall A..D aparentando mais opacas. Agora scale in_range=full:out_range=full: 21/234/235. Testes de abertura e vídeo das licenças passam. A: matriz BT.601 do FFmpeg e a conversão do croma não comparadas com o IPU. V pendente |
| HUD da corrida: imagem do carro ao lado da diferença | A | 11/10. A caixa da diferença (RaceCarIconDisplay, 003A6678) desenha a imagem 96x64 do CarIconMaker (00393B70: 00393D20 modo 0, em perspectiva), não a vista lateral 64x32 dos resultados; caixa 64x32 com x alinhado à esquerda e y ao centro (003AECA0 0, 2 em 003A0ECC; 003AFD78). A imagem é espremida na caixa como desenhada em 96x64. Ângulo de 22 graus do nariz pela esquerda (180 - 158 de 00393D20), 5 para cima: conferido contra o print do usuário no PCSX2 (Laguna Seca, picape quase de frente). A: o giro calculado de 00393CF0 e a distância exata não lidos. V pendente |
| Logger: tela de análise (Time Trial e licenças) | A | 11/10. O ícone Logger do menu rápido abre o logger.mproject sobre a última corrida. MLoggerControl (0011EA90/0011F1E0): getCarName (00120238), getPower "%d%s"/"----" (00120330), setTotalTime/SpeedCurrent/CourseMap (0011EDB8..0011F180), display_* e a análise (00122FA8: as duas entradas cortadas ao tamanho da menor, velocidade máxima, zooms). LoggerFace: desenho de 00120CF0 (grade, eixos de distância "%.1tm"/"%.1tml" e de velocidade, faixas de acelerador e freio, linhas de velocidade das trilhas A e B, cursor, rótulos e marcador), bastões de 001206A0 (cursor que para nas pontas e dá a volta, rolagem, zooms log10 limitados a 0,4..10 e 0,5..4) e teclas de 001BC2C0 (Círculo/Quadrado onLoggerMenu, Xis/Triângulo onLoggerPopup); a cada quadro o tempo de cada entrada e a velocidade no cursor "%3d%s" (001206A0 +0x368). Rótulo do modo pela tabela RaceModeName (ARCADE_TIME_TRIAL, GTMODE_LICENSE_<classe> + número). Teste logger_view (dados sintéticos: corte, enquadramento, volta do cursor, velocidade) e --negative falha; TT em Tsukuba com piloto automático até o Logger capturado. 12/10: analyze_start refaz a corrida pelo replay como 00122FA8 (Time Trial e licença), 12 ms de passos por quadro no lugar da thread, a barra do ProgressDialog seguindo a posição na fita (00122CC0), Cancel (analyze_stop) para sem analisar e devolve a corrida; o fim dá onLoggerAnalyzeFinish. TT em Tsukuba: os mesmos 616 pontos da gravação ao vivo. A: o ritmo da thread do PS2 (quanto tempo a barra leva) não medido; sem corrida para refazer, as amostras gravadas ao vivo; tempo total pelo relógio da linha (00122D38 soma as voltas); condições do CanLogger não lidas; teclado: setas = bastão esquerdo, Page Up/Down e Home/End = direito (P). Mapa do curso (RaceCourseMapFace, 00128B68): o GTCM da pista em course_color, a pista entre as bordas na janela do gráfico em sector_color (001292E0: volta inteira se maior, dois pedaços na linha) e o ponto do cursor (00129010; A: marca simples no lugar do ícone +0xCC). 12/10: LoadTrackA/B abre o seletor de melhor volta e reconstrói a trilha escolhida; prova de integração separada abaixo. 12/10: composição de 00121188..001212D8 feita: o gráfico desenhado é capturado e colado de novo uma linha abaixo a FIX 1/3 e uma acima a FIX 1/4 (o alvo do original fica sobre o fundo preto da página; a limpeza dele não foi achada), conferido numa captura de TT em Tsukuba. Relógio da volta/worker continuam A. V pendente |
| Controle: olhar para trás | A | (c) 10/10: botão configurado chega à câmera; transformação a partir de 0037B3FC..0037B418 (para-choque/traseira) e 0037B718..0037B738 (teto). Falta comparar a matriz completa e a imagem com o original; não houve ajuste no olho. |
| Setting: valores A/B/C e limites por família | E | (c) 08/10: gtmode_setting_oracle compara getters/setters 0043E2F0..0043F248 e nove arrays de 001490B0..0014D638 com o executável original; 180 corpos sorteados nas três folhas, truncamento u8/u16, ballast signed8, ASM pareado, wing somado aos dois endpoints. VM/alocação e rows/cache são fronteiras explícitas. Negativos independentes alteram corpo e limite e falham. |
| Setting: linhas de instalação e padding real | E | (c) 08/10: 00440268/004404B8/00440820/004407B0/004403C0/004402F0/00440430/004406C8/004408D8, 00443ED0. BRAKECONTROLLER do disco tem 11 bytes; reserva do wrapper não define largura da linha. setting_defaults_oracle compara 9216 corpos/buffers completos, três folhas, nove categorias, linhas completas/curtas/ausentes; negativo altera padding e falha. Lookup dos bytes/quantidade SpecDB é fronteira explícita. Não prova setGearAuto completo. |
| Setting: peças e campos na entrada da corrida | C | (c) 08/10: gtmode_setting_race percorre os 722 CARS reais: 150054 instalações, 99636 campos A/B/C, roundtrip serializado e 2166 cópias completas de setup 0x178 em entry+0x20 e desgaste 0x20 em entry+0x3468, preservando corpo do chamador. 00441248/004452F8, 0034D490. Negativo altera campo recebido e falha. Tolerância zero; ownership de upgrades concedido pela fixture. Não certifica trajetórias, todas as combinações simultâneas ou UI inteira. |
| Home: dia da semana | E | (c) 08/10: 00136270 +34..4c, addiu -5 com wrap32 seguido de div/mfhi signed. gtmode_weekday_oracle: 65536 resultados contra instruções originais, low32 de inteiros largos, extremos, resto negativo, extras ignorados e argumento ausente nil; negativo altera retorno e falha. Boxing/refcount são fronteiras VM. Restante do Home/calendário não recebe esta nota. |
| Licenças: auditoria completa das fitas como jogador | A | (c) 08/10: gtmode_license_teacher abre e dirige 80 provas com comandos do disco e opções 00110230. 23 ouros, quatro pratas, dois bronzes, 45 reprovações e seis sem conclusão no timeout; somente B-1/B-7/B-13/IB-6/IB-7 têm ouro e tempo GT4M exato. 15 headers têm race_time zero. 12 fitas RaceTraining agora montadas como TrainingWorld/conductor próprios, carros/quantidade do RACE e guia visível. Falha no primeiro frame eliminada; B-5 reprova fora da pista após 4168 frames (1316,335 m aluno / 1344,462 m guia). Slot 2 B-5 recebido e montagem conferida; posição do guia ainda difere 6,866 mm no tick 8. Dinâmica preservada. Não certifica trajetória, todas as provas, demos completas nem causa física. CTest B-1 e negativo de tempo passam; auditoria completa retorna falha, deliberadamente não mascarada. docs/GT_MODE_COMPLETION.md. |
| Licenças: encerramento da Demonstration Training | A | (c) 08/10: usuário relata B-5 iniciando e retornando ao pré-race logo após largada no PCSX2. Causa/duração ainda sem captura. Slot 2 tem CC8=0/DC=0 (tentativa); 00115290 +20..28 entra em DC=1/modo 2 e +68 limpa DC ao voltar. Importador 003D5F00 usa world+124A0; query de recorder 003D5EC0 +8..34 usa world+123CC. 6490 comandos da fita não certificam duração do Scene. Auditoria DC=0 não exige que a demo termine volta/ganhe ouro e não demonstra erro na física. Aguardar captura durante contagem; não inventar tempo nem exigir fim de volta. docs/GT_MODE_COMPLETION.md. |
| Licenças: classe, quantidade, grid e métodos de Training | E | (c) 08/10: 0010FBD8 +20..84, 003D5AB8 +28..2c, 003BB478 +20..28, 003ECC70, vtable 00685F50. gtmode_training_oracle compara as 80 escolhas/clamps reais, grid das 12 Training e 28672 resultados de getters/controle de largada com instruções originais. Negativos de classe/getter/controle falham. Fronteiras explícitas de menu, construtores-base, lookup RACE e queries do mundo/replay. Não certifica montagem integral de garagem, trajetória, carros na imagem ou todas as demos. |
| Licenças: montagem inicial B-5, setup e regras | C | (c) 08/10: slot 2 fornecido pelo usuário; gtmode_training_capture confere dois setups 0x178 fora de ponteiro/padding não inicializados por 00444480/00444690/00445220, grid, flags, gearbox/ASM/TCS e 128 bytes de regras. Corrigidos padrões do guia em entry +3450/+3454/+3458 (003B4B38 +11c..124). Tolerância zero nesses campos, negativo altera chave e falha. Posições só diagnóstico: aluno até 0,259 mm, guia 6,866 mm no tick 8; --trajectory com limite 1 mm falha. Não prova trajetória completa ou renderer. |
| Licenças: passo Training a partir da mesma captura | E | (c) 08/10: gtmode_training_step compara 0034DB30 original com o passo nativo no slot 2 B-5: 64 passos, dois carros, 553280 bytes idênticos incluindo o estado inicial; nenhuma máscara. Comandos variados com seed de teste, fronteira de pad original explícita nos dois caminhos. Negativo corrompe posição no primeiro passo e falha. Não certifica montagem inicial, fita inteira ou outras provas. O oráculo original ainda para em VU0 de áudio no frame 93 em um teste mais longo. |
| Setting: intervalo entre relações de marcha | E | (c) 00355278: 3584 conjuntos signed16, uma a sete marchas, lo/hi e células não escritas idênticos; negativo altera hi[1] e falha. Junto às leituras/escritas/ranges acima: 90824 comparações. Montador do record, getGearRange, setGearAuto permanecem A fora das amostras específicas provadas abaixo. |
| Setting: curvas do motor, normalização e gráfico de marchas | C | (c) 08/10: gtmode_setting_curve, 722 carros A/B/C, 12996 builders 0034B960/0034B8E8 e 12996 callbacks 00148B60 (stock e duas configurações de upgrades simultâneos) executados do original. RPM/contagens exatos; float abs 2e-5 + rel 2e-6, pior 1.19209289e-7. Negativos pico/label falham. Record/SpecDB são fronteira compartilhada; motor/drive/normalização independentes. Corrigidos pico elétrico, nove marchas e RPM anterior ao conversor. Não certifica todas as combinações de upgrades nem pixels. |
| Setting: adaptador de desenho do câmbio Standard | C | (c) 08/10: slot 2 Honda BEAT, GraphFace C7E710: type B8=1, width BC=2, count C0=10. gtmode_setting_reference compara 10 pontos (2e-5), 5 strokes/centros/espessura (1e-4), vértices convexos da API, 8 campos `--` e zero widgets omitidos; compara corpo capturado A20550. Negativo desloca vértice; outro torna visível labeli. Layout do disco compartilhado, não oráculo da cobertura/antialias GS ou de todos os estados. |
| Setting: câmbio automático e limite de estágios | E | (c) 08/10: gtmode_setting_auto executa 00149538/0014A2D0/00354FF8/003553A0: 20394 arrays completos de 23 elementos e 6417 corpos de 0x4b8 idênticos, 722 carros, todas as transmissões disponíveis, A/B/C, Auto min/max/meio e override -1/0/1. SpecDB, record inicial e VM são fronteiras. Auto de caixa fixa, desabilitado pelo script, não é forçado. 32 máximos comparados com 0044A1E0 (type-1). NIKE ONE passa com oito marchas; relação 8 não sobrescreve Final. Negativos RANGE/BODY/STAGE falham. |
| Setting: relações até nove marchas | E | (c) 08/10: 4608 conjuntos signed de 00355278, 1..9 marchas, buffers completos de 16 shorts/sentinelas. Com getters/ranges são 180936 comparações; negativo RATIO altera hi[1] e falha. |
| Setting: conversões e labels de potência/torque | E | (c) 12/10: gtmode_setting_units executa 001AF760/00472A08/00472758 e 001AF598/004729A0/004726B8: 655360 entradas signed, sete potências e KGFM/FTLB/NM, inteiros e dez labels idênticos. HP tinha fator um ULP acima: agora preserva bits 3F7C76F2, também no catálogo/Logger/abertura do replay; kW usa 3F3C49BA. Unboxing/boxing da VM são fronteiras. Dois negativos alteram inteiro de torque/potência e falham. Conversão somente de apresentação, nenhuma fórmula/parâmetro de física alterado. |
| Setting: movimento dos painéis | E | (c) 08/10: gtmode_setting_motion, dois atores do script (SettingRoot/EngineCurbePopup), 3240 updates de eixo contra 002C1BB0, entrada/saída e entregas 60/120/240 Hz, float byte a byte. Destinos do script L282/L3695 preservados. Negativo desloca x no quadro 17. Não prova carga assíncrona, cursor/transições completos ou pixels GS. |
| Setting: lista de desenho do gráfico de marchas | C | (c) 08/10: reference com RECOMP_TESTS executa 002E88F8 e 0025B578/0025BAA0 sobre slot 2: seis retângulos/parentes, dez endpoints, dispatch de pares, largura/RGBA. Geometria abs 1e-4, cor 1e-6. Métodos de contexto +198/+260/+268 são gravadores. Negativo altera geometria e falha. Cobertura/antialias GS fora da fronteira. |
| Setting: tela, presets persistidos e retorno | A | (c) 08/10: scripts originais, A/B/C, apply/cancel/save; gtmode_setting_catalog_menu passa 45414 painéis (incluindo EngineCurbePopup) e 36750 controles nos extremos em 722 carros stock/upgrades, negativo acusa alteração antes de apply (C só de integração). Slots 1/2 Honda comparados no app; corrigidos ScaleBar pulado, popup aberto no binding, GraphFace default/pares/ordem dos vértices e estilo closed value. Imagem GS integral/antialias/ordem assíncrona continuam A. EngineCurbePopup abre após implementar GetTorqueForDisplay/GetUnitOfTorque. Power/Torque ainda ignoram rotate=90 do projeto. Usuário adiou acabamento visual e pediu seguir outra área (08/10). Sem valores ajustados no olho. docs/SETTING.md. |
| Arcade: elegibilidade do carro em terra e neve | E | (c) 06/10: restaurada a ponte `MCarData.IsDirtRunnable` ausente. `0012E498` em `0012E4F0` verifica o campo +0x38 da estrutura decodificada; `00448B48` move o campo +0x30 da linha ARCADEINFO (versão de terra) para lá. `arcade_dirt_oracle CORE ISO` executa ambas as funções originais, fronteiras explícitas de valores Adhoc/labels/SpecDB: 3280 rótulos, 556 aceitos e 2724 rejeitados, zero diferenças no helper e no método registrado. 90 rotas do script original (CarScroll.SelectBox.onActivate L4381..4382) em 11 pistas de terra e 4 de neve, SingleRace/TimeTrial, com Ka, RS200 Rally e M3 rejeitado. `--negative` inverte resposta e falha; `GT4_DIRT_BINDING_NEGATIVE=1` remove a ponte e reproduz o bloqueio. Release e menu_navigation passam. Sem valores estimados. Escopo: elegibilidade; não certifica o grid, pneus, física ou imagem. A lista original EnemyData.dirt_enemy_carlist tem 115 candidatos e inclui Fusca e Ka; não só carros de rally. |
| GT Mode: ficha de compra, campos ausentes | A | (c) 06/10: BuyCarDialog L6388..6409 agora recebe getCatEngineType/Cam/Displacement, dimensões, getCatMs e getCurrentPs/Tq/Ms em carros construídos/importados. ENGINE strings +4/+8/+0 (00446138/004460F8/00445FF8); CHASSIS +8/+10/+12 e RACINGMODIFY +8 (00446038/68/98/C8). Estatísticas atuais usam folha equipada e desgaste +0x498 do corpo, cálculo nativo de 00146648/00364868/0034F3F8. `gtmode_purchase_menu ISO --purchase-specs` verifica 456 campos de 38 usados e textos da própria BuyCarDialog do CR-X: L4:DOHC, 1595cc, 3800x1675x1270, 986kg, potência/torque preenchidos. `GT4_PURCHASE_SPECS_NEGATIVE=1` injeta nil:nil e falha. Build Release e 23/23 CTest passam. Falta oráculo diferencial dos valores atuais, arredondamento/conversão/formatação e catálogo com peças modificadas; não é prova E. |
| Tuning: instalação de rigidez, tipo 31 | E | (c) 05/10: `00440B50 +0x308` → `004468F8`, ramo `00446AC8`: flag +0x168 em A/B/C. `gtmode_championship_oracle CORE` compara corpo 0x4b8 inteiro em bateria de 8192 instalações incluindo tipo 31. Negativo `GT4_RIGIDITY_NEGATIVE` falha. |
| GT Mode: black cars visíveis, aquisição e save | A | (c) 05/10: `gtmode_purchase_menu ISO --special-purchase` abre as três listas reais na semana 99, encontra R390/GT-One/787B/R92CP, usa os objetos produzidos por UsedCarRoot L5617..5635 com pintura/quilometragem do script e executa GetCarRoot/Auto Save. Créditos, quatro corpos completos 0x4b8 e bits de vendido preservados após reload; `GT4_SPECIAL_PURCHASE_NEGATIVE` altera pintura e falha. Fronteira explícita: addUsedCar/calendário/set vendido chamados pelo harness; ainda não prova clique completo em BuyCarDialog, preço contra EE nem apresentação PCSX2. |
| GT Mode: limpeza e recarga do estoque usado | A | (c) 05/10: Module.clearStatic de `00304820 +0x10 -> 003063F8` limpa membros próprios; método nativo load agora fica no host e sobrevive à limpeza de UsedCarRoot L5701. Mesmo teste abre JP/US/EU sequencialmente sem erros de clearStatic/load; --shop-catalog verifica remoção de membro, preservação do módulo pai e segunda limpeza; `GT4_CLEAR_STATIC_NEGATIVE` falha. Falta comparação diferencial da VM original, incluindo registro interno de módulos. |
| GT Auto: textos e luz padrão | A | (c) 06/10: removida compressão automática inventada no desenho de TextFace; `00245408 +0x80/+0x100` submete estilo original, GTAutoRoot autoriza scale_x 0,95 mesmo nos retângulos de 36/24 pixels. --customization-purchase grava glyphs dos quatro rótulos e compara larguras em retângulo original/ampliado; `GT4_LABEL_SCALE_NEGATIVE` falha. LightSet `00210830 +0x9c..0x12c`: mundo RGB zero, direção (0,1,0,0), ambiente 0x3e999999, difusa/especular 0,5 substituem defaults escuros inventados. Teste confere bits normalizados e negativo `GT4_LIGHT_DEFAULT_NEGATIVE` falha. Release e 23/23 CTest passam. Captura local `build/gtauto_text_light_corrected.png`; falta lista de desenho/oráculo e comparação do passe de iluminação com PCSX2. Não prova paridade de todo o renderer. |
| GT Auto: valores de sujeira, óleo e setter de limpeza | E | (c) 08/10: gtmode_service_values, 12288 comparações bit a bit com 00343A80/003437F0/003438A0; curva inicializada por 003660D8, parâmetros extraídos do CORE. Getter de sujeira usa wear+10, setter visual wear+C, não campos intercambiáveis. Óleo compara retorno, wear e globals de inicialização; sujeira compara retorno, setter compara bloco 32 bytes. Limites/exemption/zero maximum/mileage negativo/float finito; três negativos independentes alteram bits de dirt/oil/setter. Boxing VM, geometria física, cena e mixer fora da prova. |
| GT Auto: animações e sons dos serviços | A | (c) 09/10: slots06/07 BEAT MR comparados novamente. Corrigidos composição rígida do carrinho (linhas GL contra colunas Mot1), luzes Light0/1/2 dos canais 0C..0F, sombra de contato 004524B8 e projeção vertical 002173AC -> 00107328. Corpo e rodas/freios usam limites da suspensão, não raios. Reflexo ALPHA 0x68 e regular 0x64, FIX original; duas amostras regulares e uma refletida, 00217058/00216E78. Contexto adicional separado durante entrada, 00217430..48; targets/meshes/texturas residentes, sem readback por frame. gtmode_gtauto_menu: quatro serviços, cobrança, Back, fim e Save/reabertura; negativo falha. A: rasterização/cobertura GS, recorte final da página, pele/iluminação integral e sincronismo áudio/UI completo ainda não certificados. Backend software sem esses passes. Quatro sons no backend SPU existente; relógio escalar E separado abaixo. Forma de onda/fase IOP/UI e reverb compartilhado não comparados; cauda offline de 1 s continua A. Nenhum valor visual ajustado no olho. Provas/capturas locais em build/gtauto-finish. |
| GT Auto: amostragem de modelos e câmeras Mot1 dos serviços | C | (c) 09/10: gtmode_service_motion executa 0042B440 nas dez fitas de modelos e três câmeras: 3133 streams, matrizes, look/FOV, fade/lift/shadowOpacity e canais Light0/1/2, tolerância 0,00002; máximo 9,53674316e-7. Extrapolação dos segmentos extremos e Horner 004861A8 preservados. Negativos de matriz e canais falham. Sin/cos e stack VU são fronteiras; não prova composição final com modelo/skin, pixels ou sincronismo integral. |
| GT Auto: parâmetros de alinhamento e passes dos adicionais | E | (c) 09/10: gtmode_service_placement, 4096 casos bit a bit com 00156AA0 +14C..3D4: alinhamentos 0..12, yaw, escala, hidden/reflexão. Negativo inverte draw e falha. Consultas da geometria, matriz VU/GS, skin e pixels são fronteiras; E só dos argumentos dos transforms e decisão de desenho. |
| GT Auto: relógio escalar de Mot1 dos serviços | E | (c) 09/10: 184320 estados contra 0021B4F8/0021B528 e 46080 avanços/amostras de cópias de controlador, bits idênticos. Normalização da câmera viva no desenho (00156F18); adicionais normalizam cópia temporária (00273AC8), mantendo relógio vivo depois do fim. playModelMotion não zera relógio (00153E20); preview studio mantém conductor próprio. Negativos CONTROLLER/DRAW_CLOCK falham; compra/entrada e negativo de playback passam. Exclui troca enfileirada e sincronismo GS/IOP integral. |
| GT Auto: parâmetros do levantamento no óleo | E | (c) 09/10: 4096 casos contra 00156950 e 16384 consultas de limite contra 00364B30 +68..70. Clamp usa axle[2]-axle[4], não raio do pneu; corpo sobe e hubs/rodas/freios caem até limite de suspensão. Bits idênticos, negativos LIFT/DROP falham. Leitura da geometria por adaptador da carreira, sem alterar solver. Rasterização fora da prova. |
| GT Auto: pose motion=2 e entrada do corpo | E | (c) 09/10: 92160 quadros contra 00153BB8/00151960: posição, pitch/roll, yaw sem shortest-arc e fade incrementado, bits idênticos. Negativo POSE falha. Controladores vazios nessa prova; E só do easing/fade. |
| GT Auto: coeficientes e acumulação dos passes | C | (c) 09/10: FIX de 004A2808 é E em 4096 casos, negativo BLEND. GPU: 132 casos dos passes 0x64/0x68 e acumulação em duas amostras 00217010 (1/(i+1)); tolerância 1/255 simples e 2/255 acumulado, máximo 0,719/255. Negativos de FIX e índice de amostra falham. Não prova cobertura/texturas/pixels completos do GS. |
| GT Auto: correção de vídeo e offsets das amostras | E | (c) 09/10: 16 modos/projeções contra 001060F8/00107328; NTSC modo2 usa 1/0x3F8C8B43, confirmado em 006184D0+14 dos slots6/7. Três offsets quantizados a 1/16 contra 004A2C58/004A2BC8, tabelas CORE00629DE0..F4 e ganho 0x3F333333. Negativos PROJECTION/JITTER falham. Conversão final para clip GL e cobertura continuam A; port aplica caminho NTSC de duas amostras, outros níveis de qualidade não certificados. |
| GT Auto: relógio, despacho e rampas escalares dos sons | E | (c) 09/10: service_sound_clock, 171953 comparações com 0055A858/0055AB98/0055D420/0055D470. Taxas, relógio float, despacho com atrasos zero/coincidentes e rampas inteiras; negativos RATE/RAMP/SCHEDULE falham. Driver US capturado a 60 Hz; default 120 BPM/240 ppqn dá 8 ticks/frame (480/s), primeiro despacho após somar 8. Conversão amostra/frame conferida nesse default. Parser, registradores completos, reverb e PCM/latência fora da prova. |
| GT Auto: movimentos e matrizes do mecânico | A | (c) 05/10: Mot1 lê canais 5/6/7/8/9/19 e pilha 2/3 (`0042C818/0042C848`), captura frames no marcador 4 (`0042A660`); Euler/quaternion/matriz de `0048B888/0048A970`, usando `gt4_sincosf` original como fronteira. --shop-catalog encontra 20 ossos no início/fim das duas entradas e duas lavagens, todas as matrizes finitas, PELVIS com translação local exata e LOWER_SPINE composto com tolerância 0,000001. `GT4_MODEL_FRAME_NEGATIVE` e `GT4_MODEL_CHANNEL_NEGATIVE` falham. Falta oráculo diferencial da composição/curvas e alinhamentos de serviços. |
| GT Auto: mecânico na entrada | A | (c) 09/10: playModelMotion preserva tempo; pose motion=2, fade e câmera/modelos separados. Compatibilidade de car_crew_time acompanha o primeiro controlador sem somar dt duas vezes; modo studio=0 continua atualizado por conductor próprio. gtmode_purchase_menu --customization-purchase passa e GT4_CREW_PLAYBACK_NEGATIVE acusa relógio alterado. Skin × inverse-bind residente, amostra temporária normalizada pelo loop do controlador. Sequência completa/pixels de entrada e backend software ainda não comparados ao PCSX2. |
| Tuning: padrões de instalação do câmbio, tipo 11 | E | (c) 05/10: `004402F0 +0x44..0xb8`, linha GEAR de 45 bytes do disco; 1024 corpos/buffers idênticos ao original, três folhas, limites invertidos e linha ausente. Negativo `GT4_GEAR_DEFAULT_NEGATIVE` falha. A geração automática posterior não faz parte desta prova. |
| Tuning: relações automáticas após instalar câmbio | A | (c) 05/10: `gt4_career_vehicle_auto_gears` porta `00443240`, reutilizando `gt4_car_record_rows` e `gt4_car_auto_ratios` da física; restaura memória/contexto FPU e altera somente as 24 relações da folha selecionada. `gtmode_purchase_menu --shop-catalog`: 3 estágios reais Ka, chave instalada, relações estáveis no segundo cálculo; negativo `GT4_AUTO_GEARS_NEGATIVE` falha. Falta oráculo completo dessa integração, não declarar E por reutilizar física. |
| GT Auto: calendário de compra de rodas/asas | A | (c) 05/10: `putBuyWheelEvent`/`putBuyWingEvent` usam eventos originais 6/7, chave WHEEL/WING e avanço de dia; fonte `0042F3C0 +0x18..0x34`, `0042F408 +0x1c..0x2c`, wrappers `00135138`/`001352B0`. `gtmode_purchase_menu --shop-catalog`: quatro categorias e asa, preservação dos bytes reservados no helper e cinco avanços de dia; negativo `GT4_CUSTOM_EVENT_NEGATIVE=1` falha. Limite: ponte Adhoc zera bytes de histórico de pilha desconhecidos; ainda falta comparação com execução original e ciclo de compra/save pela tela. |
| GT Auto: compras de rodas/asas e persistência | A | (c) 05/10: `gtmode_purchase_menu ISO --customization-purchase` executa os scripts WheelShopRoot L11686..11694 e WingShopRoot L12361..12369, cancela/compra, confere Cr 10000 → 9500 → 8300, instalação, eventos 6/7 e corpo 0x4b8 idêntico depois de save explícito/reabertura em outra VM. Negativo `GT4_PURCHASE_CUSTOM_NEGATIVE` falha. Também conferido Auto Save pela saída original GTAutoRoot.onCancel para TopRoot: dirty zero e reabertura preserva créditos/corpo/eventos, sem store explícito após as compras. Não prova render 3D ou execução EE original. Comandos de modelo/movimento atendem à entrada; serviços com modelos múltiplos atendidos funcionalmente na continuação de 08/10, apresentação A. |
| Mapa GT Mode: aviso de tuning compatível | A | (c) 05/10: mesmo teste, Ka/Ford: script TopRoot L1259..1292 gera mensagem em DealerUSA com filho ex visível; negativo `GT4_TUNING_NOTICE_NEGATIVE` falha. Falta comparação de desenho com original e caso TunerCar/múltiplas oficinas. |
| Menus do Arcade (scripts originais em interpretador nativo) | V | (d) |
| Showroom (carros e cores originais) | V | (d) |
| GT Mode: mapa e páginas | V / A | Corrigido uso de FOV inteiro em tan: 00217350..00217390 / 002179B0..002179F4 usam metade do ângulo. Captura build/map_fixed.png; projeção completa permanece A por fatores 0,8, 0,85 e deslocamento 0,15 ainda calibrados. |
| GT Mode: cards de eventos, imagem dentro de Ticket | C | ImageFace 002A2608 +0x7c..+0xc0 usa client size herdado, não tamanho da textura. gtmode_purchase_menu --ui-regression: três cards Volkswagen, textura 640x206 desenhada com altura 95 do Ticket, tolerância zero. GT4_CARD_SIZE_NEGATIVE altera altura esperada e falha. Captura build/vw_cards_fixed.png. Escopo: retângulo da imagem; não certifica todos os pixels. |
| GT Mode: estoques especiais e preparadores | C | gtmode_purchase_menu --shop-catalog lê 100 ADC semanais (12.192 entradas) e quatro black cars na semana 99; compara getTunerList dos 722 carros com GENERIC_CAR +0x10 e TUNER_LIST (até 10 bytes), enum do CORE 006A7658. 00140FA8 -> 00448788/004478F8/00448DF0. GT4_TUNER_CATALOG_NEGATIVE e GT4_BLACK_STOCK_NEGATIVE falham. Escopo: catálogo; não prova aquisição dos quatro, todos os carros de prêmio ou avisos renderizados. |
| GT Auto: câmera de entrada e da loja de rodas | A | Ponte `001391D0 -> 0013A678` lê Mot1 dos GPBs originais; cache por projeto, tempo no widget, look-at/FOV aplicado no backend GPU. `gtmode_purchase_menu ISO --shop-catalog`: 22 amostras iguais à leitura direta das duas câmeras, cache reutilizado, arquivo ausente rejeitado; `GT4_OPTION_CAMERA_NEGATIVE=1` falha. Leitor Mot1 compartilhado, sem prova independente da matriz/tempo original. Captura `build/gt_auto_camera_wait.png` mostra carro/roda na loja após carga assíncrona. Ainda faltam comparação PCSX2, equivalente software e movimentos do carro/mecânico; não conta como E/C da apresentação. |
| GT Auto: asa selecionada no modelo | A | CarFace.wing solicita `/wing/menu/<label>` (`00454378 +0x20..0x48`); `004533A8 +0x30..0xa8` usa CAR4 CarInfo+0x50, geometry+0xd0 posição e +0xdc*0.5 escala. Callback 47 (`00453698 -> 00453950`) seleciona suportes pela flag de `00453488`. Asa salva lida da folha ativa body+0x100. `MOON_PROBE_TEST_WING=1 showroom_probe`: BEAT/Ka/120i/Focus RS adicionam geometria e p000 restaura campos de desenho; negativo MOON_PROBE_WING_NEGATIVE falha. Capturas locais wing_probe.png e gt_auto_wing2.png. Sem comparação PCSX2 do encaixe/escala, nem compra/save completo: não conta como E/C da apresentação. |
| GT Auto: modelo da roda selecionada | A | `00454270 +0x20..0x48` fornece `/wheel/menu/<label>`; seleção `CarFace.wheel` aciona recarga assíncrona, e `car_garage` lê a roda normal salva em body+0x468. Teste de contrato `MOON_PROBE_TEST_WHEEL=1 showroom_probe ISO CAR out.png`: BEAT, Ka, 120i e Focus RS preservam campos desenhados da carroceria, alteram quatro meshes de roda e restauram campos exatamente com w000; negativo `MOON_PROBE_WHEEL_NEGATIVE=1` falha (exit 1). Ainda sem comparação PCSX2 do modelo customizado/encaixe; câmera GPU já lê o Mot1; movimentos do carro/mecânico e comparação PCSX2 continuam pendentes. Contrato não prova E/C do renderer nem compra/save completo. |
| GT Auto: seleção e estado de rodas/asas | C | 0014D9B0/0014DBA0 -> 00440938/00440988; 0014DF50/0014E0B8 -> 00440A10/00440A68. 2.829 seleções reais WHEEL/WING, quatro categorias, corpo completo 0x4b8 preservado fora de +0x468/categoria e três setup +0x100, tolerância zero; restrições dos 722 carros conferidas com GENERIC_CAR +21/+22. GT4_CUSTOMIZATION_NEGATIVE altera byte e falha. Compra pelas telas, preview/modelo e reload ainda pendentes; não constitui C do GT Auto completo. |
| GT Mode: lojas usadas, carregamento e lista | C | Script original UsedCarRoot carrega usedcar/us_used%04d; ponte agora lê ADC real e converte argumento string de makeUsed (0013E8E8 +0x38..+0x9c). gtmode_purchase_menu --ui-regression confere 24/38/38 carros com arrays do disco nas três lojas, semana zero, sem erro de script; GT4_VISIBLE_MENU_NEGATIVE força lista vazia e falha. Não prova todos os 100 estoques nem estatísticas completas dos carros. |
| Carreira: usados vendidos, bits e mudança de semana | E | 0043B638/0043B690, reset 0043B608: gtmode_championship_oracle compara 8192 estados, retorno, 32 bytes e semana contra original; todos os 256 ids, get/set e preservação/reset. GT4_USED_STOCK_NEGATIVE altera byte e falha. Escopo: primitiva e ponte, não fluxo visual inteiro. |
| GT Mode: disponibilidade de pistas | E / A | E das primitivas 0042EA38/0042EAE8: gtmode_home_oracle compara 4096 listas ordenadas de 128 chaves, inserção vazia/cheia, duplicatas, consultas e override. GT4_HOME_NEGATIVE falha. Ponte A: initAvailable lê US_carlist do boot e CourseData.has_reverse (60 chaves), consulta o save e recebe setCourseAvailable dos presentes por data. --home-status testa pista bloqueada/liberada/override e serialização; --ui-regression exibe 8 World e 6 Original no perfil inicial. Falta carreira contínua completa para provar todos os desbloqueios por data. docs/HOME.md. |
| Home: garagem visível, filtros, colunas, ordenação, ver/trocar/vender carro | A | `gtmode_purchase_menu ISO --home-garage`: botão Garage abre GarageRoot; 3 carros salvos, 30 textos de colunas, 13 ordenações nos dois sentidos, filtros de país/tuner, CarViewRoot, Change com Auto Save/reload, Assessment com venda/créditos e paginação de 13 carros (3 na segunda página). `GT4_HOME_GARAGE_NEGATIVE=1` reduz a contagem conferida e falha. Ponte baseada em 00170088/00170278/001705E0, 001412A8, 00435110, 00146D28..001479B8 e scripts L12582..13302/13777..14008. A: desempates usam inserção estável; faltam oráculo PS2 e comparação visual. Status/calendário receberam uma ponte separada em 09/10, descrita nas linhas abaixo; este teste só prova o fluxo da garagem. |
| Home: B-Spec, contadores, distância e consulta do histórico | E | gtmode_home_oracle CORE [ISO]: 4096 casos contra 00467C58/00467D90/00467F60/004680D8/00468B58, 0043B018, 0042F060 e 0057F480/00472C28/0057F360. Zero diferenças em quatro skills/getters, sete contadores, anel de 2922 dias (índice inicial variável), hoje/futuro/fora da janela e km/milhas com conversão assinada. Pesos lidos de D_00623E40. GT4_HOME_NEGATIVE altera retorno e falha; CTest positivo/negativo passam. Escopo: leitura do perfil; não implementa evolução durante B-Spec. docs/HOME.md. |
| Home: Status, diário e persistência | A | gtmode_purchase_menu ISO --home-status executa HomeRoot/StatusRoot/LogRoot sem erros, quatro máximos B-Spec, distâncias, vitórias/troféus/prêmios, nove schemas de MCalendar.getEvent (00133370), labels reais SpecDB, seed de pistas e profile roundtrip. GT4_HOME_STATUS_NEGATIVE altera contagem e falha. Porcentagens %% agora desenham um % (00494198..004941C8); probe compara glyphs/largura com literal. Save PS2 preserva matrizes +0x94 e override. Capturas locais home-final/status/diary; falta lista de desenho/imagem PS2 e importação ponta a ponta com experiência não zero. Não certifica renderer/iluminação da garagem nem todas as outras ações Home. docs/HOME.md. |
| Licenças: registro de tentativa e argumentos do histórico | E | (c) 06/10: career_license.c, 001105A8 +0x68..0x100 e 00110800 +0x20..0x78: estados concluídos 2/3/4, record persistente, melhor tempo/medalha da visita, argumentos do evento ou avanço de dia. gtmode_license_result_oracle CORE: 16640 casos e sequência bronze/falha/prata empatada/ouro mais lento; zero diferenças contra as duas funções originais. Negativos GT4_LICENSE_RESULT_NEGATIVE e GT4_LICENSE_HISTORY_NEGATIVE falham; 3/3 CTest específicos passam. Fronteiras: estado concluído, lookup do record, conteúdo de ranking/ghost e inserção do calendário. Sem estimativas na primitiva. Integrado pela ponte C abaixo; licenças disponíveis, conjunto completo A; demonstração/ghost/Logger têm provas específicas abaixo. |
| Licenças: condições nativas de aprovação/reprovação | E | (c) 06/10: license_session.c, 003F8818/003F8980/003F8AB0/003F8D70/003F91F0/003FA248/003FA2B0/0035FE80. gtmode_license_session_oracle: 30720 casos contra original, blocos completos de conductor/corrida; negativo altera resultado e falha. Fronteiras explícitas: carro, eventos, gravação. Dispatcher/start/splits/ghost não recebem E por esse teste. |
| Licenças: linhas antigas das pistas de exame | E | (c) 0046BCB0/0046A510/0046B9F0: gtmode_license_lines_oracle compara linhas e AD completo após setup/clear de testcourse_100/200/500/1000m, fisco_slalom e tsukuba; negativo altera flag e falha. Fixed-point, stride antigo e sobreposição de campos preservados; sem ajuste visual. |
| Licenças: modelos e seleção dos objetos de pista | C | (c) 10/10: course_gadget.c, COURSE_DATA +BC/GDGT +10/+1C e RNW4 +44/+70/+54/+9C; 0033BDB0 +114..124, 00395B60, 003E2710 e 003E53B8 +28..3C. course_gadget_oracle percorre 80 exames/36 pistas, decodifica 137 modelos normais e compara 915 matrizes com 003E24F0 bit a bit; sin/cos 0057D8A0 é fronteira host explícita, por isso C. Deep Forest seção 2: 25 objetos, seis suportes/faixas e 19 cones. Negativos MATRIX/SELECTION falham; ponteiro inválido rejeitado. Geometria/texturas residentes e compartilhadas. Física tem prova separada abaixo; modelos de sombra ainda não certificados. 12/10: massa dinâmica inválida também rejeitada antes do solver. |
| Objetos de pista: física dos cones e coffee breaks | C | (c) 12/10: course_objects.c traduz 003E2B28 (integração angular em três etapas e gravidade), 003E3748/003E3840 (apoios, impulso e atrito), 003E3C48/003E3ED0/003E43B8 (cinco planos do carro, separação e impulso), 003E44E8/003E4708 (paredes e limite de energia). Massa, inércia, alcance e até oito apoios vêm dos descritores GDGT de 0xA0 bytes. race_run mantém estado próprio, sem depender do renderer; 0033C538 executa os objetos depois do passo do mundo; race_game publica posições/ângulos/visibilidade. course_objects_original compara 1024 casos com as instruções EE originais, tolerância relativa 3e-5; rotações VU e sin/cos são fronteiras host explícitas. 256 idades de blink (003E20A8) coincidem exatamente; remoção usa 003E2100. Negativos CONTACT/GROUND/MOTION/WALL/BLINK reprovam. coffee_objects executa as cinco provas l0c0001..5 com dados do disco: B/IA falham no contato, A/IB contam 100 cones e S conta 50; verifica impulso/movimento, ausência de dupla contagem, término e reinício. Contato passa por 00350530 -> 0035FE80 -> 003F8D70. Integração C; ainda sem captura de trajetória completa de cone no PCSX2, sombras e áudio próprios dos objetos não certificados. Forças externas +4C/+50 não possuem produtor no port; o teste de replay original 00463BA8 dentro de 00343BF0 permanece fora desta prova. |
| Piloto atrás do vidro: ordem nativa de desenho | A | (c) 12/10: race_view desenha piloto antes da carroceria; o vidro mantém seus writes de profundidade e passa a mesclar sobre o piloto, enquanto a lataria opaca continua o ocultando. driver_windshield verifica no GL vidro com alpha/depth write e carroceria opaca; negativo desenhando vidro primeiro reprova. Capturas nativas locais build/coffee-glass-{front,rear,side}.png: Copen aberto do B coffee, driver2.mdl com 20 slots, piloto visível pelo para-brisa. C para a dependência de profundidade testada; item completo A porque a sequência GS original e a imagem PCSX2 não foram comparadas. |
| Licenças: terreno e luz dos objetos, slots 5/6 | C | (c) 10/10: license_objects_probe ISO prefixo RAM5 RAM6 confere 25 objetos de B-3/Deep Forest em cada captura. X/Z até 2 mm, Y fixo exato e móvel até 0,01 mm (máximo 0,00190735 mm); matriz até 0,00001 (máximo 5,96046447e-8); luz igual aos bytes capturados. 003E29F8, 003E2268/003E2600 e 003E2710 +E8..240; consultas RNW4 de leitura, nenhuma física alterada. GT4_GADGET_RAM_NEGATIVE desloca Y 2 cm e falha. Vistas de inspeção Start/Finish detectam 22431/5717 bytes de cor; GT4_GADGET_DRAW_NEGATIVE omite objetos e falha. Provas locais extracted/handoff/license-objects-20261010. |
| Licenças: imagem dos objetos de pista | A | (c) 10/10: Start/Finish, suportes, cones e demais tipos usam MDLS original. Inspeção visual confirma elementos; composição/rasterização/luz completas continuam A. Passe de sombra 003E4A28 +F0..1A8, modelo ímpar, falta. Vistas de inspeção miram faixa original a partir do olho capturado, não provam câmera PS2. Screenshot do slot 5 mostra 4,766 s e RAM 12,783 s: não comparar como mesmo frame. Sem ajuste numérico pelo print. |
| Licenças: carro na câmera antes da contagem | A | (c) 10/10: race_game usa kind 4 na licença viva (003744F0/00374720), bank COURSE_DATA +D4/replay_start_camera1, opening=1 e clock +60 de 0037D338 +F8..110; antes herdava +D0/grid do Arcade. license_objects_opening: B-1/B-3, oito amostras com pixels do carro, relógio BC+60 e saída em BC=0 durante 480 frames por prova. Negativos omitem carro e retêm câmera após BC=0, ambos falham; demo do menu passa. Prova de integração/visibilidade, não imagem completa contra PS2. Lente tem C separado; near .1/viewport/look-at e comparação integral da abertura continuam A. Física preservada. |
| Licenças: carregar 80 exames e dirigir B-1 | C | (c) career_license_disc/race_run, 0011D0F8/00447400/00444440/003BDB98/003BDC88: dados do disco, dois carros, sem exigir garagem. gtmode_license_disc abre 80 exames B/A/IB/IA/S e executa quatro frames cada; negativo rejeita um carro. gtmode_license_drive ISO l0b0001 980 acelera/freia por comandos de controle: 10248 ms, estado 3/prata; negativo invalida estado. Não prova conclusão física dos outros 79, referência, instrução, câmera ou HUD específicos. |
| Licenças: ranking, calendário e save nativos | C | (c) gtmode_license_profile: perfil sem garagem, 400 tentativas injetadas em 80 exames, medalhas, melhor tempo persistente, quatro rankings por exame, save/reload após cada visita, calendário +80 dias; mais 13 inserções conferem empate estável, limite de dez e descarte da cauda. 00432040/004321B8/00432268 e 001105A8/00110800. Tolerância zero; negativo altera melhor tempo e falha. 3/3 CTest isolados passam. Ponte de perfil usa API já commitada da VM. Não prova resultado físico dos 80 nem conteúdo binário do ranking PS2; descrição do replay e ghost pendentes. |
| Licenças: retorno à escola com menu em cache | C | (c) gtmode.ad L8226..8227/L8254..8296/L8394..8398: onActivate esconde context.cursor_visible e mostra Pane.Veil ao sair; onInitialize esconde o véu e atualiza medalhas, sem restaurar a visibilidade do contexto antigo. menu_license_return_prepare reinicializa essa página e, em 10/10, descarta também o cursor oculto herdado do projeto em cache, preservando coordenadas/interpolação. gtmode_license_profile executa a ativação original nos cinco grupos, exige cursor oculto na saída, um draw do cursor após retorno e outro após navegar à direita, tolerância zero. Negativos omitem reinicialização ou restauram cursor_visible=0 e falham, inclusive GT4_LICENSE_CURSOR_RETURN_NEGATIVE. Não certifica pixels/tempos integrais contra PS2; resultados de tentativas são fixtures. |
| Licenças: resultado, retry e pedidos de música | C | (c) race_result.c: /menu/US/result_license.strb, 003E1BF8/003DF898/003EC2B8/003E0A50; título 003BCB38/003E0940, diferença 003E1EA0, medalhas/PCM 003E1C80. gtmode_license_movie verifica sete estados e duas saídas, textos, desenho e enums PCM; negativo altera prêmio e falha. Sem comparação sonora/visual com PS2; troféu 3D integrado com nota A separada. Navegação mantém estimativa A existente de botão mais próximo. |
| Licenças: contornos vetoriais da palavra FAIL | C | (c) 07/10: `/menu/US/result_license.strb` shape 0, fills +3294; A +32b8 contém dois contornos (+3490/+34a4), incluindo o recorte interno. `strobe_player` agora preserva os segmentos retos, diagonais e concavidades em 13 trapézios residentes; não substitui o A pelo bounding box. `gtmode_license_movie` compara cobertura enviada ao backend com teste independente de ponto nos contornos originais: 47863 pontos por reprovação (-1/0, duas saídas), zero diferenças fora de 0.0001 unidade local das bordas. Dois negativos reproduzem bounding box/recorte preenchido e falham. 9/9 testes de movie/negativos/navigation; captura app B-1 fail mostra as quatro letras. Sem certificação do raster/antialias GS ou de curvas/interseções arbitrárias. Cache sem tesselação/alocação por quadro; mesmo batch 2D. |
| Licenças: quadro animado na reprovação | C | (c) 06/10: 003E0E38 +0x38..0xa0 / 0033DD88, extrender screen de /menu/US/result_license.strb. gtmode_license_movie exige movimento e compara o retângulo final contra bounds/último parâmetro lidos independentemente do disco, tolerância zero, nos estados -1/0 e nas duas saídas; estados aprovados pulam esse quadro via Action 0. GT4_LICENSE_SCREEN_NEGATIVE desloca x e falha. 9/9 testes de movie/drive/negativos passam. Não certifica interpolação 1/256, câmera, atores ou imagem 3D. |
| Licenças: composição PC da reprovação | A | (c) 06/10: captura GPU residente antes do HUD; quadro STRB sobre preto, sem readback CPU. Tratamento 003E0E38/003EFEA8/0033DE30 integrado: primeira imagem sem efeito, parâmetros publicados para a próxima; saturação *= bits3f7ae147 por callback. CLUT/mistura conforme provas abaixo, cache CPU/GPU só muda com opções. App B-1 fail termina 0/17599 ms e dessatura a cena; referência local slot 03. A: amostragem/captura/viewport completo e ponte parcial de opções (brilho/contraste PC; temperatura/saturação de opções neutras). Sem ajuste pelo print. |
| Licenças: fiscais e câmeras animadas do resultado | A | (c) 06/10: /pit/license_001, stand/success/failed, SKINNING/CRANIUM/pen/board, três câmeras, posições RNW4. Inclui pitman01 residente e seus 20 binds; sombra pela direção Env0/normal, limite 1.5 e bias bits3c23d70a de 00408490. Luz por superfície 00409B68/0040A1D0: quatro das nove candidatas, ambient +EF0 e fallback 00398210. Env0 emprestado do curso residente: loader da física carrega só RNW4/.ad. Resultado reinicia no primeiro desenho e usa bits3c888888/pacing 60 Hz. App result/fail/retry --concourse passa. Provas E/C separadas abaixo. A: near .1/viewport, composição skin/partes rígidas, raster/material/lighting GPU, fade 8 bits e interpolação STRB; referências slots 02/03/04. Sem ajuste no olho. |
| Licenças: CLUT, mistura RGB e decaimento da reprovação | E | (c) gtmode_license_image: 004A8CB8/004A8AD0/004A8BB0 compara 105 CLUTs completas de 256 RGB, incluindo unswizzle; 004A7D90 compara 401 conjuntos de coeficientes; 003E0EE0 compara 600 multiplicações, byte a byte. Negativos LUT/MIX/DECAY falham separadamente. Opções 003EFDF0/003EFC10 são C na fronteira powf host; domínio brilho/temperatura -10..10, contraste/saturação padrão. Overflow EE 7fffffff representado por max IEEE fora desse domínio (A). Não certifica pixels GS. |
| Licenças: filtro da reprovação na GPU | C | (c) license_image_probe: 327680 pixels, cinco saturações/dois brilhos; CLUT + três MODULATE/add contra CPU alimentado pelos coeficientes testados contra EE, tolerância 1 byte. Negativo altera matriz enviada e falha; próximo desenho 2D restaura programa/blend/textura. Prova shader/integração, não um quadro inteiro contra GS ou outra escala. |
| Licenças: matriz da sombra dos fiscais | E | (c) gtmode_license_image compara 512 matrizes contra 004085C4..00408678/00407FA0, direções antes/depois do limite e ny=0/1, byte a byte. Negativo SHADOW altera projeção e falha. Direção segue 00398758/00398438; sin/cos host, não certificados nesse teste. Malha/raster A separados. |
| Licenças: luz específica dos fiscais | C | (c) gtmode_license_image compara 00397FF8 em 512 cores byte a byte e 00409B68/003E6C60 em 96 contextos de nove luzes direcionais/pontuais/spots (30/60/180 graus), seleção estável de quatro. Tolerância .000002; sqrt/pow/cos e chão como fronteiras explícitas. Negativos LIGHT/POINT falham. Global ambient e normal integrados; pixels VU/GS não comparados. |
| Licenças: relógio e reset da UI no término | E | (c) gtmode_license_image executa 003DFE20/003DF978/003E1C28/003E1C80/003E1E10/003E1EA0: passo bits3c888888, reset posterior ao avanço na primeira resposta, cinco estados aprovados/duas reprovações; primeiro desenho tempo zero, segundo passo original. Negativos CLOCK/RESET alteram passo/primeiro tick do helper nativo e falham. App usa esse mesmo helper e pacing 60 Hz; interpolação STRB float e sincronismo de pixels/entrada A. |
| Licenças: diretor do LicenseConcourse | E | (c) license_concourse, 00408F90/00409138/00409258, 004096F8/00409860. Oráculo compara 14400 passos de timers/fases/posição pronta, solicitações stand/success/failed e ciclo cam01/02/03; guarda anterior confirma que o corte é pulso ao entrar em fase 2. Negativo altera relógio e falha. Não certifica desenho, fade ou abertura da UI. |
| Licenças: relógios, seleção de movimento e curvas escalares | E | (c) license_motion, 00427698/00429960/00429BC0/0042C348, 0042DAA0/0042DC00/004861A8. Oráculo usa as 37 entradas nomeadas do disco e compara 88800 passos, átomo/tempo local e todos os canais a cada 31 passos. Polinômio na ordem Horner original, com extrapolação do último segmento. Negativos independentes de tempo e curva falham. Fluxos look/FOV/CameraCoord/TargetCoord também bit a bit contra 0042B440 em 630 amostras de câmera. Não certifica tan/projeção ou mundo. |
| Licenças: cena completa atrás do menu | A | `0011D1B8 +44..60` conserva RaceLicense em Scene mode 2; `0012BFE0 +28..7C` desenha a cena completa. `license_menu_probe` exige rodas e diferença ao ocultar o carro (387986 bytes de cor na árvore local); reprodução move o carro sem alterar fita/miniatura/logger da tentativa. `003EA8C8` seleciona a tentativa retida em E48C; professor automático sem tentativa é P, como política PC deliberada. Retorno reconstrói o menu e Start restaura o exame inicial. Negativos de cena vazia e fita falham. `pregrid_probe` continua separado para o Arcade. Falta comparar a lista de desenho/pixels com o original; câmera e descompressão têm provas próprias. |
| Licenças: sequência controlada da câmera do menu | E | Mot1 do curso +D4, `replay_start_camera1`; `003EBC10`, `00429960 +40..148`, `00429BC0`, `0042C348`. Oráculo compara 14400 passos das seis entradas, controles 1..5: clocks, átomo, tempo local e canais escalares bit a bit. Negativos alteram clock e canal. Seletor `0037D5B0 +14..6C`, tabela `00621290`: 2401 frames comparados com o original e negativo independente. Banco residente emprestado, sem cópia do curso nem alocação por tick. Não certifica controlador completo de Scene ou raster. |
| Licenças: amostra da câmera de abertura do menu B-1 | C | Slot 05 fornecido pelo usuário: Scene mode 2, Race +DC=1, Camera kind 4/contexto 2. `license_menu_probe` encontra Scene pelas vtables +64/+80, reproduz 285 ticks e compara eye com tolerância 1 mm (erro máximo 0.000274181366 m) e lente com 1e-5. Negativo desloca eye em 0.1 m e falha. App/probe/oráculos compilam na árvore staged isolada, com probe do slot 05 e 9/9 CTest focados passando. Não certifica matriz, trajetória inteira ou pixels. Leitura PINE posterior confirma kind 1/TV com BC=0. |
| Licenças: descompressão dos comandos das 80 demonstrações | E | (c) 07/10: `disc_replay_oracle CORE ISO --licenses` compara todos os 394876 comandos de 32 bytes contra `00345D70`/`00345BE0`, sem diferença. `--licenses --negative` altera direção no primeiro quadro e falha (exit 1). Base `00579E00` lê a tabela `00655888` do disco e verifica CRC; multiplicação/divisão inteira PS2 usada somente para decifrar assets, nunca na simulação. Pool zero e capacidade de `00346610`/`00346890`; entrada limitada e sem leitura fora do buffer. Essa prova cobre descompressão, não montagem, trajetória, câmeras ou os botões. `00110230 -> 003EAD58` carrega apenas o gravador do professor, conservando o exame atual. 93 arquivos (80 licenças + catálogo US de 13 demos), 556738 comandos no total. Integração tem registro separado; física não alterada. |
| Licenças: botão Demonstration e retorno | A | `00110230 +98..110 -> 003EAD58`: fita/opções do professor no exame atual, sem restaurar IDs históricos. `0011D1B8 +44..60`: Scene/RaceInfo +DC=1. `disc_demo_probe` B-1 entrega os 1471 comandos, consome a fita, move o carro, não cria resultado, retorna e preserva fita/miniatura/logger anteriores; cancelamento também passa. Negativo altera direção e falha. `disc_demo_app.py --case license-return` entra pelo botão original e volta sem medalha/melhor tempo; negativo exige 1472 comandos e falha. `0039BEC8 +FC..108` omite caption nos modos 8/9. Não prova trajetória/câmera/pixels contra PCSX2; seleção do menu passivo tem registro separado. Nenhuma edição em src/core/physics. |
| Replay Theater: catálogo US e reprodução das demos do disco | A | `config-main.ad L146..160`, `DeviceDemo 0017C348 +40..54 / 001D19F8 +168..26C`: 13 arquivos Rdemo0..12 em ordem, não todo o diretório. Lista de cada manager é própria (main/sub não se sobrescrevem); nomes próprios conservam carregar/salvar/apagar replays PC. `disc_demo_probe --theater` carrega as 13 pistas/grids, reproduz 120 passos, confere pose finita/checkpoints e saída. `disc_demo_app.py --case theater-return` seleciona Demo, Bentley/George V Paris, Play e retorna ao Theater após a fita. 10/10 CTest focados passam, incluindo quatro negativos. FreeRun Rdemo9 usa sessão TimeTrial existente; início/restauração entram na fronteira de frame da app (A). Miniaturas têm prova E separada. A: datas e comparação completa de trajetória/câmera. Variantes PAL/KR fora do disco NTSC-U v2.00 suportado; miniaturas originais têm prova E, não ausência funcional. Física preservada. |
| Replay do disco: restauração do checkpoint | E | `0035ABA8 +2C..D8`, `003F85F0`, `00463930`, `003455F0`, `0045B948 / 0045CF18`: car state, links, history e collision hulls. `disc_checkpoint_oracle` compara os 0x10A0 bytes do carro e 0x1DC8 da área de colisão com a função EE original usando o snapshot do Rdemo9; sem diferenças. Negativo altera car+400 e falha. Adaptador fica em src/app/disc_checkpoint.c; solver e arquivos de física não alterados. E apenas para essa operação/entrada: relógio/fração/ordem dos callbacks de replay e montagem da sessão continuam A. |
| Licenças: ciclo do menu, estado e relógio do fade | E | (c) 07/10: `0012B928 +40..B0/+258..2C8`, `0012BC18 +6C..114` e callback `0012B888 +1C..4C`: fecha em 1 s, duas chamadas de desenho em preto, agenda reconstrução no mesmo modo, reabre em 0,5 s. `gtmode_license_menu_cycle` compara os 28 bytes MenuRaceBase+A0..B8 e os passos de mundo em 49071 amostras contra essas rotinas do executável, quatro comprimentos de fita, três ciclos e três atrasos do callback. Negativo muda alpha e falha. Scene, fila de tarefas e GS são fronteiras declaradas; esta prova não inclui a detecção de EOF, a duração real da reconstrução ou pixels. |
| Licenças: apresentação completa do menu com demonstração | A | (c) 07/10: `0011D1B8 +44..60` NÃO importa professor; `003EA8C8` escolhe E48C, ou E84C pelos flags F054/E0. Slot05 e menu-late retêm 1721 comandos do jogador, contra 1471 da demo B-1. Port copia a tentativa e conserva opções; professor automático quando não há tentativa é P como política PC deliberada. Ao acabar, fecha/reconstrói/reabre e repete a fita em runner existente. `license_menu_demo` compara comandos no pad e três ciclos (4410 amostras + 3 comandos iniciais) com entrega a 60/120/240 Hz: corpo/rodas/distância/abertura/clock repetem bit a bit; fita/miniatura/logger anteriores preservados e Start restaura o exame. Negativos alteram fita e posição e falham. `disc_demo_app.py --case license-menu-loop` verifica dois reinícios pela UI em 58 s, sem resultado/medalha. Fase C: slot05 recorder+40=286/Mot1+188=4,74998 (285 ticks de câmera); um passo de mundo precede câmera (`00389990 +134..154`). Probe com a fita de 1721 comandos capturada confere cursor/pad e carro/eye em 1 mm, lente 1e-5; negativo eye+0,1 falha. A: estado menu-late em cursor 1026 (passo nativo 1025) falha o limite de 1 mm no Z do carro (0,0329589844 m); não corrigido por ajuste da física ou tolerância. TV escalar conferida separadamente (novas linhas abaixo); matriz TV/manager, ordem assíncrona do Scene, sinal completo de fim (`003EB370 +298..2CC -> 00389758 -> 003BC530 -> 0033CCF0`) substituído pelo EOF decodificado, reconstrução e pixels/alpha GL ainda A. Near .1 e adaptador 640x480/448 permanecem A. Sincronismo completo não provado; física não editada. |
| TV: operador e gerador aleatório | E | (c) `tvcam_original` executa 00384E58/00384EE8 e 00578598: 8.000 passos, estado/filas/sorteios bit a bit, oito sementes e sete rigidezes. Conversão 00578598 +18..48 monta IEEE com plzcw/shifts; não divide por 2^32. Semente 12345678 de 003721D8 +44..48 -> 005792D8. Draw de roll consumido mesmo com kind400 (003747B8 +128..15C). GT4_TVCAM_OPERATOR_NEGATIVE altera posição do operador e falha no comparador exato. Restrito ao operador, não ao diretor inteiro. |
| TV: passo escalar CameraShot | C | (c) `tvcam_original`: 4.800 quadros contra 0037BF00/0037C138, cinco kinds, vinte fixtures, lentes/roll/curvas variando, zoom1/4. Inicialização e update no mesmo primeiro quadro (0037C6D8..748); divisão por zoom posterior à mola (0037C888); bits e ordem de operação preservados. Tolerâncias: estados/ângulos 0,0002, velocidade angular 0,01 grau/s (máximo observado 0,00183105468), eye/lente/roll 0,0001; RNG/contadores/fim exatos. World, libm, matrizes/VU e manager são fronteiras explícitas. Negative yaw+0,1 falha; não certifica composição GS/diretor. |
| TV: escolha da tomada pela distância e volta | C | (c) 16/10: `gt4_tvcam_distance` como 003FBE70 +1C8..1F4 (volta 00343BC0 = car+0x5B0 + corrida+0xCBE5, reduzida às voltas que a tabela cobre por 003DDEC8; alvo+0x34 = distância + volta x comprimento) e `pick` como 003FD980/003DDFF0/003DDF18/003DDF88 (dobra pelo período após REP4 +0x18, busca binária, continua do início um período atrás). Antes usava a distância módulo uma volta e só achava tomada em pistas cuja tabela começa na volta 0: Capri, Nürburgring, Le Mans, cidades etc. caíam na câmera do carro. Varredura do disco: 107 de 110 pistas cobertas a 100% nas voltas 1..3 (ss_rialt e testcourse_* têm buracos na própria tabela). tvcam_original continua bit a bit. A: alvo+0x3C (003FCAB0) tomado como 0; comprimento de 00395400 tomado como REP4 +0x14 |
| TV: ponto do carro seguido pelo app | C | (c) 003FBE70 +E4..118 preenche CarTarget+14 por 00344798/003447F8/00344828; 003FC618 +C devolve esse anchor, distinto da origem do modelo +894. Corrida/menu usam gt4_race_run_place (API apenas de leitura). `license_menu_demo` confere 4.410 alvos/distance contra campos +888/+8A4 e blend+554, byte a byte; negativo usa origem do modelo e falha. Fonte e captura menu-late confirmam +888. Restrito à escolha do alvo/blend, não à trajetória PS2. |

| Licenças: ouro B-1 com comandos do professor como jogador | C | `003F8AB0` conserva resultado na demonstração DC=1 e classifica tentativa DC=0; limites vêm do RACE do disco. `license_teacher_gold` testa apenas o pad original como jogador, sem editar física: estado 2/ouro, 10083 ms, exatamente os 10083 ms armazenados no GT4M e abaixo de 10150 ms. Negativo exige prata/estado 3 e falha. Demonstração mantém resultado ausente; teste não modifica app nem concede medalha pela demo. Não certifica outras 79 trajetórias, render do troféu ou física inteira. |
| Replay Theater: miniaturas das 13 demos US | E | `001CFAF0 +1F8..21C` lê icon0.ico e cria a miniatura; `001D3B20 +18..1B4` pula vértices/animação; `001D3D70 +14..38 / 001D3FE8` selecionam as 64 primeiras colunas de cada linha de 128 pixels, com máscara 0x40 e alpha 0x8000. `disc_replay_icons` compara todos os 53248 texels RGB555 das 13 miniaturas contra essas funções EE, byte a byte; truncamento/cabeçalho inválido rejeitados. Negativo altera um texel e falha. Manager usa cache RGBA 64x64 separado de thumbnails PC 128x96; teste exige isIconValid/getIconTexture nas 13 entradas e app `--case theater-icons` mostra os assets. E restrito aos texels/arquivos testados; expansão RGB555 herdada de tex1.c, filtro/GL/layout do Theater A, sem certificação dos pixels da tela. |
| Licenças: poses acumuladas e posições locais dos fiscais | C | (c) license_pose_oracle executa 0042B440/0048B888/0048A970 com pilha/multiplicação e sin/cos como fronteiras explícitas. 2590 fluxos totais (1960 atores, 630 câmera); matrizes com tolerância .00002 e maior erro 1.19209289e-7. license_people_oracle compara 144 posições/headings de 00408B08, byte a byte, com CarInfo/rodas/atan2 como entradas/fronteiras. Negativos de pose, câmera e posição falham. Não certifica shaders, cabeças/paleta publicada, terreno, sombras ou imagem final. |
| Licenças: coeficientes da lente do LicenseConcourse | C | (c) 06/10: 0037DC78 +0x3a0..0x3d0 converte Mot1 radianos *180 / bits40490fda; 003794A8 +0x28..0x98 usa metade do ângulo e aspecto bits3f3340cd. license_camera_oracle compara 2000 casos byte a byte com esses blocos originais, tan 0057D380 substituído explicitamente por tanf host. Negativo altera sx em 1% e falha. FPU restaurada. Não certifica libm do PS2, matriz look-at, near plane ou pixels. App passa esses coeficientes à câmera; fiscais usam a mesma transformação de pixels da pista/carro, adaptador A. |
| Licenças: relógio, gatilhos e fator do fade | E | (c) 06/10: 0033C4F0 +0x2dc..0x328 soma dt*rate e limita 0..1; 003BC6E0 +0x10c..0x16c com 004097F8/00409828 muda rate para -1/2 na fase 1 e +1 na fase 2, só valendo no tick seguinte; 0033FA40 +0x2c..0x48 conserva cúbica e ambas subtrações. license_camera_oracle: 1720 passos, 1120 pares enabled/fases/rate/clamps, 1001 fatores, byte a byte; sequência fecha/corta/abre, incluindo endpoints. Três negativos independentes alteram posição, rate e fator, todos falham. Integração desenha preto (00620200) antes do HUD/resultado conforme 0038B5A0 +0x18; raster GL/alpha de 8 bits permanece A, sem readback/alocação por frame. 18/18 CTest específicos; árvore staged isolada compila app/oráculo e passa 5/5 novos testes; app B-1 result/fail/retry --concourse passa na árvore local, sem certificar pixels PS2. |
| Licenças: layout extra, molas e seleção de CLUT do troféu | E | (c) 06/10: license_presentation, 003BD808 +0x78..0x1d0, 003E18D0/003E1A80/003E2058 e 00498CA0 +0xb0..0x140. gtmode_license_presentation_oracle CORE compara os oito cabeçalhos de elementos, 9600 passos completos das dez molas e 768 seleções de CLUT contra o executável original; negativos deslocam x ou alteram TEX0 e falham. Não cobre HUD comum, projeção, renderer, material/arquivo malformado nem composição do troféu. |
| Licenças: HUD e desenho nativo do resultado | A | (c) 003BD150/003BD740/003BCE70: tempos/ícones originais, contador FC_26*80,80 e supressão de Lap/contagem para exames com limite de contato; sem posição/Last Lap/introdução Arcade indevidos. 003E1B58/003E18D0: trophy.mdl tem geometria única e três variantes de CLUT, não três modelos. Extrender usa nome do próprio asset (+0x10); STRB aplica a cor adicional (+0x1c), antes ignorada e responsável pelo cronômetro preto. Fonte usa 00492978. gtmode_license_movie confere sete estados/duas saídas, troféus e glifos coloridos; negativos da ponte de troféu/cor falham. Cronômetro completo (oito glifos), sem reticências/recorte de campo, segue 00479CA0/00490700; negativo reduz glifos e falha. Desenho 3D ainda sem comparação PS2; uma amostra em vez da acumulação original de três; composição/projeção/lighting do adaptador e interpolação STRB float em vez de 1/256 permanecem A. |
| Licenças: fantasma da melhor tentativa | A | 17/10: como no PS2 (relato do usuário), repetir a licença mostra a melhor tentativa aprovada (ouro/prata/bronze) transparente, a 0,55 como o do Time Trial. A: o mecanismo do original não foi lido (a sessão da licença 003F64D0/003F6588 não chama o gravador do Time Trial; 003BB478 limita a corrida às entradas do evento); aqui é só apresentação: a pose do carro (posição, rotação, rodas) por passo da física, interpolada, sem carro na simulação, física e tempos intactos. Save Ghost/Load Ghost (quick-license canSaveGhost/canLoadGhost) gravam/leem um arquivo de melhor volta com o bloco P `LGPS` (poses e código do exame), só para o mesmo exame, pista e carro. license_app retry: tentativa de prata guardada e mostrada na repetição; replay_file e 148 de 149 testes de licença/TT/replay/HUD passam (license_menu_demo já falhava). Sem comparação lado a lado com o PCSX2 |
| Licenças: entrada/resultado/retry no aplicativo | C | (c) tests/gtmode/license_app.py --case exit/retry/next/leave: navega GT Mode → B-1, acelera/freia pelo controle nativo, chega prata/10248 ms, registra uma tentativa e retorna ao LicenseRoot, reinicia, abre B-2 ou volta à escola. Tolerância zero para estado/tempo; saves/capturas isolados em build/ui-audit/license-app. Regressão corrigida: descritor de exame não fornece RNG de grid; cópia incondicional travava. SetLoadingMode ligado à apresentação de carregamento PC (001AC248/00104E30). Captura com --shot-delay agora permite transições; retorno reinicializa quick-license para atualizar BestTime e LicenseDetailRoot para esconder o véu da saída. Negativo do aplicativo exige medalha errada e falha. Não compara visual/áudio nem prova os outros 79 exames. Primitivas e ponte testadas não equivalem a licença completa. |
| Licenças: tempos no menu, instrução e acesso ao exame 16 | A | (c) goldTime/silverTime/bronzeTime são strings de 00127AF0 → 0042E478, diretamente atribuídas por quick-license.ad L276..278; ponte corrigida e conferida nos cinco grupos pelo gtmode_license_cycle, negativo injeta inteiro e falha. available_next original L122..149 rejeita exame 16 sem os quinze aprovados e libera depois, nos cinco grupos; negativo altera gate e falha. Instrução traduzida completa via L253..256/getTextSize, quebra e medida usam mesma métrica 00492978 do desenho. Layout de parágrafo/equalized e rolagem mantêm adaptador A; ainda falta comparar com PS2. |
| Licenças: amostragem nativa do Logger | E | (c) 06/10: logger.c/h, 0011E400/0011E450/0011E310/0011E348, getters 00343B48/00344698/003446A0/00344798/00344828 e consultas 0011DBB0/0011DBE8/0011DC80/0011DD10. gtmode_logger_oracle CORE: 30600 entradas, 132046 pontos de oito bytes, 549756 consultas e 36 espaçamentos; buffer completo, estado float e resultados bit a bit. Executa também bloco original 00123050..00123078 para espaçamento. Toward-zero e intermediários sem FMA. Negativo altera speed_gear e falha; 4/4 CTest do Logger passam, incluindo B-1 abaixo. Fronteiras: proteção de buffer vazio distinta; replay, segunda volta/ghost, workers, tela e FPU excepcional não certificados. Sem valores estimados na amostragem. |
| Logger: carregar trilhas A/B de replay | C | 12/10: LoadTrackA/B original solicita seletor do labomode; `race_resources` carrega cada banco, aceita carro diferente na mesma pista, compara todos os pontos byte a byte e preserva o outro banco, gravação corrente e metadata de carro/potência. Rejeita outra pista e cancelamento conserva dados. Testa também partida de melhor volta pelo snapshot 0x10A0. Negativo modifica banco não escolhido e falha. Amostrador tem oracle E acima; esta prova é da integração nativa. Fração/relógio completo da melhor volta, worker e UI A; formato/seletor de arquivos PC P. |
| Licenças: coleta do Logger durante B-1 | A | (c) race_game coleta estado nativo depois de cada passo, conserva dados até nova largada e encerra na chegada. Adaptação: coleta ao vivo, enquanto 00122FA8 reconstrói uma sessão de replay para analisar dois carros; só jogador, sem demonstração inventada. gtmode_logger_drive ISO/CORE dirige B-1, prata/10248 ms em 1094 frames, 164 amostras e máximo 18.863647 m/s; cada byte e estado float conferido com 0011E450. Negativo altera freio e falha. App --case exit com MOON_LICENSE_LOGGER_LOG=1 confirma 164 pontos, distância 102.898658 e retorno; captura local build/ui-audit/license-app/exit. LoggerFace/MLoggerControl e trilhas A/B implementados; logger_view/race_resources testam integração com negativos, UI integral A. A coleta ao vivo continua diferença explicitada nesta linha. |
| Licenças: certificados e 15 carros-prêmio | C | (c) gtmode_license_award: PresentData/scripts gtmode.ad L14176..14206/L14456/L14712 originais, medalhas injetadas; cinco certificados, 15 condições/carros, corpo completo do veículo, flags de prêmio, save/reload e entrega única. Negativo altera byte do carro e falha. startStream do PaperPane ligado a 0023E5C8/0023FA78 pelo PCM existente; volume herdado do adaptador não certificado contra PS2. Presenter headless não prova animação/áudio/imagem final nem completar 80 exames dirigindo. |
| Carreira: créditos, preços, garagem, calendário, óleo, prêmios, odômetro, notas de licença | E | só a regra isolada; ver `GT_MODE_LOGIC_VALIDATION.md` (d) |
| Carreira: compra usada capturada, script e Auto Save | E | gtmode_purchase_menu: GetCarRoot original, BEAT usado slots 04 -> 06; dinheiro, unidade/corpo do carro, selecao e reload identicos ao PCSX2. Reexecutado 03/10. GT4_CAREER_TEST_NEGATIVE=1 altera saldo em 1 credito e falha. Escopo: transacao capturada; nao certifica todos os estoques, carros preparados ou pixels da concessionaria (c) |
| Carreira: venda selecionada capturada, estado e save | E | gtmode_sale_oracle: CR-X slots 02 -> 03, preco 1353, saldo 5939; garagem 0x81d0 e calendario 0xb6c8 identicos, save/reload preservados; 2048 produtores de calendario e 256 indices de cor. Reexecutado 03/10; GT4_CAREER_TEST_NEGATIVE=1 altera saldo em 1 credito e falha. Nao certifica handler grafico completo ou outros namespaces de carros (c) |
| Carreira: seleção do acerto ativo da garagem | E | (c) `gtmode_active_setup_oracle`: 768 casos (3 folhas x 256 estados) contra 00441248/00441228 e 004452F8, RAM/SPR completos; controle com um byte alterado retorna 1. Limite: seleção e cópia de 0x178 bytes; índice inválido é rejeitado na fronteira nativa. |
| Carreira: conversão das regras do evento | E | (c) `gtmode_active_setup_oracle`: 4096 linhas RACE forçadas contra 0035E108, bloco de 0x80 bytes e RAM/SPR fora da pilha da chamada idênticos; GT4_CAREER_RULES_NEGATIVE altera um byte e falha. Não prova a seleção de adversários nem a transação de resultado. |
| Carreira: configuração de desgaste da sessão | E | (c) mesmo oráculo: 4096 pares contra 00367AF8/00367918, RAM/SPR completos, modificadores globais ligados/desligados e todos os seis caminhos de rejeição forçados; GT4_CAREER_WEAR_NEGATIVE altera um byte e retorna 1. |
| Carreira: sorteio dos índices de adversários | E | (c) `gtmode_enemy_indices_oracle`: 8192 casos contra 003BB5A8/00578598; índices e estado RNG idênticos, listas liberadas/bloqueadas/mistas e esgotamento do conjunto sem repetição. Controle com índice alterado retorna 1. Fronteiras de dados/construção explícitas; não certifica resolução do bloqueio por país, pintura ou montagem completa do grid. |
| Carreira: montagem dos mundos GT e campeonato | A | (c) `race_desc.event_row` copia RACE e regras; mundos 00684678/00684BF0 e modos 13/14 vêm de 003EDE50/003EF498. Smoke com am_sunday_0001, dois BEAT, 120 frames em ambos os mundos; controle negativo falha. Pista/grid do smoke são fornecidos pelo teste, ainda não selecionados pelo frontend original; sem prova da sessão inteira. |
| Carreira: construção de carros preparados dos adversários | E | (c) 00444690/00444860; GT4_TUNED_RUNTIME_DB=1 gtmode_tuned_oracle: 2558/2558 linhas ENEMY_CARS, retorno e 0x178 bytes idênticos em purchase-slot06 e award-slot07. Fronteira SpecDB lê o disco, lógica original executa no oráculo. GT4_CAREER_TUNED_NEGATIVE altera peça e falha. Escopo: construção isolada, não grid ou ciclo completo. |
| Carreira: bloqueio regional de adversários NTSC-U | E | (c) 00448C10/00448CA0/0044A5A8: 4096 casos originais com regiões e chaves base/preparadas; a função de pertinência do CORE retorna zero. GT4_CAREER_COUNTRY_NEGATIVE altera retorno e falha. Não representa as regras de nacionalidade de entrada do jogador. |
| Carreira: seleção ponderada das pinturas do grid | E | (c) gtmode_paint_oracle: 768 execuções de 0040A6F0/0040AA48, todos os blocos de pesos (0x304 por carro), índices e RNG final idênticos. Cores fixas/pendentes, preto/branco/duplicadas, 1..6 carros e até 32 cores. GT4_CAREER_PAINT_NEGATIVE altera peso em um bit e falha. Fronteira de acesso à paleta explícita; integração do evento separada. |
| Carreira: paletas usadas no sorteio de pintura | E | (c) gtmode_paint_oracle extracted/core.bin disco.iso: 722 carros, 2166 casos com flags reais e bit 1 forçado dos dois lados; contagem e RGB contra 00445828/00445AB8. Quinze carros omitem a última pintura no sorteio. Lacuna de ordem do carro 619 preserva a última linha lida, conforme 004458D0. GT4_CAREER_PALETTE_NEGATIVE altera RGB e falha. |
| Carreira: interpretação da pintura no arquivo RACES | E | (c) gtmode_paint_oracle: 4096 casos contra 004474B0/00445880, incluindo zero, UINT64_MAX, truncamento/sinal e índices rejeitados. GT4_CAREER_LIST_PAINT_NEGATIVE altera índice e retorna 1. |
| Carreira: inicialização e continuidade do RNG de adversários/pinturas | E | (c) gtmode_grid_paint_oracle: 4096 estados iniciais contra o construtor 003BB820/003BB7E8/005792D8, apontado pela tabela 0061736c; estado 00621AE8 inicializado com 0x12345678, não o zero armazenado no CORE. GT4_CAREER_GRID_CHAIN=1: 30 grids consecutivos em cinco séries do disco, alterando as provas existentes e repetindo a primeira quando não há próxima; setups completos e RNG final idênticos contra 003BB5A8/004474B0/0040AA48/003EE3C8. Fronteiras de construção preparada/disco explícitas. GT4_CAREER_RNG_NEGATIVE e GT4_CAREER_RNG_CHAIN_NEGATIVE alteram o estado e retornam 1. pregrid_probe confirma no aplicativo preservação no reinício/replay com essa semente; essa integração não é prova EE da sessão completa. |
| Carreira: preparação nativa do descritor do evento pelo disco | E | (c) career_event.c / gtmode_grid_paint_oracle: cinco eventos e seis sementes por evento, linha RACE, pista, número de entradas, cópia integral do carro equipado, adversários/pinturas RACES, setups completos e RNG final contra 003BB5A8/004474B0/0040AA48/003EE3C8. Construção preparada e acesso a dados são fronteiras explícitas. RNG fornecido pelo chamador; inicialização/continuidade têm prova separada abaixo. Lançamento no frontend permanece no item A. GT4_CAREER_EVENT_GRID_NEGATIVE altera setup +0x19 do carro equipado e a comparação retorna 1. |
| Carreira: estado de pré/pós-corrida entregue aos seletores originais | C | (c) pregrid_menu: ponte MQuickWork recebe IsSessionFinished, prize, GrandPrize, raceLabel e IsBSpec do chamador; dez ramos getStartPage originais de evento/missão/campeonato e retorno à pré-corrida na mesma VM passam, replay vazio após reset. --selector-negative escolhe uma página errada e retorna 1. Esta prova é da escolha da página/ponte; resultado da corrida e apresentação completa permanecem pendentes. |
| Carreira: pinturas aplicadas na montagem nativa do evento | E | (c) gtmode_grid_paint_oracle: cinco eventos do disco (Sunday Cup, Paris, neve, gelo e Grand Canyon), seis sementes por evento, blocos completos de setup 0x178 e RNG final contra 003BB5A8/004474B0/0040AA48/003EE3C8, incluindo os pneus exigidos. Construção dos carros preparados e acesso ao disco são fronteiras explícitas, com provas isoladas; esta linha certifica a composição de sorteio/pinturas, não a sessão GT completa. GT4_CAREER_GRID_PAINT_NEGATIVE altera uma pintura e retorna 1. |
| Carreira: pneus exigidos na entrada de provas de terra/neve | E | (c) gtmode_event_tyre_oracle: 722 carros e 18050 casos contra 003EE3C8/00447550, tipos 0..4, ambos/eixo isolado já equipado e lista ausente; blocos completos de setup idênticos. Tipo vem de COURSE +0x10 por settings +0xa0; níveis 11/12 são do original. Acesso a dados/getters/setters são fronteiras explícitas; não certifica montagem completa da sessão. GT4_CAREER_EVENT_TYRE_NEGATIVE altera chave de pneu e retorna 1. |
| Carreira: processamento do resultado de evento antes do pagamento | E | (c) gtmode_event_result_oracle: 8192 casos contra 00114498, conclusão/abandono, posições 0..6, modos 0..3, recorde, posição anterior, A-spec, prêmios e overflow; estatísticas, recorde, posição e argumentos do histórico idênticos. Fronteiras de consulta do resultado/calendário e apresentação explícitas. GT4_CAREER_EVENT_RESULT_NEGATIVE altera contador e retorna 1. Créditos são pagos depois pelo menu de prêmio; ciclos Sunday Cup, 144 séries/527 etapas e 144 carros-prêmio têm integração C abaixo. Carreira contínua integral A. |
| Carreira: pagamento do prêmio pelos scripts de evento/campeonato | E | (c) gtmode_event_payment_oracle: callbacks originais onPrizeCheck/makePrizeFast nos dois projetos, 126 casos e 20370 incrementos por projeto contra 0043AFB0; saldo de 64 bits, overflow, marca de save e gravação/recarga nativa conferidos. menu_save_refresh restaura incMoney nesses projetos. GT4_CAREER_EVENT_PAYMENT_NEGATIVE altera saldo e retorna 1. Apresentação/conclusão da tela e corrida completa ficam fora desta prova. |
| Carreira: montagem de grid pelo disco | A | (c) `race_run_test`, RACE_EVENT=am_sunday_0001 e RACE_EVENT_GRID=1: r_mini, seis entradas, 25 candidatos e 600 frames, BEAT equipado preservado e adversários preparados construídos. Pinturas da lista e sorteio ponderado aplicados com estado RNG explícito; provas E separadas acima. Teste não prova o frontend nem a sessão completa. |
| Carreira: entrada de carro equipado na corrida | A | (c) `race_desc.owned_car` leva a folha ativa e o desgaste à montagem nativa; BEAT da captura purchase-slot06 percorreu 120 frames em Tsukuba, dois carros, peças/acertos/desgaste preservados mesmo com opção de pneu Arcade diferente; controle negativo retorna 1. Ainda falta comparar a sessão GT completa contra o original e ligar o evento ao aplicativo. |
| Carreira: ponte do grid preparado para a camada jogável/reinício/replay | A | (c) race_game_start_prepared recebe o descritor GT e usa as pinturas finais dos setups. Guarda cópias do carro equipado e linha RACE; race_game_restart e replay retêm os seis setups e RNG após liberar os buffers do chamador. pregrid_probe --career: 1201 ticks sem alterar os 32 MB, um passo da simulação, abertura de replay e reinício; --career-negative altera setup +0x19 e falha. Arcade também passou. Sem comparação da sessão inteira/largada GT contra o original; entrada pelo menu já conectada no item A da prova avulsa, progressão de todas as séries ainda pendente. |
| Carreira: inicialização e três primeiros ticks da contagem de largada | E | (c) gtmode_start_oracle: 2053 casos contra 003C0190/003C0208, cinco linhas RACE do disco e 2048 combinações forçadas de tipo/segundos/limite; bloco de sessão 0x1000 idêntico, incluindo registros, fila e contador. Só 003973D0 (luzes/apresentação) excluído. GT4_CAREER_START_NEGATIVE altera segundos e retorna 1. Não certifica seleção da largada lançada nem sessão GT completa. |
| Carreira: escolha comum do tipo de largada | E | (c) gtmode_start_select_oracle: 2048 estados contra 003883EC..00388438 (bloco completo 0x1000) e 131072 combinações contra 003475B8/0034C1B0; flags de sessão/pista e índice de grid, incluindo todos os bytes e sinais. Método Time Trial +0x50 contra 003F5528. Sem callbacks substituídos. GT4_START_SELECT_NEGATIVE e GT4_START_GETTER_NEGATIVE alteram saída e retornam 1. race_run usa a seleção comum original; prova limitada à regra/getters, sem certificar inicialização completa GT ou ciclo da carreira. |
| Carreira: passagem do evento e carro equipado do menu | C | (c) menu_get_career_request / gtmode_event_request: exit_menu e evento _start_page do script original distinguem solo_event/championship; label, rota e 0x4b8 bytes do carro equipado conferidos sem tolerância, incluindo atualização de desgaste +0x498, rejeição de carro ausente/modo Arcade e cópia após fechar a VM. Sunday Cup do pedido prepara seis entradas em r_mini com RNG explícito de teste. Fronteiras: finish do host e transição visual; seleção/requisitos da página não exercitados. GT4_CAREER_REQUEST_NEGATIVE altera carro +0x19 e retorna 1. Adaptador ligado a nat_finish/main.c para lançar provas avulsas e campeonatos; ciclo do campeonato tem registro separado. |
| Carreira: integração da prova avulsa A-spec | A | (c) main.c aceita solo_event, usa o carro equipado e grid preparado, abre quick-event; resultado vivo atualiza perfil antes do replay, getStartPage original escolhe prêmio/pós-corrida; pagamento e saída compartilham o save. gtmode_event_cycle_oracle passa 56 casos contra 00114498, pagamento autoral e reload, com chegada/posição injetadas; páginas AfterRoot/PrizeRoot executam 60 ticks sem erro. GT4_EVENT_CYCLE_NEGATIVE altera resultado e falha. Desgaste gravado após tentativa; metros A/B recebem checkpoint único antes de sair, repetir ou avançar etapa, preservando o pagamento entre VMs; as rotinas têm oráculos próprios. Ainda falta comparação de uma corrida completa, de todos os consumidores de RNG na sequência e da progressão final de todas as séries; entrega Sunday Cup tem prova C separada; metadados dry=1/tire=0 e apresentação permanecem aproximados. Não prova B-spec/endurance/licença/missão. |
| Carreira: aplicação do resultado ao perfil e persistência | C | (c) gtmode_event_cycle_oracle: 56 combinações de chegada/abandono, sete posições e quatro driver modes; record, melhor posição, prêmio acumulado e sete contadores sem tolerância contra 00114498; prêmio pago pelo script quick-event e créditos/prêmio acumulado preservados após save/load. Fronteiras: chegada/rank/lap/aspec/prize e lookup original do registro; calendário usa a rotina nativa já provada isoladamente, sua memória não é comparada neste teste. Negativo GT4_EVENT_CYCLE_NEGATIVE detectado. |
| Carreira: gravação do desgaste após tentativa | E | (c) gtmode_event_wear_oracle: 4096 casos contra 0010FA58, corpo completo de 0x4b8 bytes idêntico, incluindo setups preservados; getter do carro equipado é fronteira explícita. GT4_EVENT_WEAR_NEGATIVE altera +0x498 e falha. Aplicação ligada ao resultado vivo antes do replay; event_cycle confirma os 32 bytes após save/load em 56 casos. Não prova a atualização posterior dos metadados compactos da garagem. |
| Carreira: conversão e soma de distâncias A/B | E | (c) gtmode_event_wear_oracle: 4096 conversões de 0035DDB0/0010FA58, incluindo ramo unsigned e saturação de cvt.w.s; gtmode_event_distance_oracle: 4096 somas contra trecho 00114044..00114078, A em memória e B em v0 antes do store no delay slot; overflow uint64 idêntico. GT4_EVENT_DISTANCE_NEGATIVE altera um bit e falha. event_cycle confirma save/load. No aplicativo o checkpoint soma antes de sair/repetir/avançar, sem duplicar; a ordem completa contra PS2 ainda pertence ao item A. |
| Carreira: pontos, resultado e grande prêmio do campeonato | E | (c) gtmode_championship_oracle compara 8192 estados com 00119CB8/00119F28: seis totais, abandono, empate (não vence), classificação, registros da etapa/série, 112 bytes de estatísticas e argumentos do calendário. Pontos assinados de 006A6EF0; grande prêmio int16 de RACE+0x2c vezes 100. Fronteiras explícitas: getters de chegada/lap/prêmio/A-spec e apresentação. GT4_CHAMPIONSHIP_NEGATIVE altera estatística e falha. |
| Carreira: identificação e avanço das etapas do campeonato | E | (c) 4950 comparações em 99 tamanhos de série com 003EF0E8 e 003EF088/003EF388; sufixo NNRR, base NN00, primeira etapa NN01. GT4_CHAMPIONSHIP_LABEL_NEGATIVE altera label e falha. Construção da corrida fora desta prova. |
| Carreira: gravação do contexto suspenso do campeonato | E | (c) gtmode_championship_oracle compara 4096 blocos vivos de 0x90 bytes contra 00119BD8 e seus getters originais: flag/etapa/última etapa, label de 32 bytes, seis chaves de carro, pinturas assinadas e pontos. Limpar flag preserva todo o resto. GT4_CHAMPIONSHIP_CONTEXT_NEGATIVE altera flag e falha. Os 0x88 bytes serializados excluem os primeiros oito bytes da classe; integração da suspensão/retomada tem prova C separada. |
| Carreira: persistência e restauração do campeonato | C | (c) contexto original de 0x88 bytes serializados ligado ao save, isSuspended, suspended_racecode/session e abandonRace. gtmode_championship_cycle preserva contextos/pontos nos 34 campeonatos/200 etapas após reload e executa sureNoSuspended original até o pedido championship; GT4_CHAMPIONSHIP_RESUME_NEGATIVE altera pontos e falha. pregrid_probe --championship restaura uma etapa real pelos prepared keys/pinturas salvos: seis setups de 0x178 bytes e RNG conferidos sem tolerância contra o grid previamente preparado. GT4_CHAMPIONSHIP_RESTORE_NEGATIVE altera pintura e falha. Fronteiras: saída/comando do host e conclusão da corrida injetados; não é prova da sessão física inteira. |
| Carreira: operações da pilha de seleção e checkpoint | E | (c) gtmode_championship_oracle compara 8192 estados/retornos e 0x248 bytes contra 0042FB48, 0042FD00, 0042FD78, 0042FDF0, 0042FE88 e 0042FF48: strings terminadas por zero, limite length+strlen <255, vazios, pop/ref, consulta fora da pilha, negativos, preservação dos bytes não usados, checkpoint dos 256 bytes e load com saved_length negativo sem modificar o estado. GT4_MENU_STACK_NEGATIVE altera um byte e falha. Conversão dos argumentos em strings é uma fronteira explícita; ponte Adhoc tem prova E separada. |
| Carreira: tempos brutos do pós-corrida | E | `00114390` +0x454, car +0x5b6/+0x718: tempo da linha se terminou, marcador 0x157529ff se não; getter `00127DF8`. `gtmode_championship_oracle` executa ambas as rotinas: 4.096 listas de 1..6 entradas, tempos uint32 e índice do jogador bit a bit. `GT4_GRID_TIME_NEGATIVE` falha. Ordenação/catálogo são fronteiras explícitas neste oráculo. |
| Carreira: lista e tempos entregues ao pós-corrida | C | Implementado `MQuickWork.getGridTime`; eventos individuais agora recebem a ordem do resultado, campeonato mapeia driver ao slot inicial antes de mover carro/pintura. `pregrid_menu` executa páginas pré/pós de evento e campeonato com seis entradas, confere getter/tempo formatado/marcador/foco; `pregrid_probe --championship` confere sessão real com ordem invertida e HUD injetado (não corrida completa), tolerância zero. Controles `GT4_GRID_TIME_BRIDGE_NEGATIVE` e `GT4_GRID_TIME_RESULT_NEGATIVE` falham. Apresentação visual completa fora da prova. |
| Carreira/campeonato: retorno de `Option` à página correta | C | Corrigido retorno que forçava `RaceEventRoot`/`ChampionshipRoot`. `switch_to` usa `getStartPage` original ao retornar à carreira, preservando `AfterRoot` se terminado e foco `Option`. `pregrid_menu` executa botões/seletores originais em pré/pós dos dois projetos; tolerância zero em página/comando/foco; `GT4_CAREER_OPTION_RETURN_NEGATIVE` falha. Pós testado agora com seis entradas e `getGridTime`; formatação conferida em tempo válido e marcador. Comparação visual completa não coberta. Build Release passa; integração SDL completa ainda não provada por este teste. |
| Setting: tipos removíveis e tipos sem peça normal | E | `00140AE0 -> 0044A250` (13,18,19,20,22,23,27,28) e `00140BA8 -> 0044A288` (20,22). Métodos `isRemovableType` e `isRemovedNormalType` ligados aos carros; 8.192 tipos signed32, categorias válidas/inválidas e chamadas Adhoc comparados ao original. `GT4_PART_PREDICATE_NEGATIVE` altera retorno e falha. Não prova a ação de remover/instalar. |
| Setting: estágio instalado por categoria (`getEquipedParts`) | E | `00140530 -> 0043F4C8`, getters `004462A0..004468E0`, fallback `0044A1B8`. 8.192 casos contra rotina original e ponte Adhoc: 31 categorias, A/B/C válidos, chave ausente, byte signed da suspensão, TIRESIZE direto e fallback normal lido do CORE.GT4 (+0x6a7c08). `GT4_EQUIPPED_STAGE_NEGATIVE` falha. Bytes das linhas são fronteira explícita do oráculo; consulta real do disco tem prova separada C. Não cobre slots fora de 0..2 nem instalação. |
| Setting: consulta das linhas de peças do disco | C | `_getSettingPartRow`, `00443ED0/00443F00`: namespace alto32/id baixo32 signed. `pregrid_menu` compara sem tolerância 66 linhas reais (primeira/última de 33 tabelas, incluindo DEFAULT_PARTS) com bytes de `gt4_specdb_row_at`; testa id ausente +0x10000 sem truncar para 16 bits. `GT4_SETTING_DISC_NEGATIVE` falha. Leitor SpecDB é fronteira compartilhada desta prova; não constitui prova independente do descompressor. |
| Setting: remoção da chave na folha selecionada | E | `00446AE0`, jump table `006A6E60`: tipo reduzido a uint16, 30 offsets, somente chave uint64 recebe UINT64_MAX; demais bytes preservados. `gtmode_championship_oracle`: 4.096 corpos completos, três folhas e categorias válidas/inválidas comparados ao original. `GT4_REMOVE_PART_NEGATIVE` altera byte e falha (exit 1). Não prova instalação nem inicialização de ajustes. |
| Setting: defaults de instalação dos tipos 2, 3, 4, 5, 10, 14, 24 e 27 | E | `00440B50 -> 00440268/004404B8/00440820/004407B0/004403C0/00440430/004406C8/004408D8` e clamp `004452A8`: chave, defaults e buffer existente de 0x142 bytes; controlador de freios, suspensão, TCSC, ASCC, drivetrain tipo 10, LSD, direção e NOS. `gtmode_championship_oracle`: 4.096 corpos e buffers completos contra original, todas as folhas por categoria, linhas presentes/ausentes, bounds invertidos e ponte Adhoc. `GT4_INSTALL_DEFAULT_NEGATIVE` altera default e falha (exit 1). Linhas SpecDB são fronteira explícita com bytes sorteados; construção do buffer, lookup real e fluxo inteiro fora da nota. Buffer novo mantém máscara de validade: células não escritas não são defaults provados; linha ausente nessas células retorna erro. |
| Setting: instalação que grava somente chaves | E | `00440B50`, jump table `006A6890`: 17 categorias, onze na folha selecionada e seis nas três folhas; quatro categorias sem operação e namespaces inválidos preservam corpo. `gtmode_championship_oracle`: 8.192 corpos completos e chamadas Adhoc `changeParts` comparados ao original. Chave resolvida é entrada explícita; lookup, save e tela inteira fora da prova. `GT4_INSTALL_KEY_NEGATIVE` altera byte e falha (exit 1). Categorias que inicializam parâmetros continuam pendentes, com erro explícito sem mutação. |
| Setting: caminho de remoção de `changeParts` | C | `001403A0`: chave ausente, estágio zero e categoria removível acionam remoção; demais consultas ausentes preservam corpo. Ponte Adhoc comparada em 4.096 casos com a primitiva original e predicado já provado, tolerância zero no corpo completo. Lookup da chave é fronteira explícita do teste. Instalação por chave e defaults agora implementada/testada em setting_defaults_oracle/gtmode_setting_race; instaladores E e integração 722 carros C, combinações integrais A. |
| Setting: consulta de peça original ausente (`isDefaultNoParts`) | E | `0013F3A8 -> 004455B0`: GENERIC_CAR aponta DEFAULT_PARTS; chave/registro ausente retorna falso; tipo 20 testa muffler em DEFAULT_PARTS+0x98 contra UINT64_MAX. Oráculo compara 4.096 consultas e chamadas Adhoc com o original, três folhas, registros presentes/ausentes e tipos 20/22. Lookup/existência do registro são fronteiras explícitas. `GT4_DEFAULT_PART_NEGATIVE` inverte retorno e falha (exit 1). |
| Setting: consulta de peça original com disco real | C | `pregrid_menu`: 17 carros do catálogo, GENERIC_CAR e DEFAULT_PARTS reais da ISO, retorno de `isDefaultNoParts(20)` conferido sem tolerância com as linhas SpecDB. `GT4_DEFAULT_DISC_NEGATIVE` inverte retorno e falha (exit 1). Leitor SpecDB compartilhado pela referência; não prova independente da descompressão nem construção completa do cache. |
| Setting: existência de peça por carro/tipo/estágio | C | `001401A8 -> 0044A048`: método `isExistParts` usa CARS do carro e sentinel UINT64_MAX; TIRESIZE mantém chave especial. `pregrid_menu` compara 2.890 consultas em 17 carros do catálogo, tipos -1..32 e estágios -1..3, tolerância zero; lookup `gt4_career_part_key` já tem prova original separada. `GT4_PART_EXIST_NEGATIVE` falha. Esta ponte usa o lookup nativo também como referência; cache MCarData, listas, compra e instalação fora da nota. |
| Setting: reset do cache e campos auxiliares do wrapper | E | Construtor `00147D88` chama `00449D58` em wrapper+0x18 e `00146628`: chave -1, ponteiro/tamanho zero, seis palavras auxiliares +0x174..0x188 (somente +0x180 vale -1). `gt4_career_setting_wrapper_reset` compara 4.096 wrappers completos com ambas as rotinas originais; `GT4_SETTING_WRAPPER_NEGATIVE=1` altera +0x180 e falha (exit 1). Identidade do ponteiro de tipo é argumento explícito, não prova de identidade de endereços do host. Outros bytes são preservados, inclusive buffer das peças, que o construtor não inicializa. Wrapper ligado ao contexto Adhoc em gtmode_game.c; construção/folhas testadas por gtmode_setting_catalog_menu/gtmode_setting_race. beginSetting disponível, apresentação integral A. |
| Setting: preparação das 11 linhas de peças (`setupPartsBuffer`) | E | `0013FEA8 -> 00147D50 -> 004401C0`, getters `0043FE58..00440170`: selecionam 11 chaves da folha A/B/C e destinos no buffer. 4.096 casos em `gtmode_championship_oracle`, buffer completo comparado ao original e chamada Adhoc nativa; linhas ausentes preservam bytes e corpo do carro não muda. `GT4_SETTING_BUFFER_NEGATIVE=1` altera um byte e falha (exit 1). A obtenção das linhas é fronteira explícita, com bytes sintéticos; consulta do disco implementada, sem nota própria. A ponte aceita buffer novo com máscara de validade das células escritas; inicialização dos bytes que o original deixa indeterminados não integra esta prova. `beginSetting` integrado; gtmode_setting_catalog_menu e gtmode_setting_race provam catálogo/setup com negativos, apresentação integral A. NOS.dbt tem largura 8 e destino +0x13a, fim +0x142. |
| Setting: atualização das três regulagens ao sair (`endSetting`) | E | `0013FF38 -> 00441350`, `004462E0` e `00441300`: tipo da transmissão em setup+0x60 seleciona conjunto em live+0x470; grava live+0xf8 nas três folhas, passo 0x178. `gtmode_championship_oracle`: 4.096 corpos completos e chamadas Adhoc comparados ao original, todos os tipos byte, chave ausente, preservação e retorno nil. `GT4_END_SETTING_NEGATIVE=1` altera um byte e falha (exit 1). Consulta SpecDB é fronteira explícita: integração com disco implementada, ainda sem prova própria. `beginSetting`, demais setters e tela completa fora desta nota. |
| Setting: propriedade `slot` da regulagem A/B/C | E | `00140C70` lê live +0x490; `0043E2E8` grava uma palavra nesse offset (body serializado +0x488). Host do carro agora lê/grava o corpo, sem fabricar slot. `gtmode_championship_oracle`: 4.096 corpos aleatórios, setters inteiros/signed/long comparados com `0043E2E8`, getters comparados à palavra resultante e preservação integral dos demais bytes; três seleções verificadas em `gt4_career_active_setup`. `GT4_SETTING_SLOT_NEGATIVE` falha. Begin/endSetting, peças/regulagens e ciclo da tela não cobertos. |
| Setting: máscaras de opções indisponíveis e seleção | E | `002C9B68` aplica min(lista, itens) ignorando decoração (bit 13); se bloqueia índice atual, `002C9740(start=0)` encontra primeiro habilitado, mantendo índice se todos bloqueados. `ui_insensitive_oracle` executa as rotinas originais: 4.096 máscaras, comparação exata de flags e índice com host nativo. `GT4_OPTION_INSENSITIVE_NEGATIVE` falha. Traversal de containers e redraw são fronteiras; prova restrita a itens lógicos Composite/Button. |
| Setting: ponte `OptionMenu.setInsensitive(context, lista)` | C | Método disponível no host, antes ausente. `002C7A80` converte inteiros para booleanos; teste passa via chamada Adhoc com listas inteiras, inclui palavra baixa zero em long. Tolerância zero em máscara/índice/retorno nil. Wrapper completo de conversão de tipos do EE e efeitos de redraw fora da prova; floats/objetos não provados. Tela Setting disponível; testes de catálogo/referência/curva abaixo delimitam E/C, apresentação integral A. |
| Campeonato: tela `Setting` e opções de sessão | A | Rota ligada: o comando COMMAND_SETTINGS do menu rápido (`00118EF0`, despacho 15) abre o projeto `setting` e a volta aplica a regulagem escolhida à corrida (main.c: `menu_riding_car_body` -> `race_game_setting`, a corrida montada é refeita se ainda não terminou), voltando ao seletor do evento/campeonato. A tela em si está nas linhas "Setting: ..." (scripts originais, A/B/C, aplicar/cancelar/salvar, 722 carros). A: a ida pelo campeonato não foi refeita lado a lado com o PCSX2 (c) |
| Campeonato: conversão da extensão da pista | E | `00119AB0` (`cvt.w.s`) trunca e satura; corrigido `nearbyintf` para `gt4_career_course_length`. `gtmode_championship_oracle` executa `00119918` e getters `00395400`/`00396800`: 16.384 casos bit a bit, incluindo decimais, negativos, overflow, subnormais, NaN e infinitos. `GT4_COURSE_LENGTH_NEGATIVE` falha (exit 1). Rótulos/modo são fronteiras explícitas; comparação cobre quick +0x4f0/+0x4f4. |
| Campeonato: `Laps` e `CourseLength` no pré-corrida | C | Ponte publica voltas efetivas (RACE +0, eventos do disco +0x43) e extensão do runway +0x14, com conversão E acima. `pregrid_menu` confere campos em 3 projetos; `pregrid_probe --championship` passa nas 3 etapas, metadados conferidos nas 2 seguintes, tolerância zero. Controles `GT4_QUICK_METADATA_NEGATIVE` e `GT4_QUICK_GRID_METADATA_NEGATIVE` falham. Desenho e opções da sessão não estão cobertos. |
| Carreira: ponte Adhoc da pilha de seleção | E | (c) nos mesmos 8192 casos do gtmode_championship_oracle, gt4_game_menu_stack_action recebe strings/profundidades e compara com as funções originais o estado vivo de 0x248 bytes, checkpoint serializado de 260 bytes, texto/nil de pop/ref e booleano de load. GT4_MENU_STACK_BRIDGE_NEGATIVE altera buffer da ponte e falha. Conversão toString de objetos arbitrários e arquivo de save completo são fronteiras explícitas; a prova cobre as entradas textuais usadas pela pilha. |
| Carreira: pilha de seleção na retomada e save nativo | C | (c) lista aproximada substituída pelos buffers e contadores originais; menuPush/menuRef/menuPop/menuClearStack e saveMenuStack/loadMenuStack usam as rotinas com prova E. Checkpoint de 260 bytes em championship.menu_stack_ps2; cursor atual é transitório. gtmode_championship_cycle mantém seleção textual e códigos nos 34 campeonatos/200 etapas após save/reload, chama a confirmação sureNoSuspended original e prova pedido championship. Falha de carga sem checkpoint preserva a seleção. GT4_CHAMPIONSHIP_MENU_STACK_NEGATIVE altera texto e falha. Conversão de argumentos via VM e importação da pilha do save PS2 ainda não têm comparação neste teste; não prova apresentação ou corrida física inteira. |
| Carreira: ciclo completo de campeonato | A | (c) main.c lança quick-championship, mantém participantes/pinturas/preparação entre pistas, acumula pontos e grande prêmio, executa pagamento e próxima etapa. gtmode_championship_cycle passa os 34 campeonatos/200 etapas do disco com resultados injetados, pagamento pelos scripts, consumo dos prêmios sem reabrir pagamento e save/reload por etapa; GT4_CHAMPIONSHIP_CYCLE_NEGATIVE falha. pregrid_probe --championship carrega três etapas reais e confere grid, preparação e RNG preservados. Suspensão/retomada pelo contexto original ligada ao aplicativo, com provas separadas; pilha de seleção com operações e ponte E, persistência C; faltam opções de sessão e comparação de corrida completa; apresentação/metadados ainda têm aproximações. Não constitui E do ciclo completo. Usuário autorizou edição da física em 03/10/2026. |
| Carreira: retorno e entrega de carro-prêmio da Sunday Cup | C | (c) gtmode_award_delivery: todas as provas existentes da Sunday Cup resolvidas no catálogo do disco; resultado injetado passa por menu_apply_career_result, retorno parcial consome present_result e preserva spot_result; retorno final executa makeEntitledPresentListByProject e abre PresentRoot. get_car original entrega carro via construtor nativo, 0x4b8 bytes conferidos sem tolerância contra esse construtor (já com prova E própria), avanço de um dia, prize_cars=1, disponibilidade e pedido PCM get_car; save/load preserva tudo e nova consulta não entrega de novo. GT4_AWARD_DELIVERY_NEGATIVE altera desgaste e falha. Não é comparação de toda a entrega contra memória original; prova isolada dos 768 registros continua em gtmode_award_menu. Não cobre todas as séries, garagem cheia, escolhas de pintura/RNG ou apresentação. |
| Carreira: modelo 3D do troféu | A | quick-event.ad L893 (PrizeRoot) pede `piece/prize/` + 1st/2nd/3rd/other.mdl ao ModelFace `model`: o caminho do script passa por cima do `model_set_path` do mproject e, fora dos pacotes, é lido do volume (`/piece/prize/1st.mdl`, como os filmes do FlashFace). O `model_rotate_v` do mproject (0, 1,7143, 0) gira o troféu desde o começo; sem ele o modelo descansa, e os scripts do mapa seguem mandando no `target_mode`. Câmera, ângulo de visão e luz 0 do mproject pelo mesmo desenho dos ícones do mapa (agora em todos os projetos). Conferido abrindo a PrizeRoot: troféu de 1º no centro, girando; ícones do mapa do GT Mode iguais. A: as flags 0, 1, 0 do setModelSetPath, a luz 1 e o env_image_path não usados; sem print do PCSX2 lado a lado (c) |
| Carreira: listas de país, carros permitidos e `IsTestCar` | E | (c) `gtmode_admission_oracle` executa 001A1C60/001A2BB0/00448CD0 e 004489A0. Países: 256 seletores, valores e ordem exatos, inclusive Europa e array vazio. Listas: todas as 709 provas, 256 restritas, 2698 labels, nil/array e ordem; ponte MRaceData exercitada em cada uma. IsTestCar compara bit 5 de GENERIC_CAR +0x12 nos 722 carros e a ponte MCarData. Alocação/boxing/append da VM e IDI são fronteiras; leitura comprimida tem prova separada abaixo. Não certifica toda a admissão de cada evento. GT4_ADMISSION_NEGATIVE e GT4_ADMISSION_BINDING_NEGATIVE falham. |
| SpecDB: linhas largas de `ALLOW_ENTRY` | E | (c) removido limite host de 256 bytes, preservando capacidade e limites do arquivo. `gtmode_admission_oracle` executa inicialização 004490F8 e decodificador completo 004493F8/00449440/00449510/00449558/00449600 nas 58 linhas de 480 bytes do disco: todos os bytes e guardas conferem exatamente; buffer de 479 rejeitado. Só memcpy é fronteira. GT4_WIDE_ROW_NEGATIVE altera byte 479 e falha. Prova limitada a essa tabela/linhas válidas. |
| Menus: garagem viva ao fechar diálogo de compra | C | (c) `apply_pages` preserva main.game nas transições internas (0022F6A0); refresh continua na entrada entre projetos. Antes, BuyCarRoot L6246..6278 segurava referência da garagem invalidada pelo refresh ao fechar BuyCarDialog; GetCarRoot falhava na seleção. `gtmode_sunday_playable` executa compra autoral, confirma carro, créditos, count=1, riding_index=0 e Auto Save/reload com tolerância zero. GT4_MODAL_REFRESH_NEGATIVE reintroduz refresh no modal e reproduz falha de índice; GT4_SUNDAY_PLAYABLE_NEGATIVE altera débito e falha. Presenter headless explícito; não compara toda a VM com memória PS2. |
| Carreira: elegibilidade A-spec por licença | E | (c) `gtmode_admission_oracle`: 8192 casos (256 seletores RACE +0x45, 32 combinações de séries B/A/IB/IA/S) contra 00110498, sem diferença; 004312B0 é fronteira de série aprovada. Zero permite, 1..5 consultam série, demais rejeitam. GT4_ASPEC_ADMISSION_NEGATIVE altera elegibilidade e falha. Não prova novamente o consumidor dos dezesseis registros de licença. |
| Carreira: `IsASpec` e Start no executável | C | (c) `MQuickWork.IsASpec` publica elegibilidade 00114334..00114340/005C4610, consultando race_record.isLicensePassed do perfil. O campo ausente prendia quick-event.ad:311 no presenter modal. `gtmode_admission_oracle` confere ponte das 709 provas sem licenças; `run_sunday_entry.py --start` ativa botão original, chega a COMMAND_START e captura corrida com HUD/UI menu=0. --negative exige rota errada e falha. Sem comparação de toda a VM/fluxo com PS2; B-spec continua pendente. |
| Carreira: percurso jogável da Sunday Cup | A | (c) gtmode_sunday_playable --career-cycle compra Lancer usado por 9582 de 10000 créditos pelo estoque/script original e executa 13 corridas físicas, incluindo as cinco pistas da série, sem chegada/posição injetadas; takeover 003454A8 modo5 é fronteira de input. Uma vitória física, saldo4218 e metros46864/0. PrizeRoot original paga; corpo/desgaste, calendário/contadores/Home e save/VM reabertos por prova conferem. Grid de pagamento usa labels/pinturas/potência/peso reais. run_sunday_entry.py --start chega ao Start no aplicativo com cópia do save; origem intacta. Não houve vitória em todas as cinco provas nem entrega conjunta do carro-prêmio; entrega C separada usa resultados injetados. Ciclo integral contra PS2, RNG completo, UI/metadados e validação integral dos modos continuam A; faltas de código atuais separadas no inventário de 13/10. docs/CAREER_CYCLE.md. |
| Carreira: persistência de quilometragem, desgaste e primeiro desbloqueio | C | (c) --career-cycle: 13 corridas físicas, pagamento autoral, checkpoint idempotente entre VMs, corpo/setup/desgaste completos, calendário, contadores, Home, save/load e VM reabertos por prova. Primeiro curso de PresentTimeData (Deep Forest) bloqueado inicialmente e disponível após 13 provas. Campos comparados sem tolerância; não compara sequência completa contra PS2. GT4_CAREER_MILEAGE_NEGATIVE omite primeira gravação e falha. Fontes 00114044..78/0011975C..80 e scripts originais de presentes. Main recebe desgaste vivo ao repetir; rotas reais retry/next permanecem sem teste completo. |
| Carreira: odômetro e venda com desgaste vivo | C | (c) gtmode_event_wear_oracle amplia os 4096 estados originais: getOdometer/getUsedSellPrice usam corpo+498 apesar do cache compacto antigo, sem tolerância nos retornos inteiros. 00146528 +1C..24 ->003434C8 e 00441128/004411F0. GT4_LIVE_MILEAGE_NEGATIVE altera preço e falha. Fórmula de preço tem prova original própria; extensão é ponte, não novo oráculo da UI. |
| Carreira: janela de pista liberada e retorno de páginas em cache | C | (c) GetCourseDialog.onLoad L15577..80 prepara atores usados por open_motion L15585..92 antes de pushPage L15593. gtmode_course_dialog abre, confirma, fecha e volta à Home sem erros; remover ator com GT4_COURSE_DIALOG_NEGATIVE falha. --career-cycle executa entrega por data na TopRoot, com persistência. A: fase integral de loading do projeto, timing/render e importação GT3/GT4p não certificados. Reinicialização das páginas em cache segue novo projeto GTmodeProject.onLoad L141..174. |
| Carreira: entrega nas 144 séries do disco | C | (c) 09/10: `gtmode_series_awards ISO` executa PresentData/PresentRoot originais em 144 séries, 527 etapas, 33 campeonatos e 16 endurance; compara contagem com extractSubRaceCode. Bloqueio antes de completar, get_car automático, corpo de 0x4b8 do prêmio contra construtor nativo, calendário/estatísticas, consumo único e save/load sem tolerância. Pedido do filme final conferido, decodificação é fronteira. Chegadas/vitórias são injetadas aqui; não certifica todas as etapas físicas. GT4_SERIES_AWARDS_NEGATIVE substitui vitórias da primeira série por segundos lugares e reprova. docs/CAREER_CYCLE.md. |
| Carreira: bônus final em segunda abertura de PrizeRoot | C | (c) `0022F6A0 +104..184` permite startPage na mesma página; quick-championship.ad L1044..1045 exige reinicialização para o bônus. `gtmode_championship_bonus ISO` confere prêmio 10000 mais bônus 20000, AfterRoot, consumo único e save/load sem tolerância; resultado injetado. GT4_CHAMPIONSHIP_BONUS_NEGATIVE altera um bit do valor da segunda abertura e reprova. Corrigido adaptador que pulava essa abertura. |
| Carreira: carro-prêmio após série física completa | C | (c) 144 séries/527 etapas físicas/144 entregas: 43.537.320 frames, 654 pits. `run_series_suite.py` + `series_receipts.py` verificam chegada/rank nativos, título autoral, entrega única, corpo 0x4b8 contra construtor, pagamento/desgaste/save/load e setup 0x178 sem tolerância. Campeonatos permitem posições menores; eventos independentes exigem todas as vitórias. Quatro receitas de inputs em series_victory_inputs.json; peças/posse por APIs originais são fixtures, perfis independentes, não compra/car carreira contínua. Negativos omitem writeback, removem série/candidato ou corrompem recibo e reprovam. Prova: build/career-finish/receipts-144.json, escopo e reprodução em CAREER_CYCLE.md. Trajetória PS2 e apresentação integral excluídas. |
| Endurance: consultas virtuais no adaptador | E | (c) autorização limitada a integração comprovada: 0035AF10 +0..4 retorna zero; 0038B980 +0..C lê objeto+E408. `gtmode_endurance_queries CORE` compara 8192 retornos inteiros de 32 bits com funções originais recompiladas, palavra lida preservada e função desconhecida ainda rejeitada. GT4_ENDURANCE_QUERY_NEGATIVE altera retorno e reprova. Nenhuma fórmula ou parâmetro alterado. Não estende E à corrida completa. |
| Endurance: catálogo, admissão e largadas | C | (c) `gtmode_endurance_catalog ISO`: os 16 eventos de PresentData, oito por tempo e oito por voltas, carro stock original do grid legal pela getViolatedRegulatoin autoral; limites RACE versus sessão e 600 frames de cada largada, sem tolerância nos campos. GT4_ENDURANCE_CATALOG_NEGATIVE altera minutos e reprova. Duração completa não coberta por este teste. |
| Endurance: término físico, pit e persistência | C | (c) `run_endurance_suite.py ISO --jobs 4`: os 16 eventos completos, 26560637 frames e 164 pits; inclui oito por voltas e oito por tempo (99h45 nos limites), três de 24h. Limites, chegada nativa, pagamento autoral, desgaste e save/load sem tolerância nos campos. Le Mans antiga usa Minolta stock como fixture legal: 5202529 frames, sexto lugar, 80000 e 47 pits. Os demais usam carros stock do catálogo. GT4_ENDURANCE_RULE_NEGATIVE altera limite; GT4_ENDURANCE_SUITE_NEGATIVE remove registro; ambos reprovam. Logs/results.json em build/career-series/endurance-suite; reunião final reutiliza runs aprovadas da mesma build/disco. Takeover/pit são entradas explícitas do teste; vitória, prêmio-carro e trajetória/apresentação PS2 completas não certificados. |
| Endurance: run automática do GT40 em Le Mans antiga | A | (c) fixture stock parou na volta 155. Render da RAM nativa mostra a frente na barreira de pneus, fora do asfalto; captura ainda tem combustível, pit service inativo e modelo acelerando. Comparação isolada 00465078 e 600 steps 0034DB30 contra original passam, mas não provam como a trajetória inteira chegou ali nem certificam correção dessa run. O bloqueio CreateThread pertencia à câmera: teste usa o gate original de apresentação D_006208B0, sem ampliar HLE. Falha da run nativa preservada nos logs locais. Solver/parâmetros intactos. |
| Endurance: continuação do step a partir da captura presa | E | (c) `endurance_stall_step.py physics_run RAM OUT`: 600 chamadas 0034DB30, seis carros, 15347136 bytes (601 estados completos de 0x10a0 por carro) idênticos ao original recompilado. Odômetro sem progresso em ambos. Única alteração da fixture: D_006208B0=1, gate de câmera 0035F658 +0..14; caminho nativo usa a fronteira headless existente. Captura inicial nativa, apresentação/threads e trajetória anterior excluídas da prova. GT4_ENDURANCE_STALL_NEGATIVE altera posição no frame 1 e reprova. Nenhuma mudança em arquivos da física. |
| Carreira: inscrição de conversíveis (`isOpenCar`) | E | (c) faltava a ligação chamada por regulation.ad:328. Registro 00141B90 +0x1D14 ->0013F040 +30..64, setup ativo 00441248/004454C0, row 00448820, bit 4 de GENERIC_CAR +0x12 via 004489A0 +0..C. `gtmode_admission_oracle CORE ISO` confere retorno da API registrada nos 722 carros, três setups/chaves ativas e 2558 ENEMY_CARS (4724 comparações), inclusive chave diferente do cache spec_id. Alocação/boxing e lookup comprimido são fronteiras; não prova toda a VM. GT4_OPEN_CAR_NEGATIVE e GT4_OPEN_TUNED_NEGATIVE alteram retornos e reprovam. Spider & Roadster vence três provas físicas com Viper SRT10 e entrega Prowler. |
| Carreira: consultas de aspiração na inscrição e garagem | E | (c) `gtmode_aspiration_oracle CORE ISO`: 34656 retornos de isNAtune/isSuperCharger/isTurbo/getAspiration, 722 carros, três setups e quatro combinações stock/turbo/supercharger/peça removida, idênticos ao original. Registro 00141B90 +2004/+20AC ->0013F598/0013F668; isTurbo 00143AF0 ->0013F488; getAspiration 001467A0. TURBINEKIT usa setup+B0, não NATUNE+A8; byte9 por00445650. GENERIC_CAR+17 e supercharger type28 stage1 por0013F528. Boxing/lookup são fronteiras; corpo preservado. GT4_ASPIRATION_NEGATIVE inverte retorno e reprova. |
| Endurance: vitória física e carro-prêmio em todos os 16 eventos | C | (c) 16 vitórias/16 carros-prêmio, 25.935.205 frames/489 pits; três eventos de 24h incluídos na prova das 144 séries. Chegada/rank nativos, prêmio 0x4b8 contra construtor, pagamento/desgaste/save/load sem tolerância. Pad e pit por combustível/desgaste são inputs explícitos; carros/posse de peças são fixtures legais, perfis independentes. Negativos de writeback, catálogo e recibo reprovam. Prova: receipts-144.json; trajetória/apresentação PS2 integral fora deste escopo. |
| Adhoc: Integer comparado com String em restrição de comprimento | E | (c) `gtmode_integer_string_oracle CORE`: 336 resultados dos operadores <,>,<=,>=, incluindo limite3400, sinais, sufixos, texto inválido/vazio e extremos de32bits, idênticos ao original. Callbacks002FD0B0/002FCF80/002FD148/002FD018 chamam RHS vtable+58; String00314A40 ->005A2F88/005A7170 converte decimal e trunca a32bits. Car.getLength0013E370 retorna String; a VM rejeitava a comparação com limite inteiro e liberava carros longos. Escopo: Integer à esquerda, String à direita, operadores relacionais; demais coerções não certificadas. GT4_INTEGER_STRING_NEGATIVE inverte resultado e reprova. |
| Endurance: trajetória do GT40 até a barreira | E | (c) endurance_trajectory.py: checkpoint nativo frame 3.600, 2.460.000 frames, seis carros; SHA256 de cada estado completo idêntico no original recompilado, step nativo e gt4_race_run_frame de produção (62.818.585.536 bytes brutos/caminho, 88.560.036 bytes compactos/caminho). 148.292,846 m; último avanço frame 489.142 da janela. O original também prende com os mesmos inputs automáticos de teste. Outra tentativa, checkpoint 2.440.800: 15.000 frames/383.065.536 bytes brutos idênticos; 6.464,5 m, último avanço 5.744. Capturas distintas; primeiros 3.600 frames e corrida integral de 24h excluídos. Gate D_006208B0=1 exclui câmera 0035F658 +0..14. Negativo de posição reprova; fingerprints conferem com hashlib e negativo. Física intacta; cobre car states, não EE inteira. CAREER_CYCLE.md e trajectory-full-digest.log. |
| Controlador: seleção e comandos de ritmo como inputs | E | (c) physics_controller_event: 256 valores de seleção contra 00362E00 (car+104+466; limpa +467), 2.000 comandos dos cinco níveis de ritmo e seleção/evento contra rotinas originais, memória EE comparada. CTest inclui 256 seleções e 200 eventos; GT4_CONTROLLER_PACE_NEGATIVE altera car+14+70 e reprova. Só a fronteira de comando/seleção; não certifica estratégia completa de B-Spec ou controle SDL. |
| Regras de entrada, tuning e efeitos | C | Resumo: requisito de licença do evento (00110498), regras de carro do Arcade (IsDirtRunnable e lista de adversários), instalação de peças e regulagens (linhas "Tuning: ..." e "Setting: ...", várias E contra o original) e efeito das peças na física (potência, peso, pneus, câmbio; Tune Shop V com o print do usuário). Ver as linhas detalhadas (d) |
| Saves | C | Resumo: save nativo em arquivo (sem memory card, P), compra/venda/carreira/licenças salvos e recarregados idênticos (linhas "Carreira: ..." e "Licenças: ranking, calendário e save nativos"), pilha de menus salva, preferências de música e opções. Ver as linhas detalhadas (d) |
| Opções novas: ASM e TCS ligados; auxílios nas licenças | A | 10/10, do código: o construtor da GranTurismo4Option (004363A0) grava assist_asm (+0x10D0) e assist_tcs (+0x10D4) = 1, offsets lidos dos acessores 00182628/00182720; também split_boost_level 1 e a apresentação de slides 1/8/4. Nas licenças, RaceLicense (0011CB38) nunca grava os auxílios do aluno (ficam os 1, 1 da entrada, 003B4B38) e RaceTraining grava 1, 1 (0010F880 -> 003C0D78/003C0D98); o nosso usava as opções (ASM desligado). Testes: menu_navigation e gtmode_license_disc/cycle/drive passam. demo_movie_interval agora usa o 3 original com corridas nativas; um save antigo do port guarda os valores velhos |
| Título da música na corrida: só faixas da playlist, uma vez por faixa | A | 10/10, do código: 0038AB98 entrega a cada quadro ao painel (003A22A0) o registro da faixa do gerenciador de música (0033C450 -> 0033D950, +0x18); 0033D568 só copia esse registro quando começa uma música de corrida (enum 12, 00438170 -> 00462288), e os jingles (largada, chegada, licença) não o tocam. O nosso mostrava o título de qualquer áudio. Conferido com MOON_SOUND_TRACE em Tsukuba: título só na faixa de corrida, nada no jingle de chegada. Pendente: comparação lado a lado com o PCSX2 |
| B-Spec: corrida com o piloto no carro do jogador e as ordens de ritmo e ultrapassar | E | 10/10, savestates B-Spec 05..08 do usuário (Tsukuba, Honda BEAT): o mundo da corrida tem +0xCC0..+0xCCC = 1 (0 em A-Spec: ar_02/03 e licenças), e 003F1760 dá ao carro do jogador o tipo 1 (car+0x56A), dirigido pelo próprio piloto. O botão Spectator do menu da prova é COMMAND_SPECTATOR (14 de D_006187B0, quick-event L363); o Start volta ao A-Spec, e a corrida é reconstruída quando muda. Ordens pelo bloco de eventos (cond+0xF8F8 + carro*4), que 00426D50 copia para o comando (+0x10, ev[3] bytes) e 00349FD0 entrega a 003F19E0: ritmo 4 (+0x6D 0..4; slot 05 2 Steady, slot 07 4 Push com +0x70/+0x74 0x3F8CCCCD), ultrapassar 5 (+0x6E, slot 08), box 6. A tabela D_006220B0/C8/E0 (003F1790) passa a ser arredondada para o mais próximo, como o div.s do PS2 (1,1, 0,8 e 1,2 iguais ao savestate). bspec_tests: o carro anda sozinho com o controle parado, ritmo e ultrapassar chegam ao piloto, a quilometragem B-Spec (+0x7B0) acompanha a total; negativo A-Spec: tipo 0, parado. Controles: setas esquerda/direita ritmo, Triângulo (O) ultrapassar, Círculo (P) box (A: o tratador original da tela não lido, nem se uma ordem expira). Pendente: Race Monitor e painel de ritmo (racemonitor.strb), velocidade do tempo, menu de box (pitmenu.strb) |
| B-Spec: Race Monitor e vista ao vivo (racemonitor.strb) | A | 10/10, do código: o monitor do mundo (+0xE424; 003D8DB8 quadro, 003DA150 desenho e troca de modo) dirige /menu/common/racemonitor.strb pelos nomes das instâncias. Modos +0x28 (slots 05/07/08: 2 ao vivo, quadro 1; slot 06: 1 monitor, quadro 0; 0 desligado), troca com desbotamento de 0,2 s. Painel de ritmo 003DB520 (PACE1..5 o ritmo, PACE6 ultrapassar, PACE7 box, PACETEXT key_pace1..5/key_overtake/key_pitin), classificação 003DAF60/003DA7C8 (BARn na ordem da corrida: GRID, ICON de estado 003DA6F8, NAME cortado com "..." 0048F790, TIME a última volta, DIFF +0x11E8/+0x11EC, BEST, LAP, PIT, BESTMARK; o carro olhado aceso, CURRENTCAR, CURRENTCARGRID, TOTAL 003B00A8 sem milésimos, LAP RaceMonitorLapFormat1/2), fileira de distâncias 003DBF90 (largura 62 + 38 x lim((d-20)/80), centrada; GAPn em ft/m ou voltas; PACEUP_MARK 003FA508 pelos tempos de setor; DISTUNIT "Gap (%s)"), velocidade "x %d" piscando com L1 (003DBBD0), mapa ligado por X, painel de box 003DB7B8 (paradas planejadas, TireShort_nn). Botões como os tratadores 003D9520/9720/95F0/97A8/9918/9838 (R1, R2, setas, L1+setas x1..x3, Triângulo, Círculo, cima/baixo o carro olhado). Ordens por 003F1BD8 portado (gt4_bspec_order: tipos 4..9, recusas da sessão e do box). Player de Strobe: degradês (tipos 1/2 da tabela +0x78, 12 bytes, paradas razão+cor), _x do host, texto cortado. Lado a lado com os prints do usuário no PCSX2: estrutura, textos e fileira iguais. A: o mapeamento físico dos botões, câmera da vista ao vivo por +0x94 (2 TV, 1 atrás, 0 no carro), a velocidade aplicada como passos por quadro, a linha de ajuda no estilo da pausa do replay, a fonte e a posição das letras do mapa, os sons. Depois (11/10): painel de pneus (TIRE_PANEL/"tire"), HUD do B-Spec na vista ao vivo, ícone do carro em perspectiva (00393790 modo 0, ângulo ajustado ao print: A), faixa de cor de cada carro ("chip" de 003D7930: o chip de pintura 00445AE8, VARIATION +0x24, desenhado como 0042E160: uma cor cheia, mais em faixas 0/1/2, 3/4, 7/8 ou de baixo para cima pelo bit 27, degradê pelo bit 26) e coluna de setores ("sector", 003DC7B0: por setor do circuito, tempo do carro em cond+0xD7A4 + (carro*16+setor)*4; sem tempo cinza 0x282828/0x181818 alternado; com tempo pela fração de carros mais rápidos, vermelho o mais rápido até azul-escuro; marca 0xC8C8C8 no quinto de baixo do setor atual, car+0x5B2). Pendente: comparação V com o PCSX2 |
| Menu de box (pitmenu.strb), A-Spec e B-Spec | A | (c) 11/10: fluxo 0038AB98/0035BC70/BCE8/BCC8/BF08 existente. GT Mode agora filtra compostos por propriedade (004078D0/0043F3A8, 212.992 consultas bit a bit em pit_controller); zero ownership mantém filtro real, distinto do catálogo Arcade. Callbacks Strobe já existentes; pneus/combustível indisponíveis desativam atores como 003D3080/30D0/3120. Gate de troca A/B-Spec vem da licença (00110498 -> 00114250+F4/00119918+174), compartilhado com IsASpec da UI, default options+970=1 (003BFB90+50); sessão capturada continua lendo 003C12A8. Modo nativo world+CC8 atualizado após evento 10. gtmode_open_car e dois negativos passam; gate/ownership isolados E, integração e UI ainda A. Posse de input do monitor integrada e C em prova separada acima; comparação integral PCSX2 pendente. |

| Regulamentos: textos Sunday Cup/FF Challenge | C | (c) 11/10: gtmode_regulation_dialog executa EventInfoDialog L9758..9927 e compara titulos, quatro/cinco pares de labels/descricoes do RT03 US, incluindo Required License e FF cars only. Ka/FF e BEAT/MR exercitam texto normal e textv de infracao; tolerancia zero nas strings. GT4_REGULATION_TEXT_NEGATIVE substitui Single Race por Championship e reprova. 2/2 CTest passam. Nao certifica glifos, quebra/alinhamento, opacidade ou divisorias. |
| Regulamentos: divisorias e apresentacao do popup | A | (c) usuario apontou ausencia de divisorias em 11/10. Layout atual: Sunday Cup, quatro linhas de 55; FF, cinco de 48; fundos contiguos. MBox 002B82E8 inicializa espacamentos em zero; mproject e adc comuns nao declaram espacamento adicional. ColorFace 002854F8 usa vertices 0..clientWidth/Height. Ainda sem causa original comprovada; nenhuma divisoria ou texto foi ajustado no olho. Proximo: RAM do popup original, quando usuario puder; PCSX2 atual pausado fora desse popup, MBoxes encontrados sem linhas. |

#### Missões: resultado e retorno

| Item | Nota | Prova ou observação |
|---|---|---|
| Missões: decisão de resultado e falha | E | (c) `gtmode_mission_flow`: 4096 filas/estados contra 003E05A0 e 00389638. Evento 5 de entrada 0 abre ResultArcade; rivais sozinhos não abrem. Evento 10 chama world+0x280 (003ED188), inclusive com prioridade no mesmo frame. 003DFE20 +0x28 habilita saída sem esperar os rivais. Memória/decisão exatas; render, alocação, texto de linhas e execução do fade são fronteiras. GT4_MISSION_FLOW_NEGATIVE troca decisão e falha. Nenhuma fórmula/parâmetro da física alterado. |
| Missões: vitória, prêmio, registro, save e retorno nativos | C | (c) `gtmode_mission_cycle`: missão 9 vencida em primeiro com 1280 frames de inputs, prêmio 1000, scripts originais PrizeRoot/AfterRoot, recorde e dinheiro idênticos após save/reload (tolerância zero). Missão 10 perde em quarto, sem prêmio nem painel sobre a corrida, retornando a AfterRoot. `mission_app.py` confirma ambos no executável real com fita de controle, sem injetar resultado/estado físico. Piloto de teste P, apenas escolhe inputs. GT4_MISSION_CYCLE_NEGATIVE muda a classificação salva; --negative exige página inexistente: ambos falham. Prova restrita a esses ciclos; 14/10: ciclos físicos também das missões 1, 4, 8, 21, 22 e 24; ver auditoria contínua abaixo. Outras 27 vitórias completas pendentes. |
| Missões: descrições multiline do Hall e pré-corrida | C | (c) `gtmode_mission_cycle` desenha os projetos originais e mede 68 descrições (34 Hall, 34 pré-corrida) com fontes do disco: largura não excede o TextFace authored, tolerância 0,001 unidade. Exercita a quebra multiline existente no renderer, sem substituição dos textos. GT4_MISSION_TEXT_NEGATIVE excede a largura e falha. Não certifica composição/pixels completos contra PS2. |
| Missões: apresentação integral do resultado e saída | A | (c) 003BF6E8 +0x80..0xCC constrói ResultArcade, não uma classe ResultMission; 003DF898 -> 003EC2B8 escolhe `/menu/US/result.strb`. O port usa esse movie, clock 003DF978 +0xB0 a 60 Hz, e retorna a QuickWork (0011BB60) sem replay automático. Capturas nativas de PrizeRoot/AfterRoot locais; câmera, composição, áudio e animação integral ainda sem comparação PCSX2. Usuário não consegue fornecer estados de resultado/falha agora (11/10); regra 5 impede ajustar no olho. A alegação anterior de fade ausente na falha foi corrigida pelo oráculo de encerramento abaixo: o original usa duração zero nesse caminho. |
| Missões: timing do encerramento por falha | E | (c) `gtmode_mission_exit`: 2048 casos contra 003ED188/0033CCF0/0033C490/00389E10/0038AAB0/00389ED8/0038A098. Pedido arma world+D80; tick seguinte encerra antes do avanço da corrida, world+D18=1. Caminho normal e especial/demo com duração zero deixam clocks de fade +CF48..54 inalterados. Cleanup/scheduler são fronteiras; ordem original rastreada, sem certificar mixer/render. GT4_MISSION_EXIT_NEGATIVE encerra cedo e falha. Ponte C: exe missão 10 pede saída e para no frame 9150, sem outro passo físico; `mission_app.py --negative-step` soma um passo no log e falha. |
| Missões: catálogo completo de resultados e persistência | C | (c) `gtmode_mission_catalog`: 136 ciclos nas 34 missões (abortar/perder/vencer/perder depois de vencer), com resultados forçados somente na fronteira corrida/menu, explicitamente sem afirmar vitórias físicas. Scripts originais PrizeRoot/AfterRoot, Cr. 272500 pagos, classificação melhor preservada e save/reload a cada ciclo, tolerância zero. Contador acelerado pelo botão original (prize_action.ad L27..35/quick-mission.ad L562). GT4_MISSION_CATALOG_NEGATIVE muda a classificação esperada e falha. Não certifica pixels, carros-prêmio entregues nem 34 corridas completas. |




### 4.4 Modos e recursos do jogo (inventário completo)

Lista tirada dos projetos de menu do próprio disco (`/projects/*`), para
nenhum modo ficar esquecido. Cada linha vira vários itens quando a área for
detalhada. Revisado em 13/10/2026: disponibilidade funcional e prova integral são separadas; A não significa ausência de todo o modo.

| Modo ou recurso | Projeto no disco | Nota | Observação |
|---|---|---|---|
| Abertura, avisos, vídeo de introdução | `boot`, `demo_movie` | V | (d) |
| Arcade: corrida simples | `arcade`, `quick-arcade` | A | Jogável, incluindo garagem, adversários/terra-neve e retorno. split_owned, pregrid_menu, race_resources e testes de transferência/configuração têm provas delimitadas E/C. UI, áudio e render completos contra PS2 ainda A. |
| Arcade: Time Trial, ghost, recordes | `quick-tt`, `ranking` | A | Ghost PC, ranking e Logger implementados; tt_game_probe --saved e course_ranking com negativos. Ghost de melhor volta PS2 ainda F; prova de todos os ramos e apresentação A. |
| Arcade: 2 jogadores (tela dividida) | `arcade` (SplitRoot) | A | Jogável com dois controles ou teclado; regras e dados do HUD E, apresentação integral A; espelhos implementados (dados E, imagem A) e replay 2P C nativo. Atalhos/arquivo PC P. Ver Battle2P em 4.2. |
| Treino livre / Free Run | `quick-freerun`, `quick-practice` | A | 12/10: CourseEntryRoot FreeRun/Practice (`exit_menu("freerun"/"practice")`, 00117668/00118670) levam o carro em uso (garage.riding_car, corpo 0x4b8 como está) à pista escolhida (game.course_code) e abrem FreeRunProject::FreeRunRoot / PracticeProject::PracticeRoot; Exit volta ao CourseEntryRoot pelo major_menu_* copiado ao projeto. Classes originais: Free Run em RaceFreeRun (003B9A18, tabela 00680140, modo 11) com DynamicsConductorFreeRun (003F54E0, 00685890), as mesmas que o Time Trial do Arcade usa (RTTI 11RaceFreeRun/24DynamicsConductorFreeRun); Practice em RaceFreePractice (003B92D0, 0067FC70, modo 12) com DynamicsConductorFreePractice (003F4EB8, 006856E0), métodos próprios portados em session.c: largada 003F4F10 (jogador no box, 00345400, os outros 300 m atrás e 200 m entre si a 120), sem voltas (003F5008), ordem pela melhor volta (003F5270/003F5350), volta/split 003F5150/003F5028 com mensagens 2/3 e diferença ao melhor da corrida, after-cars 003F4F60 (melhor volta da inscrição +0x1198, depois do reset de recordes 003C0108 que passou a vir antes da sessão, como na sequência 00118EF0). Adversários do script (PickUpEnemyCars, setEntryCarCode). Replay salvo guarda o modo (campo WMOD, P). Teste gtmode_practice_session (6 carros em Tsukuba: tabelas, 182 quadros no box, saída pela pista dos boxes, quatro voltas sem fim de corrida, ordem pela melhor volta) e negativo com a sessão do Arcade reprova; replay_file confere o campo. Conferido no jogo: carro no box com a equipe, Free Run e Practice com Start e Exit. A: sem comparação com o PCSX2; outras funções do mundo RaceFreePractice que o motor não chama não foram lidas. V pendente |
| GT Mode: Home, garagem, status, calendário | `gtmode` | A | Implementados em career_home.c/gtmode_game.c e scripts originais. gtmode_home, gtmode_home_progress, gtmode_status_start, profile_storage e negativos; getters E, integração C delimitada. Evolução B-Spec F, carreira contínua completa e apresentação A. |
| GT Mode: concessionárias, usados, tuning, GT Auto | `gtmode` | A | Compra/equipamento, estoque e serviços implementados. gtmode_purchase_menu, shop-catalog, gtmode_setting_race (722 carros), fitted_parts e provas GT Auto E/C; apresentação/sincronismo e todas as combinações integrais A. |
| GT Mode: corridas de evento | `event`, `quick-event` | A | Regras E, integração C: 144 séries/527 etapas terminadas fisicamente, títulos e 144 entregas de prêmio; perfis independentes e inputs explícitos, sem injetar resultados. Apresentação/fluxo PS2 integral ainda A (c) |
| GT Mode: campeonatos | `quick-championship` | A | Etapas, pontos, prêmio e suspensão/retomada ligados; regras E e integração C em 4.3. Tuning Car GP completo ganho fisicamente, bônus e carro-prêmio conferidos; demais campeonatos dirigidos e apresentação integral PS2 pendentes (c) |
| GT Mode: endurance | `quick-event` | A | Todos os 16 eventos vencidos fisicamente e carros-prêmio entregues C; boxes/pagamento/desgaste/save, integração E, física intacta. GT40 investigado em janelas E, incluindo 2,46 milhões de frames; apresentação e trajetória integral de cada corrida fora da prova (c) |
| Licenças | `quick-license` | A | 80 exames carregam; regras E, ponte/save/prêmios C. Demonstração, ghost, Logger e controles de resultado implementados; license_load, license_menu_demo/disc_demo_playback, gtmode_logger_drive, license_result_input e negativos. Sincronismo de todas as trajetórias, medalhas alcançáveis e apresentação completos ainda A. |
| Missões de pilotagem | `quick-mission` | A | as 34 montam; sessão 00685DA0, decisão de resultado/falha e timing da saída E. Resultado localizado do original integrado; vitórias físicas das missões 1, 4, 8, 9, 21, 22 e 24 e derrota da 10 C, 68 descrições com quebra conferidas. Catálogo de 136 resultados controlados/prêmios/save nas 34 C, distinto de vitórias físicas. Apresentação integral PS2 e outras 27 vitórias físicas completas pendentes. Ver linhas "Missões: ..." e `docs/MISSIONS.md`. |
| **B-Spec** (dirigir como chefe de equipe, aceleração do tempo, ordens ao piloto, monitor de corrida) | `quick-*`, `gtmode` | A | piloto e ordens em E; monitor, vista ao vivo, faixa de cor, setores e menu de box em A (ver 4.3); falta o fluxo completo do GT Mode conferido e a comparação V |
| Ajustes do carro (setting) | `setting` | A | Instaladores/defaults E; gtmode_setting_race confere 722 carros, 150054 instalações e 2166 entradas A/B/C, save e setup C, com negativo. Curvas e UI têm provas específicas; todas as combinações simultâneas/trajetórias e imagem integral PS2 A. |
| Power & Speed / pista de testes | `gtmode`, `quick-mt` | A | 12/10: sequência machinetest e world mode15 integrados. machine_test_tests termina fisicamente 400 m/1000 m/6845 m, marcas/splits e velocidade máxima; negativo usa sessão Arcade e falha. machine_flow inicia pelo Start original do quick-mt, termina 400 m sem resultado injetado, confere resultado, reproduz fita inteira e retorna ao menu; negativo reintroduz tempo zero. Integração C nos casos testados; sessão/pixels/tempos integrais PS2 A, sem comparação EE ponta a ponta. |
| Power & Speed: resultado e Session Record | `quick-mt` | C | 12/10: ResultArcade modo5 lê entry+1B0 (003DFFF8), modo4 lê car+6CC e formata velocidade (003E0490). menu_enter_quick_start reinicializa quick-mt no retorno, como 00115A90 +380/+3A4 -> 0010FE40. pregrid_menu confere Replay/Save, recorde de tempo, velocidade em km/h/mph e ausência de recorde; negativo conserva inicialização antiga e falha. MUnit 001AF3B0/001AF480 e %.2t de 00490A40 entregam centésimos ao script. machine_flow confere 0'23.317 na chegada e na volta após replay completo; 7/7 CTests com negativos. C apenas ponte/estado nativos, não paridade SPU/GS nem algoritmo da sessão integral. Capturas locais build/machine-finish/captures; sem comparação PCSX2. (c) |
| **Replay** após a corrida | `quick-*` | A | Gravação/Play/retorno implementados; replay_file, replay_types, replay_hud e negativos, comandos originais E. Câmeras e apresentação integrais A. |
| **Replay Theater** (assistir replays salvos) | `gtmode` (ReplayTheaterRoot) | A | Lista, Play, playlist, retorno e 13 demos US implementados; disc_demo_probe, disc_demo_app, outside_menu e negativos. Miniaturas das demos E em prova separada; datas, trajetória e composição integral A. |
| **Salvar e carregar replays** | `memcard` | A | Formato PC escolhido P; replay_file roundtrip/CRC/truncamento, replay_types e ps2_media_import testam fluxo e importação, com negativos. Comandos PS2 E/adaptadores C; melhor volta PS2 como ghost F e trajetória integral A. |
| **Photo Travel** (carro em cenário para foto) | `photo_shoot` | A | Destino/carros da garagem, cenário `/crs/ss_*`, mapa CRPM+14, controles dos scripts, captura congelada, retorno e troca de cenário funcionando. `photo_travel_flow` testa Brooklyn/Gion, focal numérica, captura e três exports; negativo altera hash. Sem comparação PCSX2; matriz/libm, seleção inicial da região 0, mapa/projeção, clipping interno, near PC 0,2, modelo do menu, luz/material e sincronismo de widgets ainda A. Retrato/aspectos não conferidos; profundidade de campo, filtros, exposição/white balance ligados; coeficientes E e compensação da lente C, composição óptica integral A (provas abaixo). Áudio específico por destino não comprovado: auditoria dos 15 CRSS sem emissores e script photo_shoot sem pedido de BGM; isso não certifica todos os caminhos de áudio. (c) |
| Photo Travel: auditoria do ambiente CRSS | `photo_shoot` | E | 12/10: photo_ambient executa a função original 00457F60 nos CRSS dos 15 cenários /crs/ss_*, em 120 blocos de câmera. Todos sem emissores, L/R zero e pitch 1; negativo injeta ganho e falha. Prova restrita ao ambiente CRSS, sem afirmar ausência de música em todo o modo. photo_shoot.adc e seu mproject não pedem BGM; as 15 faixas photo_ da biblioteca PCM não provam vínculo por destino. (c) |
| Photo Travel: dados de entrada dos destinos | `photo_shoot` | E | `photo_mode_data`: poses X/Z/ângulo, limites, contagens/offsets CRPM e dimensões do mapa nos 15 `/crs/ss_*` comparados diretamente aos bytes do disco; campos 0x18..0x58, lidos por `0036FAE8`/`003C65E8`. Corrupção/truncamento rejeitados sem mudar a saída; negativo desloca X do carro e falha. Prova restrita aos dados lidos, não à pose 3D final. (c) |
| Photo Travel: exportação de foto | `photo_shoot` | P | PNG sem UI na resolução da janela, em `<save-directory>/photos`; Enter salva, Esc volta, F5 aciona Shoot. GPU retém a prévia, readback só ao salvar. `photo_travel_flow` confere PNG/dimensões, congelamento, nomes separados e nova foto diferente. Substitui Memory Card/USB; álbum PhotoRoot agora lê PNGs exportados (P/C acima); Photo Drive integrado ao replay 1P, prova separada abaixo. (c) |
| Photo Travel: obturador | `photo_shoot` | A | `00462EC8 +B4..10C` monta `/sound/gt4shutter<tipo>.ins`; bank 4 e sequence 0 vêm de `gt4se.inf`. Usa o decoder/mixer já existente; `photo_mode_data` decodifica os 19 bancos typea..types, mas ainda falta comparação de forma de onda/volume/relógio com SPU2. (c) |
| Photo Develop: exposição, white balance e filtros | `photo_shoot` | E | 12/10: `photo_optics` executa 003EFC10/003EFD80 e compara 3366 combinações de coeficientes bit a bit, incluindo mature/monochrome; pow da biblioteca é fronteira do oracle. Negativo altera ganho e falha. Prova dos coeficientes, não dos pixels finais. |
| Photo Develop: amostras da lente | `photo_shoot` | E | 12/10: MT 00579438/00579548 e disco de lente 003323B0; 1024 amostras comparadas bit a bit. Seed 12345678 e contagem 32 de 003C4D98/9C. Negativo altera amostra e falha; RNG privado não muda corrida. |
| Photo Develop: deslocamento e compensação de foco | `photo_shoot` | C | 12/10: 00332550, 512 matrizes contra execução EE; tolerância absoluta 1e-7, negativo altera compensação e falha. Prova restrita à transformação numérica. |
| Photo Develop: composição óptica completa | `photo_shoot` | A | 12/10: controles ativos, 32 passes deslocados da lente, CLUT e composição de canais reutilizados de license_image. Autófoco central por depth GL, sensor/near/far/pose/projeção ainda A. Média GPU com alpha contínuo e viewport nativo P substituem tiles 2x2 e truncamento FIX do GS. Captura local build/race-resources.png; comparação PCSX2 pendente. Exposição temporal ligada (prova escalar E abaixo); câmera/partículas/objetos/box e composição GS integral ainda A. |
| Photo Develop: obturador e interpolação temporal | `photo_shoot` | E | 12/10: photo_optics executa 0036F628/0036FA20 (19 valores em 00620EF0 e conversão), 00333680 (limite), prefixo escalar 00334730 e 0033B358. 736 combinações de índice/amostra com tempos e 12 componentes bit a bit, mais três limites de exposição. Negativo TIME altera t e reprova. Fronteira no primeiro callback de mundo da 00334730; não prova passes GS nem imagem inteira. |
| Photo Drive: entrada GT Mode e sessão | `gtmode::CourseEntryRoot`, `quick-freerun` | C | 12/10: botão original PhotoDrive.onActivate, sequência photo_drive, retorno CourseEntryRoot, pista/cor e corpo equipado 0x4b8 conferidos em photo_drive_menu. RACE photo_drive do disco: um carro, mode 0 e zero voltas; 0011AC54/0011ACB4/0011B160 constrói descritor/GT world 003EDE50, 0011B288 abre quick-freerun. Sem lista RACES de adversários. race_resources monta GT world mode 13, dirige, congela 32 MiB ao fotografar, retoma e reproduz a fita; tolerância zero no estado preservado. Negativos route/freeze reprovam. Integração nativa, não ciclo visual PCSX2 completo. Física não alterada. |
| Photo Drive (sessão e foto durante replay) | `photo_shoot` | A | 12/10: 32 amostras temporais, 19 tempos originais de 1/60 a 1/4000 s, histórico por tick. Carros/rodas/sombras e 64 canais de piloto interpolados sem avançar física/fita; câmera e visibilidade restauradas. photo_motion confere oito poses, amostras não cumulativas, ausência de histórico e restauração; negativo corrompe posição. race_resources confere PNGs diferentes entre 1/60 e 1/4000, câmera/visibilidade e 32 MiB intactos. A: câmera editada recebe delta histórico host, partículas/objetos/box sem interpolação temporal completa, GS sem comparação integral. F5/colchetes/PNG são P; PhotoPause usa STRB e textos originais, com controles/banco descritos na seção 4.2; apresentação integral permanece A. |
| Slideshow: seleção musical salva por perfil | `labomode/slide` | C | 12/10: option.slide_playlist mantém o formato original 0x810, linhas de 004379F0 e seleção/order/shuffle dos scripts; blob slide_playlist_ps2 separado do Jukebox de corrida. outside_menu compara todos os bytes após reiniciar o armazenamento e abrir outro projeto, refresh/capture de menu em cache e capture de VM antiga. slideshow_playlist_negative corrompe a seleção e falha. Armazenamento nativo por perfil P; prova C restrita à ponte/persistência, não ao áudio final PS2. (c) |
| Salvar e ver fotos, slideshow | `labomode::PhotoRoot/SlideRoot` | A | Álbum, playlist, preview e slideshow com música implementados; estados E e geometria/fluxo C acima. PNG sem metadata original, composição/filtragem GL e som reamostrado sem comparação PS2. Reprodução 44,1→48 kHz e ganho final não conferidos; seleção, ordem e shuffle da lista musical persistidos por perfil; outside_menu confere reabertura do disco e captura de menus em cache, com negativo obrigatório. Print/USB continuam fora do escopo; revelação de filmes implementada com imagem A, conforme tabela acima. |
| Imprimir fotos | `print` | N | a decidir |
| Registrador de dados (telemetria) | `logger` | A | Tela, telemetria e carregar trilhas A/B implementados; composição integral pendente. Provas detalhadas nas linhas Logger acima. |
| Opções (as três páginas, incluindo o menu secreto) | `option`, `option2`, `option3` | A | Scripts/páginas carregam, remapeamento, volumes/unidades e persistência implementados/testados (controller_input/controller_defaults_oracle, options_lifecycle, outside_menu). Favoritos/filtros e aplicação da correção de cor à corrida/replay ainda F; LAN N, não confundir campos salvos com efeito funcional. |
| Idiomas | `language` | A | 13/10: disco suportado NTSC-U v2.00 tem somente /projects/language/US e config-main.adc init_main caso us seleciona apenas US (instruções130..138). Getter US corresponde a esse caminho; sem teste diferencial automático do seletor para elevar a E. PAL/KR não são idiomas ausentes do disco US. |
| Save do jogo | `memcard` | C | Save/perfis/auto-save nativos implementados em save.c/save_state.c; profile_storage, gtmode_home, gtmode_setting_race e ciclos/prêmios conferem reabertura e negativos. Arquivos PC P; importação GT4 PS2 tem prova própria C. Não certifica todos os formatos/regiões de memory card. |
| Troca de carros entre perfis locais | `gtmode`, `arcade` | P | 12/10: profile_storage_test executa Trade/Cars.onActivate original, débito, carro adicionado, persistência e origem intacta (corpo 0x4b8), cancelamento e entrada Arcade. Negativo corrompe saldo e falha. Armazenamento local é adaptação PC. |
| Herança de créditos/licenças US | `gtmode` | A | 12/10: 001D4500 +48..58 e 001D4598 +50..58 selecionam GT3 US nas duas operações. menu_legacy_import.inc: u64 complementado +0x50 e cap 100000 (001D49C8 +5C..D0); B/A por índices +70/+74 e stride154 (001D4698 +A0..260), grau12 sem rebaixar ouro. gtmode_extras_menu passa pelos helpers StatusRoot, transferência única/reload, falta/truncamento/zero/índices inválidos; negativo altera saldo e falha. Fonte sintética, falta save real; containers GT3 e outras regiões pendentes. Filesystem P. |
| Códigos GT Mode: dinheiro/licença/evento/categoria | `gtmode` | C | gtmode_extras_menu: sequências do ADC via menu_send_key, barreira365/366 dias e comando errado, saldo/gold/vitória/red de categoria. Foco fixture explícito; navegação integral pendente. Negativo apaga resultado e falha. |
| Usados: seleção semanal e black cars | `gtmode` | C | gtmode_extras_menu: 1401 datas, nove fronteiras (0/6/7/692/693/699/700/1393/1400), reentrada na página e listas comparadas aos ADCs independentes; quatro black cars na semana99. shop-catalog: 100 listas/12192 entradas. Negativo stock esvazia lista de referência e falha. Apresentação integral pendente. |
| Today's Special: seleção diária de rodas | `gtmode` | C | gtmode_extras_menu compara 58 dias com as29 listas do ADC. getElapsedDate(period) corrigido conforme00133288 +6C..7C; negativo reproduz bug e falha no dia29. Prova da seleção, não da apresentação integral. |
| Jukebox / músicas | `gtmode`, `slide` | A | 12/10: MusicTheater -> exit_kick_project("slide") com next_menu_arg MusicRoot; scripts originais do slide (musiccontrol.h). game.getAllMusicList (65 faixas: título, artista, álbum, id), option.bgm_playlist [original, usada, ligada, ordem] + shuffle lido e gravado no blob bgm_playlist_ps2 do save (004379F0), main.default_library_playlist de config-main init_baseplaylist, option.replay_bgm_volume; sound.startStreamEx/isPlayingStream/pauseStream/stopStream no stream PCM. Conferido: lista, tocar, pausar, sair e voltar ao GT Mode TopRoot. A: áudio não ouvido nesta máquina (sem saída), sem comparação com o PCSX2. V pendente |
| Segredos, carros pretos, códigos, desbloqueios | vários | A | Implementados: gtmode_extras_menu compara 1401 datas/black cars, códigos e 58 dias de rodas; getters/desbloqueios têm provas próprias E/C. 13/10: falha observada no fixture foi corrigida em trabalho concorrente (pin durante GC, docs/notes/SUITE_FIXES.md); build/suite_fixes/extras-focused.log confirma 5/5, incluindo quatro negativos. Não equivale a aprovação da suíte completa. Carreira contínua completa e UI A; favoritos ainda F. |
| Rede local (LAN) | `network`, `quick-mt` | N | a decidir |
| Online | `online` | N | Serviço original indisponível; um backend novo exigiria decisão separada de escopo. |
| EyeToy | `eyetoy` | N | a decidir |

Auditoria funcional atualizada: [CLASSIC_RESOURCE_AUDIT.md](CLASSIC_RESOURCE_AUDIT.md). Notas de imagem A não significam ausência de todo o modo; consultar as provas detalhadas antes de reutilizar pendências históricas.

### 13/10/2026 — faltas funcionais confirmadas no inventário

Auditoria somente documental, sem implementação. Fonte/testes e estimativas
em [CLASSIC_RESOURCE_AUDIT.md](notes/CLASSIC_RESOURCE_AUDIT.md).

| Item delimitado | Nota | Evidência da ausência e limite das provas existentes |
|---|---|---|
| Evolução da experiência B-Spec nas corridas | C | Produtor e persistência implementados em 14/10: cálculos E em 4096 casos contra 00468710/00468860; corrida B-Spec física, Retry, não duplicação e save/load C. Ver registro B-Spec experience progression abaixo; trajetória/apresentação integral A. |
| Favoritos de carros/pistas e filtros associados | E | See section 4.3: favorites_oracle and favorites_menu, positive and deliberate negative cases. |
| Aplicação de Screen Setting/correção de cor à corrida e replay | C | See section 4.2: original parameters E, application/GPU boundary C (one byte), separate race/replay preferences and profile persistence, positive and deliberate negative tests. |
| Photo Travel: regiões, colisão/oclusão e can_shoot | F | photo_mode.c explicita 003FECB0/00371530 não portadas; usa região0/limites globais. menu_widgets.inc can_shoot só testa ready. Movimentos/mapa 003704D0 incompletos; dados dos 15 destinos E, fluxo básico C. |
| PhotoPause: rejeição de câmeras e limite de zoom por obstáculos | F | photo_travel.c constrói banco de até64 câmeras (003FFC68/003FFD10), porém substitui busca/parede por altura nativa; partição/rejeição e mínimo dinâmico de zoom ausentes. Banco, zoom18frames e editor existem, apresentação integral A. |
| Foto: interpolação temporal de partículas/objetos/equipe | F | Histórico interpola carros/rodas/sombras/64 canais do piloto; objetos/partículas/box sem histórico completo. photo_motion/race_resources provam os estados já suportados, não esses passes. Câmera editada host A. |
| Ghost de melhor volta importado do PS2 | F | FileReplayLicense 001CFE20/001D0360 não importado como ghost. GHST nativo testado por tt_game_probe --saved não prova leitura do formato PS2. |
| Filme PS2: body/LOD CDTL, GDGT, clipping SMOK e projeção develop | F | ps2_media.c/photo_travel.c restauram wheel/wing/DRVR/SMOK/PTCL/ENVP, mas body/LOD, gadgets, plano SMOK e uso integral de far clip não existem. Aspecto/retrato já aplicado; rotação/composição integral A. film_develop C não certifica esses dados ausentes. |
| Sombra noturna por lâmpada | A | Passe 0038F168/00451EE0 integrado; parametros C contra original em 192 casos e negativo. Consulta de chao, agendamento completo e pixels PS2 pendentes. [Audit](notes/NIGHT_COURSES.md). |
| Sombras dos gadgets/objetos de largada e chegada | A | Odd-model pass restored from 003E4A28; scalar projection/alpha/model selection C and GL/Vulkan pixel/immutable-state checks pass with dedicated negatives. Complete original raster and all-course coverage pending; see Original course gadget shadows below. |
| Backfire: tween/cor original e brilho no chão | F | draw_afterfire desenha chama básica; 00456850/004A60E0 e passe a2=1/model1 não implementados. Produtor/áudio já E/C; não voltar a listar chama ou som inteiro como ausente. |

Seleção nesta tarefa: nenhuma. A implementação aguarda escolha do usuário.

### 4.5 De propósito (P) e não será feito (N)

Conforme as regras do projeto, a base atual é única e a fidelidade é medida no
**preset Original**. Classic em títulos históricos não define uma edição
separada. Diferenças P precisam ser explicitadas; N registra ausência/escopo,
não equivale a implementação nem impede o usuário de selecionar o item.

| Item | Nota | Justificativa |
|---|---|---|
| Vulkan submission and resource reuse | P | One steady-state render/present submission instead of two; dynamic uniform offsets retain every draw value while reusing texture/sampler descriptors. Framebuffer objects persist until attachment destruction; fixed attribute/pipeline keys avoid temporary per-draw containers. vulkan_submission compares six-car Tsukuba at 1280x896 and 1920x1080: identical draw counts, final RGB and full 32 MiB physics state; submission/descriptor/framebuffer counts fall. Split-submission, uncached-descriptor and transient-target negatives must fail their respective gates. Three alternating old/new executable runs at 1280x896: median 12.871/12.341 ms (4.1% frame-time reduction); optimized 1080p sample 14.204 ms, about 70 FPS. VSync off, GTX 1650; no universal FPS guarantee or new PS2 fidelity claim. Physics rate unchanged. See VULKAN.md. |
| PC launcher control repaint | P | Native BUTTON WM_ERASEBKGND reproduced a white fill before owner drawing. Disc-backed buttons suppress that intermediate erase; dropdowns compose backdrop, original Options frame and text off-screen before one copy, retaining native input. launcher_control_paint checks 200 real erase messages leave all pixels untouched and compares complete dropdown RGB at zero tolerance; native-eraser negative fails. PC presentation contract only; no claim of PS2 raster equivalence or universal flicker coverage. See LAUNCHER.md. |
| Launcher do Classic antes do disco | P | (c) 11/10: preparação nova para PC, autorizada pelo usuário, sem equivalente PS2. Win32/GDI, Arial do sistema, paleta plana de option.mproject/ScreenRoot; formas/layout novos, sem logo, fonte, script, imagem ou áudio extraído. ISO selecionada fica no lugar; valida SYSTEM.CNF e CRC dos executáveis NTSC-U v2.00 só ao selecionar/Play/bypass. Vídeo, sete volumes e remapeamento 1P/2P persistem nos formatos existentes; merge da carreira preserva dinheiro/opções. Slideshow usa PNGs locais do álbum/cache do Photo Travel, dois quadros pré-escalados, ciclo de 8 s/transição de 1,5 s escolhidos para PC; sem cursor desenhado. Refino: desenhos procedurais substituidos por sete icones existentes Tango 0.8.90 (dominio publico, rasterizacoes Nigel Tao), baixados por tools/setup_launcher_icons.py com revisao/SHA-256 fixos e COPYING conferido; cache local launcher_icons ao lado de launcher.ini, nenhum asset do GT4. Atlas alpha 80/144 px bilinear premultiplicado; faltantes/danificados deixam slots transparentes. Arte original do conjunto sem redesenho; composicao e escala PC P. Resumo contextual e navegação da barra por setas exercitados pela janela real. `launcher_config`, `launcher_without_disc`, `launcher_window` e tres negativos: 6/6; teste de ISO real/Play, cancelamento, conflito, caminho Unicode, PNG danificado, dissolução e repetição; pixels de alpha/premultiplicacao, transparencia e cache quebrado. Render do Photo Travel continua A; nenhuma imagem é distribuída. Evidencias locais em extracted/handoff/launcher-tango-20261011 e launcher-20261011. Windows apenas; instalador e FFB adiados. |
| PC launcher calm camera dissolves and settings composition | P | Eighteen 12-second dollies (216-second cycle), 1.5-second smoothstep GPU dissolves using one opaque outgoing capture; one live car render per frame. Unit checks cover all cut endpoints, midpoint and monotonicity. Real OpenGL/Vulkan midpoint captures compare with independent outgoing/incoming frames: maximum channel difference 1/255, limit 3/255; replacing the dissolve with the old view fails. Entire settings captures match before/after live preview, and a corrupted right-edge pixel fails: paint/print restore the temporary GDI viewport before backing-copy/children. Pale home heading veil removed; light GT4 letters use only a dark shadow. Nine focused checks pass including deliberate negatives; no full suite. These durations and layout are authored PC presentation, not original PS2 behavior. [Details](LAUNCHER.md#camera-transitions-and-settings-composition). |
| PC launcher preview stalls | P | Vulkan readback now uses a reusable coherent buffer, preferring CPU-cached memory, with bulk RGBA row copies. GPU resolves the unchanged 2x render before transfer (870,800 instead of 3,483,200 pixels). Window owns cached premultiplied images/expanded glyphs, skips overwritten background work and paints on new preview frames. Exact GDI output across 34 composed fixtures and no warm rebuilds; corrupted-pixel negative detects errors. GPU versus CPU resolve checked in both backends, tolerance 1/255 per channel; Vulkan synthetic/disc image comparisons retained; 21 focused checks passed, including deliberate raster/resolve/frame negatives. No full suite. GTX 1650: former 60-frame run averaged 385.675 ms readback and 16.506 ms parent paint; final 180-frame run took 3.933 s including startup, means 11.479 ms read/resolve and 6.132 ms parent paint, 112 parent paints. No universal 60 FPS claim; other-device performance pending. [Method and limits](LAUNCHER.md#preview-performance). |
| PC launcher cinematic car and GT4 dock | P | Wide home screen uses runtime GT4 fonts, Options checker/panels and Options icons; 34 cached images checked against disc decoding with an altered-icon negative. Native live preview reads active save car/paint, fitted wheels and active-sheet wing (Ford GT fallback), 18 custom dolly shots, animated studio lights/reflections, launcher-only fill/highlights and 2x supersampling. Camera/save tests include invalid garage and deliberate wrong-pose negative; parts fixture covers second setup sheet, stock fallback and wrong-wheel negative. Real launcher renders OpenGL wide/detail shots and Vulkan, with non-flat content and changed-frame checks plus impossible-count negative. Existing settings/navigation checks retained; 18 focused checks pass. No full suite; no game assets distributed, no race physics or Switch implementation changed. Grade P for custom PC presentation; complete PS2 pixel equivalence and all-car framing not claimed. [Details](LAUNCHER.md). |
| Moon Engine PC branding | P | Product labels in the PC launcher, window and Windows executable metadata use Moon Engine. Existing MoonEngine.exe, CMake targets and Moon Engine preference/save paths remain compatible. No Switch renderer or packaging rename. |
| PC Vulkan renderer and backend choice | P | 15/10: native Vulkan device/surface/swapchain, SPIR-V shaders, textures, 2D/3D draw state, clipped alpha/depth clears, captures, shadow targets and FSR. No OpenGL context in the Vulkan fixture. Cross-backend synthetic image tolerance 2/255; disc-backed Ka/Tsukuba/showroom/Options limits mean RGB 1/255 and <=0.1% channels above 32/255. Recorded means 0.020918, 0.103857 and 0; intentional pixel/image/state negatives detect corruption. Khronos validation reports no API errors for the tested showroom and Tsukuba frames. 20 focused tests passed (including explicit corruption negatives and GL regressions); no full suite. Single-sample scenes, no anisotropy/HDR/adapter picker/device-loss recovery; Linux and exhaustive scene/replay coverage pending. Host-backend comparison, not PS2 certification. [Implementation and tests](VULKAN.md). |
| PC Vulkan race rendering stalls | P | Compatible draws share render passes; deleted/replaced images survive until fence completion; staging reservations count interleaved data once. Vulkan race reflections use the existing GPU remap, with CPU composition retained through `MOON_REFLECTION_CPU=1`. `vulkan_race` checks 60 Tsukuba frames: zero GPU readbacks, two submissions/frame, full-frame CPU/GPU maximum difference 1/255 (limit 2/255); negative restores six CPU readbacks/frame and fails. Synthetic fixture checks 128 consecutive draws without extra pass/submission and image retirement without a wait. Ka/Tsukuba 640x448, GTX 1650, 120 frames without VSync: 46.165 to 11.804 ms/frame; render-only probe, not complete-race FPS certification. [Evidence and limits](VULKAN.md#race-rendering-performance-correction). |
| PC Vulkan resident static geometry | P | Static vertex buffers remain device-local, edited/deleted versions survive through their fence, interleaved attributes share lookups and unused sampler descriptors are not rewritten. `vulkan_simulation` compares a running six-car Tsukuba race at 1280x896: exact final 32 MiB simulation memory and RGB pixels versus streamed geometry; one-bit state negative detects corruption. Vertex uploads over 120 measured frames: 3,427,332 versus 756,293,364 bytes. Final paired wall-clock means: 12.171 versus 14.138 ms/frame; no FPS gate or claim for all races. Synthetic fixture verifies initial subrange assembly, nonzero offsets, reuse and edits/deletion with pending draws; forced streaming fails the residency assertion. [Evidence and limits](VULKAN.md#resident-static-geometry-and-running-race-measurement). |
| PC Vulkan frame queue, preparation and pacing | P | Two independent submission slots with selectable one-frame low latency; per-image present semaphores; separate GPU timestamps, CPU wall intervals, fence waits and frame percentiles. Driver cache validates device/UUID and recovers from corrupt headers. Startup warms common draw paths; race loading prepares static geometry. Optional absolute-deadline FPS cap and queue choice persist through launcher reopen/in-game settings. Positive/negative pacing, cache and settings tests; synthetic queue/resize/pending-image checks and five 1080p race/replay pairs require exact RGB and full 32 MiB state. GTX 1650 first run: 9.801-12.502 ms/frame average across both modes, with no consistent double-queue advantage; occasional >16.67 ms frames remain. Unseen shader variants still compile lazily; no guaranteed minimum FPS or PS2 image certification. [Evidence and limits](VULKAN.md#frame-queue-preparation-and-frame-pacing). |
| PC Vulkan command reuse and recorded variants | P | Repeated pipeline/dynamic/vertex commands omitted; static meshes retain base bindings, disabled inputs share immutable arena defaults, mesh-local material/texture cache keeps draw order and alpha-fail/DATE/ghost passes. Versioned/checksummed handle-free pipeline recipes warm matching shaders at link; corrupt metadata falls back to lazy creation. Three 1,200-frame 1080p race/wet/replay pairs preserve exact RGB, all 32 MiB and draw counts; negative forces repeated commands. Variant cold/warm/corrupt/disabled checks preserve exact bytes. Tsukuba: 4,337,586 to 56,570 dynamic commands; 1,400,742 to 68,075 vertex binds, average 15.012 to 14.675 ms/frame in one GTX 1650 pair. Longer tests still show >16.67 ms frames; unknown states and late shader links remain first-use work. PC invariance only, no PS2 or minimum-FPS claim. [Evidence and limits](VULKAN.md#command-recording-and-known-pipeline-variants). |
| Shared OpenGL/Vulkan material cache and single-pass batching | P | Both PC backends reuse mesh-local texture/material state and join adjacent visible VBO ranges with identical material, source model, lamp and wheel-blur attributes. Alpha KEEP remains single-pass; fail-write, DATE, blended, skinned and ghost draws remain separate. Synthetic RGBA fixtures check eligible and excluded cases, with disabled-optimization negatives for each API. Six 600-frame 1080p dry/wet/replay pairs preserve exact final RGB, all 32 MiB of simulation, triangle counts and visibility decisions. Dry-course calls fall from 859,312 to 818,972 with 30,988,185 triangles unchanged; material applications fall from 845,808 to 202,764 against the uncached reference. This is PC invariance, not PS2 certification or a 300 FPS guarantee. [Scope, diagnostics and performance limits](RENDERER.md#shared-pc-material-submission). |
| Optional PC spatial visibility and resident reflection pipeline | P | Independent shared-store settings: Classic disables both, Enhanced enables both; apply/cancel/reload/preset tests and deliberate negative. Immutable mesh BVH uses conservative union bounds and inherited plane masks; skinned/mutable/small and authored-list submissions retain flat traversal. 1,000 randomized cameras over 4,096 boxes match flat visibility; flipped-result negative fails. Both APIs compare four modes across 600-frame dry/wet/replay cases: unchanged draw/triangle/visibility totals and full 32 MiB state, hierarchy-only RGB exact, resident remap <=2/255 (observed at most 1/255 in these final captures). Dry-course support tests 2,601,141 -> 1,042,772; CPU reflection readbacks 3,600 -> 0 over 600 frames. Runtime return to Classic checks stale GPU bindings. No original geometry LOD replaced, no occlusion culling implemented, no guaranteed FPS gain or new PS2 image certification. [Implementation and scope](RENDERER.md#optional-spatial-visibility-and-resident-reflections). |
| Tabbed PC Screen Settings | P | Display, Rendering and Controls reuse the disc fonts, pane, checkerboard and dropdowns. Shared preset and OK/Cancel remain accessible; switching tabs preserves the draft, hidden rows are excluded from navigation, reopening selects Display. L1/R1 cycle closed pages. Actual-executable desktop/scene settings tests cover apply, cancellation, reload, presets and edits across tabs; the negative corrupts a cross-tab value and must fail. 4:3 and 16:9 screenshots inspected. PC layout adaptation, no PS2 equivalence claimed; Switch page unchanged. [Options](OPTIONS.md#pc-settings-pages). |
| Resolução da tela em vez de 448 linhas entrelaçadas | P | motivo de existir do port |
| Sem serrilhado e sem o pontilhado de cor do PS2 | P | idem |
| Tempo de carregamento | P | leitura direta do disco |
| Save em arquivo, sem telas de memory card | P | não há memory card |
| Teclado e controles modernos | P | SDL GameController: A/B/X/Y = Cross/Circle/Square/Triangle; gatilhos substituem pressão L2/R2, zonas mortas herdadas do port (8% sticks, 5% gatilhos), navegação por eventos e arrays de configuração nativos persistidos. Sem emular protocolo pad nem serialização PS2; controle físico ainda não conferido. Volante raw e force feedback SDL integrados; ensaio/calibração de G29 continuam pendentes, conforme linha A em 4.2. |
| Câmera de inspeção durante a corrida (F3) | P | pedida pelo usuário: WASD, Q/E, setas, Shift; começa na vista ativa e atravessa cenário. Velocidades 3/15 m/s e giro 1,5 rad/s são escolhas PC. `race_inspect_probe` verifica posição, movimento, eixos, comandos neutros, retorno e reset; controle negativo falha (c) |
| Pixels idênticos aos do PS2 | N | exigiria emular o chip gráfico |
| Verificação online DNAS | N | o serviço não existe mais |
| Rede local entre consoles | N | a decidir |
| Impressão de fotos, EyeToy | N | a decidir |

## 5. Próximos passos de comprovação

### 5.1 Decisões em aberto (de escopo)

- **Replays/fotos:** arquivos nativos e PNG escolhidos como adaptação P;
  importação de replays/JPEG/filmes GT4 PS2 implementada, com limites em
  [PHOTO_MEDIA_COVERAGE.md](notes/PHOTO_MEDIA_COVERAGE.md). Ghost PS2 F;
  reconstrução/composição integral de filmes A, subpasses ausentes F.
- **Rede local, impressão e EyeToy:** ainda ausentes N no escopo documentado;
  LAN consta explicitamente do inventário para seleção do usuário.
- **Tourist Trophy:** outro jogo; não bloqueia completar recursos do GT4.

### 5.2 Ordem

1. Construir o comparador de lista de desenho (seção 3) e aplicá-lo ao HUD
   da corrida. Meta: tirar de V todos os itens de HUD da seção 4.2.
2. Aplicar o mesmo comparador à tela de resultados e aos textos da largada.
3. Automatizar a conferência das câmeras (matriz e projeção).
4. Resolver cada item A da seção 4.2 lendo o código, não o print.
5. Só depois, continuar acrescentando telas.

### 12/10/2026 — retorno do pré-race, configurações e Retry

| Item | Nota | Evidência / teste |
|---|---|---|
| Pré-race: reentrada e retomada da música PCM | C | 00115A90 -> 0010FE40 e licenças 0010FBD8 recarregam o projeto rápido a cada ciclo. O cache nativo agora refaz onInitialize na página escolhida pelo getStartPage original; projetos sem seletor usam sua página corrente. quick-event/quick-championship onInitialize e quick-license onInitialize pedem pcm/default_quick. pregrid_menu executa reentrada do LicenseRoot e Option/retorno pré e pós-prova event/championship nos scripts do disco: grupo vazio na saída, novo serial e default_quick na volta, foco Option e página do seletor preservados. A volta da demonstração da licença usa essa mesma reentrada. quick_pcm_return_negative elimina o grupo e falha. Comparação exata desses campos; não certifica toda a aparência nem waveform (c). |
| Configurações da corrida: retorno direto ao chamador | C | Laço nativo 0010FE40: o comando Option retorna ao projeto rápido que o abriu. main.c guarda o projeto exato (inclusive quick-mt/practice), consome o destino major_menu sem abri-lo e retorna pelo seletor original. quick_options_app_return usa o exe real e seus injetores CLI: quick-arcade -> option -> quick-arcade, nenhum TopRoot intermediário, PCM retomado e captura do pré-race. Negativo exige option -> arcade e falha. pregrid_menu exercita seletores event/championship pré/pós-prova, foco e tempos. O teste integral do exe cobre Arcade; licença e demais variantes usam o mesmo desvio, sem captura integral PCSX2 (c). |
| Licença 1P: Retry da pausa reinicia a prova | C | pause.strb EXIT_MENU("retry") -> estado 5 (003C3260); License World habilita Retry em RacePause +28 (003BBF90). draw_pause antes só encaminhava esse estado em Battle2P; agora toda pausa que o script habilita gera o pedido único. App faz restart da prova preparada, rearma aplicação do resultado e loading. license_menu_demo executa pausa original B-1, Down/Enter, consome uma vez, reconstrói a prova e verifica pose inicial, fita vazia, sem quit/replay. license_pause_retry_negative descarta o pedido e falha. Fórmulas da corrida não alteradas; comparação visual completa da pausa PS2 pendente (c). |

| Volante atrás do vidro: ordem nativa de desenho | A | 12/10: race_view desenha o volante com o piloto antes da carroceria, mantendo matrizes/ângulo MDLS e depth writes do vidro. driver_windshield verifica piloto e volante atrás do vidro e oclusão pela lataria; negativos driver_windshield_negative e steering_windshield_negative restauram desenho depois do vidro e reprovam. 3/3 CTest passam. Captura nativa Copen B coffee em build/coffee-steering-front.png inspecionada. C restrito à dependência GL testada; sequência GS e comparação integral PCSX2 pendentes. |

### 13/10/2026 — launcher com ISO e Options em tempo de execução

| Item | Nota | Evidência / teste |
|---|---|---|
| Launcher: gráficos Options e cache | E | launcher_skin compara exatamente 356.489 texels de 12 assets de /projects/option/US/OptionRoot.gpb com decodificação independente, dimensões e geometria Item/label/bg/ScrollWindow de option.mproject. Fontes originais são lidas em runtime; cache preserva bytes e glyph tables. Cache truncado é reconstruído. launcher_skin_negative altera um canal do Screen e falha. E se limita aos dados comparados; não certifica raster GDI. |
| Launcher: seleção e menu PC | P | Sem ISO válida, só título, pedido NTSC-U/SCUS-97328 e seletor; fundo procedural grafite, quadriculado e botão dourado conforme pedido. launcher_validate_disc habilita o menu completo com oito ações PC; ISO inválida remove o menu mesmo após cache válido. launcher_window testa janela real, válida/inválida/válida, navegação, Apply/Cancel, conflitos, remapeamento 1P/2P, persistência e Play. Somente testes launcher: 8/8; negativo de resolução reprova. Sem assets do disco no repo/pacote e sem mudanças na física. |
| Launcher: composição/raster completo | A | Captura nativa de OptionRoot comparada com launcher. Geometrias, quatro cores dos ColorFace, tiles e fontes vêm do disco. Ações PC, detalhes em três slots e diálogos são adaptações P; texto de detalhes/campos usa fonte do sistema e rasterização GDI ainda não tem comparação pixel a pixel com GS. [Notas](notes/LAUNCHER.md). |

### 13/10/2026 — atualização do launcher pelo servidor Oracle

| Item | Nota | Evidência / teste |
|---|---|---|
| Atualização HTTPS e publicação PC | P | MOON_VERSION do CMake gravada no exe e VERSIONINFO. Worker WinHTTP com Bearer e TLS, sem bloquear janela; versão maior oferece Update, igual/menor/erro ficam silenciosos. Preferência check_updates persiste. Download temporário confere tamanho e SHA-256 BCrypt, inclusive antes de CreateProcess; setup /VERYSILENT /CURRENTUSER instala no diretório atual e reabre o jogo. publisher gera pacote/Inno/hash/latest.json, SCP .incoming e promoção SSH com manifesto por último. Domínio/token/SSH só em publish.ini externo e header ignorado no build, sem credenciais no repo. Rota estática configurada no Caddy do nuntius; demais rotas preservadas. 12/12 testes launcher, incluindo HTTPS real: 401 sem token/errado, download original e recusa de SHA adulterado; negativos de versão/hash/tamanho/JSON, UI e publicação. launcher_installer.py provou instalar/atualizar/reabrir em dois ciclos isolados, preservando arquivo existente; AppId de teste e sem atalhos, Files/Run originais. Release 0.3.0 publicado no Oracle. [Notas](notes/LAUNCHER.md). |

### Launcher presentation, mouse cursor and audio (13/10/2026)

- **E, scoped data:** launcher_skin compares 34 runtime ISO/cache assets
  exactly, plus cursor hotspot (16,0) and original font paragraph bounds.
  No disc asset is stored in the repository/package. Cache OGS6.
- **C, native audio integration:** launcher_audio compares four GTSE-routed
  waveforms and a separate SQT prefix with zero sample difference; mute,
  missing disc and an intentionally corrupted sample are exercised. This
  does not certify full SPU2 waveform/reverb parity.
- **P:** physical mouse cursor, 140 ms hover, centered PC forms, original
  Options button/pulldown art throughout the launcher, Updates: On/Off and
  map sequence 0 BGM. Requested PC preparation, not a PS2 screen clone.
- **P:** FSR1 EASU/RCAS reconstruction in an owned hidden GPU context;
  launcher_fsr verifies exact RGBA against the existing game backend,
  invalid input, context recreation and deliberate pixel corruption.
- **A retained:** full GDI/GS raster comparison remains unproved. Captures
  of all forms were inspected locally. Launcher-only positive/negative
  regressions pass; see [launcher notes](notes/LAUNCHER.md). Physics unchanged.


### Launcher without a startup console (13/10/2026)

P: the Windows player executable now defaults to the GUI subsystem,
including local builds. Console test harnesses remain unchanged; developers
can explicitly opt into MOON_WINDOWS_GUI=OFF. Existing CMake caches need
-DMOON_WINDOWS_GUI=ON (applied to build/Release/MoonEngine.exe here).
launcher_gui checks the actual player PE subsystem; launcher_gui_negative
rejects the console test harness. Only launcher tests were run.
Launching a .cmd file still starts its own command interpreter; open the
executable or a direct Windows shortcut instead.


### User-supplied Windows icon (13/10/2026)

P: replaced the executable/launcher icon with the image explicitly supplied
by the user, preserving transparency and proportions. The ICO contains
16/24/32/48/64/128/256 px variants; the installer uses the same ICO on its
next build. Local verification compared all seven compiled RT_ICON resources
byte-for-byte with the ICO entries. No game behavior changed.


### Launcher disc tile logo fit (13/10/2026)

P: the Game disc logo is centered within the Options tile and fitted to
72% of its icon slot, preserving the decoded image aspect ratio. This is
an intentional PC launcher layout adjustment, not a PS2 geometry claim.
The rebuilt launcher screenshot is build/launcher-window/options.png;
launcher_window and its negative exercise the valid-disc home screen.


### Composited launcher page transitions (13/10/2026)

P: native Windows page creation is enclosed in WM_SETREDRAW suspension;
the launcher uses WS_EX_COMPOSITED so the parent and descendant controls
are buffered together. Redrawing resumes after layout, values and focus
are ready, with RDW_ALLCHILDREN | RDW_UPDATENOW. This prevents newly
created Controls fields painting over the previous Home background.
launcher_window and its negative cover page navigation, settings and the
rendered completed pages; they do not measure display scanout during the
transition. No PS2 animation timing or gameplay was changed.


### Launcher panel margins and music mute (13/10/2026)

P: footer buttons were moved inside the details panel; the Controls player
selector now fits inside its header without overlapping the title. Removed
the duplicate Cancel/Default/OK footer text. Music: On/Off uses the disc
button artwork and persists mute_music in launcher.ini. Muting passes zero
BGM volume to the existing mixer while retaining menu effects and all game
volume settings; Audio preview and Test audio respect the launcher mute.
launcher_window exercises mute, persistence across reopen and restoration;
its negative remains active. launcher_config and launcher_audio cover
configuration and the mixer (including mute) with their negative cases.
Screenshots are build/launcher-window/controls.png and audio.png. Layout
is a deliberate PC adaptation rather than a claim of PS2 pixel parity.


### Launcher Play consumed by SDL event pump (13/10/2026)

P/C (PC integration): SDL WIN_PumpEvents consumes WM_QUIT with
PeekMessage(PM_REMOVE). Play dispatched during controller polling destroyed
the native launcher, but GetMessage then waited forever with BGM alive.
Exit now also checks IsWindow before waiting; controller polling returns
when SDL dispatched destruction. pump-play queues Play through SDL itself.
The uncorrected build timed out; the corrected launcher forwards BOOT and
the packaged 0.3.2 opens the native game window. 18/18 launcher tests pass.
Release 0.3.2 also restores the existing FFmpeg binary and license omitted
from 0.3.1. No physics or disc-version logic changed. Friend PC validation
remains pending; local reproduction covers the reported hidden-process hang.


### B-Spec experience progression (14/10/2026)

E (bounded arithmetic): career_bspec ports 00468710 seed and 00468860
writeback, band selector 004683B8/00468418, victory doubling, u16/u8
saturation and course aggregate deltas. bspec_progress compares 4096 full
banks/style tables/course bytes with locally recompiled original code;
negative flips one output byte and fails. Boundaries replace profile lookup,
course lookup/label and course byte store. The original kind=0 branch leaves
four temporary bytes unwritten; this synthetic boundary supplies zero for
those bytes. Runtime 00468A40 integration returns -1/1, not that branch.

C (integration): prepared GT events/championship rounds seed the existing
003F1E60 player-style consumer before any driver samples, using live initial
parameters +FE0. Existing native driver_stats accumulates data unchanged.
Result writeback, before wear/replay, persists the 0x180 bank and per-course
8-byte skill through the existing profile backend; imported +228 course
bytes are retained. Reverse suffix 24 uses 00468690. Once-only writeback
and replay exclusion prevent duplicate awards. No solver, dynamics constant,
AI formula, input recipe or physics update cadence changed.

bspec_career runs a prepared Sunday Cup B-Spec race to physical finish
(8517 ticks), compares writeback, Retry seed, duplicate result, native
save/store/reload and replay exclusion. Its negative changes the checked
bank. gtmode_home getter oracle regressions also pass: 6/6 focused tests.
Logs: build/bspec-completion-tests.log and Testing/Temporary/LastTest.log.
Release executable updated locally; no server publication in this task.
Full PS2 trajectory, every car/course/mode and visual status comparison
remain A. This closes the missing career experience producer, not all
B-Spec presentation/behaviour equivalence.


### Brake lamp material lighting (14/10/2026)

C (PC GPU dependency): removed the global lamp==2 -> unlit override in
render3d. Callback 0 (00453698; MDLS dispatch 00455708) selects released/
pressed geometry and textures; material kind remains independently decoded.
The previous override was explicitly A and lit all triangles in the pressed
branch, including non-emissive lens areas. Authored kinds 0/5 still use their
existing unlit path; other materials retain scene light/specular/reflection.
This supersedes the earlier approximate full-brake-branch unlit treatment.

brake_material compares full framebuffer pixels of a lit material in normal
and pressed branches (identical), versus an authored unlit control (different).
Negative changes one output byte and fails. brake_material_focus loads both
real race Focus assets: focus_rs_02/fdeu0005 has 8 affected triangles;
focus_st170_jp_03/fdeu0007 has 66. Complete car pixels match an independently
selected pressed stream with original material kinds retained. car_brake_branch
and negative preserve the MIRA join regression: 5/5 focused tests passed.
Evidence: build/brake-material-final-tests.log and Testing/Temporary/LastTest.log.
Full PS2 light/texture/glow appearance remains A pending original image
comparison; these tests do not establish whole-car PS2 raster equivalence.
No pedal, physics or material texture values changed. Local executable updated.


### Continuous career validation and broader licence evidence (14/10/2026)

C: a persistent isolated career slot now spans separate physical series
processes. Prior car bodies/records, credits and prizes are checked after
reload; original PresentTimeData and CourseData check dated course unlocks
and authored reverse directions. Positive/negative tests cover stale records,
bodies and availability. Fixture ownership and controller inputs remain
explicit boundaries: this is not proof of every paid purchase or admission.
The harness now stays in quick-championship between rounds and completes
the authored prize chain at each GT Mode return; Dream Car/25-percent
Nuvolari and the mid-series 50-percent XJR-9 are each delivered once.
Controlled 25/50/100-percent award tests cover bodies, fixed colours, flags
and one-shot eligibility across save/load and championship VM switches. Failed FGT standings are not counted as championships won.
Current accepted branch: 142/144 series, 510 physical stages, 144 awarded
cars, Cr. 13628090 and day 655; 511 recorded races include one preserved
paid attempt. All dated forward/reverse course unlocks pass after reload.
Snow-hard rh_nursnow_h_0000 and Formula GT ex_formula_1500 remain unproved
on that branch; full-catalog continuity remains pending.

C: physical mission cycles 1, 4, 8, 9, 21, 22 and 24 validate original success
board, actual grid, payout, persisted rank/credits and reload (zero tolerance).
The standard cycle also preserves the physical losing mission 10 case.
The other 27 native victories and full PS2 presentation remain unproved.
Large rewards use original makePrizeFast/watchers rather than a fixed wait.
Drivers are P test inputs, never injected car state, finish or rank.

E within the evaluation-method boundaries: 80 actual licence setups / 400
RAM checkpoints and 183999 frame evaluations from student attempts using disc
tapes match original 003F8D70; car/event callbacks are shared boundaries.
Negative byte changes fail. These are not 80 complete PS2 trajectory, medal,
demonstration or rendering comparisons; that aggregate debt remains A.
No game dynamics, vehicle parameters or cadence changed. Details and local
logs: [continuous audit](notes/CAREER_VALIDATION_20261014.md).
Focused regressions: career/presents 10/10, missions 15/15, licences 8/8;
no whole-suite run and no executable publication.

### 16/10/2026 — Optional desktop menu input (P)

- **P**: `controls.desktop_menu`, Classic Off / Enhanced On, in the native
  Screen Settings PC extension. Mouse hover, left release, right Back,
  wheel/menu-bar steps and WASD feed the existing focus/activation bridge;
  arrows/Enter/Escape/pad retain their previous adapter in Classic. No
  solver, vehicle parameter, simulation cadence or replay format changes.
- Reuses the disc cursor and its existing angle update (original
  `func_0022FFD0`, `gt4_cursor_update`), authored focus faces/sounds and
  transitions. Immediate mouse-tip placement is intentionally P, not a
  PS2 behavior claim. Hit testing uses painted/clipped geometry including
  live list offsets and dropdown overlays. Native PC callbacks now bind
  their receiver explicitly; AO_NATIVE is not auto-bound by the VM.
- `options_store` imports video.ini into options.ini without deleting the
  source, preserves unknown keys, saves atomically and tracks preset edits.
  Registry/limitations: `docs/OPTIONS.md`. Existing original game-options
  sidecar/save formats and Switch preferences are preserved. No launcher
  source changes; named user presets remain pending.
- **12/12 focused tests passed**: `desktop_pointer` and intentional negative
  check Classic no-op, hovered focus, rendered cursor tip, dropdown selection,
  insensitive/hidden/outside controls, pad handover, keyboard/text separation;
  `desktop_events` and negative inject SDL motion/buttons/wheel/keyboard;
  `desktop_options_store` and negative verify migration, persistence, both
  presets and Modified. `desktop_settings_app` checks both values, OK/Cancel,
  reload and presets in the real executable. Existing `options_lifecycle`
  family (4) and `menu_navigation` passed. Full suite was not run.
- Captures/logs stay local under `build/desktop-settings-test/` and
  `build/desktop-tests.log`. No automatic PS2 pixel-equality claim; new
  desktop input is P. Slider dragging, desktop remapping and specialized
  Photo/Logger mouse tools are pending, rather than silently approximated.


### P — Desktop hover, scrolling, map and pause refinements (16 October 2026)

- Optional Desktop Input now separates mouse hover from controller focus.
  Painted/clipped button bounds include descendants; hover invokes original
  widget faces and preview callbacks without selecting or scrolling a list.
  Arcade course evidence: original `set_crs` (local Adhoc dump, lines
  2478–2546), `CrsScroll.SelectBox.onValueChanged` (line 2679). The real
  car/course projects are exercised with the user's disc, including changing
  course image paths while retaining list index and viewport offset.
- Original modal OK prelight is tested without arrow-key selection. Cursor
  ownership/position survives dialog opening and animation. GT Mode hover
  uses the authored landmark rotation (DealerGerman velocity 0,2,0); optional
  edge pan moves the viewport without changing the focused landmark.
- Race pause mouse bounds come from the same STRB placement/shape records
  used by its renderer; hover/click executes existing button transitions and
  callbacks. The cursor is loaded from the user's disc. No game assets are
  committed. Controller-only paths remain available with Desktop Input Off.
- New registered settings `controls.gtmode_edge_pan` and
  `interface.input_hints` have Classic Off / Enhanced On values. Legends are
  transformed only at presentation, according to the latest input device.
  Wheel/edge rates are deliberate PC behaviour, documented in OPTIONS.md,
  not claims about PS2 timings. No vehicle physics or save/replay format
  changes are part of this work; Switch is not migrated.
- Validation: 18 focused CTests passed (desktop navigation/pointer/events,
  store and intentional negatives, existing options lifecycle/menu navigation,
  pause buttons and pause confirmation), plus `desktop_settings_app` against
  the updated main executable: both values, Apply/Cancel/reload, mixed values,
  Modified, Classic and Enhanced. Popup hover also compares rendered original
  button faces and cursor coordinates after animation. Local captures/logs:
  `build/desktop-settings-test/`, `build/desktop-adaptation-tests.log`.
  Full suite was not run. Slider dragging, desktop remapping and specialized
  Photo/Logger mouse tools remain pending. This is P, not a pixel-equality
  claim for the new interaction model.


### P — Optional expanded original-style main menu (16 October 2026)

`interface.main_menu_overview` presents the populated original Arcade
`TopRoot.StartScroll.SelectBox` as a five-entry vertical viewport, using
its original localized labels, arrows, backgrounds and actions. Wheel
scrolling changes the viewport without changing selection; hover brightens
the pointed entry and the full painted row accepts a click. There are no
new panels or columns. Positions, five-entry capacity and readable neighbour
opacity are deliberate PC presentation, not claims about PS2 behaviour.
Classic leaves the original renderer/list geometry and actors untouched;
turning the option off restores its original presentation. The setting is
persistent, included in Classic/Enhanced and exposed only in the in-game
Screen Settings extension. Switch does not enable the override.
Validation: desktop_navigation checks all five visible rows and far edges,
hover targets, wheel access to the final entry without selection changes,
restoration and the original Options action. Store tests check both presets;
desktop_settings_app checks persistence, individual Off, Apply/Cancel and
modified presets. Captures remain local in build/main-menu-overview.png
and build/main-menu-classic.png. No game assets are committed.


### P / UI corrections — Mouse feedback and list bounds (16 October 2026)

- P: `interface.smooth_mouse_cursor` and `interface.mouse_hover_feedback`
  are individual shared settings, Classic Off / Enhanced On, on the in-game
  Screen Settings Controls tab. Position (30 ms), angle (55 ms), icon
  crossfade (90 ms) and gold prelight (60 ms) are deliberate PC effects;
  details/limits in OPTIONS.md. Original cursor align values and disc icons
  are reused. A new mouse/modal context starts at the actual pointer, and
  pad/controller updates still use the original cursor update when Classic
  disables desktop input. No physics, replay or save format is changed.
- Main menu hit testing accepts the full row even beyond the narrow authored
  client, including both ends of Gran Turismo Mode. Hover feedback paints
  an eased gold tint/outline on real Options tiles and Exit, independent
  of controller focus. Existing native confirmation styling is retained.
- Dropdown hover now selects the authored prelight state of the pointed
  item, without committing the pending/selected value. Popup capacity is
  bounded by actual row height, rather than assuming 19 rows always fit.
  Tall vertical selectors/ListBoxes clip to their authored viewport;
  the short original main selector retains its neighbour overhang. After
  wheel scrolling, the paint interval follows the viewport offset instead
  of the unchanged controller index. These are UI integration corrections,
  not a new assertion of PS2 pixel equality.
- P: Screen Settings PC pane is wider and has a continuous light-gray body;
  original title/frame/fonts/buttons are retained. Switch layout is unchanged.
- Area tests: desktop_pointer checks exact direct mode, no popup pull,
  monotonic easing/settling, 30/60 Hz agreement, original-angle settling,
  icon crossfade draw counts/alpha and authored popup hover faces;
  desktop_navigation checks whole-row hover, actual gold draw calls on
  Options tiles/Exit, Off suppression and CourseRoot clipping/last row
  visibility after wheel scrolling. Store/app tests cover both new values,
  persistence, Cancel, both presets and Modified. Intentional negatives
  remain active. Captures/logs stay local under build/mouse-feedback-*.
  No full suite or console-wide visual equality claim.

Validation result: 10/10 focused CTests passed (including existing Options
lifecycle and intentional negatives), plus the real executable settings
exercise. Final hidden-window captures checked the wide gray pane, opaque
dropdown and Course Registration bottom clipping. Main executable rebuilt.


### P — Preserve authored hover and connect cursor context (16 October 2026)

Supersedes the generic gold overlay described in c08abd0: all rectangular
hover tint/border drawing was removed. GT Mode landmarks retain their
original ModelFace rotation and main-menu text retains its existing prelight.
Hover Feedback only enlarges otherwise-static Options icons (up to 4.5%)
and text (up to 2.5%), eased over 60 ms. Authored state-mask/model hovers are
excluded; no new assets or game-rule changes are involved.

Smooth Cursor now maps the mouse position within default-alignment targets
to the original nine angle values, preserving explicit authored alignments.
Horizontal-strip edge scrolling supplies directional cursor context. The
mapping/rates remain deliberate PC behaviour, disabled by Classic; original
pad cursor updates and script-owned cursor icons remain intact.

Validation: eight focused CTests passed: desktop_pointer/navigation and
intentional negatives, plus four Options lifecycle checks. Render-call
checks reject the removed gold overlays, compare icon geometry with feedback
On/Off and cover Exit. Cursor tests cross a single default-alignment target
left/right, verify intermediate/settled angles, exercise strip-edge turning,
and retain icon-crossfade/Classic checks. No full suite or new E claim.


### P — Cursor inertia with continuous interaction feedback (16 October 2026)

Smooth Cursor keeps the visual distinction between free pointer, clickable
target, explicit authored alignment, strip-edge direction and script icon.
The abrupt nine-zone mapping from 90b4060 is replaced by continuous 0..-16
degree resting tilt within default-alignment targets. A tip-anchored 6%
hover enlargement eases over 60 ms. Position/rotation use presentation-only
critically damped springs (70/s, 45/s); horizontal velocity/acceleration
supplies bounded +/-12 degree lean which settles at rest. Cursor lag stays
bounded to 24 UI units, and icon crossfade stays at 90 ms. Constants are
intentional PC design, not claimed PS2 timings. The existing option remains
Classic Off; pad cursor, raw hit testing and vehicle physics are unchanged.

Focused cursor tests cover 30/60 Hz position agreement, stationary settling,
continuous target tilt, clickable/free size distinction, bounded direction
reversal/lean, explicit alignments, strip-edge direction, icon crossfade and
raw-mode position. This supersedes the earlier mouse-motion description;
no full suite or new original-game fidelity grade is claimed.

Result: 4/4 focused pointer/navigation CTests passed, including intentional
negatives. Main Release executable rebuilt successfully.


### P — GT Mode edge navigation over text bands (16 October 2026)

Map Edge Pan now accepts the full TopRoot pointer area, including the black
text/footer band outside the map's clip widget. Pointer coordinates are
clamped to map edges before computing pressure, retaining the existing speed
and content bounds. Icon hit testing is unchanged and stays clipped; footer
clicks do not activate hidden landmarks. Classic/Off remains inactive.
Desktop navigation positive/negative tests cover footer downward pan, Off,
selection retention, content limits, outside-screen rejection and no footer
click-through. This is a refinement of controls.gtmode_edge_pan (P), with no
Switch, physics, save or replay changes. Only focused area tests were run.


### 16/10: montadoras por mouse e scroll de regulamentos

- **P** Desktop Input: BuyCarRoot.CarScroll mantém a pré-seleção sob o mouse
  sem mover a faixa; clique chama onActivate/set_col_select do script.
  EventInfoDialog usa onKeyPress original no wheel da área de regulamentos
  (passo 40, clamp pela altura), sem competir com MoveActor.
  desktop_navigation e gtmode_regulation_text cobrem On/Off, prévia/clique,
  posição e alcance até a última linha do evento pickup; negativos ativos.
- **C** Ponte de entrada da montadora: removida exclusão de BuyCarRoot da
  pose original; tz 8.5 -> 0, ry 180, interpolação 0.025 e fade nativo
  00153B50/00153BB8. desktop_navigation verifica parâmetros/progresso;
  gtmode_service_placement compara fórmulas de posição/fade com EE original.
  Não é prova visual integral: iluminação/reflexo aproximados continuam A.
- **E (dados delimitados)** Placa Ford: teste lê /piece/tuner_logo/ford.img
  escolhido por DealerRoot.onInitialize (instruções 54..63), 378x135,
  preto opaco interno e canto transparente. O preto integra a imagem.
- Miniaturas menores do vídeo e divisórias dos regulamentos continuam
  pendentes; nenhuma correção de tamanho/linha foi declarada concluída.
  Evidências e limites: docs/notes/DESKTOP_MANUFACTURER_VALIDATION.md.


### 16/10: miniaturas, gradientes e barra horizontal

- **E (cores/ordem delimitadas)** ColorFace: 002854F8 +E0..118 lê as cores
  0,1,3,2; corrigida a ordem dos cantos inferiores em color_list. O teste
  desktop_navigation grava o quad real de CourseRoot, confere vermelho à
  esquerda e alpha zero à direita; MOON_GRADIENT_NEGATIVE rejeita inversão.
- **C (integração)** SelectBox: interpolação 60 Hz de 002D5660/002D6868,
  dimensões intermediárias e avanço dos vizinhos; barra usa deslocamento
  negativo do SelectBox, mantém regra distinta do ListBox. Teste leva a
  última pista e volta, confere movimento e limites da barrinha.
- **P** interface.compact_car_strip e interface.regulation_separators:
  miniaturas 80% e corte do fundo pela margem inferior original (4).
  Classic desliga ambas; não são prova de tamanho/linhas PS2. Causa das
  divisórias originais permanece A no registro anterior. Testes alternam
  70/56/70 pixels e 48/44/48 com geometria/conteúdo preservados (tol0.01).
- Nove testes focados passam, incluindo aplicativo real, presets, gravar/
  recarregar e ambos valores. Capturas locais revisadas. Barra e gradiente
  corrigidos na UI compartilhada; sem validação no hardware Switch.
  Evidências/limites: docs/notes/DESKTOP_MANUFACTURER_VALIDATION.md.


### 16/10: hover de cores e retorno à primeira amostra

**P** Desktop Input retém o índice de prévia das cores, em vez de restaurar
zero após trocar a pintura. Assim voltar à primeira amostra chama novamente
onValueChanged/set_col originais, inclusive após sair e voltar à faixa.
Hover Feedback também compõe verniz suave e reflexo animado sobre a amostra,
sem mudar tamanho, RGB ou pintura do carro; Off/Classic omite esses passes.
desktop_navigation usa SVT F-150 Lightning 03 com múltiplas pinturas e confere
1/0/1/0, índice do CarFace, posição fixa da faixa, reflexo mudando no tempo,
On/Off/On e mouse desabilitado. Quatro testes focados pointer/navigation,
incluindo negativos, passam. Executável Windows recompilado após fechar.

### 16/10: pre/post-race car-list abbreviation coverage (C, scoped)

`tests/ui/pregrid_menu.c` now checks the actual retail Adhoc initialization
of quick-event and quick-championship before/after a race, and quick-arcade
before/after completion. Fixtures include the 1949 Volkswagen 1100, three
New Beetles, Ka, CELICA, Karmann Ghia and Lancer Tommi Makinen. Names that
fit are preserved; overflowing names end in three period characters and
measure narrower than their own authored TextFace column. The scripts
already call `abbreviate`; no production behaviour was changed in this audit.
`gtmode_machine_menu_return`, `hud_font_original` and its deliberate negative
passed. The latter compares the shared abbreviation/metrics against original
0048F790. C applies to list initialization and measured strings, not final
pixel visibility. The reported cut-off name in the Start/Replay list has
not been reproduced; do not mark that visual report resolved. The separate
in-race result movie and F-150 reflection facets remain outside this proof.

### 16/10: regulation dividers moved to baseline presentation

Jonhline clarified that the row cuts are original presentation, not a player
improvement. Removed Regulation Dividers from Screen > Rendering, preset
writes and runtime option reads. The legacy `interface.regulation_separators`
key may remain in old files but has no effect; no save format changes.
RegHeader, RegDesc and RegDescLicense background paint now always leaves the
resource's `pad_bottom` (4) open. Text positions, row heights, clip and scroll
limits are unchanged. Compact Car Strip remains independently optional.

C (native integration only): `gtmode_regulation_text` checks the background
height against row height minus resource padding in Sunday Cup, FF Challenge
(normal/violation) and Sports Truck, including the 44/48 paint/row case and
wheel/keyboard access to the bottom. This supersedes the optional P treatment
in the 16/10 readability note. It does NOT establish exact original geometry:
002854F8 draws the complete allocated ColorFace; the full FBox/MBox allocation
and a matching PS2 regulation capture are still needed to certify where the
original cut comes from. That fidelity debt remains A; do not call it E/C
against the original merely because the native regression passes.

Validation: 7/7 focused CTest cases passed; the divider-height negative
failed as intended (exit 1). A hidden Release executable accepted the legacy
Off key and showed Rendering without the retired setting. Release rebuilt.

### 16/10: original regulation geometry audit (supersedes guessed divider)

Executed original `002854F8` through the EE analysis oracle: five allocated
heights (48,55,68,128,220) submit vertices (0,0),(100,0),(0,h),(100,h),
with no four-pixel reduction. Executed original MBox allocation `002B8778`
for four rows in 220 pixels and five rows in 240 pixels, two columns,
zero spacing and original expand/fill flags: y=0,55,110,165 and
0,48,96,144,192, with full row heights. Exact assertions, zero tolerance.
Traversal, eligibility, measured requests, geometry access and recursive
child drawing are explicit boundaries; this is not a complete FBox/layout
or raster proof. Retail templates declare pad_bottom=4, but neither test
uses that as a paint cut. `regulation_geometry_original` combines those
original checks with real retail EventInfoDialog integration. Its negative
subtracts padding from the captured native paint height and must fail.

Removed the named RegHeader/RegDesc/RegDescLicense paint-time subtraction.
It was an unsupported approximation and the previous baseline claim did
not establish it. The option stays retired. E is limited to the original
vertices and MBox allocation fixtures; native dialog presentation remains
A until the reported visible cuts are traced through the raster path or
compared with an original capture of this exact popup/edition. This audit
does NOT claim that the user's missing-divider report is fixed.

### 16/10: retail Sunday Cup reference supplies the missing visual evidence

Jonhline supplied a retail screenshot
and confirmed retail GT4 v2.00. The reference shows thin transparent cuts
between all four regulation rows, with no cut below the last. At header
column scale ~1.3, the three low-contrast seam bands occupy 2--3 image
pixels (sample y=155..157,227..228,299..300), consistent with a two-unit
logical seam, not the previously guessed four-unit padding subtraction.

The native draw boundary now leaves two logical units transparent only
when another visible MBox row follows. Applies to RegHeader, RegDesc and
RegDescLicense, including Classic. No row geometry/text/prize/scroll changes.
`gtmode_regulation_text` checks first-row paint height h-2, last-row full
height, compact-strip independence and Sports Truck wheel/keyboard limits.
Four focused tests pass, including the original geometry oracle and negatives.
Release rebuilt. No option, save change or push.

Scope: the two-unit coverage is reference-derived reproduction, not a claim
that original 002854F8 itself subtracts two units. Its full-rectangle and
MBox-allocation proofs remain valid; native presentation coverage is tested
separately. The GS/face-allocation mechanism behind the original visible seam
remains A; no complete pixel identity or source-derived seam formula claimed.

### 16/10: slot 2 proves the regulation seam comes from GS scissor rounding

Read-only audit of SCUS-97328 (77E61C8A).02.p2s, saved 22:53:39,
SHA256 83c15cacc3d6c2ef807d46f69141546270777bbb44609f9533746e0674f1d044.
The local EE objects confirm four 55-unit FBoxes, backgrounds normalized
(0,0,1,1), global row Y=79,134,189,244. Original 0025B498 executed over
this RAM returns full 100/300 by55 backgrounds. Their recorded GIF packets
have SCISSOR1 X=120..219 and Y=36..60,62..86,88..112,113..137. Thus the
first two boundaries omit one GS field row; the third has no omitted field
row. Screenshot resampling must not be interpreted as constant padding.

Source chain: 002311D0 intersects widget bounds, 00215F78 invokes 00215FE0
virtual-to-framebuffer conversion, then 004A2DF8 halves interlaced Y and
truncates origin and extent SEPARATELY. 004A2E98 packs inclusive max as
origin+extent-1. This explains why trunc(y+h) would erase the actual seams.
`gt4_ui_scissor` reproduces the conversion; the regulation FBox boundary
uses its quantized clip while leaving every ColorFace rectangle full-size.
Removed the screenshot-derived constant two-unit subtraction. No option.

E scoped: `regulation_geometry_original` compares 804 conversions to the
recompiled original 00215FE0/004A2DF8, exact integer extents, including all
four captured rows. Framebuffer queries and final GS-state write are named
boundaries. The own negative changes one returned extent and fails. Existing
ColorFace/MBox original checks are retained. `tools/regulation_savestate_audit.py`
independently reads the user's .p2s, computes row clips from its objects and
finds the matching recorded SCISSOR1 packets; its negative changes one bound
and fails. Memory, screenshot and audit JSON stay under captures/, untracked.

C native integration: actual retail EventInfoDialog retains full background
heights, uses a quantized clip bottom, and retains text and Sports Truck
wheel/keyboard scroll limits. 5/5 focused tests passed, including negatives.
Release rebuilt. The current adapter is scoped to the captured retail NTSC
interlaced 640x448 (640x224 field) presentation; other original video modes
and full final framebuffer pixels are not certified by this proof.


### 17/10: UsedCarRoot sold-out stamp

C (native integration), A (complete PS2 framebuffer equivalence): the retail
UsedCarRoot Item.SoldOut resource specifies geometry (420,7,64,56), rotate
-15, hb_24 at scale 1.4, orange (230,134,0), shadow 2.7. Its multiline flag
was incorrectly treated as a request for automatic word wrapping, producing
SOLD\nOUT. Bypass that adapter for the authored stamp, retaining explicit
newlines and all other faces' existing wrapping behavior.

TextFace now submits rotated ASCII glyph atlas regions, including shadows,
through the existing batched 2D path. Pivot source: 002679B0 +0xFC..0x1C4
translates by half the widget dimensions before/after 0024BEF8 rotation.
The new platform entry exists for SDL/OpenGL/Vulkan and deko; Switch hardware
was not tested. Combined skew/rotation and rotated Gaiji/KNJI are not covered
by this change; no claim of a complete widget-transform implementation.

used_car_stamp loads the actual retail menu, forces one stamp visible, draws
120 frames, checks seven orange glyphs at -15 with a common pivot and no
inserted newline. The negative explicitly reinstates the previous wrap and
fails. desktop_navigation and its negative pass. Release rebuilt. These
checks do not certify final pixels or reproduce the reported slowdown:
headless timings omit driver/GPU/audio and are diagnostic only. No performance
fix or PS2-exact rendering grade is claimed from them.


### 17/10: PC Screen Settings footer allocation (P)

The widened PC pane moved Cancel/OK but retained the authored 320-wide
footer background allocation. The footer now shares PCBody's inset/width,
its background explicitly fills that rectangle, and its two buttons remain
centered. PC adaptation only, no option or original/Switch layout change.
Hidden real-executable screenshots reproduced the old truncated strip and
confirmed the corrected span; symmetric empty footer pixels differ before
and agree afterward (local build/footer-before.png, footer-fixed.png).
Build succeeded as MoonEngine-footer-proof.exe; replacement of the running
MoonEngine.exe was blocked by Windows. No PS2 framebuffer equivalence claim.


### 17/10: mouse feedback on the green Exit pictogram (P)

interface.mouse_hover_feedback now applies the existing eased 4.5-percent
image enlargement to image/labo/exit.png and exit_over.png under Exit,
including roots outside OptionRoot. Preserve authored image selection;
no added rectangle, replacement artwork or controller animation. Classic
and Hover Feedback Off bypass this adaptation. desktop_navigation explicitly
hovers OptionRoot.Exit, checks its animated prelight, then compares drawing
with the setting Off/On. Positive and negative tests pass. Main executable
was locked; the corrected build is build/Release/MoonEngine-hover.exe.


### 17/10: hovered car thumbnail owns the original strip blob (P)

With desktop pointer and interface.mouse_hover_feedback enabled, horizontal
strip magnification targets the hovered item rather than the retained
controller focus_index. Reuse the resource magnify_ratio and existing
002D5660/002D6868 interpolation (0x3E4CCCCC per 60-Hz tick), including
relaxation of the previous thumbnail. Do not move the strip or overwrite
controller selection. Feedback Off/controller retain the old target.
The actual Arcade Ford catalog regression checks preview, unchanged index
and offset, enlargement of the hovered card, relaxation of the old one,
and Off/On reversal. desktop_navigation and its negative pass. Main Release
rebuilt successfully. P for input mapping, no new PS2 pixel-equivalence proof.


### 17/10: desktop paint, adjustment bars and hover audio (P)

Hover Feedback adds the existing eased 4.5-percent enlargement to paint
chips, including their clearcoat/reflection. Raw widget hit bounds, RGB and
controller presentation remain unchanged. Slider/ScaleBar decorations also
receive feedback and expose their existing popup on hover.
Desktop Input adds a code-drawn white adjustment hand at the pointer hotspot
over bars (intentional desktop artwork, not claimed to be an original sprite).
Integer SliderBar/ranged ScaleBar click/drag maps to authored min/max and
knob travel, uses assign_index and onValueChanged, clamps beyond endpoints,
and releases capture on release/disabled pointer/page changes. Non-ranged
photo ScaleBar direct manipulation is not covered. Left press/held-motion
are new SDL event actions; normal buttons still activate on release.
Original hover callbacks continue animating but their sound.play requests
are suppressed during hover dispatch. Emit one pointer-navigation sound on
entry, none on leave, coalescing entries within 100 ms. Controller sound
requests outside hover dispatch remain untouched. This adaptation belongs
to controls.desktop_menu, rather than modifying original pad audio.

Tests: desktop_navigation measures swatch enlargement, renders 20 hand rows
instead of the arrow, exercises bar click/drag/clamps/release and counts actual
sound playback requests (entry/leave/reentry/cooldown/script suppression and
non-hover playback). desktop_events checks press/held/release translation.
Both existing negative cases still fail as expected. Full listening comparison
and whole-game coverage are not certified by these checks.

### 17/10: preserve the authored Quick Setup entry fade (C integration)

Retail setting.ad onLoad L32..33 constructs MColorTransition then calls panIn.
The app called menu_setting_begin before its generic cached-fade clear,
therefore erasing this request. Clear the cached SettingProject fade before
onLoad and skip the later clear for that project. Keep the existing original
colour-transition period (00286EA0, 0.25s), with no guessed extra loading
screen/delay. gtmode_setting_menu verifies the script leaves more than 0.24s
of fade; its negative reinstates the clear and fails. This proves the request
and integration order, not PS2-identical full-frame pixels or every other
project's loading flow.


### 17/10: project entry fades, including Logger (C integration)

Audit: tests/ui/project_entry.py reads project-level onLoad bytecode directly
from 21 retail project files (the save route uses boot). Each constructs a
MColorTransition; ten call panIn: arcade, gtmode, boot, setting, event,
labomode, photo_shoot, logger, slide and photo_save. Logger source: logger.ad
L20..25; Setting: setting.ad L30..33. Quick race projects do not request
project-entry panIn and retain their separate page/race transitions.

menu_project_entry reconstructs this original transition fragment at the
project boundary, for fresh and cached entries. Use MColorTransition's
existing constructor/period (00286EA0), not a new universal duration. Clear
stale cached fades before entry scripts; never erase their new requests
afterward. The first menu_enter also prepares the fragment for direct
viewer entry. Holding black/pending reveal follows the context-update pause
around startPage (0022F6A0); asset-loading animation time cannot consume the
new reveal before the destination commits. Page-owned transitions remain
script driven. No new loading screen or option was introduced.

project_entry_fades compares the native boundary against each disc-derived
request, twice (fresh/cached), including black held during 0.5s simulated
loading and submitted GS-quantized alpha 1 -> 68/128 after seven ticks -> 0.
The negative reinstates the erased Logger fade and fails. Six focused checks
passed including Setting and desktop navigation with negatives. Main Release
rebuilt. Real Logger screenshots/logs are local build/logger-fade-*.png/log;
they show the initialized LoggerMenu, not a certified temporal PS2 pixel
comparison. Coverage is project-entry requests/hold/alpha; all within-page
transitions and every full application route are not certified by this test.


### 17/10: original slider hand and manufacturer edge panning (P)

Replace the code-drawn hand with the actual image/common/slider/pinch.png
from the user's disc. Inspected decoded retail source: 36x59, value plate
above y25, hand ink x9..29/y25..55; pointer fingertip x17/y25. Draw that
atlas region at the pointer, without rotation; draw only the value plate
at the hovered slider's knob, avoiding two hands. Outside mouse hover the
original complete pinch remains. Desktop Input controls this adaptation.
No disc pixels are bundled. The 21x31 crop is native asset coverage, not
a new hand design.

Arcade MakerRoot/HistoryRoot clip-space panels now use the existing mouse
edge-scroll policy (10-percent edge zone, 1.5 viewport widths per second),
clamped to authored content dimensions. This is a desktop adaptation; it
does not change controller focus or the original geometry.
desktop_navigation checks the original hand texture dimensions/UV crop,
absence of arrow draw, and pans the actual manufacturer viewport left/right,
retaining focus; disabled pointer leaves it unchanged. Its negative remains.
Text placement and license bronze trophy reported in the same message were
explicitly deferred by the user; neither is claimed fixed here.


### 17/10: main menu blank side space is not clickable (P correction)

Removed the invisible x48..592 main-menu hit extension. Hover/click now
uses the row bounds recorded by its paint pass, preserving full-row access
without selecting through blank side space. desktop_navigation tests both
visible row ends, blank left/right space, clearing hover and ignoring a
blank-space release. Positive/negative passed. Main executable was locked;
corrected build is build/Release/MoonEngine-hover-bounds.exe.


### 17/10: fullscreen mouse confinement and Replay Theater return

P: Desktop Input now requests SDL mouse-only grab while fullscreen, focused
and not minimized. Reevaluate flags during event polling; Off/windowed/focus
loss release the request. Keyboard is never grabbed (Alt+Tab stays available).
The Switch path is a no-op. Existing controls.desktop_menu owns the feature.
desktop_events checks enabled/disabled, fullscreen/windowed, focus loss and
minimization. Actual two-monitor OS confinement remains a manual hardware
check, not claimed verified by the flag-policy test.

C integration: load_replay records the actual caller's project instead of a
stale per-VM major_menu_project default (arcade), and updates the shared
major project label. App copies the caller's major project/page context for
ReplayTheaterRoot as for PhotoRoot. Original labomode.ad ReplayTheaterRoot
exit L3258..3261 reads those fields and requests the menu sequence.
project_entry_fades now exercises original theater exit from both Arcade and
GT Mode, checking each return route; a manual negative forces the stale
Arcade origin for GT Mode and fails (build/replay-return-negative.log).
Four focused registered tests passed. Main remained open; combined build
including corrected main-menu hover is build/Release/MoonEngine-menu-fixes.exe.

### Switch touch cursor ownership (2026-10-17, P)

With Touch Menus enabled, direct contact hides cursor artwork until the next
controller/key input; release, cancellation and context transfer preserve
that state. Widget contact feedback and original controller behavior remain.
Three focused pointer checks include actual cursor draw counts and a negative
that forces reappearance. Native Release compiles. The user confirmed the
preceding libnx menu touch works; this cursor follow-up awaits console confirmation.

### 17/10: blob de mouse recortado na borda da faixa

P: com feedback de mouse ligado, SelectBox/ListBox horizontais revelam a
largura corrente da miniatura apontada, corrigindo o destino do scroll pelo
excesso em cada borda. Mantidos viewport, magnify_ratio authored e easing
0x3e4ccccc (002D5660/002D6868); controle/feedback desligado preservados.
Componente compartilhado, sem declarar todas as telas verificadas.
desktop_navigation usa BuyCarRoot original com viewport de 100, exige a
miniatura completa de 70 e deslocamento negativo; negativo próprio
MOON_BLOB_EDGE_NEGATIVE desliga feedback e falha no deslocamento.
Dois testes registrados passaram. Corrigido isolamento do fixture mouse
após fixture touch (cursor oculto), sem mudança de produção nessa parte.
Build Release principal atualizada; comparação visual em execução pendente.

### Switch touch selection before confirmation (2026-10-17, P)

At the user's request, Touch Menus now selects/previews on the first completed
tap and accepts only on a subsequent tap on the same target, without a timing
window. Native dialogs follow the same policy; sliders remain direct.
Root change, drag, cancellation or controller input invalidates confirmation.
Host tests assert zero activation after selection, one after the second tap,
no duplicate release activation, dialog parity and deliberate failure cases.
Six focused tests pass, including actual dropdown pending-versus-committed selection. Native Release builds; console validation of this follow-up remains pending.

### 17/10: Passed sobre o título do License Center

C para integração do selo: LicenseRoot no gtmode.mproject retail define
Passed em {-30,-52,128,128}, rotate=-20, label sem geometry/align. O port
usava default top-center e descartava rotação do Composite. Aplicados ao
label que ocupa o client o align=4 do construtor 00246F58 +C4 e o rotate do
pai, com pivô central como 002679B0 +FC..1C4 / 0024BEF8. Escopo restrito
a Passed; transformações genéricas aninhadas continuam sem certificação.
used_car_stamp agora executa LicenseRoot original, torna cada selo visível
isoladamente e confere seis glyphs, rotação, pivô authored e posição vertical
central nas cinco categorias. MOON_LICENSE_STAMP_NEGATIVE força o alinhamento
antigo e falha; dois testes registrados passaram, incluindo SoldOut.
Build principal Release atualizada. Pixels completos contra PCSX2 pendentes.

### Switch touch-only Back overlay (2026-10-17, P)

The user-requested Back button is displayed only after touch, inside the
existing `controls.touch_menu` option. Native disc confirmation-button artwork
is reused; the new placement, 150 ms smoothstep fade, eight-unit slide and
selection crossfade are intentional presentation differences. Two taps use the
same selection/confirmation rule; main-menu Back, native modal cancellation
and pause Escape retain their existing actions. Hidden/fading-out targets are
noninteractive; controller input hides the overlay and restores its cursor.
Seven focused pointer/navigation tests pass, covering visibility, two-tap Back, no duplicate release,
controller cancellation, 30/60 Hz reveal agreement and deliberate negatives.
Console image/input validation remains pending.

### Switch Back uses the original Exit icon (2026-10-17, P)

Replaces the visually rejected resized confirmation button with OptionRoot's
original green Exit, retaining authored 567/390 position, 58x40 dimensions,
child image paths and geometry. The fallback is suppressed when an existing
return control is painted; Options and native confirmations keep their own
controls. Four focused pointer checks include original Exit geometry/image
comparison, existing-control visibility detection, two-tap behavior and
intentional negatives. Placement/animation of the fallback outside Options
remain a presentation adaptation; console capture pending.

### Switch touch during title attract playback (2026-10-17)

P: With `controls.touch_menu` enabled, a touch-down during the automatic
title demo invokes the same attract cancellation and Escape path as a pad
press (0010BA28/0010BE20). Previously touch events returned before that
branch and could not leave the demo. Classic leaves touch disabled; normal
races and user replays do not acquire touch driving controls. Hardware
return-to-title verification is pending.

### Persistent touch hover (2026-10-17)

P: `controls.touch_menu` retains the desktop hover presentation after the
selection tap: authored alternate faces/models, paint clearcoat/reflection,
parameter-bar preview, strip magnification, and eased static Options/Exit
artwork. The same 60 ms easing is shared with desktop hover. Static targets
without authored feedback gain small gold corner accents. Selection is
communicated through the element itself, without instructional text. Visual
feedback does not change hit bounds. The detached green Exit uses the same feedback.
Confirmation, drag, cancellation, page changes and controller handover clear
the preview. Classic and desktop mouse behavior remain unchanged. Focused
host checks include persistent painted accents, hidden targets, handover
and a deliberate missing-preview negative. Console appearance is pending
visual verification.

### 17/10: hover do card inteiro nas listas

P: interface.mouse_hover_feedback amplia em 4,5% o conjunto de cada card
Composite de SelectBox/ListBox não horizontal com bg ColorFace, usando o
pointer_light suavizado existente (60 ms). Subárvore inteira recebe a mesma
escala/centro; geometria e escala de texto restauradas exatamente após draw.
Chip deixa de receber aumento duplicado quando pertence ao card ampliado.
Faixas horizontais continuam com seu blob authored, sem este novo efeito.
Implementação compartilhada por estrutura, não pelo nome UsedCarRoot; teste
usando Item retail de UsedCarRoot verifica nome, preço, especificação e chip
crescendo juntos, geometria original intacta e opção Off reversível.
MOON_CARD_HOVER_NEGATIVE desliga o efeito e falha no nome; used_car_stamp e
seu negativo passaram. Build principal atualizada. Cobertura visual de todas
as listas e recorte dos cards que cruzam os limites do viewport pendentes.

### Animated touch selection and protected paint samples (2026-10-17)

P: `controls.touch_menu` distinguishes rest, selection and the second press
through motion. Selection retains each scenario's shared desktop hover,
with a 220 ms arrival and a gently breathing underline outside the target.
Static non-card controls also receive converging exterior accents and an
initial light sweep. A second press compresses the shared hover before the
original release action/transition, without an added delay. The green Exit
uses the same treatment. Cards keep their whole-composition animation; no
gold corner or light sweep paints over their color sample. Classic and
desktop mouse behavior remain unchanged. Focused host checks cover 30/60 Hz
arrival, second-press compression, hidden/cancelled targets and missing-
preview negatives; the original UsedCarRoot fixture checks that gold
feedback never overlaps the sample throughout the arrival. Hardware visual
confirmation is pending.

### Touch feedback without decorative overlays (2026-10-17)

P: At the user's request, all added gold corners, underlines, breathing
highlights and light sweeps are removed. This supersedes the decorative
arrival described above. Touch selection retains only the shared desktop
animations and authored faces: whole-card enlargement, paint hover, strip
magnification, Options/Exit easing and second-press compression. There is
no instructional text or separate colored overlay. The original authored
artwork/colors remain authoritative. Focused host tests require zero added
gold draws while retaining eased selection, press/release response and
equal elapsed-time behavior at 30/60 Hz. Classic/controller behavior stays
unchanged; the paint-sample overlap negative remains active.

### 17/10: piscada ao cancelar showroom Arcade e música na pausa

C do caso de fade: arcade.ad CarRoot.CarScroll.SelectBox.onCancel L4301
pede panOutIn antes de start_page, cujo page_util.ad L40 pede novamente.
O bridge reiniciava a opacidade em zero na segunda saída. Agora usa a
opacidade atual; 00287350 +60..88 / 002873B0 atualiza o campo +20 existente,
sem limpá-lo a cada frame. Getter ui_fade_opacity mantém o preto alcançado.
desktop_navigation executa o cancel original após selecionar Ford no
MakerRoot e exige saída monotônica em todos os frames ainda de CarRoot.
Restaurar ui_fade(0,1,period) provoca falha nessa asserção (log local
build/fade-blink-negative.log). Transições parcialmente interrompidas A.

C do transporte PC solicitado para a pausa: race_audio_pause segura o
decoder PCM por atomic, sem silenciar os SE do menu. race_music_pause usa
PCM/racing sequência 8 retail, confere 120 blocos silenciosos sem avanço e
retomada sample-exact contra decoder independente; negativo remove a pausa
e falha. Isso verifica transporte, não prova o comportamento BGM do PS2.
Quatro testes focados passaram; nada alterado para loads/Used Cars após a
correção do relato pelo usuário. Principal bloqueado por processo aberto;
build alternativa pronta em build/Release/MoonEngine-pause-fade.exe.

### 17/10: orientação Android em paisagem

P: após relato de abertura integral em retrato, DiscActivity e GameActivity
reforçam SENSOR_LANDSCAPE em onCreate/onResume, além do manifest existente.
GameActivity intercepta SDLActivity.setOrientationBis para preservar essa
política durante criação/redimensionamento da janela SDL. Ambas as rotações
paisagem são aceitas; não altera o aspect ratio do conteúdo e não controla
o seletor externo de documentos. Base SDL release-2.30.9, SDLActivity.java
setOrientationBis é o ponto de substituição de screenOrientation.
Compilação Java/native e APK assinado verificados no host; aparelho não
conectado por ADB, confirmação visual no telefone pendente. Não declarar C/E
de orientação real com apenas esses checks de empacotamento.

### 17/10: conteúdo Vulkan vertical dentro do app em paisagem

P/C do contrato de apresentação: o blit final não pré-rotaciona os pixels,
mas vk_context.cpp declarava preTransform=currentTransform. Agora seleciona
IDENTITY quando suportado, deixando o compositor girar o quadro. Fonte:
Khronos surface_rotation (https://docs.vulkan.org/samples/latest/samples/performance/surface_rotation/README.html),
caminho sem pre-rotation; Android anuncia todos os transforms suportados.
vulkan_surface confere oito rotações/espelhamentos e fallback válido onde
identity não é suportado; negativo restaura declaração currentTransform e
falha. vulkan_frame/negative conferem comparação GL/Vulkan no PC. 4/4 passaram.
APK ARM64 Release recompilado, ZIP e assinatura v2 verificados. Correção de
orientação visual no celular ainda A até confirmação; não há aparelho ADB.
Composição pode custar banda/tempo adicional; pre-rotation nativa otimizada
continua pendente. Artefato: dist/MoonEngine-android-render-landscape.apk.

### 17/10: instalador Switch identifica somente SCUS-97328

P/C do contrato pedido: removidos SHA-1 obrigatório e VER=2.00 do instalador
nativo, manifesto e instruções do pacote. disc_identity.h lê SYSTEM.CNF via
ISO9660 e compara apenas o serial de BOOT2 a SCUS-97328; nunca decide pelo
nome do arquivo ISO. NRO/setup/transferências continuam com hash de integridade.
test_switch_disc.py executa o leitor real em fixtures independentes: VER 1.00,
2.00 e 9.99 passam; outro serial/número, lixo e CNF ausente falham. Disco real
local também passou. Self-test, teste FTP com negativo e dois testes de pacote
passaram. Confirma aceitação por serial, não execução integral de cada revisão.

### 17/10: falha USB após ISO no instalador Switch

P/C do host: SHCreateItemFromParsingName rejeita o caminho com barras `/`
que o pacote construía para o NRO. shell_item normaliza com make_preferred.
O self-test agora executa a transação Shell completa em pastas descartáveis,
confere ISO reutilizada, backup NRO, hash do substituto e configuração
preservada; publicação atrasada é aguardada e tamanho errado continua falhando.
Restaurar o parsing antigo provoca E_INVALIDARG 80070057 no self-test
(build/switch-usb-path-negative.log). DBI agora atualiza/cache e aguarda até
10 s a publicação dos arquivos. Sem prova física desse relato no console.
Logs persistem etapa/HRESULT; Nxlink captura saída e usa cinco tentativas,
sem declarar corrigida a descoberta real sem teste na rede do usuário.
Self-test, FTP/hash negativo, sete fixtures de serial + ISO real e dois testes
de pacote passaram. Mesmo NRO; nenhuma ISO ou save incluído no pacote.

### 18/10: setFocus durante onInitialize entrega o foco depois (Photo Travel)

C: o setFocus original (00233268 -> 00235C80) só guarda o widget em root+0xC8,
sob mutex, sem evento; o update do gerenciador (0022FE6C..0022FF4C) compara
com o foco entregue (0022F000) e manda FocusLeave/FocusEnter (00295260,
002946B8). Um setFocus dentro de onInitialize chega ao onFocusEnter depois
do resto do onInitialize. PhotoTravelRoot.init_menu (gtmode.ad L13605..13630)
foca o primeiro Ticket e o onInitialize esconde Slide em seguida (L13518);
Ticket.onFocusEnter (L13703..13712) carrega o swf e mostra Slide. Antes o
nosso disparava na hora e o preview do primeiro curso ficava escondido.
menu.c set_focus guarda o foco anterior enquanto a página inicializa e
deliver_held_focus manda os eventos ao fim de menu_enter/panIn (o onEnter
legado continua no mesmo lugar). Teste photo_travel_focus: Slide visível com
image/gtmode/photo/ss_louisburg.swf; com o disparo imediato fica 0; o
negativo falha. Ordem de várias trocas na mesma página: só a última entrega,
como no original. Comparação visual com o PCSX2 pendente.

### 18/10: hover do mouse não deixa a concessionária acesa (P)

P: com o mouse só o item sob o ponteiro aparece destacado (ui.c desenha hover,
não foco), mas o foco guarda a animação do próprio FocusEnter (arcade.ad
MakerRoot::hidden::Item L3601 bg.actor.out=1). Ao voltar do CarRoot o
fabricante clicado continuava aceso com o ponteiro em outro. pointer_set_hover
apaga a animação do foco (onFocusLeave de apresentação, sem setFocus) quando
o hover começa em outro item da página; o controle a acende de novo
(menu_send_input). Teste arcade_dealer_hover: clica ford, volta com botão
direito, passa sobre chrysler: ford 0, chrysler 1; sem a correção ford fica 1;
o negativo falha.

### 18/10: teclado do nome mostra maiúsculas na primeira letra (P)

P: a tela de perfil (save_ui.c sobre boot.mproject EntryRoot Keyboard, sem
script no disco; o EntryRoot do ranking.ad começa em minúsculas e não tem
maiúscula automática) já gravava a primeira letra maiúscula, mas mostrava as
teclas minúsculas. show_case mostra UpperCase com o campo vazio ou com Caps
Lock/Shift do teclado do PC, e as teclas na tela digitam maiúsculas com Caps
Lock; o foco fica na mesma tecla ao trocar. Teste save_keyboard_case: vazio
maiúsculo, "A" minúsculo, caps maiúsculo, apagar tudo volta a maiúsculo; o
negativo falha.

### Directional candidate collection (18 October 2026)

C: `0026F9E0` collection, `0026F868` intersections and `0026F7E8` empty-top
handling are ported at the UI boundary. A focusable widget stops descent only
when admitted; clipped-out widgets can still expose eligible children. The
nearest `navigate_box` of the current focus starts its own clipping/priority
group, with nested clips restored on return. No simulation code changes.
`menu_navigation_collect` compares the candidates, order, groups and both
projected regions against the recompiled original in 2,000 generated 40-widget
trees (four directions, hidden/insensitive nodes, nested and empty clips).
Widget getters are fixture boundaries; collection, recursion, projection and
clip-stack code run from the executable. Its WILL_FAIL negative corrupts a
candidate's group and is rejected by the same comparison. The comparator oracle
also compares 40,000 actual original clip-stack intersections.
A: class-specific default flags and original heap-address tie ordering are not
proven by those fixtures. Native stable collection order resolves exact ties;
visual coverage of all authored screens remains part of the dynamic tour.

### 18/10: faróis pela hora do dia da pista (callback 7 "timezone")

C: o callback 7 do modelo do carro (00453698, nome "timezone" na tabela do
ELF) devolve o +0x7C do objeto de desenho, copiado a cada quadro do +0x1680 do
carro (00390C90, setter 005F6FE8). A montagem da corrida (00387FB0) grava em
todos os carros o valor do Env0 +0x10 da pista: bit 0x80000 dá 2, senão bit
0x8000 dá 1 (noite), senão 0. No disco: valor 1 em hongkong, lasvegas, seattle,
route5(+club), motegi (3), sarthe/sarthe24/sarthe_old24, paris_rally e nos
ss_ de Brooklyn, Fremont, Gion, San Marco, Shibuya, Times e Tsukiji (e versões
r_); nenhuma pista usa 2. Antes o nosso escolhia sempre o ramo 0 (dia), e os
faróis nunca acendiam. model.c escolhe o ramo da pista (fora da tabela segue
o caminho padrão); race_view.c lê o Env0 e passa por car_model_tables_time_zone.
Teste car_time_zone: os 600 modelos /car/lod decodificam nos dois; o dia é
idêntico ao de antes; 597 têm o callback e mudam à noite (ac__0014: 3664 ->
3668 triângulos); o negativo (noite = dia) falha. car_brake_branch e negativo
seguem.

Safety car: 00388290 compara o rótulo do carro (00447CA0) com D_0061870C =
"r34_gtr_v_gtcolor_pc_01" e, se for ele, grava 1 mesmo de dia: luzes sempre
acesas. race_view.c faz o mesmo por carro.

Brilho das luzes: depois da sombra, 00451D18 +17C desenha o modelo 3 do carro
(00452998) no mesmo quadro da sombra do modelo 2 (pose no chão, 0.02 acima,
viés 0.008) quando o slot +0x78 do carro é maior que 0. Esse slot (003D7430,
lido na vtable 0x682F98 do carro numa captura de corrida) devolve o Env0
+0x58 da pista: 0 nas de dia, 0.2 a 1 nas de noite (route5 0.75, hongkong 0.5,
sarthe24 0.25, ss_times 1). Escala de cor (1, 1, 1, esse valor) e deslocamento
0 (004A60E0/004A6100), mistura do próprio modelo. O modelo 3 existe em 560 dos
600 carros: um quadrado no chão logo à frente do para-choque (ac__0014: z
-2.39..-1.78 com a carroceria em -1.87..2.10, alfa de vértice 19/128).
car_model.c carrega o modelo 3; race_view.c desenha com o modo de sombra do
render (cor do vértice, alfa vezes o valor) e, quando a sombra P (mapa de
profundidade) substitui a sombra autoral, desenha a luz antes de sair.

Photo Travel: só os carros da corrida (RaceEntry, construídos em 003B4B38)
gravam o +0x7C (004513B8, chamado só por 00390C90). Fora da corrida todo carro
passa por 00451A00 (showroom 00156950 e 001D6DC8), com o objeto de desenho
zerado no construtor (00450A04) e uma pose genérica (vtable D_00688698) cujo
slot +0x78 devolve 0 (00459D60): dia e sem brilho. O Photo Travel fica assim, e
race_view.c descarta o modelo 3 quando photo == 1.

Teste car_time_zone também confere: 560 modelos de brilho; route5, hongkong e
sarthe24 com bit 0x8000 e +0x58 > 0; tsukuba e laguna sem bit e com 0.
brake_material e brake_material_focus seguem. Captura em route5 com o quadrado
virado para trás só para o teste (chave temporária, já removida): aparece uma
mancha clara e fraca no chão, com uma linha na borda mais próxima. Não comparado
com o PCSX2: a cor, a mistura e essa borda ficam A até a captura equivalente.
Na câmera de perseguição o quadrado fica escondido pela própria carroceria.
Fora desta mudança: na corrida, o céu da route5 sai azul de dia, enquanto a tela
de pré-corrida mostra céu escuro.

### 18/10: mapa do GT Mode no controle não prende nas pistas do canto (P)

P: a grade autoral do mapa (navigate_source/target) fica longe dos marcos
(DirtAndSnow: a célula 520 à direita). Com o scroll nativo do mapa (não o
MScrollWindow traçado), a célula do foco sai do clip, o 0026F9E0 corta a
origem até ficar vazia e nenhuma direção acha vizinho: o jogador ficava preso
no canto esquerdo de baixo. navigation_pick refaz a coleta sem clip quando a
origem do foco sai vazia e, nos marcos do mapa (ModelFace "bg"), sempre: lá o
clip corta a grade deslocada conforme a rolagem, e direções sumiam ou mudavam
no meio da animação (Home sem esquerda nem direita). Fora do mapa segue a
coleta original. Teste gtmode_map_navigation: "dddr" a partir de Home chega em
TunerCar; os 37 marcos têm saída; 127 movimentos iguais com o mapa rolado
160/90 unidades; Home vai a GTAuto pela esquerda e a ExtremeHall pela direita;
sem a correção para em DirtAndSnow; o negativo falha. A: o mapa do PCSX2 no
controle não foi comparado direção por direção.
menu_navigation_collect e os testes de navegação seguem.

### 18/10: mostrador do turbo no painel interno (posição, ângulo e cor)

C: 003A9008 põe o mostrador (+0x964) em (320+0xAF, 452-0x15), 54x54, alinhado
(0, 1): termina em 431, então o topo fica em 377, com o centro na altura do
conta-giros. O nosso usava 404 como topo (27 abaixo, encostando no ícone do
óleo). 00407568: faixa 0..2.4, 003BE828(0, 216) soma 90 nas duas pontas (90 a
306, partindo do "-", preso em 306 + 2), desenhado pelo 003BE5F8 dos
mostradores grandes (vtable D_00686298) em 5 passos a 1/6, não no 0x80FFFFFF de
+0x2C. O modelo é o do turbo (slot 0x60, 00407638: D_006A42F0), escalado 1.35
depois de -0.5 (+0x54 - +0x50) e por 27 (003BE818): de 13.5 atrás do pivô
(meia largura 1.6) a 22.95 à frente (1.0), com as cores do modelo base
(D_006A2900): 0xC8 no eixo, 0x40 nas bordas. Antes era um traço de 1 de
largura. O degradê sai em três faixas por lado (P, a plataforma não tem cor por
vértice no 2D). O valor é o +0x60C do carro direto (0039E104 -> 003446B0 ->
slot 0x90, 003A94A8 em +0x97C), na faixa 0..2.4: 1.0 (pressão ambiente) fica
no "0". Medido na nossa corrida: ~0.66 em marcha lenta, ~0.58 segurando o giro
na largada, 1.7 a 1.95 acelerando. hud_suggested_gear confere 90+108 a 1.2 e
90+216+2 no máximo; o negativo segue. V: captura em tsukuba lado a lado com
print do PCSX2: mesma posição, espessura e cor, mostrador na altura do centro
do conta-giros. A: o ponteiro em marcha lenta e acelerando não foi comparado
com o PCSX2 no mesmo momento.

### 18/10: GT Auto, mecânicos da lavagem só depois do giro

C: loadModelOptionMotion (001391D0 -> 0013A678 -> 00154290) só troca o
controle da câmera em +0x88; quem liga o +0x10 dela e das trilhas adicionais
é o playModelMotion (001525F0 -> 00153E20). O nosso já tocava a câmera ao
carregar, então durante o giro da lavagem (CarWashRoot L11864..11887, antes
do playModelMotion) o canal de fade dela já acendia os mecânicos parados na
pose 0, presos ao carro girando. Rodas e asa (L11480, L11595, L12261) nunca
chamam playModelMotion: a câmera fica no quadro 0. gtmode_gtauto_menu confere,
nos quatro casos de serviço, câmera parada ao entrar e tempo 0 enquanto as
trilhas não tocam; gtmode_gtauto_camera_negative liga a câmera ao entrar e
falha. V: lavagem no app quadro a quadro: sem mecânicos no giro, entram com o
carro parado. Troca de óleo 4WD (L12050..12077): o script usa
oil_change_start no alinhamento 9 e oil_change_end no 10 (0x00156E38 e
0x00156E18 conferidos), então o mecânico entra pela frente e sai por trás;
showroom_probe com MOON_PROBE_SERVICE_4WD mostra um mecânico só, nessa ordem.
A: essa troca 4WD não foi comparada com o PCSX2.

### 19/10: mouse no Arcade, prévia do carro já selecionado (P)

P: o hover numa lista (pointer_set_hover) mostra o item pelos callbacks do
script (CarRoot CarScroll onValueChanged) e devolve focus_index. Só fazia
isso com índice diferente do foco, então, depois de passar por outro carro,
voltar ao selecionado (o primeiro de cada montadora, ao entrar) deixava o
último carro na tela. Agora a lista guarda o que a prévia mostra
(hover_shown) e mostra o selecionado de novo; teclado e clique zeram isso.
arcade_dealer_hover confere, duas vezes, Ford Ka de novo depois do 2º e do
3º carro; arcade_car_hover_negative tira a marca e falha. A: não testado no
app com o mouse de verdade.

### 19/10: freio aquecido e transparência do borrão das rodas (A)

Freio: 003F38D8 grava o calor do freio (0..1) na roda +A4; 003D75F0
(vtable 0x682F98 +C0) faz calor² × Env0 +4C / +38 e 00390C90 -> 00451550
grava no bloco do freio +28, o alfa (004A60E0) com que 00453798 desenha as
camadas misturadas do freio (GS 0x48 aditiva e 0x44). Antes essas camadas
eram descartadas; agora vão num modelo à parte (car_model brake_glow) com
u_valpha. A razão do Env0 é 0,1 em todas as pistas extraídas e o calor
medido com o piloto automático chega a 0,062 (Tsukuba) e 0,016 (Sarthe),
então o brilho fica perto de 0,0004: invisível em pilotagem normal, como
na conta do original. A: não comparado com PCSX2.

Borrão: 00453BB0 desliga ABE (004AB040(6) limpa PRIM bit 6), profundidade
(5) e testes de alfa/destino (4, 3) antes das rotações, então o acúmulo é
média direta de RGBA, como o nosso. Lido de volta no Impreza WRX STI em
Tsukuba (16 amostras, 0,93 rad): alfa por anel do acumulado 0,45/0,34/0,29
contra 0,44/0,33/0,29 da textura já borrada do próprio disco (banco); o
banco só difere no RGB, mais escuro onde é transparente (asset pronto, não
o caminho em tempo real). GL e Vulkan dão os mesmos números. O aro desenha
com 0x44, sem teste de alfa, depois do freio (ordem do MDLS). A: aparência
ainda sem print do PCSX2 na mesma câmera.

### Race display option persistence and existing HUD elements (18 October 2026)

C: constructor `00436400/00436498` supplies word `0x3FA`; the seven missing
`race_display_*` element defaults now match it and all seven persist through
native option capture/restore. `menu_race_display_flags` reads the preference
word only at presentation. Older preferences with only Simple/config are
expanded using option.ad L2124..2128, without overriding explicitly saved flags.
Original option.ad RaceRoot.apply L2104..2131 is executed by
`race_display_options` for Full/Simple x Map x Steering. It checks the resulting
word, stored preferences and restoration of each independent element at 0/1.
The WILL_FAIL negative corrupts the expected lap bit and the comparison rejects
it. No new setting or simulation option is introduced; `race_display_mode`
remains the unrelated PS2 HDTV setting, without a PC rendering effect (P).

C: `0039F718/0039F878/0039FAD0` gate map, position, lap and time; labels and
figures now follow those bits together. `0039FFF8` gates the whole panel on
meter, then suggested gear and G-meter independently. Both native camera
panels follow that grouping, without suppressing countdown/messages or licence
medal targets. `0039F878` alone also gates replay interval lines with time;
their clock still advances while hidden. Tests record actual HUD image draws
for both panels and each independent bit; suggested-gear regression and its
negative also pass. Eleven focused tests pass, silent and without a visible
window. Release links successfully; no full suite is run.

F: the native HUD still lacks the name elements at RaceDisplay +0x23DC/+0x291C
drawn by `003A0178`. Their stored option bit does not implement the timed
replay messages (`0039CFD8/0039D138`); this reader and its original layout/state
remain open. Steering-wheel verification is recorded below. No physics code
is changed.

The three reported pre-existing menu failures were rebuilt and repeated after
integration with the same 60-second limit and isolated preferences:
`options_lifecycle` still fails `option_pending==index`;
`gtmode_setting_race` and `gtmode_setting_catalog_menu` still time out (the
catalog repeats `focus_index==lo+1`). These match the pre-change baseline,
not new passes or new failures. Local output: build/ui-known-failures-after.log.

### Original steering-wheel HUD element (18 October 2026)

C: `00344680` reads car +0x560 and controller kind +0x569; presentation in
`0039E7CC..0039E818` multiplies the normalized display value by -90 degrees.
The native HUD now reads those fields at presentation, without changing the
simulation. `003A1EE0` permits kinds 0/3 only after a nonzero steering value
has been observed, excludes modes 8/9 (split), and requires the steering bit.
`0039FFF8` draws it independently of meter and suppresses G-meter while it is
visible. This restores the existing original option, not a new improvement.

C: `003A5D30`, layout `003A1970` and update `003A5D20` supply the authored
170-square replay_sign_gtforce image centred at (320,450), four quarter-step
rotation samples, 7/16 opacity and previous-angle state. The first frame uses
zero angular difference. `hud_steering_oracle` runs the original visibility
routine for 1,792 combinations and original image/transform drawing for 1,000
angle pairs, substituting graphics calls only. The negative changes an angle
and is rejected. `hud_detail` loads the actual disc image and checks native
geometry, pivot, opacity, four samples, sticky visibility, kinds, option off,
meter off and split exclusion; its WILL_FAIL negative demands drawing with
the option disabled and is rejected. All four tests pass silently. Visual
PCSX2 comparison and timed car-name messages remain open; this is not proof
of the complete HUD or complete race-display lifecycle.

### Linear-box end packing and dialog order (18 October 2026)

C: `00266338` reads bit 2 of widget +0x9C (`pack_reverse`, registered by
`00258670/0025C560`). Allocator `002E99E0` consumes either the near or far
edge for each eligible child in authored order; it does not reverse the child
list. Native HBox/VBox allocation now applies that choice, axis-specific
expansion/fill, centring within an expanded slot, spacing and padding.
Existing VBox content-height requesting and overlay faces are preserved.

`pack_reverse_oracle` executes `002E99E0` and the original reverse accessor
for 4,096 fixtures of 1..8 children. Only widget queries, request/available
sizes, child iteration and final rectangle publication are fixture boundaries;
the allocator's arithmetic and branches execute unchanged. Both ends and
all expansion/fill combinations match within 0.0001 UI units. The WILL_FAIL
negative flips reverse and the rectangle comparison rejects it.

C (authored resources/native integration): `pack_reverse` loads cursor's
ConfirmDialog from the disc, checks its reverse-packed Buttons row, and draws
the original No/Yes pair at x=0/120 inside the centred x=40 composite. Its
fixed-position children must not be swapped by their reverse property. The
test also draws horizontal and vertical three-child fixtures with mixed end
packing; its negative expects the old forward position and fails. Nine
focused packing/dialog/pregrid tests, including negatives, pass silently.
`tools/ui_audit.py` was rerun; report remains local in extracted/.

A: this does not prove the entire request/allocate lifecycle, runtime property
setters, grid/MBox reverse packing (`002B8778`) or the Logger screens. The
existing VBox request policy was not replaced. A matching PCSX2 screenshot
is still needed before marking shared-dialog visual parity complete.
The additional first_run_dialog rebuild encountered an unrelated unresolved
`moon_save_package_is_file` from local save-UI work; it is not recorded as a
pass. No physics code or new user setting is introduced.

### Watched replay car caption (18 October 2026)

C: the native caption follows a change of watched entry, not every frame
(`0039CDF0/0039EC40/0039CFD8`). Entry zero is the constructor's initial target.
It uses CAR_NAME's short string at +8 (`003B4D80 -> 00445450`), copied to at most
127 bytes; results continue using the full string at +0. The presentation owns
its cached strings. No physics fields or update routines are changed.

C: original `003A4E00/003A98E8/003A9780` match 6,300 native envelope steps
at three cadences and five durations, tolerance 0.0001 in opacity and identical
state transitions. Original `0039CCF0` matches 3,072 car-name mask decisions:
Normal replay selects the name-only word even with Simple settings. The
original `003A1970` layout table is executed with geometry setters substituted:
chase x=24,y=328,width=280, alignment 4/1; onboard x=320,y=430,width=240,
alignment 2/1. Original callback `0039CFD8` selects five seconds and .25-second
rise/fall; the actual disc font is hn_20R#1*90,90. Long names fit their width.
The WILL_FAIL oracle negative corrupts opacity and is rejected. `hud_name`
checks disc-backed glyph draws, target changes, expiry, Full/Simple hiding,
Normal replay override and suppression of new captions while the display
fades; its negative requires drawing a disabled caption and is rejected.

A: these scoped comparisons do not prove all replay camera transitions or
free-camera suppression (+0x43), nor the secondary text-message slot +0x291C
queued by `0039D138`. That slot is distinct from the replay mode plate already
drawn at the upper left. Visual PCSX2 comparison remains open. The complete
race display is not declared exact on the strength of the caption tests.

### Original static UI queries (18 October 2026)

C: MCarData.GetShortCarName (`00131068` registration, `0012DFC8 -> 00448280`)
now reads CAR_NAME's +8 string index instead of leaving the engine method
undefined. The shared full-name callback retains field +0. `native_ui_queries`
compares every available disc catalogue row's short label, including more than
50 rows whose full label differs; the WILL_FAIL negative substitutes the full
label and is rejected. Unknown labels return the original empty-string path.
No car parameters or physics change.

C (retail NTSC-U scope): MUnit.GetUnitOfMass (`001AF928`) returns the literal
"kg" at `00693048`; it does not choose a new mass unit. MSystem.IsOEM
(`001AD528`) constructs false with `002FE278`, argument zero, matching this
retail build. Both bindings are exercised through their actual script modules.
The test uses dummy audio, no window and an empty mod overlay. This restores
existing original queries, introduces no player option and makes no claim about
OEM/demo executables. The dynamic discovery tour supplied the boot IsOEM warning;
caller-dependent warnings in unrelated roots are not hidden by this fix.

### Original OptionMenu item enumeration (18 October 2026)

C: MOptionMenu.Count/getItem now expose logical popup choices to scripts,
including hidden choices, instead of remaining undefined. Evidence:
002C7970 -> 002C9B50 -> 0057CE60 counts the entry range; 002C7840 ->
002C9AF8 retrieves an entry. arcade.ad L900..905 enumerates those entries
and selects by name. option_items exercises more than 100 authored choices,
checks object identity and hidden-item counts through the VM. Its WILL_FAIL
negative rotates the expected item and is rejected. This covers valid indices;
it does not claim the separate MSelectBox callbacks or malformed-index parity.
No player option or physics change is introduced.

### Headless UI discovery tour (18 October 2026)

P (developer tooling, no player option): tests/ui/ui_tour.c opens arbitrary
original project/root pairs without the launcher, profile UI or a visible
window, using dummy audio. Unknown roots fail rather than silently opening
another screen. The runner preserves VM diagnostics, records the active root
after initialization and captures the requested tree under extracted/.
ui_tour_report and ui_tour_cursor pass; WILL_FAIL negatives reject dropped
warnings and unknown roots. Scope excludes network/online/message/eyetoy/
print/event projects and named network/printing roots.

A: the local discovery run covered 172 requested roots with 172 captures and
zero nonzero exits; 84 roots emitted diagnostics and three redirected during
initialization. Captures are discovery evidence, not PCSX2 visual comparisons.
Direct entry supplies no caller-specific car/stock/navigation arguments, and
the harness does not install the full game's 3D/movie presenters. Missing
context must be reproduced through the authored route before changing game
behavior. The earlier main-executable tour could silently fall back to another
project; only extracted/ui_tour_native/report.json is the corrected run.
The first-run adaptation owned by another session is outside this work.

## Packed HDR depth transport

The HDR scene depth copy now uses matching packed depth/stencil formats:
DEPTH32F_STENCIL8 for reversed depth and DEPTH24_STENCIL8 for normal depth.
Previously the destination was depth-only DEPTH_COMPONENT32F, which violates
OpenGL depth-blit format requirements (OpenGL 4.6, section 18.3.1):
https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf
A rejecting driver could leave the depth sampled by ambient occlusion and
volumetric lighting invalid. Vulkan accepts the same packed target formats.
The original preset with HDR disabled does not use this copy.

C (PC depth transport only): six focused hdr_depth cases pass, including
negative comparators, both APIs and both OpenGL depth conventions. Four
resolution/AA modes are checked. Maximum copied depth error is zero with
reversed depth and 2.98023224e-08 with normal depth (tolerance 2e-6).
The 18 daylight regression cases also pass. Evidence is in
build/suzuka-shadow-proof/depth-details.log and depth-tests.log.
The reported black asphalt patches near Suzuka's first corner have not been
reproduced locally; this fix does not establish that report's cause or visual
resolution on the affected computer.

### Save transfer footer stays outside onboarding (18 October 2026)

P: the optional native Export control belongs to the normal save-management
page, not the welcome/Auto Save dialogue sequence. Applying the transfer-tools
option every frame previously re-exposed that button behind all onboarding
modals. Visibility now combines the option with the current page state, and
re-entering the save page also honors the disabled option. This changes no PS2
script or physics; original dialog resources remain unchanged. No new option
is introduced. save_export_visibility checks both option values at all eleven
onboarding steps, repeated option application and normal-page return. The
WILL_FAIL negative exposes the footer during onboarding and is rejected.
Both focused tests pass with dummy audio; Release rebuilt successfully.

### Controller recovery after pointer scrolling (18 October 2026)

P: mouse wheel scrolling can move the viewport without changing script focus.
At the pointer-to-controller boundary, reveal that retained focus before the
original clipped directional navigation runs. Previously an offscreen focused
widget could be excluded from the candidate list, leaving the cursor stuck at
the clip edge. This repairs the existing optional pointer adaptation, adds no
option and leaves controller-only navigation and physics unchanged. Original
navigation evidence remains 0026F9E0 / 00270118; pointer takeover has no PS2
equivalent. scroll_takeover reproduces a scrolled clip with retained offscreen
focus and checks focus movement, visibility and subsequent navigation. Its
WILL_FAIL negative restores the offscreen viewport and is rejected. Seven
focused pointer/navigation checks, including negatives and the navigation
oracle, pass with dummy audio. No full suite was run.

### Results use the original entry short names (19 October 2026)

C: ResultArcade's 003DFBC8 calls its name callback (table +0x50),
003DFC50, which copies entry +0x350C with 005A609C. That entry contains
the short CAR_NAME +8 label (003B4D80 -> 00445450), already loaded for
the watched-car HUD. The presentation bridge incorrectly supplied the full
CAR_NAME +0 catalogue name, causing long model descriptions to overlap the
time column. Results now receive the existing short label; catalogue names,
fonts, movie geometry, timing and physics remain unchanged. No option is
introduced for this original-behavior correction.

result_name opens a real Tsukuba session and invokes the actual draw_result
bridge; it distinguishes the cached short name from a deliberately different
full label and verifies the ResultArcade row. Its WILL_FAIL negative replaces
the short cache with the full label and is rejected. Eight focused result,
short-name VM and HUD/oracle checks pass with dummy audio and hidden windows.
Release rebuilt. A: this checks label selection, not complete visual parity
of every result movie or every catalogue label's rasterized width; PCSX2 visual
comparison and the exact screenshot's six-car composition remain unverified.

### Launcher column bounds and long labels (19 October 2026)

P: launcher labels and values now share a measured column budget. A long
value reserves its rendered width before the label is drawn; labels, action
captions, the preview title and unbreakable description words fit their
available width. Value-arrow hit testing uses the same fitted scale. The
status column also fits long disc/profile/car labels. The comparison divider
is constrained by the panel width. This repairs the existing PC launcher
presentation and introduces no setting, original-game behavior or physics
change. It does not turn any graphics effect off.

launcher_layout captures all six tabs through the real launcher on OpenGL
and Vulkan with isolated preferences, dummy audio and hidden windows. It
checks 126 measured row bounds, the panel background and preference immutability.
Both focused tests pass; twelve captures were generated and representative rows reviewed.
The WILL_FAIL negative supplies an overflowing measured label and is rejected;
this is a deliberate checker control, not an EE oracle. Captures remain local
under build/launcher-layout. A: captures check the initial visible rows and
values, not every value, custom font or resize state. The reported dark scenery
remains open: local Classic/Enhanced comparisons show a lighting difference,
but do not establish a safe correction or complete visual parity. The missing
panel/dark text from the supplied cropped picture was not reproduced in these
current-build captures. Do not declare those rendering reports fixed.


### 2026-10-19 â€” P: preserve baked course detail under Daylight

The optional Daylight pass previously multiplied absolute lighting into course
colours that already contain baked illumination. Lighting is now relative to the
existing artistic model at neutral noon. Its 35-degree reference is not recovered
PS2 metadata. This is P presentation calibration, not original lighting parity.
Classic/Daylight-off and physics are unchanged.

The old shader fails both new GPU positives. The fixed fixture covers four source
levels and four normal cases, including generated normals: noon preserves each
channel within 1/255, switching off restores bytes exactly, and evening still
changes lighting. Deliberate-loss negatives use WILL_FAIL. All 22 focused Daylight
tests passed with dummy audio; no full suite. Eight local before/after launcher
captures cover Laguna/Tsukuba in OpenGL/Vulkan, recovering wall detail. These are
visual checks, not no-mod PS2 oracles. Other scenes remain unreviewed; real shadows
remain.


### 2026-10-20 — P: launcher panel fade and title clearance

Launcher guide text drew two fixed-opacity shadow passes even when the panel's
text alpha had faded to zero. Both shadow alphas now follow the foreground, and
zero-alpha labels submit no glyphs. The description starts at the first option's
baseline instead of twelve scaled pixels above it, keeping its actual font ink
below the tab divider. This fixes the existing PC launcher presentation, adds no
setting and does not alter original game menus or physics.

launcher_panel_fade captures the same fixed Laguna camera before opening and
after closing General, in OpenGL and Vulkan. The right-column pixel region must
match exactly (no residual LIGHT ROOM/DARK ROOM shadows). It also captures Save
Transfer and checks actual title glyph bounds against the tab divider. The
WILL_FAIL negative injects persistent residue. Both focused tests passed; no full
suite. Open/closed/title captures were visually checked. Other resolutions and
all other scenes are not covered by this comparison.


### 2026-10-20 — P: discoverable custom Music folder

Music is now prepared beside the active options store at startup, independently
of successful disc seeding or the custom-music toggle. Jukebox scanning uses the
same helper, with recursive UTF-8 directory creation and a logged failure instead
of silently ignoring it. Windows launcher Sound offers Open Music Folder; it
creates that same folder and opens it through ShellExecuteW, without changing
playlists or enabling playback. Existing audio.custom_music still gates playback.
No new setting, original menus and physics unchanged.

music_folder covers nested Unicode paths, repeated creation without losing a
sentinel, short-buffer/empty-config rejection and blocked directory creation.
The WILL_FAIL negative wrongly expects creation through an existing file. Both
passed. custom_music_ui passed; a fresh hidden launcher created Music before
Jukebox and its Sound-button capture was checked. Windows Explorer's interactive
window was not automated. No full suite, no Switch/Android validation.


### 2026-10-20 — P: custom playlist order survives context changes

The custom audio worker used to discard its order and cursor on every context or
preview-generation change, restarting ordered lists at their first song. It now
keeps an independent in-memory pass for Menu, Racing, Replay, Slideshow and
Launcher. Returning starts the next selection in that context; preview does not
consume that pass. A changed playlist invalidates its saved order. A new process
still starts a new pass. Existing shuffle insertion and PCM decoding are retained;
no change to original disc-only playback, settings defaults or physics.

custom_music_sequence switches all five contexts between two distinguishable WAV
fixtures, previews one track, then checks that every context selects the other
track. custom_music_sequence_shuffle checks the same non-repeating pass shuffled;
the WILL_FAIL negative demands the repeated first track. The former worker fails
the positive sequence test; the fixed worker passes it, shuffle, negative, and
existing custom_music/custom_music_negative (5 focused tests). Release rebuilt;
no full suite, no Android/Switch or interactive remote-user verification.


### 2026-10-20 — P: first-drive resize and bounded Vulkan transfers

Reproduced the first-drive crash with real SDL window sizes: OpenGL survived,
Vulkan lost its device and exited 0xC0000005. The width-filled capture requested
1920x1344 at y=-132 from a 1920x1080 screen. Guide captures now temporarily use
the fitted game viewport and restore the launcher's fill setting afterwards.
A second invalid blit on returning to the launcher showed that Vulkan also
needed the OpenGL transfer semantics: jointly clip source/destination intervals,
retaining scale and flips, before issuing Vulkan blits/copies. Valid rectangles
remain unchanged. Integer clip endpoints round to the nearest pixel; no claim
of exhaustive fractional OpenGL raster equivalence. No physics/new setting.

Four focused tests passed: vulkan_frame and its negative, vulkan_blit_clip and
its WILL_FAIL negative (valid, clipped, scaled, flipped, empty intervals). The
real hidden first-drive resize script passed in both APIs at 1920x1080, 640x480,
and 1280x720, then returned to the launcher without writing settings. Its negative
omits the first resize and is rejected by the capture-size assertion. Welcome
capture visually checked. These application checks used an isolated executable
because the user's running standard executable blocked replacement. No full
suite; no manual OS maximize-button or Android/Switch validation.

### Event bronze trophy source-colour audit (20 October 2026)

C (material data only): quick-event.ad L837..844 defines 1st.mdl, 2nd.mdl,
3rd.mdl, then L893 selects piece/prize/ + model_files[order]. The native
setModelSetPath registration 002BB220 calls 002BE908/002BE938/002BE968:
the three arguments write loading bits 2/3/4 of ModelFace +0x308, not a
colour index. The third-place asset really contains a rose material:
material 1 diffuse RGB (0.4, 0.2, 0.22039999067783356), specular
(0.6000000238418579, 0.36000001430511475, 0.49112001061439514).
It must not be recoloured to an assumed generic bronze.

Original 0049CEA0 selects material descriptors and 0049E228 loads their
ambient/diffuse/specular vectors at +00/+10/+20, storing them at rendering
context +A60/+A70/+A80. prize_materials loads all three actual disc models
and checks decoded lit vertices against every source material's ambient and
specular vectors without tolerance; the third-place diffuse is also checked
explicitly. The WILL_FAIL negative recolours that source expectation and is
rejected. Both focused tests pass, dummy audio and no mods. No game code or
physics was changed by this audit.

A: this proves source hue and material transport only. The complete GPU
reflection coordinates, blend strength, lighting and final pixels in the
reported trophy screenshot remain unverified. An embedded-reflection-map
experiment retained the pink hue and was removed; it is not a shipped fix.
The trophy rendering issue is not declared closed by the material test.


### 2026-10-20 — C/P: GT Auto oil background regression coverage

The original custom-music hotfix executable reproduces the user's exact vertical
bands on entering GTAutoRoot -> OilChangeRoot with Vulkan/Enhanced at 1920x1080
16:9. In the background crop its maximum difference from OpenGL is 195/255.
The bounded framebuffer-transfer correction already delivered in 4e35009 fixes
this case too: both that v2 package and the current Release retain the authored
heading, description and textured floor. No additional renderer workaround,
effect disabling, setting, transaction or physics change was needed here.

New gtauto_oil_background drives the real page through right/accept with an
isolated Cappuccino profile, both APIs and Classic/Enhanced (texture smoothing
on for Enhanced). It compares a background crop outside the moving models, max
channel tolerance 2/255, and requires the expected page and image dimensions.
Its WILL_FAIL negative repeats a single image row to simulate the lost vertical
detail. Both focused CTests passed with dummy audio; no full suite. Captures of
the old build, v2 and current Vulkan were inspected. C is limited to this
cross-backend crop; the capture/composition remains the existing PC adaptation
(P), not a PS2 pixel oracle. Paid-animation captures were also inspected on both
APIs. The user's separate crash was not reproduced by these successful entries;
this is not a claim that every possible GT Auto crash is solved.

### Completion audit approval list (20 October 2026)

A/P: [Completion audit](notes/COMPLETION_AUDIT.md) reconciles the requested
15 areas, existing scoped E/C evidence, remaining original F subpaths and
optional P limitations, with provisional engineering effort ranges. It
supersedes stale missing-experience/favourites/Screen Setting and demo/gold
counts by citing later original-code tests. This is document/source review,
not a new full-game comparison or certification; no audited feature was
implemented, no suite was run and no new PCSX2 images were available for
this audit. Local document links were checked. Implementation of that list
awaits user approval; the custom-car/course publication condition remains open.


### Original course gadget shadows (20 October 2026)

C, bounded presentation: the GDGT reader retains the odd shadow model alongside
its normal even model, selected by descriptor index *2 + pass (003E53B8).
The race renderer projects it before the normal object using 003E4968's
homogeneous plane matrix, Env0 +AD0 direction and original Env0 +10 bit 0800
(00340170..19C / 0033ED28..54). Alpha uses 003983E0's
(sun*(Env0+AB0 - Env0+AB8) + Env0+AB8)*shade. Depth bias is 0.005 from
003E4B40..50. Object transforms, contact/physics fields and update cadence are
read-only; mesh/texture resources remain resident and are freed together.
This restores an original pass, so it is not an optional enhancement.

gadget_shadow_original compares 2048 complete 16-float matrices/alpha values
(tolerance 2e-6 absolute/relative) and 4096 model selections against locally
recompiled original instructions. Independent matrix, alpha and even-model
negatives fail at their intended assertions. Initial oracle execution failed
because the excluded draw callback was not bound; the hook was fixed, not
silenced or skipped. The first GPU link also failed on a nonexistent library
name, corrected to the existing shared SDL stack.

Both actual-race-view GPU tests load Tsukuba's six model pairs and 59 placements:
1155 changed RGB pixels on each backend, byte-exact Off restoration and unchanged
placement-state bytes after draw. Disabled-pass negatives fail on zero changed
pixels; negative teardown closes video normally. Six original prop-dynamics
checks plus Coffee Break contact/count and negative regressions pass (16/16
focused cases total); the four GPU cases pass again after teardown repair.
Main Windows Release builds. Logs: build/gadget-shadow-final-tests.log,
build/gadget-shadow-cleanup-tests.log and initial gadget-shadow-oracle-tests.log.
Captures: build/gadget-shadow-<api>-on.png / -off.png.

A: complete original raster/blending/fog and homogeneous-to-affine placement
remain unverified against equivalent PCSX2 frames. The diagnostic capture
initializes its static viewer ground planes with the existing immutable surface
query; live races retain their original object-state bridge. No all-course,
reflection/pregrid/PhotoPause or console GPU coverage is claimed. This closes
the absent shadow pass, not complete race/Classic visual fidelity.


### Enhanced night: retain authored local radiance (20 October 2026)

P correction within graphics.night_sky / graphics.daylight, no new option.
The procedural night multiplied already-lit course radiance by near-zero solar
irradiance; modern car lighting could also erase the original local RNW4 response.
The new night_baked GPU positive fails before the fix on both APIs (source20,
night0). The fragment path now retains its original lit colour before modern
shading, blends back to that radiance during authored night, then adds optional
car lamps. HDR retains linear radiance, SDR retains encoded source colour.
Original fog remains after the mix; the procedural sky is separate.

The weight is 1-smoothstep(-.12,.08,sun elevation), and only nonzero original
Env0 time zones may retain authored night radiance. Day courses are not forced
bright at night; noon and Classic have weight zero. The weight depends on the
same recorded frame clock as the sky, never track labels, wall clock or physics.
This preserves existing baked artificial light; it is not light-source recovery,
new global illumination or removal of baked daytime shadows.

Twenty focused GPU tests cover night surfaces/cars, HDR/SDR, source night/day,
legacy restoration, the existing baked-day/noon path, sky, car lamps and authored
night guards, including deliberately disabled-source negatives. A car fixture
initially lacked its modern_light flag and was corrected without relaxing the
radiance equality. Retained synthetic radiance agrees within 1 byte/channel;
Off restoration is byte exact. Both APIs captured all 25 source environments
again: build/night-balanced-<api>/index.html. Route5 now retains lit buildings and
asphalt; the optional lamps remain additive. No equivalent PCSX2 frames, complete
source-scene photometry or console GPU certification. Overall visual grade A/P.
Logs: build/night-baked-before-tests.log, night-baked-final-tests.log and
night-baked-regression-tests.log. Initial test compile errors were fixture-only
(include entrypoint alias and light-field name), corrected before GPU comparisons.


### Full daily sun/moon cycle (20 October 2026)

P, optional PC presentation: graphics.night_sky plus graphics.daylight enable the
full daily path. The existing graphics.solar_cycle is exposed as Sun / Moon Cycle
(Fixed / Real Time / 60x), not a second competing clock. Real Time is one solar
hour per 3600 recorded-frame seconds; 60x is one hour per 60 seconds, so a full
cycle lasts 24 real minutes. The sky uses only the recorded frame index /60.
Sunrise/sunset follow the existing artistic equinox/35-degree model. The moon is
a stylized full moon opposite the sun, not a lunar ephemeris or monthly phases.
Stars rotate about that model's celestial pole and wrap with the same 24h clock.
Moon/star rays below the horizon are rejected before the daytime integrator's
horizon clamp. Moon radiance uses the existing RGB optical depths
(.0428,.1,.24414), elevation floor .08 and disc transition .009..008 radians.
These are presentation choices, not recovered original-game astronomy.

Solar Time now has 25 entries: the existing 24 explicit-hour indices keep their
mapping; new index24 is Course. Enhanced defaults to Course (12:00 for source day,
22:00 for nonzero original Env0 time zone); Classic retains inactive index5.
An explicitly selected 12:00 now really means noon even on a night course; the
former implicit 12:00->22:00 substitution is removed. Stored indices0..23 remain
manual hours. New/default presets select Course, not an inference that overwrites
old modified player choices. Personal presets accept index24. Startup and runtime
share video_settings_solar_hour, fixing the startup path's previous +7-only mapping
for late hours. No profile, vehicle command or physics/replay format is changed.

celestial_cycle checks 1441 clock samples over a day, opposite unit sun/moon
vectors, sunrise/moonrise and actual GPU disc contrast at six times. Star motion
and exact midnight repetition are checked on both APIs. Moon-disabled and
clock-held negatives fail their intended assertions. Initial disc comparison used
the window centre instead of the actual content viewport; querying the viewport
fixed the fixture without weakening its contrast threshold. The early CPU-negative
shutdown attempted video teardown before initialization; it now only tears down
a started device, and returns the intended assertion failure rather than crashing.

celestial_clock uses the actual app: 240 live and replay ticks both publish exactly
4 seconds / solar23.0666656, pause30 updates preserve the frame count and complete
scene bytes, and deliberately changed reported time fails. This is bounded clock/
pause integration, not an entire 24h simulated endurance or universal replay-image
proof. launcher_quality checks 25 choices, Course/manual-noon resolution, night-hour
persistence, Classic and Enhanced defaults. Focused sky/baked/lamps/guard regressions
remain in build/celestial-final-tests.log. Original-code comparison is inapplicable
to new astronomy; original Classic rendering remains the separate baseline.

Local previews on both APIs: build/celestial-preview-<api>/index.html and cycle.mp4,
97 fixed-camera Route5 samples at quarter-hour intervals. PNG0 and PNG96 are byte
identical. No original-game assets/captures are committed. Authored baked lights,
car lamp activation and callback7 model selection still follow source time-zone
rules: the optional daily sky does not certify fully dynamically relit tracks,
automatic day-car geometry swaps or monthly lunar phases. Full scene comparison A.


### Optional car-lamp occlusion (20 October 2026)

P: the existing independent `graphics.car_lamps` option now includes a perspective
shadow atlas for its additive light. Classic Off / Enhanced On; no new setting,
physics, replay controls or source time-zone/brake gates. The original RNW4 lamp
shadow/model-3 floor glow passes remain separate. This is a PC adaptation, not a
claim that these spot shadows reproduce PS2 passes 0038F168/00451EE0.

Each active light gets a 256x256 depth tile (24 maximum, 1536x1024 RGBA8 atlas,
one reusable depth target). Source geometry supplies the emitter pose. Resident
course/scenery, posed rigid car bodies/wheels and visible live gadgets are
submitted from the light's view, independently of the camera visibility list.
Ghosts, reflection copies and blended non-cutout surfaces are excluded; authored
alpha rejection is retained. Packed linear depth, four comparisons and a 3 cm
receiver bias affect added light only; emissive lenses and baked lighting stay.
Atlas entries are invalidated on each new lamp frame. No GPU readback.

C within bounded tests: lamp_occlusion OpenGL/Vulkan checks an opaque blocker
behind the viewing camera, movement restoring illumination, transparent glass,
alpha rejection, final atlas slot 23, HDR and byte-exact Off restoration. The
negative omits the atlas and fails the same blocked-surface assertion. Real
Focus/Route5 integration changes 7587 OpenGL / 7604 Vulkan pixels (max 30/channel)
and restores Classic byte-for-byte, with sampled car/driver state unchanged;
its no-caster negative fails. 26 focused cases pass including existing solar
shadow, night radiance, sky and car-lamp regressions. The initial build lacked
the fixture's miniz dependency; repaired without relaxing assertions. Local log:
build/lamp-occlusion-regression-tests.log. Native comparison images:
build/lamp-occlusion-<api>-{classic,clear,blocked,restored}.png.

A/P limits: 256-pixel tiles can alias; caster coverage omits skinned drivers,
pit crews and particles; light inference still needs a broad car survey. No
complete-race FPS/memory benchmark, 24-lamp race stress test, console GPU coverage,
whole-frame API parity or equivalent PCSX2 capture. This closes missing rigid
world/car occlusion in the tested path, not every transparency or shadow case.


### Car-lamp model coverage, ghosts and bounded cost (20 October 2026)

P/C: `graphics.car_lamps` (Classic Off / Enhanced On) now accepts lit kind-1
pressed-brake geometry, recovering missing rear emitters on F-150, BEAT, R390,
GT-One and the pace car in the 12-model survey. All twelve night models produce
two front/two rear candidates; day models retain rear candidates, with the pace
car's source always-night selection. These counts do not certify optical axes,
all materials or full PS2 images. Ghost entries no longer emit physical light.
A native six-car fixture checks all 24 depth tiles, ghost/opaque/day/brake gates,
pose/driver state preservation and no renderer readback. The initial survey and
ghost positives failed before their corrections; deliberate negatives fail.

Lamp-only support-point frustum bounds reduce unnecessary shadow submissions;
19,995 generated-box decisions match an independent corner reference, excluding
five numerical-boundary cases. Focus/BEAT Classic/lamps/night images before and
after culling are byte-identical on both APIs (12 comparisons). 38 focused cases
pass. Native gallery: build/lamp-coverage-gallery/index.html, 24 car/API runs.
Bounded six-visible-car timing on GTX 1650 is about 14.1 ms OpenGL / 26.9 ms Vulkan
with occlusion, native 640x480 and other enhanced passes Off. This is a static,
unpaced snapshot, not a full race FPS or all-options benchmark; Vulkan remains
a performance concern. Physics and replay formats are unchanged. No equivalent
PCSX2 capture; full appearance A/P. Details, failures, timings and limits:
[coverage note](notes/CAR_LAMP_COVERAGE.md).


### Direct car-lamp atlas and shadow batching (20 October 2026)

P: performance-only change inside graphics.car_lamps, Classic Off / Enhanced On.
All 24 256px depth tiles remain; one atlas framebuffer replaces 24 scratch/copy
transfers. Contiguous caster ranges merge only with identical shadow-relevant
texture/alpha/brake state and retained visibility, preserving primitive order.
Directional solar maps retain their previous path. No physics/replay changes.
42 focused automated cases pass; legacy OpenGL depth also passes the reference
and deliberate negative. Logs: build/lamp-atlas-final-tests.log and
build/lamp-atlas-legacy-depth.log.
C: GPU copy-reference fixture on OpenGL/Vulkan retains 159000 occupied pixels,
zero alpha mismatches and max depth error 5.96046e-8 (limit 2e-6). A missing-last-
tile negative fails. Four native before/after six-car images are byte-identical.
Native submissions fall 4527 to 4080 with the same triangle count. The modest
paired timing gain is hardware/sample-specific; Vulkan remains above 16.67 ms.
Depth storage grows approximately 11.25 MiB in the tested D32F/S8 mode; full-race
FPS, memory residency, broader GPUs, Enhanced combinations and PS2 imagery are
not certified. [Reference, measurements and limits](notes/CAR_LAMP_COVERAGE.md#direct-atlas-and-shadow-only-batching-20-october-2026).


### Tagged emissive lamp materials (20 October 2026)

P: graphics.car_lamps, Classic Off / Enhanced On. Lens emission now follows the
loaded night-lens/pressed-brake geometry tags, independently of lit/unlit material
kind. Group creation and draw merging retain that tag, preventing nearby paint
from acquiring lens emission. Brake release keeps the authored released branch;
no source geometry, original lighting, physics or replay command change.
34 focused cases pass on OpenGL/Vulkan, including missing-emission negatives,
lit/unlit night and pressed branches, released lamps, adjacent-paint exclusion,
HDR bloom outside the lens and byte-exact Off restoration. Four native BEAT/F150
API captures retain Classic byte-exact against prior images. Local captures:
build/lamp-emission-<api>-<car>-enhanced-lamps.png; logs:
build/lamp-emission-final-tests.log and build/lamp-emission-native.log.
A/P: generic source inference/axis/output and .22m emission falloff remain; no
all-car material or equivalent PCSX2 image certification. This extends emission
to tagged lit lenses, not arbitrary untagged emissive surfaces.


## 2026-10-20 — Original backfire interpolation, tint and ground pass

- **E (bounded callback):** `00452F28 -> 00456850 -> 004A7B40` compared
  with the native draw adapter at 599 particle times. Integer branch and uploaded
  fractional tween match exactly; the deliberately erased fraction fails.
- **C (native rendering/data), A (complete image):** both MDLS models and all
  six branches retain position/UV/RGBA endpoints. Original `00452AC8` uses model
  0 in the body frame and model 1 in the shadow frame, zero exhaust Y for the
  ground pass (`00452DD0`), yaw only, local +.02 and projection +.005 bias.
  `Env0 +17B4..17C0` supplies RGBA, +17C4/+17C8 the independent sizes
  (`00398718`, `00452A90`, `00452AB0`, `004A60E0`). Ground submission,
  colour, inactive gate and source-frame positions have bounded adapter checks.
  Both OpenGL and Vulkan tests demonstrate fractional colour/geometry, tint,
  and native route5/Focus flame plus ground glow. Missing tween and omitted ground
  negatives fail. No physics/producer changes; original behavior also in Classic.
- Decoder allocation now zeroes all triangle metadata (including `night_lens`)
  instead of allowing allocator bytes to classify a material as a lens.
- **24 focused tests pass**, including producer, preload, driver/pit tween and
  optional lamp emission regressions; `build/backfire-final-tests.log`.
  Initial fixtures incorrectly assumed every vertex had endpoint alpha 1 and
  required late ground frames below the original alpha-test threshold to draw;
  corrected to the authored ranges/threshold. The initial scene looked at the
  car's front; the final rear view exposes the exhaust. A missing-ground negative
  initially lacked a post-mutation assertion; fixed and rerun.
- Local captures: `build/backfire-scene-{opengl,vulkan}-{off,on}.png` and
  isolated `build/backfire-{opengl,vulkan}-{flame,ground}.png`.
  No equivalent PCSX2 capture, all-car image survey, Switch/Android GPU run or
  complete PS2/VU pixel comparison. Full-image fidelity remains **A**; this
  supersedes the historical F for absent tween/tint/ground, not that A rating.


## 2026-10-20 — Photo camera regions, edge sliding and PhotoPause restrictions

- **E (bounded data/selection):** `003FECB0/003C9E70/003C9DB0` match
  6,615 region/centroid samples across all 15 Photo Travel destinations;
  `003FFC68` matches 1,999 distance/FOV limits with atan as the library boundary.
- **C (bounded geometry/angles):** native `003FEE08 -> 003C9A38` edge/corner
  contact and four-pass remaining-movement logic compared with the original on
  160 consecutive moves per nonempty destination region, absolute point tolerance
  .002 m. `003FEF60/00370EF8` aiming window, zoom adjustment and rejection compare
  across 36 headings per region (.001 degree). Host trig is a boundary, not an
  original-library/pixel proof. The comparator caught and corrected the corner
  remainder origin and angle-window wrapping; negatives corrupt these results.
- **E (bounded ordered list):** `003FDC50/003DE268` native active REP4 point
  ordering, look-back and lap wrap match 505 positions over route5, Tsukuba,
  Sarthe24, Hong Kong and Motegi, including the start/end seam. The executable's
  `003DE198` partition base is explicitly zero; no invented spatial partition.
- **C (bounded rejection):** `00380C50/003FDD60` compare 2,000 camera samples,
  car dimensions, near clip and FOV. Pose/matrix boundary is identity in this
  comparator; arbitrary frozen renderer pose conversion remains A. Native
  PhotoPause now rejects the original enlarged car box, retains the dynamic zoom
  minimum, and keeps authored height/pitch bounds and maximum FOV per camera.
  Random camera placement also calls the existing original `003FE7B0` wall query
  with .15 margin before the ground test. No solver formula, parameter or state
  was changed. `race_resources` exercises these callbacks and the complete
  32-MiB world image stays byte-identical while taking/developing a replay photo.
- `can_shoot` is a writable original property, default true (`00192BE0`,
  `005CD6A8/005CD6B0`); the shipped photo script tests it and opens its warning
  dialog when false. Native getter/setter and default now retain that contract;
  the UI test verifies warning/no photo and then successful capture. This is not
  evidence of a complete original visibility/occlusion calculation; the former
  audit conflated the property with that missing proof.
- **23 unique focused tests pass after repair**, including dedicated region,
  movement, aiming, point-order, zoom and can_shoot negatives plus existing photo
  flows and replay memory/return guards. Initial `photo_travel_flow` failed because
  its fixture lacked the modal warning presenter; added acknowledgement and
  rechecked the positive and negative. Original run: `build/photo-constraints-final-tests.log`;
  repaired flow: `build/photo-constraints-repair-tests.log`.
- Native captures: `build/photo-flow.png`, `build/race-resources.png` and its
  editor/preview companions. No equivalent PCSX2 capture, all dynamic camera
  paths/visibility pixels, arbitrary-pose oracle, Switch or Android GPU check.
  Full camera/Photo Travel fidelity remains **A**. Map-car motion, input ramps,
  native floor search and complete original visibility semantics still need
  further coverage; these scoped results do not certify the whole Photo Mode.


## 2026-10-20 — Photo Drive viewfinder and original camera HUD

- **C, native integration / A, full PS2 image:** restored the original PhotoPause
  composition from `003C5AB8`: landscape photograph 410 x 308 at (20,20),
  portrait 308 x 410 at (20,20), wide horizontal scale 0.75 with source
  translations 72.480003356933594 / 289.5. The scene previously filled the
  screen underneath the controls and its optical centre missed the viewfinder.
  The editor now reserves the original black area for the authored HUD.
- `/menu/common/racephoto.strb` supplies all geometry, 35 edit fields, bitmaps
  and scripts; no replacement HUD artwork. Two focus brackets are open,
  three-segment stroke-only paths (fill FFFF, line table +70, white width 1.75).
  The STRB player had discarded them. Straight strokes now render from their
  stored endpoints without closing the path. Curved stroke caps/joins and GS
  raster equivalence remain **A**.
- The camera counter uses embedded textured glyphs. The embedded-font path
  treated their rectangular texture bounds as solid polygons. Bitmap glyphs
  now retain the authored texture/UVs and the field tint, restoring digits
  instead of solid blocks. Other font sizing/rounding remains **A**.
- `003C4BD8 -> 00475488` advances the 12-fps movie by 1/12 per PhotoPause
  update. The host interval adapter now supplies that cadence at 60-Hz updates.
  Focus presentation is retained after native depth readback clears its request,
  until the movie consumes it (`00374130 -> 00380D78` supplies the original
  camera trigger byte). This repairs the previously lost focus animation.
  Complete original autofocus/depth and trigger timing are still **A**.
- Focused GPU tests: `photo_hud` checks original labels/readouts, counter digit
  transparency, bracket pixels, portrait switching, manual-focus state and a
  focus pulse after its camera request was consumed. OpenGL and Vulkan each
  pass; negatives remove brackets, restore solid glyph bounds and restore a
  full-width photograph. All three deliberately fail. `photo_pause_movie`
  and its negative also pass: 10 cases total.
- Initial GPU positives failed because the test expected a 12-fps movie to
  advance during a 1/30-second interval. The repaired test exercises the actual
  PhotoPause update adapter. Both initial failure and repaired results are kept
  in `build/photo-hud-tests.log` / `build/photo-hud-repair-tests.log`.
- Existing Photo Drive/session, film import/develop and format regression run:
  15 focused cases pass (`build/photo-hud-final-integration.log`). The session
  checks full 32-MiB world preservation, camera restoration, save/return and
  black margins outside the photograph. This does not certify pending CDTL,
  GDGT or SMOK reconstruction work. No physics formulas, parameters or state
  were changed. Vulkan session evidence: `build/photo-hud-vulkan-session.log`.
- Native captures: `build/race-resources.png.editor.png` (OpenGL),
  `build/photo-hud-scene-vulkan.png.editor.png` (Vulkan), plus isolated authored
  HUD captures `build/photo-hud-opengl.png` / `build/photo-hud-vulkan.png`.
  No equivalent PCSX2 image was captured: full Photo Drive appearance stays **A**.
  Android/Switch were not built or exercised for this change.


### 2026-10-20 — P: launcher initial saved paint

studio_run previously awaited the default-colour model, then asynchronously
requested the selected colour. A live course scene could read the old active
paint and stop pumping the room until another action. It now submits car and
paint atomically with showroom_set_selection before waiting for readiness.
No new preference, original game logic or physics change.

launcher_initial_paint starts with Cappuccino '91, selected second paint (Saturn
Silver), and a Tsukuba launcher scene, without simulated user actions. The first
model load must report the matching authored colour ID, the scene must load and
a capture must exist, on OpenGL and Vulkan. The old v2 executable fails this
check (initial colour ID 1 instead of 2); the fixed Release and deliberate wrong-
paint WILL_FAIL negative both pass (2 focused CTests, dummy audio, no full suite).
Silver captures were visually checked on both APIs. Coverage uses isolated
launcher preferences, not every imported save/paint combination. Startup's
existing saved-car extraction remains unchanged.

### 2026-10-20 — P: live controller tile layout

`interface.controller_tiles` now updates an already-built Options page on its
next menu tick. Classic restores the disc device tiles and translated labels
(option.ad L228..310). By Player starts from that baseline before hiding wheel
model tiles. No repeated row compression or project reopening.
`controller_tiles_live` switches both values six times, checking labels, exact
height/positions, child count and recovery from a hidden focused tile.
Its WILL_FAIL negative skips the update and catches the stale layout.
Both focused tests passed. No device binding or vehicle physics changes.

### 2026-10-20 — P/C: Driving Missions mouse grid

The existing desktop mouse setting now scrolls horizontal ScrollWindow content
with the wheel, using the existing viewport-fraction step, clamping and easing.
Classic controller navigation is unchanged. MissionRoot has two rows extending
horizontally; the old mouse path only handled vertical spaces or strips.
`mission_pointer` loads all 34 authored cards, checks the original focus overlay
fade (gtmode.ad L8854, L9007..9015), partial intermediate opacity in both directions,
horizontal easing and unchanged controller focus. The overlay intentionally fades
out on focus in the authored script. Its WILL_FAIL negative restores the old
zero horizontal target. Both focused tests passed. No added hover artwork.

### 2026-10-20 — C/E: oil colour and warning lamp

Fixed two presentation omissions without changing wear, power or physics.
MColorObject.interpolate follows 00203A60..00203ABC: first*t + second*(1-t),
not first*(1-t) + second*t. OilChangeRoot L12018/12019/12094 supplies dirty
(100,90,30) first and clean (240,195,85) second. gtauto_oil_color runs the
actual authored page for fresh/worn car state, checking both endpoint RGBs;
its WILL_FAIL negative reverses those colours. Intermediate RGBA floating
point parity remains unproved; this grade C covers the authored endpoints.

The race presentation reads watched-car wear+8 against D_00620320+20,
00344640 -> 003437B0 -> 00350830; 0039E2F0..0039E308 maps false/true to
0/2 using D_006A06A0. Previously both HUD layouts always passed zero.
The shared boundary predicate matches the recompiled getter at 12,288
normal-float below/equal/above-threshold cases (E in that scope).
Both chase/onboard drawing paths now display the original sign; split-player
and watched-car bridges read their own car. hud_oil_warning observes actual
HUD draws in both layouts. Three positive tests and three WILL_FAIL negatives
passed. Visual parity of every dashboard and whole races remains separate.

### 2026-10-20 — C: Warning versus Info dialog presentation

The application previously hid caution/bgE/titleE unconditionally and supplied
Info even when the script omitted its title. Shared dialog styling now follows
cursor.ad ConfirmDialog.open L169..203: type 2 shows the authored warning art
and warning heading; types 0/1/3/4 restore the normal heading. Missing captions
come from the disc's Confirm/Dialog/Incomplete translations. Explicit titles
are retained. The VM bridge preserves an omitted title for that selection.
`dialog_style` uses the real cursor project, alternates all five types on the
same dialog and checks warning art, both headings and translated/explicit titles.
Both focused tests passed; the WILL_FAIL negative hides the caution icon.
This proves the common style boundary, not every warning trigger in every game
mode. Existing button handling, default selection and opening/closing actors
are unchanged; full PS2 pixel comparison remains outside this test.

### 2026-10-20 — C/A: car maintenance and tuning audit

Focused audit: 28 service/Quick Tune/transmission/fitted-purchase checks passed,
including their intentional WILL_FAIL negatives. Original service value, Mot1
channel and placement oracles retain their existing narrower E/C grades.
GT Auto wash/oil script transactions, 50-credit charge, refusal with 49 credits,
full body persistence and cached reopen pass in gtmode_gtauto_menu. Camera
fade/lift/shadowOpacity sampling remains checked by gtmode_service_motion;
whole mechanic images and every service animation are not newly certified.

Three Settings menu tests passed (A/B/C ballast edits and negative control/flow
cases). Four engine/gear curve tests passed: stock and upgraded disc catalogues,
plus curve/label negatives. This audit changes no simulation arithmetic.
New tuning_effects executes the native setParts callback for a Ford Ka's three
LIGHTWEIGHT and three MUFFLER stages, then rebuilds equipped vehicle statistics.
Mass changes 965 -> 936/907/878 kg; power changes 6155 -> 6278/6401/6462 in
native hundredths. The WILL_FAIL negative substitutes unchanged stock stats.
Both tests passed. This proves equipped effects for these parts/car, not every
Tune Shop purchase, price or arbitrary upgrade combination.

A: smooth wheel/wing preview replacement still requires the original CarModel
virtual setters' transition proof and a matching visual capture. gtmode.ad
WheelShopRoot.tryon L11645 and WingShopRoot.tryon L12311 assign the parts directly;
that alone is not evidence of the native model fade. No guessed fade was added.
The common warning style is fixed separately; exhaustive warning triggers remain
part of the dynamic UI audit. These limitations are explicit, not a claim that
all car features or all screens now match the PS2.


## 2026-10-20 — P: coherent realistic sky/lighting controls and reference pass

P: user-requested Realistic Lighting / Realistic Skybox / Realistic Car Lamps
labels, with the former time/night/material/HDR/shadow controls folded into
the first two switches. Classic disables the added rendering. New daytime
clouds attenuate sunlight; direct/sky balance follows a measured noon ratio,
with relative sun normalization and no global saturation boost. The user's
requested warmer direct sun is a luminance-preserving artistic tint, not a
measured solar spectrum. Environment
reflections now integrate GGX roughness instead of a fixed angular cap.

C (bounded native/GPU checks, not PS2 fidelity): 28 focused renderer/settings
checks pass in build/realistic-warm-tests.log, including deliberately failing
negatives on OpenGL and Vulkan; 6 UI checks passed in
build/realistic-lighting-ui-tests.log. Covers analytic direct response,
independent white-environment quadrature, roughness broadening, direct/fill
contrast, clear baked noon preservation, cloud repeatability/motion and Off
restoration. Native paired Classic captures have zero changed pixels and
identical 32 MiB race state against the previous local binary, whose precise
source revision is not known. New native images are local only.

A/P: this does not establish photorealism or equivalent original imagery.
PS2 baked textures, LDR probes, generic material conversion, approximate cloud
transport and complete-radiance post AO remain limitations; no equivalent
PCSX2 capture, Android/Switch or broad GPU performance validation.
[References, failed attempts and exact scope](notes/REALISTIC_LIGHTING.md).


### 20 October 2026: mission mouse edges and controller scroll takeover

- **P**, existing `controls.desktop_menu`: horizontal mission grids now share the
  manufacturer edge-pan adapter; the fixed clip viewport keeps its hit bounds
  while content moves. Both edges preserve the authored selection and feed the
  existing optional smooth cursor animation. The GT Mode map retains its separate
  edge-pan setting. Classic input remains covered by `desktop_pointer`.
- **P**: switching from mouse scrolling to controller navigation restores the
  selected row immediately in SelectBox strips instead of easing from an unrelated
  clipped offset. Real Jukebox MusicRoot navigation covers its 65-song list,
  top/bottom offsets and controller continuation; Missions also resumes navigation.
- **C**, ListBox signed `drag_index` getter/setter only: original callback
  002AEEB0, read 002AEEE8, write 002AEF78 (+12C), inactive state -1 from
  002B5270. The Jukebox script reads this at slide L1181/L1266; an undefined value
  previously selected the wrong state path. Full drag-reordering is not claimed.
- Focused verification: all 10 cases passed: desktop_pointer and negative,
  scroll_takeover and negative, mission_pointer and negative, jukebox_takeover and
  negative, mission_edge_negative and scroll_strip_takeover_negative. Negative
  cases use WILL_FAIL and deliberately restore the fault; audio is dummy.
  Release MoonEngine.exe rebuilt. Static UI audit regenerated locally (44 classes,
  427 properties, 1042 engine names); no full suite or physics changes.


### 20 October 2026: custom Menu songs are map-only

**P**, existing audio.custom_music/audio.jukebox_folders: Menu overrides only the
MAP MIDI group. Other menu MIDI groups and menu PCM retain their disc music;
Race, Replay and Slideshow custom contexts remain independent. Original evidence:
extracted/ui_audit/gtmode/gtmode.mproject L2405 requests MAP; HOME is separate at
L3385/L3506, consumed by request_music in menu_engine_objects.inc.
The MIDI callback test enables a fake custom source, verifies MAP replacement and
Arcade exclusion. The PCM bridge compares both default_quick tracks to disc PCM
with the custom source installed and verifies contexts 1/2/3 still override.
All eight focused audio tests passed, including menu_custom_scope_negative and
menu_custom_pcm_scope_negative (WILL_FAIL), plus existing transport/preroll/loading/
volume negatives. SDL_AUDIODRIVER=dummy; no full suite. Release rebuilt; static
UI audit regenerated locally. No change to original music when custom songs are off.


### 20 October 2026: launcher Options text lifetime, all rows

**P**, launcher-only correction (no PS2 counterpart): all Options text goes through
one scoped opacity boundary at dock_name_pass, including preview captions,
Classic/Enhanced facts, warning notes and their shadows. Existing panel colours
cannot outlive the panel even if a helper supplies a fixed alpha; the boundary is
reset before drawing the launcher itself. This fixes the newer facts helper's
constant opacity without changing other sessions' preview/layout work.
Reproduced with the previous private release, Quality > Anti-Aliasing: after
closing, the yellow Classic/Enhanced line remained (maximum channel delta 234,
4267 changed channels in the checked region).
`launcher_options_ink.py` exercised every visible row in all six tabs (60 rows)
at partial and sub-byte opacity on OpenGL and Vulkan. Real closed Quality and
Interface captures match the bare launcher exactly in the tested right-column
region. Its negative disables the text boundary and fails on an actual leaked
caption; registered launcher_options_ink_negative passes with WILL_FAIL.
Dummy audio, isolated launcher preferences, no full suite. Updated the local
executable and private Windows package revision 2; UI audit regenerated locally.


### 20 October 2026: runtime can_focus in GT Mode Courses

**C**, runtime focus gating only: original can_focus registration 00258D9C /
0025C964 binds 00256B58, which calls 00265F10 to update widget+98 bit 16.
The authored GTmode CourseEntryRoot set_can_focus L3306..3309 and init_mode
L3118..3130 enable the course rows or the race-mode buttons exclusively.
The port previously stored writes outside the native widget while getter,
controller collection and pointer collection kept using static defaults.
Runtime overrides now reach all three paths without changing the defaults.
The regression failed before the fix on the first disabled FreeRun button.
`course_entry_buttons` / `_mouse` check all 29 currently available course rows
across six categories, race-button focus, outgoing-list exclusion, Back restoring
list navigation and no additional VM errors. The two pre-existing optional-arrow
actor diagnostics at script L3293/L3295 are outside this correction. Negative
removes the override and fails the same assertion (WILL_FAIL).
All 11 focused course/pointer/mission/Jukebox/scroll cases passed with dummy audio;
no full suite. Local UI audit regenerated and Release executable updated.
Limits: real captures with a copied player profile opened the buttons both with
and without audio before the correction; the user's prolonged intermittent wait
was not reproduced. User confirmation of that symptom remains pending.


## 2026-10-20 — P/C: pregrid sky initialization and cached Enhanced shading

P: supersedes the constant warm filter/artificial orbit in the earlier realistic
lighting pass. Course sky artwork retains original sun/moon placement. Surface
light follows the first enabled Env0 direction (00398210), and the pregrid now
initializes the same environment as the race. High sun is neutral, low sun warmer;
course celestial movement is suspended as requested. The clouds remain moving.

C, bounded native/GPU integration:42 unique focused checks passed, including
negative failures, on OpenGL/Vulkan. Cloud volume/shadow lookup and linear
reflection-source decode reduce repeated per-pixel work; the32 GGX samples and
original shadow/lamp features remain. Classic comparison has zero changed pixels;
all32MiB race state matches before/after for both presets/APIs. Saved baseline
was copied before this task. Six native course/API direction checks cover
Tsukuba/Laguna/Route5; a source-colour fixture rejects synthetic sky replacement.

Local GTX1650/1600x900 grid benchmark: mean frame29.979→16.797ms OpenGL,
35.424→21.937ms Vulkan; Vulkan GPU24.216→10.077ms. Scope is one90-frame sample,
not a whole-game60fps guarantee. Lookup approximation and existing PS2 renderer
limits remain; no equivalent PCSX2 capture, full-course celestial survey or
console validation. Failed Vulkan allocation and discarded preliminary timings,
exact tolerances, captures and remaining limits are recorded in
[the performance note](notes/LIGHTING_PERFORMANCE.md).


## 2026-10-20 — C/P: invariant lighting submission and building-colour comparison

Native paired captures remain byte-identical in Classic and Enhanced on both
renderers after removing repeated per-material view uniforms, caching lens-tag
state and indexing Vulkan sampler uniforms at link time. All32MiB race state
matches the pre-follow-up binary.32 focused graphics tests pass, including
missing material/sky/reflection/lens-emission negatives. Small local total-frame
gain (~5% GL/~3% Vulkan), not a guaranteed frame rate or GPU-shading speedup.

The Tsukuba building's mauve tint is also visible in the Classic capture at the
same camera. This does not establish whether the original PS2 artwork or a base
renderer difference causes it: equivalent PCSX2 comparison is pending (A).
Retained source sky artwork supplies the visible celestial placement; this is
separate from the enabled Env0 light direction used on surfaces.
[Captures, metrics and limitations](notes/LIGHTING_PERFORMANCE.md#20-october-2026--submission-overhead-follow-up-and-tsukuba-building-tint).

### 20 October: tuning values and controller adjustment cursor (C, bounded)

Original evidence: setting.ad setupSlider L77..111 prints disabled bars as `--`
and gear ratios with `%.3t` through MUtilityGetFormatString (00490A40).
Support the three-decimal format: previously GearRoot.onInitialize aborted at
L111 after the first upgraded gear. MScaleBar uses its popup child at +0xF0
(00238D10), whereas MSliderBar uses the popup digit. Integer-backed ScaleBars
now open before editing and cancel/confirm correctly. Opening hides the context
arrow (00238BE0 +4C, 002DF738 +4C -> 002306D0(context,0)); closing restores it
(00238C60 +84, 002DF7B0 +6C). Keep the authored pinch hand and mouse behavior.

`setting_display` runs stock/equipped Ka and Civic SiR-II: 34 resolved panels,
166 nonempty slider labels, disabled gears, NOS ownership masks, gear edits,
confirm/cancel, 128 controller focus moves and original cursor_align=3 targets
for sliders/dropdowns. Two WILL_FAIL controls remove a gear label or restore
the duplicate arrow. Eight focused tests passed, including menu navigation,
setting lifecycle and catalog positives/negatives. Catalog run intentionally
limited to three cars: 189 panels/153 adjustable controls. Fixed older fixture
errors that stepped closed popup bars and returned success on failed assertions.
Main Release rebuilt; static UI audit regenerated. Physics unchanged.

Limits: no new PCSX2 visual comparison, full-catalog claim or Switch validation.
Missing native part-row ranges remain errors; this does not prove all unavailable
wrenches/panels or the user's other reported tuning UI problems are fixed.


### Wheel purchase diary category enum (20 October 2026)

C: the diary bridge now loads CORE table 006A5180 (NONE, NORMAL, RACING,
DIRT, SNOW), initialized by 0042F7A8..0042F7C0 and read by 00134474 ->
0042EEF0. It no longer substitutes wheel fitting's different four-entry
0068FB80 enum (NORMAL_SPORTS, RACING, DIRT, SNOW). The write bridge follows
00135208 -> 0042EF18: inverse lookup with zero for an unknown name.
In particular the authored shop's NORMAL_SPORTS becomes NONE, as in that
original lookup, rather than leaking WheelCategory_NORMAL_SPORTS to the page.
Existing event bytes are read as stored; no speculative save migration.

diary_wheel_labels checks all five actual MCalendar.getEvent results and
localized disc text for all four nonempty categories; its WILL_FAIL negative
reintroduces NORMAL_SPORTS and fails. fitted_purchase/negative pass. The
shop-catalog fixture passes all four written wheel categories, wing events,
and reserved-byte checks, as well as actual purchase/save/reload.
A: no equivalent PCSX2 diary screenshot was captured; category text and
schemas are checked, not full visual equivalence. Physics untouched.

### 20 October: tuning field/cursor visual mismatch (C)

Corrects the earlier cursor-only validation: comparing a cursor with our own
widget rectangle does not establish its alignment with the visible PS2 field.
Local PCSX2 slot 01 backup screenshot shows SettingRoot's Exhaust background
at x78..258 (180 units), while the old native background ended around x234.
The widget/cursor already used 180: the inherited ImageFace was drawn using
its source PNG size. Original 002A2608 +7C..C0 draws the current client size;
00230B68 -> 0025BAA0 and 00230CF8 use the widget rectangle, inset 2. Preserve
that anchor and make Setting image backgrounds fill their inherited clients.
The closed/popup retained entry (002C8198 -> 00263368) and its authored
highlight/disabled overhang now follow the same width. No manual cursor offset.

Nine focused tests pass, with three existing intentional negatives and the new
WILL_FAIL setting_field_negative restoring the old narrow field. Render-call
checks cover normal/selected field dimensions and reject short backgrounds.
The strengthened observer and three negatives were rerun after its last edit.
A test-only optional directory exports an isolated equipped Civic profile for
screenshots. Captures cover parts, gear selection/editing, brakes, NOS, ballast
and aids; local PCSX2 backup is used for the parts-layout comparison. Release
rebuilt, static audit regenerated, physics untouched. Full-screen pixel identity,
all labels, unresolved tuning ranges and Switch remain outside this proof.


### Photo Travel preview return and pointer focal control (20 October 2026)

C native regression: photo_shoot.ad PreviewRoot.onCancel L2890..2892 calls
panOut, syncOut, startPage(ShootRoot). The adapter's shared render surface
kept that completed outgoing fade opaque indefinitely. Reproduced with the
old code: active root was ShootRoot, fade opacity 1, remaining time zero.
The photo_shoot page boundary now queues the incoming leg using the existing
transition's period, retaining destination initialization overrides.
A: native context/surface adaptation; original per-context transition and
GS composition have not been proved with an equivalent PCSX2 capture.

P under the existing controls.desktop_menu option: wheel input opens the
targeted floating ScaleBar before adjusting, and direct drag preserves its
floating range/step and one-argument authored onValueChanged callback.
Previously a closed focal control routed wheel detents as focus navigation;
float ScaleBars were excluded from direct drag. Controller cancel still
restores its saved value. No physical camera constraints or physics changed.

photo_travel_flow checks twelve increasing wheel detents, drag to maximum,
persistence after mouse exit, shot/PNG/frozen preview, return with fade zero,
new shot with changed camera, and destination re-entry. The presenter also
draws/animates during synchronous transition waits. Regression negative
photo_preview_return_negative restores opaque black and fails; pointer
negative dispatches the old closed-bar arrow path and fails. Existing export
hash negative still fails. Twelve focused CTests pass, including desktop
pointer/navigation and Photo Drive entry, with their negatives.

GPU runs also pass on ss_grandcanyon with Realistic Lighting/Skybox, material
shading and HDR enabled, separately in OpenGL and Vulkan. Local captures:
build/photo-canyon-gl.png, build/photo-canyon-gl.png-return.png and Vulkan
counterparts. Brooklyn covers the default/original rendering settings.
An initial Canyon fixture failed its fresh-image hash check after zooming
all the way into a wall: moving an off-screen car did not change that image.
The check now restores framing and changes live camera yaw as well; frozen
preview and fresh-shot checks both pass. No emulator, Switch or Android QA;
this does not prove every possible cause of a black screen is eliminated.


### Light & Shadow settings-page crash (21 October 2026)

C native UI correction: the Light & Shadow row table mixed launcher API VS_*
identifiers with the independent in-game widget indices. The car-lamp entry
accessed beyond the 41 allocated rows. The table now references rows 25,26,39;
an absent Options focus is also guarded when determining an entry page.
The real executable previously exited with 0xC0000005 after printing the page
title. settings_pages now opens all six pages with disposable preferences,
checks the three intended Light & Shadow controls, and captures every page.
Its negative rejects a deliberately wrong page identity. Both CTests pass.
An initial build hit an unrelated in-flight launcher declaration conflict;
the current shared tree builds again. No physics or graphics value changes;
this verifies native routing/memory safety for these pages, not PS2 pixels.

### 21 October: launcher controller binding list (P)

The launcher cycled live physical-key assignments through studio_binding_set.
Passing an occupied key immediately removed its prior assignment. Controls now
open a two-column list: a pending selection changes on hover/key/pad navigation,
confirmation edits the live map, and cancel leaves it untouched. A destination
already occupied by another action is named before confirmation. Same-value
confirmation is a no-op; changing a function no longer rebuilds unrelated actions
or erases their alternate bindings. Storage remains launcher_options' existing
per-player controller map. This is host editor presentation/repair, not an
assertion about a new original PS2 menu or altered vehicle input physics.

Four focused CTests passed: launcher_bindings / launcher_binding_input and their
WILL_FAIL negatives. They cover all 11 pending action choices for both player
indices, signed list navigation, byte-identical browsing/cancel/no-op behavior,
intentional destination reassignment, alternate bindings, and actual launcher
key events, mouse hover and direct mouse selection with isolated preferences.
Negatives restore live mutation during browsing and are rejected. Captured and
inspected the real list layout; English labels, every key visible in two columns.
Physical controller hardware was not exercised; its edge handling shares the
pending/confirm state. Main Release rebuilt; static audit regenerated. No push.

### 21 October: procedural sky layers and night guard (P / A)

Realistic Skybox now replaces source sky radiance instead of retaining its
cloud layer under procedural clouds. The authored-environment flag still
calibrates surface irradiance. Day-only sky settings also leave authored night
sky and reflection sky untouched. The existing Env0 solar/lunar directions
are unchanged; Motegi carries the original night time-zone flag.
Full daylight sky radiance is cached at 256x128 with the existing deterministic
clock; night stars/moon and the solar disc bypass that cache. This reuses the
existing sampler budget in both backends. Night avoids daytime scattering work.
Focused OpenGL/Vulkan sky and cloud-cache checks pass, including deliberately
retained-source, disabled-cloud and bright-night negatives. Real Motegi and
Hong Kong captures are under build/oct21-*-fixed-*.png: no equivalent emulator
frames were captured, so real-course visual comparison remains A. An initial
extra-sampler implementation exceeded Vulkan's limit; it was removed and the
GPU tests passed with sampler reuse. No physics changes or universal FPS claim.

### 21 October: low-beam receiving surfaces (P)

Realistic Car Lamps now orients receiver normals to the visible side before
its diffuse calculation. Opposite source winding previously removed received
light. Front radiance is increased inside this existing optional effect and
uses a soft horizontal low-beam cutoff with a right-side upsweep. Lamp placement
and direction still come from authored geometry; original lamp activation and
Classic rendering are untouched. Geometry depth still occludes lamp light.
The generic pattern uses world gravity, not body roll or market-specific optics.
This is an approximation, not measured IES photometry or a fidelity claim.
Reference: HELLA TechWorld headlight-adjustment and light-distribution guidance.
OpenGL/Vulkan surface, winding and cutoff tests and failing negatives pass.
A Route5 native capture shows illuminated asphalt/curb in front of the car
(build/oct21-headlamp-road-enhanced-lamps.png). Equivalent PCSX2 capture was
not taken; varied weather, all cars, slopes and hardware costs remain unverified.

### 21 October: loading handoff reveal (C adapter / A visual coverage)

A queued sequence or replacement Quick command no longer reveals its outgoing
page between fade-out and the app-owned destination loader. nat_finish also
retains black for replacement commands; save/load dialogs retain their existing
reveal. Original source scripts still select the fade periods and destination.
The new positive failed before the repair, then menu_navigation, its forced
reveal WILL_FAIL negative and photo_travel_flow passed. It covers both queued
projects and COMMAND_START through the actual native finish entry point.
This proves the handoff gap, not every loading transition or PS2 frame timing.
No physics changes. Car settings checks also passed with all three negatives:
a gear ratio keyboard step is 0.001 (one stored unit), not 100 units. Absolute
pointer placement can span more units per pixel; no tuning formulas changed.

### 21 October: reflected coating independent of highlight exponent (P)

Realistic Lighting no longer maps the legacy highlight shininess to environment
reflection roughness. These are separate authored passes. The existing optional
reflection mask now uses a smooth dielectric coat (roughness 0.1), preserving
source tint/mask/strength. Direct highlight conversion is unchanged; this does
not infer metallic flakes, chrome, paint class or measured material properties.
Reference: https://google.github.io/filament/Filament.md (clearcoat model).
The new exponent-invariance positive fails against the previous shader. Both
APIs pass all 18 focused material/environment checks, including removed-reflection
and bypassed-cache negatives. Independent incident-direction quadrature checks
white-environment energy at normal and grazing incidence. Initial old roughness-1
oracle expectations failed after the change; the oracle now integrates roughness
0.1 and uses the actual pixel view angle. A coarse quadrature under-resolved the
grazing lobe; increasing its resolution converged to within one display byte.
Classic restoration remains byte-exact in the fixture. Whole-car comparison,
all paint types and equivalent emulator images remain unverified (A).

### 21 October: authored sheen coverage versus guessed asphalt (P / A)

The modern additive sheen compositor treated every pixel in the original
003411D0 destination-alpha mask as rough horizontal asphalt, multiplying
radiance by 0.04..0.10. The mask does not carry that material classification.
The compositor now preserves source tint, mask, blur and intensity; encoded
LDR-to-linear conversion for HDR is unchanged. Classic output is unchanged.
A controlled GPU positive failed before repair (normal/grazing 10/17 versus
153 reference), then passes on GL/Vulkan with zero coverage still zero.
The retained rough-road-named negative explicitly reduces sheen radiance and
fails. Six focused daylight/sheens CTests pass. No track-name exceptions.
The read-only course_surface_probe captures arbitrary cameras, Classic/modern/
no-sheen and optional HDR, and surveys geometry without simulation access.
Hong Kong orla captures remain dark in both native paths; this viewpoint did
not visibly change after the compositor correction. The user's black-water
report is still A, not fixed. The mask defect is separately proven. No equivalent
PCSX2 frame is available. Local captures: build/oct21-water-*.png.

### 21 October: keep selected RNW4 lamps in modern shading (C data / P shader)

The read-only RNW4 selector now carries whether it selected a local candidate
list (0038E960..0038EBF0), distinct from the global Env0 rig. Original uploaded
directions, attenuation and colors are unchanged and still match the native
oracle in 192 cases, including missing lists. Modern shading previously discarded
all but one lamp and replaced it with global solar radiance. Selected local
rigs now retain every uploaded direction/color, without solar cloud shadows or
a solar tint over artificial light. Classic does not use the new shader flag.
GL/Vulkan positives cover a second colored lamp with the solar source off;
removing the local flag fails. Changing the unused first light cannot alter
that result, and Classic images are identical with/without the metadata.
Seven focused GPU/native light checks pass with negatives. This is not a new
track light generator. The surveyed disc uses RNW4 custom lists on Route5,
Las Vegas, Paris and some Photo Travel places, not Grand Valley. Therefore
this correction alone does not resolve the Grand Valley tunnel report.

### 21 October: authored baked light loses shadow detail through host tone mapping (P / A)

The confirmed report is Grand Valley's fully enclosed tunnel, not the gallery
with side openings. Native read-only captures reproduce darker walls with modern
lighting/HDR. Authored baked course colours were decoded as reflectance and
compressed by the host filmic curve again: a neutral controlled level 13 became
4. The retained authored-night HDR world path similarly turned source 20 into 8.
For authored world draws only, the colour now lifts through the inverse of our
own host compression curve before optional relighting and the final compression.
This is a display-preserving host adaptation; it does NOT assert that GT4 used
ACES, recover physical albedo, remove baked shadows or invent new tunnel lamps.
The authored-night world retention path uses the same lift in HDR.
Thirty-two daylight fixtures (four intensities/four normals, SDR/HDR) preserve
Classic display levels within two bytes; night fixtures do likewise. Positives
failed before repair, and targeted negatives removing the authored handling fail
at those new checks. Existing negatives retain their separate entry points.
Nineteen focused light/baked GPU/native CTests passed in OpenGL/Vulkan; after a
test failure-return cleanup, all six night cases passed again. No full suite.
Grand Valley enclosed-tunnel captures: build/oct21-grandvalley-full-fixed-*.png
and full-vulkan-*.png. Static wall crops changed from 29.94/5.15 to 58.25/13.52
red mean on the sampled left/right areas; Vulkan agrees within .01 crop mean.
Classic before/after captures are byte-identical. These are native comparisons,
not equivalent emulator frames, so whole-tunnel parity remains A. The static
probe car has neutral ground-light input and does not certify live CarOnGround
lighting or physical lamp pools. Hong Kong HDR captures also retain more source
detail, but sea animation/reflection completeness is still unproven.
Release MoonEngine.exe rebuilt; no simulation changes, assets or push.

### 21 October: Units mouse return navigation state (C host lifecycle / A full report)

Page entry now releases the outgoing desktop hover, drag, dimmed-focus and
scroll ownership, removes its UI pointer target and cursor hold, and permits
the incoming script to establish focus outside an old hover dispatch. Native
selection/defaults remain the authored page's responsibility; no setting values
or physics change. This also applies to cached page entry and modal return.
The real-disc lifecycle positive now leaves UnitRoot through the mouse dispatch
path, with OK hovered. It failed before repair because the hidden outgoing OK
remained the hover owner; seven apply/cancel cycles now clear transient state
and allow navigation on OptionRoot. A stale-owner negative fails. Seven focused
menu/options checks pass, including profile persistence and existing negatives.
The test helper originally assumed option lists wrapped downwards; it now moves
toward the requested index to respect bounded native dropdowns. The keyboard-only
return worked before repair. This reproduces stale mouse ownership, not every
possible cause of a complete freeze; full application mouse/keyboard QA remains A.


## GT Auto independent mechanic fade (21 October 2026)

C (host composition): the camera Mot1 fade reaches the additional-model
context at 00232388..A0; 00217420..48 also separates contexts for flags,
not only a translucent car body. The host had combined crew with a fully
opaque body, discarding their fade until the original 0.02 cutoff. Separate
unequal opacities now preserve the camera fade in both reflected and normal
passes. No authored duration/channel, geometry or physics was changed.
The disc-backed oil/wash GPU fixtures hold geometry/camera fixed and check
0/0.5/1 crew opacity: before correction zero intermediate colour channels;
afterwards oil has 18,851. A negative forces opacity one at the midpoint
and fails. Nine focused composition, original-motion and GT Auto menu checks
pass, including negatives. Local captures: build/gtauto-oil-fade.png.fade0.png
through fade2.png and the corresponding wash captures.
A/open: no equivalent PCSX2 capture; complete service/end-navigation visual
parity is not asserted. Wheel/wing preview fades are a separate enhancement.


## GT Auto optional wheel/wing transitions (21 October 2026)

P: interface.part_preview_fades holds the previous resident GPU preview while
the replacement loads, then uses a 0.12-second smoothstep crossfade. It never
retains the freed model, reads pixels back or mixes the Options/HUD layer.
A changed page, car, viewport, hidden showroom or disabled option clears the
transition ownership. Authored WingShopRoot.tryon (gtmode.adc L12304..12)
only changes CarFace.wing; no original fade is claimed. Classic disables the
new setting and preserves the former replacement path. No physics changes.
Disc-backed S2000 wheel and wing tests exercise three immediate requests,
require unchanged RGB throughout loading and intermediate pixel values
after installation. OpenGL and Vulkan pass; disabling the effect makes the
negative fail. Shared-store and Classic/Enhanced persistence checks pass;
all six native settings pages open, including the new Graphics control.
Ten initial focused checks and six final focused checks passed, including
negative controls. Local midpoint captures: build/gtauto-parts-opengl.png.wheel.png
and .wing.png, with Vulkan counterparts. A/open: Android/Switch, allocation
failure, arbitrary camera animation during a swap and equivalent PCSX2
captures were not checked. This is an optional improvement, not PS2 parity.


## Car coating receives direct sunlight (21 October 2026)

P: Realistic Lighting now evaluates the existing smooth reflection-coating
lobe against the sun as well as the environment. The disc's reflection
colour/mask and opacity remain inputs; base-material specular zero no longer
prevents a solar highlight on an authored reflective surface. GGX/Smith/
Schlick follow https://google.github.io/filament/Filament.md.html ; roughness
0.1/F0 0.04 and the shared renderer solar calibration remain host assumptions,
not measured automotive materials or recovered original constants. Visibility
and cloud occlusion affect the direct term; no new texture, loop or light
direction is introduced. Existing graphics.modern_lighting owns this change.
Before correction the black-environment solar coating fixture returned 0/0
for aligned/off-axis illumination and failed. Afterwards it returns 255/0;
an independent unsaturated double-precision BRDF predicts 180.36 versus GPU
180 on both OpenGL/Vulkan. Alpha stays exact; the negative disables modern
lighting and fails. Eight focused positive/negative material checks pass.
Disc-backed Tsukuba Alfa 147 captures before/after: Classic RGB identical;
Enhanced changed bounds (162,174)..(408,290), confined to the car.
Local before/after: build/oct21-alfa-front-modern.png and
build/oct21-alfa-front-modern-coat.png. Classic counterparts have exact RGB.
The read-only MOON_PROBE_MATERIAL_AUDIT diagnostic exports source geometry,
normals and coefficients to local CSV; Alfa 147/Taurus lit/reflection vertices
have nonzero normals. This does not certify their vertex ordering, material
seams or silhouette. A/open: the user's exact faceting frame, complete metallic
paint appearance, PS2-equivalent capture, HDR photo/replay coverage and
geometry smoothing remain unverified/unimplemented. No global roughness
retuning, normal welding, geometry/physics changes or full-realism claim.


## Correction: GT Auto part transitions belong to the base game (21 October 2026)

The user identifies wheel/wing transitions as original behaviour in both disc
versions. The previous entry incorrectly classified their presence as an
Enhanced-only feature from the absence of an explicit fade in Adhoc tryon.
WheelShopRoot L11638..46 and WingShopRoot L12304..12 delegate the change to
CarFace. The native wheel/wing setters dispatch through 0013ABC0/0013ABF8,
vtable slots +198/+1A0, then the asynchronous model loader (wheel 001561F0,
wing 00156340); a script-only inspection cannot establish the visual swap.

C (host availability): removed interface.part_preview_fades from native and
launcher settings, persistence fields and preset interchange. The shop
transition no longer reads any preset/preference, so Classic also retains it.
The resident GPU path and its page/car/viewport invalidation remain. Shared
settings tests reject the removed option; the six native pages must not
contain its row. Wheel/wing loading and midpoint pixels pass on both graphics
backends; negative omits the shop-page identity and fails.

A/open: source tracing above establishes delegation/loading, not the original
blend curve or duration. The host still uses its prior 0.12-second smoothstep;
these numbers are NOT recovered PS2 constants. Neither equivalent PCSX2 video
nor a second-disc timing oracle was captured. This corrects availability and
classification, not full original transition fidelity. No physics changes.


## Race render cost pass, identical image (27 October 2026)

P, PC implementation only: no option, preset or look changes. The destination
alpha test copies only the screen box a group can cover instead of the whole
viewport; debug switches are read once per scene instead of per draw; AO reads depth once
per sample; sun shadow casters reuse their corner transform and merge contiguous
groups; Vulkan images keep their sampler. Physics unchanged.

C: reference vs new binary on Tsukuba, Hong Kong and Grand Valley East,
OpenGL and Vulkan, Classic and Enhanced, 120 frames at 1080p: zero changed
pixels and identical 32 MiB race state in all 12 pairs. GTX 1650 Vulkan GPU, for
example Grand Valley East Enhanced 21.69 -> 14.96 ms and Classic 10.42 -> 4.86 ms.
150 focused GL/Vulkan graphics tests pass, negatives included.

A/open: other GPUs (the reported RTX 4050 included), other resolutions and
cameras, night courses, split screen, Android and Switch. The Enhanced shadow
filter stays the largest cost (about 5.6 ms here); making it cheaper changes the
look and needs its own option. [Measurements, tools and what was not kept](RENDERER.md#race-render-cost-pass-27-october-2026).

Second step, same day. P: Vulkan texture-unit, descriptor-set and pipeline caches;
C: 12 of 12 pairs against 202dcd4 with zero changed pixels and identical race
state, 155 Vulkan/shadow tests pass. P, option: Shadow Filter Soft (Classic and
Enhanced, unchanged image) or Fast (four taps; the Performance preset, PC only,
is Enhanced with Fast). Fast is not the Enhanced image: it differs mostly on the
car body's own shadow. A: other GPUs, Hong Kong gains nothing.
