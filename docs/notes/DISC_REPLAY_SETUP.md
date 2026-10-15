# Disc replay setup verification

The PC importer now preserves each GT4M entry's grid, driver options and
wear, the serialized RaceInfo options, and the RACE settings row even when
the stored world is Arcade. These initialization fixes do not certify the
complete trajectories of the authored demos.

## Original implementation

- `003DD838 +0x30` resolves RACE through `003BB478` and `004470E8`.
  A RACE row does not imply a career world.
- `003DD838 +0x44` and `00388E38 +0x98` preserve the first 0x60 bytes of
  RaceInfo at GT4M +0x50.
- `003DDA08 +0x30` calls `003DDAD8` for each serialized 0x228-byte entry:
  setup, wear, flags, grid, four booleans and two signed options. Runtime
  links remain intact.
- `00387CF0` retains A/B-Spec while selecting recorded mode 2/3;
  `0010BBB0 +0x1AC` marks RaceInfo +0xF0 as playback.

The native replay file format is unchanged. Disc-only import metadata is
copied into the active race before the file is freed. Solver formulas and
tape samples are unchanged.

## Automated evidence

`disc_replay_setup` compares original 003DDAD8 and the native helper on
identical destinations filled with canary bytes. All 0x3490 bytes match
for 53 authored entries and 128 varied records. The negative corrupts a
grid byte and fails. Grade E applies to this restoration operation.

`disc_replay_bspec_runtime` uses the headless runner for Rdemo1, Rdemo3,
Rdemo5 and Rdemo8. Each consumes 1200 commands, retains the B-Spec driver
kind, and moves with finite coordinates. The negative drops the mode and
fails. Grade C covers this integration, not original trajectory parity.
Eleven focused replay tests pass, including command/checkpoint oracles,
deliberate negatives, and native replay round trips.

## Unresolved A-Spec trajectory

Rdemo0, Bentley Speed 8 on paris_rally, still reaches the inside wall and
becomes stuck. The initialization fixes do not resolve that defect.
It remains grade A; the complete demo system is not fixed.

Local diagnostic comparisons matched the conductor's 0x10160-byte state
across 1800 consecutive native/original step comparisons from identical
input states. Original and native Paris runway relocation also match.
These checks share the native initial context: they do not prove that
context matches a fresh original replay session. Further investigation must
establish original initialization and controller/global settings, without
adjusting tape timing or steering to hide the divergence.

The diagnostics remain local under build/disc-pc-audit. No game data or
generated original-code output belongs in the repository.

## Surface divider correction (2026-10-14)

`0035F808 +0x1B8` computes each surface wave number with `div.s`.
Host division in toward-zero mode produced `0x41490FDA` for a value
captured as `0x41490FDB`. The native initializer now uses the existing
integer SRT divider (`ps2f_div`) for this operation, retaining the original
wavelength multiplication and storing the result bits directly.

`physics_surface_waves` rebuilds the 24 wave numbers independently from
17 existing Arcade, Time Trial and replay RAM captures. All 408 words
match bit for bit. Its deliberate negative changes one bit, reproducing
the former first-surface result, and fails. Grade E covers these wave
numbers on those fixtures. Captures stay local and are not distributed.
CMake registers the tests only when matching local captures exist.

The original-code step comparator and reconstructed common world start
both reproduce the initial defect when given the native initial context.
The compressed SpecDB rows also match the original decoder. These checks
narrow the investigation; they do not establish complete replay parity.

The complete 9996-command Bentley recording was tested without a window.
The corrected surface table carries it past the first wall (still moving
at tick 1800), but it subsequently becomes stuck near tick 2400. Full
trajectory fidelity remains A. No tape offset, steering adjustment,
teleportation or replacement driving logic was introduced. Diagnostic
programs and generated original code remain under `build/disc-pc-audit`.
