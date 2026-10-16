# Native renderer

Implementation: `src/platform/sdl/video_gl.c and src/platform/sdl/render3d.c`.

SDL owns the window and graphics context. The OpenGL backend renders decoded model geometry with texture addressing, alpha tests, blend state and depth state derived from disc data. Course visibility lists limit scene submission. UI drawing and 3D drawing share host output but use different projection rules. Whole-frame comparison remains separate from tests of individual draw states.

## Car material lighting

Surface highlights and environment reflections are separate authored passes.
The renderer retains each vertex's material specular colour and shininess;
reflection strength also uses authored vertex colours and destination-alpha
masks. It does not assign a single gloss coefficient to the whole car.

The specular-only path (`func_0049EB90 +0x84..0x94`) consumes the specular
colour block without ambient or diffuse lighting. Its contribution must be
selected before the race lighting path saturates the vertex colour. Clearing
the separate specular output first used to erase that layer entirely.
Environment geometry uses VU1 program `0x215`, independently of the surface
light blocks; the shared PC shader must not overwrite its authored colour
with ambient/diffuse lighting when no angular environment override is active.

`car_material_opengl` and `car_material_vulkan` render 21 cases each: three
specular strengths for diffuse and specular-only surfaces in both lighting
paths, plus three reflection colours under three specular strengths. Expected
channels allow one byte of GPU quantization. Both deliberately corrupted
material tests must fail. The previous shader failed the specular-only race
and environment-colour cases in both APIs. These focused checks cover pass
separation, not complete PS2 image equivalence: car appearance remains A
pending comparisons of actual glass and paint with the original game.

## Evidence and limitations

The PC renderer choice and native Vulkan implementation are documented in
[PC graphics backends](VULKAN.md). OpenGL is the default; Vulkan draws menus
and 3D scenes and remains experimental pending broader coverage.

The [development record](notes/RENDERER.md) retains addresses, experiments,
test results and outstanding limitations. Older entries describe the state
at their recorded date and may be superseded. Consult [Fidelity](FIDELITY.md)
for per-item grades; implementation coverage is not full-game certification.
