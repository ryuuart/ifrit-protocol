# Reflection lobe

A 1440 × 1080 Compose material interface about how roughness spreads a
reflection. It carries a painted bristle height field, Georgia glyph relief, a
layered metal ribbon and two scoped Point lights: the warm key lives inside the
cached hero and the cool fill is its scene peer, with matching rings marking
their positions. The calibration specimens sit in their own scenes.

Five equal curved normal ramps carry roughness **0.03, 0.20, 0.45, 0.75 and
1.00**, and nothing else differs between them: colour, metalness, normal field,
source, exposure and coating are identical. A zero-strength, zero-ambient
source removes the default ambient share, so the ramps show reflected light
alone, and the hero's moving lights leave them unchanged.

The reflected source is an analytic latitude-longitude material, a narrow strip
that stays continuous across the longitude seam. A panoramic inset shows that
same source with its front direction centred. A lower specimen reads a
continuous or stepped roughness map per pixel, and two adjacent specimens
compare coating zero and one at the same base roughness. The spread comes only
from roughness: there is no picture blur, per-specimen gain or exposure
normalisation, and the ramps are not brightness-matched.

## Controls and states

`Controls` holds body roughness, coating, painted depth, letter relief,
environment intensity and light speed. Roughness and coating clamp to 0–1,
brush depth to ±20 pixels, letter depth to ±4 pixels, environment intensity to
0–2 and light speed to ±4; nonfinite values take their defaults. `heldPhase`
holds a state from 0 to 8; a negative value follows the 36-second loop. The
sidebar's roughness bar can be dragged, and its coating and map switches change
those channels. STATE, PREVIOUS and NEXT hold or release a state while the
lights keep moving; returning to automatic states clears pointer edits.

| Seconds | State | What changes |
| --- | --- | --- |
| 0–4 | Strip reflection | Five base-only ramps, glyph counters and crossing bristles. |
| 4–8 | Roughness zero | The sharpest endpoint the lighting allows. |
| 8–12 | Roughness one | Fully rough, with broad reflection support. |
| 12–16 | Spatial roughness | A sharp roughness boundary that leaves silhouettes and type unblurred. |
| 16–20 | Clear coating | Coated hero and mapped specimen; the five ramps stay uncoated. |
| 20–24 | Seam left | The strip just to one side of the wrapped longitude. |
| 24–28 | Seam right | Its counterpart on the other side, with no dark seam. |
| 28–32 | Polar caps | Warm north and cool south caps; the ramps' normals tilt north. |
| 32–36 | Constant sky | A constant panorama. |

The brushing adds normal detail only; roughness is isotropic. The ramps are a
curved shading field on flat elements, with no mesh displacement or cast
shadows, and the strip and caps are distant environment, not area lights with
size.
