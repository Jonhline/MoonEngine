# Moon Save Editor (private preview)

Unofficial debug tool. No commercial game data is included.

Standalone native-profile debugging tool, using the Moon SDK's Tauri shell,
neutral palette, typography, compact controls and three-pane workspace.
The SDK stylesheet is reused under GPL-3.0; see SDK-LICENSE.txt and
F:/MoonEngine-SDK/gui/src/styles.css for its source. No SDK project is modified.

Build on Windows, with Rust/MSVC and cached dependencies:

    "C:/Program Files/CMake/bin/cmake.exe" --build build --config Release --target moon_save_catalog moon_save_vehicle --parallel 1
    cargo build --offline --release --manifest-path tools/save_editor/src-tauri/Cargo.toml

The single executable is `src-tauri/target/release/moon-save-editor.exe`.
WebView2 must be installed (normally already present on Windows 10/11).
There is no Python runtime, local server or command window in normal use.

## Editing

1. Close Moon Engine. Open an individual native `.ogt4` profile (for example
   `slot01.ogt4`) or its backup. PS2 memory cards and `.moonplayer` archives
   must first be imported by the game; this tool does not edit those formats.
2. Choose **Load game data** once and select the supported GT4 US disc.
   The bundled reader loads actual vehicle names, factory paint names/chips,
   exam targets and record mappings. The catalogue is cached privately under
   `%APPDATA%/MoonSaveEditor`; game data is never bundled with the application.
3. **Garage** lists owned cars by name. Edit mileage in kilometres, factory
   paint, active setup, TCS, ASM and brake balance in a form. Paint updates all
   three setups; aids update the chosen setup. Original full/compact records,
   wear and the selected-car cache stay synchronized. Power/weight summaries
   are recalculated when using Advanced vehicle parameters. Byte-backed debug
   settings allow 0..255; vehicle-specific in-game tuning limits are not applied.
4. **Licenses** has B/A/IB/IA/S tabs, numbered exams, medal dropdowns and best
   times. Grant/remove a whole license, including previously absent records, or
   change one exam/apply a medal to all 16 using actual disc targets.
   The result word and best ranking update together. Lowering a result removes
   faster contradictory ranking entries; clearing an exam clears its ranking.
   Existing reward claims remain intact; prize cars are not automatically given.
   Guided license editing rejects imported non-US profiles rather than using
   the US record mapping for them.
5. Other ordinary values use forms. **Advanced** retains the full typed tree
   and JSON editing for internal data. Apply validates native structure/types.
6. Undo/Redo retain up to 32 applied versions. Restore original in Advanced resets the
   selected field. Save copy shows a change review and requires a new name.
7. The source is never overwritten. Before writing, its bytes are checked
   against the opened version and backed up under
   `%APPDATA%/MoonSaveEditor/backups`. Existing destination files are refused.
8. To use the edited file, close the game and explicitly replace the relevant
   profile in its save directory, retaining your backup.

Every nested persisted value is accessible, including maps, arrays and opaque
import byte arrays. Collections support typed JSON editing in Advanced; numeric
identifiers are preserved. Vehicle metadata and standard licenses have guided
editors. A named event catalogue, guided compatible-parts installation and
automatic career reward reconstruction are not implemented.
Structural validation does not prove that arbitrary edits form a valid career.
There are no one-click unlocks with invented effects.

Integers are strings in the editing protocol so signed 64-bit values never pass
through JavaScript Number. Unknown nested members and ordering are preserved.
Unsupported versions, invalid checksums/types/lengths, non-finite numbers and
non-UTF8 strings are rejected rather than repaired or silently converted.

## Vehicle creation and advanced parameters

**Garage > Add vehicle** searches the real disc catalogue by manufacturer/model
and offers factory paint names. It builds a complete stock vehicle with the
engine factory, including all three setup sheets and native ownership records.
**Use this car** changes the current vehicle; **Remove vehicle** uses the native
garage operation. No credits are charged or purchase days advanced. All actions
remain unsaved until Save copy and can be undone.

**Advanced vehicle parameters** offers editable target power in metric cv/PS
and mass in kg. These change the actual setup, not only displayed summaries.
The original stored power/mass adjustment is 1–255 whole percent; targets are
rounded to the closest representable result, displayed after recalculation.
Out-of-range targets are rejected. Zero in raw percentage fields means the
original default 100%. This is not unrestricted engine-table replacement.

**All setup parameters** exposes the active sheet's brakes, suspension, LSD,
gearbox, ballast, aids, downforce, torque split, nitrous, physical multipliers
and equipped 64-bit part keys. Parameters whose physical units are not verified
are labelled by original group/index, without guessed conversions. **Every
stored car byte / part key** provides typed little-endian access to the entire
1,208-byte record, without base64/JSON work. Invalid part references can fail
native recalculation; arbitrary equipment combinations are not guaranteed safe.
The native helper runs separately and rejected operations leave the document
unchanged. Each successful result replaces only the garage, preserving other
career sections. Source saves are never overwritten.

**Advanced > Raw saved data** also has Add save section, Add field/entry and
Remove field/entry for persisted objects, arrays and maps. This is debug-level
access: valid stored types do not guarantee a coherent career after arbitrary edits.

## Focused tests

    cargo test --offline --manifest-path tools/save_editor/src-tauri/Cargo.toml

Build `save_editor_bridge` with the project's CMake, then run
`tests/ui/save_editor_interop.py BRIDGE EDITOR` (and `--negative`). The bridge
uses the actual engine writer/reader and requires an exact unchanged envelope.
CTest registers these as `save_editor_interop` and its WILL_FAIL negative when
`build/MoonSaveEditor.exe` exists at CMake configuration time.

The frontend test uses locally installed Playwright via NODE_PATH. It opens a
native-bridge fixture with a mocked dialog/IPC boundary, tests editing, error,
undo, review and search, and captures the actual frontend without a visible window.
It does not replace a manual test of Windows dialogs or the packaged WebView.

`save_editor_guided.py` additionally creates a real authored profile and stock
car with the native factory, uses the guided frontend to edit it and all 80
standard exams, then reloads the result through the engine. It checks all five
licenses, their medals/best times, mileage/paint and native TCS. Its negative
expects the wrong TCS value. Enable `MOON_SAVE_EDITOR_UI_TESTS` to register these
focused CTest checks; Node, local Playwright and installed Edge are required.
The native catalogue reader is embedded in the single executable at build time
and unpacked to the editor's cache when needed, with no visible command window.

`save_editor_vehicle.mjs` invokes the real offline vehicle helper behind the
headless frontend boundary: add/select/delete, actual power/mass, tuning bytes,
license grants/removals, absent license records, primitive field types and raw
field creation/removal. Invalid models and impossible power targets are refused.
The WILL_FAIL variant deliberately expects an incorrect physical mass. Its Python wrapper creates a fresh native fixture and catalogue; local
Playwright/Edge are required.
