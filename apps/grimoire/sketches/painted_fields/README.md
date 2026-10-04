# Painted fields

A 1440 × 1040 Compose material workbench titled FIELDWORK. Natural-media
brushes and Georgia lettering write a height canvas; separate brush passes
write water coverage. The same painted maps drive the stone, steel and bronze
panels. The lettering changes the shared surface's normals. Water coverage
lowers roughness and darkens the stone without darkening the metals. The large
steel panel catches a moving point light in front of the page; a fine reticle
shows its projected position.

The sidebar shows the painted height, the water coverage and the height's
normal before the metals add their fine brushing, along with the steel panel's
roughness targets. A white dot in the water canvas reaches full coverage, so
its centre reaches the wet roughness target.

`Fields.h` holds the painted inputs, `Surfaces.h` their interpretation,
`Lights.h` the sources, and `painted_fields.cpp` the interface and clock.

## States

The 32-second loop has eight four-second states: painted, zero impression,
recessed type, dry, gloss, matte, erase and clip, and clear the canvas.
`kStates` sets each one's signed impression depth, wetness and dry and wet
roughness; `heldPhase` pins one. The light moves independently. The first
three states and the erase state also pulse the wetness while keeping their
painted maps. The erase state crosses the input rectangle, erases a strip and
a circle and keeps the remaining strokes; the last state clears both maps.

## Sources

Eight light sources take turns, each playing all eight states over 32 seconds,
so the whole cycle lasts 256 seconds. `heldSource` pins one from 0 to 7; a
negative value follows the clock.

| Seconds | Source | What changes |
| --- | --- | --- |
| 0–32 | Front point | A nearby source moves its highlight across the page. |
| 32–64 | Front spot | The same position, with a downward cone and no environment. |
| 64–96 | Grazing sun | Parallel rays at 12° elevation. |
| 96–128 | Softbox reflection | A broad rectangle in the environment. |
| 128–160 | Strip reflection | A narrow cool rectangle in the environment. |
| 160–192 | Window reflection | Four panes sharing an open crossbar. |
| 192–224 | Ring reflection | A warm annulus with a dark centre. |
| 224–256 | Scoped rig | A warm key and a cool fill moving independently. |

The rig is a Compose scene with two lights and a modest environment. Its key
sits inside the steel panel, which is texture-cached; its fill is a peer of the
page. Both light every material receiver in the scene, and warm and cool
reticles mark their positions.

The four shaped sources are distant environment reflections with feathered
edges: they have no distance or parallax, and roughness dims them without
widening their footprint. The spot and sun omit the environment so their direct
response stands alone. Source strengths are authored, so the sources do not
emit equal power. A sidebar inset shows each panorama unrotated, so the panes
and the ring's open centre read apart from the surface response.

## What it does not do

The panels are bump-shaded and flat, not displaced. Wetness is an appearance control, not a fluid or
absorption simulation, and it has no separate water-film normal or coating.
Directional grooves are normal detail, not an anisotropic reflection. The
positioned lights' finite range is an authored falloff.
