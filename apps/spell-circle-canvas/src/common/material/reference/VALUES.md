# The values

What you CREATE to pass where a colour, a paint, a material or a table
of colours is taken. SigilMaterial owns the paint vocabulary for the
whole tree: a consumer that shades something spells these names, and no
library re-exports them under its own.

Every value page answers the same three questions in the same order:
**Make one** — every spelling that produces the value, in both
languages; **Pass it to** — every slot that takes it; and **Also
returned by** — what hands one back.

## Colour

| Value | What it is | Header |
|---|---|---|
| [`Color`](pages/types/Color.md) | Four straight sRGB floats — the one colour value — with `Oklab`, `Oklch`, `Lab` and `LinearRgb` beside it as the spaces a colour is reasoned about in. | `color/Color.h` |
| [`Ramp`](pages/types/Ramp.md) | A colour ramp as one value: the stops, the space they are walked in, the easing, the direction and the domain. | `color/Ramp.h` |
| [`Palette`](pages/types/Palette.md) | An ordered table read by index, which says there is nothing between its entries. | `color/Color.h` |

## Paint

| Value | What it is | Header |
|---|---|---|
| [`Paint`](pages/types/Paint.md) | The paint model as one Skia shader: solids, ramps, images, buffers, SkSL, blends and recipe instances, in three volatility tiers. | `skia/Paint.h` |
| [`Material`](pages/types/Material.md) | A recipe instance: bytes, bindings, slots and settings, comparable by value. | `core/Material.h` |
| [`Effect`](pages/types/Effect.md) | Post-processing over a layer already rendered — a blur, a glow, a colour map, a recipe over the content. | `skia/Effect.h` |

Where these values LAND is the consumer's business, and the page that
draws the whole colouring lattice lives beside the library that owns its
top:
[Colour, fill, paint and material](../../compose/reference/COLOURING.md).

## The headers these come from

- `color/Color.h` — `Color`, `FourFloatColor`, `rgb`, `hsv`, `Oklab`,
  `Oklch`, `Lab`, `LinearRgb`, `RampStop`, `Palette`, `RampBracket`,
  `rampBracket`, `sampleRamp`, `deltaE`, `luminance`, `withAlpha`,
  `scale`, `lighten`, `mixToward`, `mixLinear`, `fitToSrgb`
- `color/Ramp.h` — `Ramp`, `RampSpace`, `HueArc`, `palette`, `ramp`
- `color/Harmony.h` — `harmony`, `rotateHue`, `Scheme`
- `color/Extract.h` — `palette`, `closestEntry`, `PaletteOptions`
- `core/Backface.h` — `Backface`
- `core/Material.h` — `Material`
- `core/Leaf.h` — `Leaf`
- `skia/Paint.h` — `Paint`, `PaintFrame`, `Stop`, `Fit`
- `skia/Effect.h` — `Effect`
- `skia/Ramp.h` — `verticalRamp`, `unitRamp`, `paletteImage`,
  `paletteLookup`
