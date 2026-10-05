# Reflection lobe

A 1440 × 1080 Compose material interface laid out around how roughness spreads
a reflection. It carries a painted bristle height field, Georgia glyph relief
and a layered metal ribbon. The page carries no light, so every metal paints
its colour stack flat.

Five equal curved normal ramps carry roughness **0.03, 0.20, 0.45, 0.75 and
1.00**, and nothing else differs between them: colour, metalness, normal field
and coating are identical. A lower specimen reads a continuous or stepped
roughness map per pixel, and two adjacent specimens compare coating zero and
one at the same base roughness.

## Controls and states

`Controls` holds body roughness, coating, painted depth and letter relief.
Roughness and coating clamp to 0–1, brush depth to ±20 pixels and letter depth
to ±4 pixels; nonfinite values take their defaults. `heldPhase` holds a state
from 0 to 4; a negative value follows the 20-second loop. The sidebar's
roughness bar can be dragged, and its coating and map switches change those
channels. STATE, PREVIOUS and NEXT hold or release a state; returning to
automatic states clears pointer edits.

| Seconds | State | What changes |
| --- | --- | --- |
| 0–4 | Five roughnesses | Five base-only ramps, glyph counters and crossing bristles. |
| 4–8 | Roughness zero | The body and the lower specimen at roughness zero. |
| 8–12 | Roughness one | The body and the lower specimen at roughness one. |
| 12–16 | Spatial roughness | A sharp roughness boundary in the lower specimen's map. |
| 16–20 | Clear coating | Coated hero and mapped specimen; the five ramps stay uncoated. |

The brushing adds normal detail only; roughness is isotropic. The ramps are a
curved normal field on flat elements, with no mesh displacement or cast
shadows.
