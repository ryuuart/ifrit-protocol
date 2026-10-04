# Optical / Liquid

A 1440 × 1000 generated optical instrument: a measured liquid cell, overlapping
clear panes, a circular lens, brushed metal, etched scales and a smoked cover.
The checker, crosses, coloured rules and lettering beneath the glass are real
Compose content. The panes filter that destination, so their displacement reads
against the exposed calibration marks beside them. The reservoir's concave
meniscus rises at its edges, with a blue stripe on a white strip behind it.

Calibration grid, cross and ring lines keep at least one device pixel of width
at low density, while their spacing and positions stay in design pixels.

## What is drawn

The glass is an authored destination program in a Compose backdrop filter
through `material::Filter::of`. Index and thickness scale its displacement,
roughness samples a small diffuse neighbourhood, and opacity sets the coating
and its optical share. It is a planar approximation, not light transport
through the material. The liquid's absorption gradient, moving caustic bands
and reflections are authored too. The metal uses shared material layers,
normal-map shading and an inherited studio light. The high index is an
intentionally strong endpoint, not a named glass.

## States

`Controls` and `kPhases` hold the values. Six four-second states repeat over 24
seconds; the light, reflections and liquid keep moving within each state. The
fixed bottom specimens keep the endpoint comparisons visible throughout. The
opacity value drives the clear panes; the narrow smoked cover has its own
stronger coating and stays faintly visible in the exposed state.

| Seconds | State | Values |
| --- | --- | --- |
| 0–4 | Clear / working | Index 1.52, depth 20, roughness 0.08, opacity 0.34, fill 0.62 |
| 4–8 | Dry / neutral | Index 1, depth 0, roughness 0, fill 0; the calibration stays unbent |
| 8–12 | Full / dense | Index 2.4, depth 48, fill 1; no empty headspace |
| 12–16 | Frost / diffuse | Roughness 1; fine calibration detail diffuses |
| 16–20 | Opaque / coated | Opacity 1; the coating covers the destination |
| 20–24 | Nested / exposed | Opacity 0, depth 48; four overlapping clip boundaries over visible liquid |

The panes' geometry holds still within a state while the liquid beneath them
moves and is sampled live.
