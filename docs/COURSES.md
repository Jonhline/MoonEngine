# Course data and native scenes

Implementation: `src/core/course.c, src/core/physics/runway.c and src/platform/sdl/race_view.c`.

A course combines scene geometry, sky and reflection assets, RNW4 collision data, surface classes, grid and sector information, driving lines, environment lighting and camera records. The native scene loader submits visible models; physics queries the collision structures independently of rendering.

## Evidence and limitations

The [development record](notes/COURSES.md) retains addresses, experiments,
test results and outstanding limitations. Older entries describe the state
at their recorded date and may be superseded. Consult [Fidelity](FIDELITY.md)
for per-item grades; implementation coverage is not full-game certification.
