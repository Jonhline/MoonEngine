# Menu presentation

Implementation: `src/core/ui.c, src/core/ui_motion.c and src/app/main.c`.

Widget trees, faces and actors from menu projects provide drawing and animation data. The application composes movies, underlying pages and modal dialogs. Cursor, help text, fonts and sound connect to the same page lifecycle. Presentation correctness requires checking composition and timing, beyond successful navigation.

## Evidence and limitations

The [development record](notes/MENU_PRESENTATION.md) retains addresses, experiments,
test results and outstanding limitations. Older entries describe the state
at their recorded date and may be superseded. Consult [Fidelity](FIDELITY.md)
for per-item grades; implementation coverage is not full-game certification.

## Text sharpness

`src/core/glyph_sdf.c` converts the disc's bitmap glyph coverage into a
signed-distance atlas. Coverage remains available for the original soft
appearance, while the distance channel supports sharper edges at larger
window sizes. This changes presentation, not script strings or font metrics.
The [text-sharpness findings](notes/MENU_PRESENTATION.md) retain the detailed
algorithm and validation; see also [text formats](TEXT_FORMAT.md).
