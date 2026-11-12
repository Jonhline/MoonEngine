# 21 October 2026 renderer reports

## Delivered fixes and limits

- Light & Shadow settings crash: native widget indices repaired. Real process
  reproduced 0xC0000005 before repair; all six page captures and negative pass.
- Mixed/black cloud sky: procedural sky replaces authored sky radiance;
  source calibration remains for surfaces. Night and reflection guards fixed.
  Full-sky cache avoids repeated daylight integration within the existing sampler
  budget. GPU positives and negatives pass on OpenGL and Vulkan. Motegi and Hong
  Kong captures are local; equivalent PCSX2 images remain absent.
- Loading flash: old-page reveal held during app replacement handoff. Positive
  failed before fix; menu navigation, forced reveal negative and Photo Travel
  flow pass. Not all loading routes have equivalent visual captures.
- Front lamp receiving light: visible-side normals and generic low-beam cutoff,
  stronger optional radiance. Both GPU backends pass winding/cutoff negatives;
  Route5 real-course captures exist in both. No measured lamp photometry claim.
- Modern reflected coating: legacy highlight exponent no longer forces a rough
  environment lobe. Smooth generic coat retains source mask/tint. Eighteen GPU
  checks pass, including independent white-environment quadrature and negatives.
  Whole-car/all-paint comparison remains pending.

## Checked behavior, no simulation changes

Car settings ratio key steps remain one stored unit (0.001 ratio); setting_display
and missing-label, cursor and field negatives pass. Absolute pointer placement
can jump more than one unit because the whole authored range maps to bar pixels.
Original post-race grid code 00114424..00114438 tests car+5B6 before copying
car+718, otherwise retaining the no-time sentinel. The native adapter follows
this boundary. This does not establish every possible timing prediction elsewhere
in the original, and no extrapolation or physics changes were added.

## Open reports

- Hong Kong: user identifies the sea below the waterfront boardwalk/passarela as
  black. Specific surface and equivalent Classic/Enhanced/PCSX2 frame are still
  needed. Do not claim water is missing geometrically or fixed. Course reflection
  mask and additive sheen need separation from rough asphalt assumptions before
  changing a material. No track-name exceptions.
- Daylight tunnels: identify the source-lit/emissive surface and original pass;
  compare with and without modern lighting before adding or changing any light.
  Current per-object RNW4 lights do not by themselves prove tunnel lighting.
- Performance: full daylight sky cache and deep-night early-out reduce repeated
  shader work, but no universal FPS improvement is measured here. Whole-game
  performance and GPU/track coverage remain open.
- Visual certification: screenshots are local outputs under build/oct21-*;
  no equivalent PCSX2 frame was captured in this task. No global fidelity claim.

No assets, captures or generated game C belong in commits. No push.

## Follow-up: confirmed enclosed Grand Valley tunnel

The user clarified Grand Valley and then the fully enclosed tunnel, rather
than its gallery with side openings. Real native captures now cover both,
including the enclosed camera (-100,2.5,-148) toward (-120,2,-130), a static
visual car at (-112,1,-135), and GL/Vulkan/HDR variants. No solver runs or edits.
The host tone conversion crushed authored shadow detail; the repaired mapping
passes targeted negatives and keeps Classic pixels unchanged. Source light
metadata is also preserved for selected RNW4 local lists, but Grand Valley has
no custom RNW4 list, so that separate correction must not be cited as its fix.
No equivalent PCSX2 image or live car ground-light proof exists yet.

The broad sheen mask no longer assumes rough asphalt. This fixes its controlled
material error, but the sampled Hong Kong waterfront image did not change from
that correction alone. Night HDR tone conversion recovers source detail, not
proof that all sea surfaces, reflections or animation are present. Water stays
open for equivalent-frame inspection. No name-based water or tunnel workaround.
