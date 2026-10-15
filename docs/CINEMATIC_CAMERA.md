# PC cinematic camera

This is an intentional PC feature (P), available during a one-player race
or replay. F9 toggles it; Escape leaves it before normal pause/exit handling.
It starts with the HUD hidden and does not record a video automatically.

| Control | Action |
|---|---|
| Mouse | Look; orbit in follow mode with target aim enabled |
| WASD | Move relative to the camera |
| Q / E | Move down / up |
| Shift / left Ctrl | Fast / precise movement |
| Mouse wheel | Lens zoom |
| F | Follow / free camera |
| G | Attach to body / detach |
| Tab | Select next car |
| T | Toggle target aim |
| Z / C | Horizon roll |
| B | Depth of field: off, f/2.8, f/5.6, f/11 |
| [ / ] | Decrease / increase manual focus distance |
| R | Restore automatic focus on the selected car |
| P | Hold / resume the scene while moving the camera |
| H | Toggle HUD |
| Home | Reset behind the selected car |
| F5 | Enter Photo Drive with this framing |

The controller can continue driving while keyboard input controls the
camera. Replay is useful for moving shots. Body attachment follows car
position and rotation while allowing camera adjustments. Hold uses the
presentation mechanism rather than editing physics state.

Blur uses eight scene draws per frame and can reduce performance. The lens
reuses `gt4_photo_lens`; PC sensitivity, smoothing and presets are deliberate
choices rather than original GT4 camera behavior.

`race_cinematic` checks controls, movement, targeting, axes, zoom/focus,
hold/reset and unchanged physics memory, including blurred drawing. An
intentional changed byte must fail. These checks do not certify the entire
renderer or original optics; see [Fidelity](FIDELITY.md).
