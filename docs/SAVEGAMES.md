# Save games and legacy transfer

Native persistence is implemented in `src/core/save.c` and `save_state.c`;
the application storage screens are in `src/app/save_ui.c`. Profiles,
settings and native replay/media files use PC storage. The PS2 save reader
in `src/core/save_ps2.c` handles legacy data separately.

## US GT3 transfer

The US version transfers credits and B/A licenses from US GT3 data. Its
internal `inheritGT4pData` path also selects GT3 data in this edition.
Place extracted GAMEDATA at:

```text
<profile directory>/imports/BASCUS-97102GAMEDATA/BASCUS-97102GAMEDATA
```

Open a profile and enter GT Mode Status to use the transfer actions. The
credit cap is 100,000, once per profile; better existing licenses are kept.
This transfer input is an extracted file, not a complete card image or PSU.

Synthetic data and US executable research cover the bridge. Validation with
a real GT3 save remains pending; other regions and actual Prologue transfer
are not supported by that proof. See [Fidelity](FIDELITY.md) and
[media coverage](notes/PHOTO_MEDIA_COVERAGE.md) for separate import limits.

## First-run questions

To import a complete GT4 save instead of creating a new profile, answer No
to the first welcome question, then Yes to Import Savegame. This uses the
existing GT4 file picker and import validation. A newly created profile
also shows optional legacy-transfer instructions after the Auto Save
explanation. Those instructions describe the US GT3 transfer above; the
transfer itself is performed from GT Mode Status, not by acknowledging the
welcome messages. Actual GT4 Prologue input remains unsupported.
