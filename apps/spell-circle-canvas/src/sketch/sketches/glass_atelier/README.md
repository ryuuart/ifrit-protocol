# Glass atelier

A 1440 × 960 fictional optical-material editor made entirely from Compose
elements. Its working surface carries moving lettering, coloured rules,
circular figures and calibration paper; three cut panes refract that live
destination. A library rail applies the same glass to a squircle, disc,
annulus, rounded star, crescent and four-lobed contour. Material gradients and
surface lighting supply the title, readouts, controls and optical coating.

## What is drawn

The glass is `material::Filter::glass` in an element's backdrop slot, with
normals that `material::skia::bevelNormals` derives from the same outline that
clips the pane. Index and thickness follow retained motion values; the sample
reach is a fixed bound in logical pixels. A lit, translucent coating is a
separate fill over the refracted destination.

The refraction is a planar displacement: it samples the colour behind the pane,
keeps the destination's alpha and never reaches past the declared bound. There
is no light transport through a volume, dispersion, caustics, absorption or
automatic legibility adjustment. Reflections and highlights on the coating
belong to its surface lighting. A checker rail is real content beneath the
glass, so coverage, open holes and coating changes read against exposed cells.
The saturated state and the nested clip deliberately exceed ordinary interface
use, and strong displacement shows small steps along curved shoulders.

Between states the ancestors translate, the panes rotate, and the coloured
content and lighting move, all without rebuilding the tree. The compact state
changes the actual layout dimensions, and the refraction follows the new
geometry.

## Controls and states

`kControls` holds `heldPhase`, `ior`, `thickness`, `sampleRadius`, `shoulder`,
`coatingOpacity`, `roughness` and `motion`. A nonnegative held phase pins one
state; `motion = 0` stops geometric motion. Optical values are clamped to finite
ranges before they reach the filter. The loop is eight four-second states over
32 seconds, and the continuous motion repeats over the same period.

| Seconds | State | What changes |
| --- | --- | --- |
| 0–4 | Clear | Index 1.4714, depth 28 px, reach 32 px; moving lines bend at the shoulder. |
| 4–8 | Index one | The destination is undisplaced; the coating remains. |
| 8–12 | Depth zero | Zero optical thickness gives the same identity. |
| 12–16 | Reach zero | No neighbour sampling despite the stated index and depth. |
| 16–20 | Saturated | Index 2.4, depth 56 px, reach 16 px; the displacement stays bounded. |
| 20–24 | Coated overlap | Coating alpha 0.22; overlapping panes inside an extra rounded clip. |
| 24–28 | Reversed normal | Back-facing normals leave the destination intact; the coating stays lit. |
| 28–32 | Compact layout | Workbench and main pane narrow; refraction follows their geometry. |

Fixed references stay visible in every state: index one, reach zero, a
zero-width pane between brackets and a one-pixel pane. The annulus and crescent
keep their uncovered interiors.
