# Luminous layers

A 1440 × 1080 Compose study of emissive alloy lettering, a pressure ribbon,
painted bristles and a refracting glass cover. The glyph outlines keep the 0
and 8 counters. The ribbon's width runs from 0.5 to 34 pixels. White brush
deposits supply both coverage and height; colour, emission and normal response
are applied to that field afterwards. The instrument, words, colours and
geometry are invented.

## The working image

The study paints into a half-float premultiplied working surface, and its
status line reads `F16 / RASTER FALLBACK` when the backend cannot provide one.
A final pass reads the finished image, unpremultiplies covered pixels, applies
the display exposure, then either clips or applies the curve `x / (1 + x)` per
component, and restores the original alpha. Labels and controls are drawn after
that pass. Optional bloom acts on this display image, not on the raw float
values.

The values are encoded working RGB; nothing here is a scene-linear space,
calibrated radiance or display luminance. The normal maps describe planar
relief. Colour layers are overprints, not separate films, and the glass cover
is a 2D backdrop refraction. Emission does not light its neighbours: two moving
Point sources and a modest environment supply the surface lighting, so at the
default exposure the zero-emission state is dark and its relief subtle.

## Controls and states

Drag the gain, opacity and exposure sliders. **SHOULDER / CLIP**, **DISPLAY
BLOOM** and **DEPTH** switch their effects. **STATE** alternates AUTO and HELD;
**PREVIOUS / NEXT** select a held state. Returning to AUTO clears the gain and
opacity overrides. The lights and the RGB sweep keep moving in a held state.

| Seconds | State |
| --- | --- |
| 0–4 | Warm and cyan luminous alloy |
| 4–8 | Zero emission, with surface lighting retained |
| 8–12 | Live RGB colour sweep |
| 12–16 | White emission at gain sixteen |
| 16–20 | Quarter-opacity marks and an overlap comparison |
| 20–24 | Crossing deposits and reversed layer order |
| 24–28 | Refracting glass and clipped display bloom |
| 28–32 | Zero-opacity marks |

## Fixed specimens

The calibration rail isolates white emission in its own scene, under a
zero-strength, zero-ambient source. Its columns are gains 0, 1, 4 and 16; its
rows are alphas 0, 0.01, 0.25, 0.5 and 1, so each cell's centre holds
premultiplied RGB `gain × alpha` and that alpha. Narrow rotated lines and
quarter-opacity O8 glyphs show antialiased coverage. The rail gets the same
display pass, without bloom.

The lower-left specimens hold the same half-opacity copper O8, pressure ribbon
and painted bristles at emission gain four in an 8-bit, a half-float and a
float texture. A white square in each isolates that gain at half opacity,
ideally `(2, 2, 2, 0.5)` premultiplied: the 8-bit texture clips it, while both
float textures keep the values above one until the shared display pass. These
three stay fixed while the hero's controls and lights change.
