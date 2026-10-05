# Metallic linework

A 1440 × 1000 Compose study of silver and copper writing on a rough dark
substrate. A fictional routing instrument combines plated conductor paths,
annular vias, material lettering (`O8` in glyph relief, `Cu / Ag` in direct
material ink), a pressure ribbon and fixed cap, join and narrow-width
specimens. A generated brushed normal field stands in for a fine radial finish.
The page carries no light: every material paints its colour stack flat. There
is no World scene, image asset or custom paint callback, and no model of machining,
measured reflectance or conduction.

Each conductor is closed coverage: the stroke's cap and join silhouettes become
a Compose shape. The pressure ribbon uses the Ribbon band's geometry. Outline
relief combines the contour shoulder with the brushed normal, and the glyph
relief keeps the O and 8 counters open. Each material is a base, a grain and a
masked plating colour layer under one shared surface response; the layers are
colour overprints, not separately shaded films. The substrate, conductors and
lettering are separate Compose layers, overlapped inside a clipped frame.

## Controls and states

`Controls` holds metallic fraction, roughness, normal strength, depth,
shoulder and plating-mask opacity. `heldPhase` −1 advances the 20-second loop;
0–4 holds a state.

| Seconds | State |
| --- | --- |
| 0–4 | Plated conductors and material letters |
| 4–8 | Metallic zero, with the same geometry and colour layers |
| 8–12 | Roughness zero |
| 12–16 | Roughness one |
| 16–20 | Half-opacity masked plating and overlap |

The fixed strip always shows round, miter and bevel joins; butt, round and
square caps; 0, 0.5, 1, 2 and 4-pixel lines; and a 0.5–18-pixel pressure
profile. A zero width produces empty geometry, not a hairline.
