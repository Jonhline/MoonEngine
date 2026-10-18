# UI art direction

How every screen Moon Engine adds should look and behave: the in-game PC pages,
the launcher and anything a mod or a later feature brings. One idea decides it:
**it must look as if Polyphony had shipped GT4 on PC and kept it alive**, never
like a mod bolted on. When in doubt, do what the game already does somewhere.

Related: [Options](OPTIONS.md) (every non-original change is a setting),
[Editions](EDITIONS.md), [Launcher](LAUNCHER.md),
`docs/vulcan4/LAUNCHER-ARCHITECTURE.md` §7 (the launcher's own rules).

## Principles

1. **The game's own language first.** Fonts, frames, panels, the cursor, the
   menu sounds and the transitions come from the original. A new screen reuses
   an original one's layout when one fits (the PC Screen Settings extend the
   original Options pane; the launcher's bar is the dealers' bar).
2. **Read from the user's disc, never ship it.** Game art, fonts, icons, sounds
   and music are decoded from the player's own disc at run time. Nothing from
   the disc is bundled, cached in the repository or derived into new artwork.
3. **No generated art for the project.** The title and logo are drawn by a human
   artist. Until then, text placeholders, marked as such ("logo placeholder").
4. **Without the disc, a quiet stand-in in the same layout.** Before a disc is
   chosen (first-run setup), the screen keeps the same composition with drawn
   shapes and the bundled Barlow face; the disc's look takes over once it exists.
5. **Original first, improvements as settings.** Anything that is not the PS2
   game is a setting registered in [Options](OPTIONS.md); the Classic preset
   returns the original. A screen that only exists on PC is graded P.
6. **Every input, one behaviour.** Mouse (hover and click), keyboard and
   controller reach the same actions. Hover shows what a click would do.
7. **Physics and results never change for presentation.** A ghost, a camera or
   an effect is drawn, not simulated.

## The launcher (studio front-end)

Reference: the GT Mode dealers' bottom bar (a dealer screen, PS2).

**Window.** 4:3, 960x720, resizable (F11 full screen). START hands the game its
own window settings.

**The room.** The white studio with the car centred. Opening framing: side
profile. Each choice may move the camera (START front three-quarter, OPTIONS
from above, MODS closer, EXIT the car leaving the frame). Idle: the 18
cinematic shots, dipping through black at cuts. Themes: light (the white room)
and dark; a light colour tints the room, the floor and the car's lights.

**The bar** (640x448 design units, scaled with the window width):

| Part | Spec |
|---|---|
| Slots | 4, equal, symmetric, 128 apart, centred (128, 256, 384, 512) |
| Band | y 363..418; the dealers' brushed metal (DealerRoot `wall_usa.png`, a strip), a fine light top edge (white, alpha 110) |
| Icons | one size by inked area (transparent margin cut), about 48 tall, rising out of the band's top |
| Names | the disc's `hm_16` face at 0.8, white with a soft dark shadow, baseline 408 |
| Help strip | y 418 to the bottom, black (alpha 240), the selected item's line in the disc's face, centred, baseline 436, "Name: what it does." |
| Hover | the item grows about 14 % and rises 4 units, its icon and name brighten; the others dim |

**Text.** The disc's faces with the game's own pen steps (the contour pair
kerning, `gt4_font_original_step`); figures keep the width of the 0 so numbers
never jiggle.

**Pointer.** The game's own cursor replaces the system cursor and follows the
mouse; with a controller or the keyboard it parks under the chosen item.

**Sound.** The menu's own effects: cursor on a move, ok on a confirm, cancel on
back/exit, louder than the music. Music: the game's menu sets, shuffled, with
crossfades between tracks.

**Pages** (OPTIONS and later MODS). See-through panels inside the room, never an
opaque screen or a second window: near black (12, 13, 16) at alpha about 175,
a fine light top edge, full width above the bar. Tabs in the disc's face, the
open one underlined, hover lit. Rows: name left, `<  value  >` right; the arrow
the mouse would press turns gold (255, 204, 64) on a soft box. The help strip
explains the selected row.

**Corner information.** Top left: the wordmark (placeholder). Top right: the
disc's status with a green or red lamp, the GT Mode profile, the car, mods.

## The in-game PC pages

Extend the original project's pages (fonts, checkerboard, pane, dropdowns,
cursor, OK/Cancel) instead of drawing new ones; split long lists into tabs the
way the PC Screen Settings do (L1/R1, Q/E). Widescreen follows the game's own
flag (D_006184F0) and moves elements as the game does, never stretched.

## When adding something

- Find the closest original screen and copy its proportions and behaviour.
- Use the disc's assets; if one is needed before a disc exists, draw a neutral
  stand-in in the same place.
- Register a non-original behaviour as a setting with its Classic value.
- Compare side by side with PCSX2 (or the reference print) before calling it done.
