# Native renderer

Implementation: `src/platform/sdl/video_gl.c and src/platform/sdl/render3d.c`.

SDL owns the window and graphics context. The OpenGL backend renders decoded model geometry with texture addressing, alpha tests, blend state and depth state derived from disc data. Course visibility lists limit scene submission. UI drawing and 3D drawing share host output but use different projection rules. Whole-frame comparison remains separate from tests of individual draw states.

## Evidence and limitations

The [development record](notes/RENDERER.md) retains addresses, experiments,
test results and outstanding limitations. Older entries describe the state
at their recorded date and may be superseded. Consult [Fidelity](FIDELITY.md)
for per-item grades; implementation coverage is not full-game certification.
