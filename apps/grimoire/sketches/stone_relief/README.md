# Stone relief

A 1440 × 1024 generated limestone control tablet beside a comparison rail. The
controls and inscriptions are invented. The subject is carved stone: shallow
shoulders, recessed cuts, porosity and worn faces.

## What is drawn

The tablet carries an inset field, a dial with 48 incised ticks, three recessed
slider tracks with raised ribbed handles, five action keys, an ochre station
cartouche and small inscriptions. Limestone albedo and microrelief are
procedural fields. A rounded height shoulder becomes a normal map, and the same
height parameters make the raised and sunken faces. Placed text contours become
recessed normal maps. The page carries no light, so every surface paints its
colour stack flat.

Relief changes no silhouettes and casts no shadows. The broad contact shadow
belongs to the slab's presentation. The lettering uses system Baskerville and
Menlo, falling back to the default face, so the installed fonts can change its
shapes.

## Controls and states

`Controls` in `stone_relief.cpp` holds `phase`, `relief`, `roughness`, `wear`,
`flow`, `pressure` and `exposure`. Set `phase` from 0 to 5 to hold a state, or
−1 to follow time.

| Seconds | Hero tablet |
|---|---|
| 0–2 | Nominal relief, roughness and wear. |
| 2–4 | Zero relief and zero microrelief. |
| 4–6 | Deepest relief, 8 layout pixels. |
| 6–8 | Roughness 0. |
| 8–10 | Roughness 1. |
| 10–12 | Green-down normals read with the matching convention. |

The sequence repeats every 12 seconds. The rail stays fixed throughout: zero,
raised and sunken shoulders; minimum and maximum roughness; and a green-up,
green-down and deliberately mismatched normal set. The footer keeps a
zero-sized node, an 8-pixel node, a clipped oversized circle and a one-pixel
recess in view.
