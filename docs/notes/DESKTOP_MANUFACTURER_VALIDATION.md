

### 16 October: manufacturer preview, entry pose and regulations wheel

Desktop Input keeps BuyCarRoot.CarScroll's hovered index as preselection,
so the existing magnification follows the mouse and onActivate opens the
same car's authored colour selection. The strip offset stays independent
until wheel/edge navigation changes it. Classic/disabled pointer input does
not execute this route. No additional setting is introduced.

BuyCarRoot was explicitly excluded from the application's authored pose
bridge. It now passes the script's CarModel position/rotation like other
CarFaces. The original move_car starts at tz 8.5, ry 180, warps, targets
tz 0 and sets posInterpolateSpeed 0.025; native turntable/position updates
already implement these commands. CarModel opacity now resets per model
and advances through gt4_service_fade (00153B50/00153BB8); an explicit pose
uses this value instead of the standalone viewer's approximate 0.4 s curve.
The studio lighting/reflection still has existing approximate paths; this
change does not certify a pixel match for the whole showroom.

EventInfoDialog's pointer wheel uses its original onKeyPress handler over
the Regulation viewport (40-unit step, clamped to content height) instead
of competing with its MoveActor through generic scroll coordinates.
The pickup event reaches the last row with both original Down and wheel.
Actual row divider geometry remains under investigation; no invented gap
has been added.

The supplied 1920x1080, 60 fps, 6.183 s recording is headed Online Public
Beta. Its thumbnail density differs from our retail USA v2.00 fixture.
Retail mproject CarItem is 50x32, spacing 8, magnify_ratio 1.4; the local
Spec II mproject has 50x32, spacing 6, magnify_ratio 1.4. These definitions
alone do not explain the apparent size difference; runtime layout and
edition comparison remain open. Do not claim this sizing is resolved.

DealerRoot.onInitialize instructions 54..63 selects piece/tuner_logo/ford.img.
That 378x135 image includes opaque black inside the plaque (100,10 =
0,0,0,255), with transparent outer corners (0,0 alpha 0). Removing all black
would alter the disc image. Asset not committed.

Validation: desktop_navigation checks disabled/enabled hover, persistent
preselection without strip movement, authored entrance parameters, fade
progress and click-to-colour selection, plus Ford plaque texels.
gtmode_regulation_text tests pickup keyboard/wheel reachability and disabled
pointer behavior; their negative controls remain enabled. Existing
service_placement oracle checks native movement/fade against original EE.
Only focused tests are run. Full visual comparison remains pending.


### Completion: compact presentation, interpolation, gradient and scrollbar

Supersedes the open implementation items above. The user confirmed the
recording is Spec II. Compact Car Strip is explicitly a PC presentation
option (P), not an assertion that retail authored geometry was 40 pixels.
On draws at 80% of the original dimensions: 50x32 becomes 40x25.6 and the
selected 1.4 ratio draws at 56 pixels wide. Off uses original dimensions.
It keeps original textures, labels, actions and selection callbacks.

Selection dimensions now ease per 60 Hz field using bits 3E4CCCCC from
002D5660 +20..64 / 002D6868 +68..BC; the old instantaneous enlargement is
removed. Current widths advance neighbouring cards instead of overlapping
them. Scroll movement uses the same native ratio and the +/-3DCCCCCC snap
condition from 002D6868 +DC..12C. The item's current height is centred as
002D6B40 +88..A0. Original source geometry remains available in Classic.

Regulation Dividers is an independently optional background cut, equal to
the original template pad_bottom (4). It preserves text geometry, actual
content height, keyboard scrolling and the wheel handler fixed earlier.
The cause of PS2 row separators has not been proved: the original-feature
A record remains A. This optional presentation is P and is not relabelled
as proof of the missing original geometry.

CourseRoot gradients: 002854F8 +E0..118 explicitly copies source color
indices 0,1,3,2 into the TL/TR/BL/BR render strip. The port passed 0,1,2,3,
turning the authored horizontal red fade into a diagonal. color_list now
applies the original permutation for four-color faces. Source values are
unchanged; single-color faces remain unchanged. The regression records the
actual course heading quad, including both left reds and right zero alpha.
MOON_GRADIENT_NEGATIVE=1 deliberately rejects it (exit 1).

The horizontal ScrollBox thumb also treated a negative SelectBox offset as
a positive centred ListBox distance. It now computes the visible fraction
from -offset / (content - viewport), retaining the separate ListBox rule.
The regression moves to the last course and back, checking actual thumb
movement and track bounds. This is shared UI code; no Switch-specific code
was changed and no hardware test or Switch package is claimed.

Validation: nine focused CTests passed (desktop_navigation +/-;
desktop_pointer +/-; desktop_options_store +/-; gtmode_regulation_text +/-;
desktop_settings_app). The real executable test covers presets, both new
settings, Cancel, OK, persistence and dropdown navigation. The regression
checks 70/56/70 thumbnail widths (tolerance 0.01 UI pixels), intermediate
zoom, 48/44/48 regulation backgrounds and invariant content height.

Local visual checks: build/course-gradient-fixed.png (original scripts via
TopRoot and three Accept inputs; no script errors), dealer-proof-0/1.png,
dealer-entry-settled.png (three-second entry, car visible, no script errors),
and desktop-settings-test/rendering_layout.png (all seven rows and footer
inside the pane). The supplied reference and extracted assets remain local.
Showroom light/reflection approximations are not certified by these checks.
