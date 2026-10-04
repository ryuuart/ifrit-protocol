# Metallic linework

A 1440 × 1000 Compose study of silver and copper writing on a rough dark
substrate. A fictional routing instrument combines plated conductor paths,
annular vias, material lettering (`O8` in glyph relief, `Cu / Ag` in direct
material ink), a pressure ribbon and fixed cap, join and narrow-width
specimens. A generated brushed normal field stands in for a fine radial finish,
under two rotating studio boxes and a moving directional key. There is no
World scene, image asset or custom paint callback, and no model of machining,
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
shoulder, key elevation and plating-mask opacity. `heldPhase` −1 advances the
36-second loop; 0–8 holds a state. The light keeps moving in a held state.

| Seconds | State |
| --- | --- |
| 0–4 | Brushed plated conductors, raised letters and moving studio reflections |
| 4–8 | Metallic zero, with the same geometry, colour layers and light |
| 8–12 | Roughness zero |
| 12–16 | Roughness one |
| 16–20 | Six-degree raking key |
| 20–24 | Half-opacity masked plating and overlap |
| 24–28 | One source's colour sweeps the hue circle every four seconds |
| 28–32 | White source at RGB four, same intensity |
| 32–36 | Zero source RGB |

The first six states keep the moving environment and warm key. The last three
use a fixed frontal key at 90 degrees, intensity 0.35, with no ambient and no
environment, so only the source colour changes the lighting over the same
letters, lines, plating and ribbon. Black source RGB leaves the unlit captions
and background visible; RGB four is input above the display range.

The fixed strip always shows round, miter and bevel joins; butt, round and
square caps; 0, 0.5, 1, 2 and 4-pixel lines; and a 0.5–18-pixel pressure
profile. A zero width produces empty geometry, not a hairline.
