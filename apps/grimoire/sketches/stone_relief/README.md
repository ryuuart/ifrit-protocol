# Stone relief

A 1440 × 1024 generated limestone control tablet beside a comparison rail. The
controls and inscriptions are invented. The subject is carved stone: shallow
shoulders, recessed cuts, porosity, worn faces and how legibility changes when
a fixed surface is seen under a moving, oblique lamp.

## What is drawn

The tablet carries an inset field, a dial with 48 incised ticks, three recessed
slider tracks with raised ribbed handles, five action keys, an ochre station
cartouche and small inscriptions. Limestone albedo and microrelief are
procedural fields. A rounded height shoulder becomes a normal map, and the same
height parameters make the raised and sunken faces. Placed text contours become
recessed normal maps. One shared material lighting pass lights every surface.

Relief changes shading, not silhouettes or cast shadows. The broad contact
shadow belongs to the slab's presentation; the moving lamp supplies the surface
response. The lettering uses system Baskerville and Menlo, falling back to the
default face, so the installed fonts can change its shapes.

## Controls and states

`Controls` in `stone_relief.cpp` holds `phase`, `moveLight`, `lightBearing`,
`lightElevation`, `relief`, `roughness`, `wear`, `flow`, `pressure` and
`exposure`. Set `phase` from 0 to 8 to hold a state, or −1 to follow time. Set
`moveLight = false` to hold an exact light bearing.

| Seconds | Hero tablet |
|---|---|
| 0–2 | Grazing light from the upper left, slowly sweeping. |
| 2–4 | Grazing light from the upper right. |
| 4–6 | Frontal light at 90° elevation, at reduced strength. |
| 6–8 | No lighting: the albedo paints flat. |
| 8–10 | Zero relief and zero microrelief. |
| 10–12 | Deepest relief, 8 layout pixels. |
| 12–14 | Roughness 0, a polished response. |
| 14–16 | Roughness 1, a chalky response. |
| 16–18 | Green-down normals read with the matching convention. |

The sequence repeats every 18 seconds. The rail stays fixed throughout:
grazing, frontal and unlit samples; zero, raised and sunken shoulders; minimum
and maximum roughness; a matching green-up and green-down pair, which shade
alike; and a deliberately mismatched pair, whose vertical shoulder response
reverses. The footer keeps a zero-sized node, an 8-pixel node, a clipped
oversized circle and a one-pixel recess in view.
