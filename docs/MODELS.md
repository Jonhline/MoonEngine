# Native model decoding

Implementation: `src/core/model.c and src/platform/sdl/car_model.c`.

The model reader converts disc model commands into triangles, textures and draw-state records. Car components include body, wheels and conditional branches such as brake lamps. The SDL backend uploads these records and applies component transforms. This is asset decoding into a native renderer, not execution of the original graphics processor.

## Evidence and limitations

The [development record](notes/MODELS.md) retains addresses, experiments,
test results and outstanding limitations. Older entries describe the state
at their recorded date and may be superseded. Consult [Fidelity](FIDELITY.md)
for per-item grades; implementation coverage is not full-game certification.
