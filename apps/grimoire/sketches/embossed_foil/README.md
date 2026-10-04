# Embossed foil

A 1440 × 1040 Compose study of metallized blanket surfaces and pressed metal
lettering. A fictional orbital thermal-control console sits inside a sewn
gold-coloured cover with a folded silver corner. Its thermal loop, temperature
readings, heater duty, live trace and controls form one invented instrument.

## What is drawn

Foil fills and text inks share gradient and noise layers, a masked amber
overprint over a silver base, a metallic response, a crease normal map and a
matching roughness map. The crinkle normals come from a static two-scale
triangular height field: broad planar facets, smaller creases and an irregular
silhouette, with seam runs and a folded edge. Gold and silver foil sit side by
side as a material comparison.

Pressed controls and the large orbital heading use outline relief: the node's
shape for the controls, the glyph contours for the lettering. Signed depth
gives raised or recessed shoulders, including letter counters and the annular
gauge hole, and the contour normals compose with the creases. The relief
lettering paints through a foreground decoration over transparent ink; the
title, readings and fixed normal/roughness specimens use material ink directly.

A directional key and a generated latitude-longitude studio, with boxes and a
blue planet rim, light the console; both keep moving while the material is
held. The fixed specimens carry their own lighting. Only the temperature trace
reads the material clock. There is no mesh, cloth solver, thermal simulation
or image asset; the foil's insulating layers are not modelled.

## Controls and states

`Controls` holds crinkle strength, roughness, metallic fraction, signed relief
depth, shoulder width, key elevation and intensity, environment intensity,
amber layer opacity and the mask comparison. `heldPhase` holds a state from 0
to 8; −1 advances the 36-second loop.

| Seconds | State | Condition |
| --- | --- | --- |
| 0–4 | Nominal | Gold and silver foil, raised controls and material inks |
| 4–8 | Normal zero | Flat crease normal; contour relief remains |
| 8–12 | Strong crinkle | Larger crease amplitude |
| 12–16 | Roughness zero | Sharp reflected studio |
| 16–20 | Roughness one | Diffuse reflection endpoint |
| 20–24 | Frontal key | Ninety-degree key at reduced intensity |
| 24–28 | Grazing key | Five-degree key |
| 28–32 | Recessed | Negative outline depth |
| 32–36 | Layers / mask | Perforated overprint and shaped clipping |

The fixed strip always shows normal zero and strong, roughness zero and one,
frontal and grazing keys, signed glyph relief and masked layers with holes.
