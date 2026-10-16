# Planned mod format

The mod system is a design for the planned improvements (see
[EDITIONS.md](EDITIONS.md)). There is no stable package schema, supported
converter or SDK promised by this document. Mods are optional: without
them, the game uses only the user's disc content.

## Packages

The proposed package contains a manifest with name, version, author and
dependencies, plus replacement files addressed by volume paths such as
`/car/...` and `/specdb/...`, or new content. Car/course identifiers must
be explicit. Readable SpecDB tables, potentially JSON, would be converted
into the rows consumed by the native game code. The schema and conflict
resolution rules still need to be specified.

## Existing PS2 mods

The planned importer handles assets only: cars, courses and sounds from
files the user supplies. Executable patches and custom mechanics are not
imported; equivalent functionality would require native features of the game.
No existing mod assets are distributed with MoonEngine.

Different source editions can require translation to the reference
SCUS-97328 v2.00 formats and tables. Asset compatibility does not imply
that modified executable behavior can run in the native application.

## Car data requirements

The existing readers show what a car package must integrate:

1. Identity and catalog data: `GENERIC_CAR`, names, maker, year and category.
2. CAR4 menu and race models, optional open-top model, component geometry,
   camera and driver attachment data.
3. Paint variations and the model's pattern data.
4. `DEFAULT_PARTS` and physics tables for engine, drivetrain, suspension,
   brakes, tires, gearbox, steering and differential.
5. Tuning parts and price tables.
6. An engine sound bank and its selection data.
7. Catalog pictures, thumbnails and HUD icons.
8. Availability in dealerships, used stock, rewards and event restrictions.

A working mesh alone is not a complete car. Sound selection, catalog images
and driving data need to be understood before finalizing the package format.

## Proposed model authoring workflow

The intended workflow is glTF 2.0 authoring followed by conversion into CAR4,
so mod cars use the existing reader and drawing path. Proposed component
names include `body`, `body_open`, `wheel_FL/FR/RL/RR`, `brake_*`, `tire_*`,
`steering_wheel`, `lamp_brake`, `shadow`, `hull`, camera anchors and a driver
seat. Paint variants could use `KHR_materials_variants`.

A converter would map modern materials to GT4 material kinds, create Tex1
textures and pattern data, and write geometry compatible with the existing
model decoder. A GT4-style profile would constrain texture sizes, palette
formats and geometry budgets. Exact budgets and validation rules are not a
finished specification. PBR inputs cannot simply be treated as native GT4
materials. Producing CAR4 files accepted by a physical PS2 is a separate,
more demanding goal.

Validation should check mandatory components, wheel dimensions against
physics data, showroom rendering and headless race loading. Round-tripping
local disc models through a future writer would provide an independent
compatibility check. Existing tools and their licenses need review before
any converter is adopted.

## Proposed course authoring workflow

The native course readers consume scene geometry, sky and reflection data,
RNW4 collision structures, surface classes, grid/sector/finish data, driving
lines, Env0 lighting and fog, particle images and camera data. See
[Courses](notes/COURSES.md) and [Renderer](notes/RENDERER.md).

A proposed authoring format separates visual `world` geometry from
`collision`, with surface materials, grid points, sectors and a centerline.
The converter would build collision search structures, original-format
materials and textures, editable driving lines and environment data.
TV/start cameras would be additional authoring inputs.

Validation should run a native driver around the course without rendering,
check sector and finish crossings and ensure the grid fits. Driving-line
construction is a significant unresolved part of this workflow. Assets
from a related engine still require format and data checks; arbitrary
courses from unrelated games are not supported by this plan.
