# Painted fields

A 1440 × 1040 Compose material workbench titled FIELDWORK. Natural-media
brushes and Georgia lettering write a height canvas; separate brush passes
write water coverage. The same painted maps drive the stone, steel and bronze
panels. The lettering changes the shared surface's normals. Water coverage
lowers roughness and darkens the stone without darkening the metals. The page
carries no light, so each panel paints its colour stack flat.

The sidebar shows the painted height, the water coverage and the height's
normal before the metals add their fine brushing, along with the steel panel's
roughness targets. A white dot in the water canvas reaches full coverage, so
its centre reaches the wet roughness target.

`Fields.h` holds the painted inputs, `Surfaces.h` their interpretation, and
`painted_fields.cpp` the interface and clock.

## States

The 32-second loop has eight four-second states: painted, zero impression,
recessed type, dry, gloss, matte, erase and clip, and clear the canvas.
`kStates` sets each one's signed impression depth, wetness and dry and wet
roughness; `heldPhase` pins one. The first three states and the erase state
also pulse the wetness while keeping their painted maps. The erase state crosses the input rectangle, erases a strip and
a circle and keeps the remaining strokes; the last state clears both maps.

## What it does not do

The panels are flat, not displaced. Wetness is an appearance control, not a
fluid or absorption simulation, and it has no separate water-film normal or
coating. Directional grooves are normal detail, not an anisotropic reflection.
