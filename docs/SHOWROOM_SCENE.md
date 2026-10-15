# Showroom scene data

Implementation: `src/platform/sdl/showroom.c and src/core/mot1.c`.

The selection studio combines a car model with disc background, environment images, lights and camera motion. Mot1 supplies curves and constants for these properties. The native implementation resolves them into scene values; camera.mot and light.mot are data files, not compiled gameplay code.

## Evidence and limitations

The [development record](notes/SHOWROOM_SCENE.md) retains addresses, experiments,
test results and outstanding limitations. Older entries describe the state
at their recorded date and may be superseded. Consult [Fidelity](FIDELITY.md)
for per-item grades; implementation coverage is not full-game certification.
