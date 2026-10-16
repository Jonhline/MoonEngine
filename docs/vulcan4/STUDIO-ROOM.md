# THE STUDIO ROOM — what the launcher draws before the game (spec, 2026-10-16)

The captain: *"once u choose those 2 things the car gets rendered inside a white room exactly like in the
image … i just want the car rendered and i want u build that white room. but what i also want is that the
room wont be static: i want to be able to change the lighting color, make it dark mode if needed, and all
of that will affect the car's reflection. and there will also be camera movements."* Later, mandatory:
**"a reflective floor is mandatory btw"**.

## THE FLOW (his order)

1. **Ask for the GT4 ISO** (first thing — before anything else).
2. **Ask OpenGL or Vulkan** — for now OpenGL only; Vulkan is a second backend the other side is adding.
3. **The room**: the car rendered in a white studio, with the menu/dock over it.

## THE CAR (settled 2026-10-16)

The cover car is the **plain** Ford GT, not a race car and not the striped road car:

| what | code | model | note |
|---|---|---|---|
| **the one** | `ford_gt_no_stripe_05` | ford0015 | 11,301 triangles, **--color 0 = Centennial White** |
| striped road car | `ford_gt_05` | ford0006 | what we rendered first; it looked like a "special" car because of the stripes |
| race cars | `ford_gt_lm_sp2_04`, `gt40_race_69` | ford0014/0007 | not this |

`ford_gt_no_stripe_05`'s paints: 0 Centennial White / 1 Quick Silver / 2 Mark II Black /
3 Midnight Blue / 4 Mark IV Red / 5 Speed Yellow. The studio prints the paint's own name from the
disc, so the colour is picked by name and never guessed.

Measured on Baki (Windows, retail v2.00): the no-stripe body is 56 triangles smaller and its
upper-body band reads 154 where the striped one reads 140 - the stripes, gone.

## THE ROOM, MEASURED OFF HIS REFERENCE (never eyeballed — pixel statistics)

Reference: `/home/or/.hermes/cache/images/img_57c666c3474b.png`, 1920x1080, sent 2026-10-15 20:22.

| what | value |
|---|---|
| background (seamless sweep, no corner, no fixture) | `#FEFCFD` (254,252,253) |
| floor | ~`#F2F1F2` (242,241,242), soft falloff into the wall |
| car | dark body, **right of centre**, wheels on the floor, soft contact shadow |
| highlights on the body | a lavender band ~`#9694BB` and a pink one ~`#DA949D` = what the lights do to the paint |

No walls to walk into, no props, no visible light fixture: the room is the frame, the car is the subject.
**The floor is REFLECTIVE (mandatory)** — the car's underside and the light must read in it.

## WHAT MAKES IT NOT TRIVIAL

1. The car has to be decoded from the user's own disc and posed in the arcade CarFace pose — already in the
   tree (`car_model.c` + `showroom.c`), which is why this is a variant and not a new renderer.
2. The **light rig**: colour + intensity + dark mode, and it must actually move the paint and the
   reflections (the studio's `env.mdl` sphere maps are how the paint reflects today).
3. The **reflective floor**: a mirrored pass of the car about the floor plane, faded into the floor.
4. The **rig**: camera and car can be moved, and their coordinates are remembered (saved to a file, loaded
   back exactly).

## THE DISHES (each ends in a picture the captain judges)

| # | dish | gate |
|---|---|---|
| 1 | white room + car, `--studio <car> --shot FILE` | a PNG that looks like the reference |
| 2 | **reflective floor** + light colour / dark mode moving paint + reflections | 2-3 PNGs, different moods |
| 3 | the rig: move camera and car, save and reload their coordinates | a PNG after a save/reload round trip |

Dish 1 was dispatched to Sanji 2026-10-16 08:43 (log `/mnt/ssd/studio/dish1-run.log`).
