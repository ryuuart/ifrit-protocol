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

## The surface

| Value | What it is | Header |
|---|---|---|
| [`Backface`](pages/enums/Backface.md) | Which sides of a surface are drawn once something has turned one of them away. | `core/Backface.h` |

Where these values LAND is the consumer's business, and the page that
draws the whole colouring lattice lives beside the library that owns its
top:
[Colour, fill, paint and material](../../compose/reference/COLOURING.md).

## The verbs on a paint

`Paint` is this library's node: what is set on a paint after it is
built, each copying on write, has a page of its own.

| Verb | What it says | Header |
|---|---|---|
| [`uniform`](pages/verbs/uniform.md) | A named uniform, baked in or bound to a moving value. | `skia/Paint.h` |
| [`slot`](pages/verbs/slot.md) | A SECOND SOURCE for a declared `uniform shader`. | `skia/Paint.h` |
| [`amount`](pages/verbs/amount.md) | Layer strength inside a blend. | `skia/Paint.h` |
| [`fit`](pages/verbs/fit.md) | How a source with a size of its own meets the box. | `skia/Paint.h` |
| [`offset`](pages/verbs/offset.md) | The bound pan over an image or a buffer. | `skia/Paint.h` |
| [`worldSpace`](pages/verbs/worldSpace.md) | Coordinates in the composer root's frame rather than the node's. | `skia/Paint.h` |
| [`bleed`](pages/verbs/bleed.md) | How far the node paints beyond its own box. | `skia/Paint.h` |
| [`quantizeTime`](pages/verbs/quantizeTime.md) | The injected clock, stepped. | `skia/Paint.h` |

## The combinator

| Function | What it does | Header |
|---|---|---|
| [`over`](pages/functions/over.md) | One material stacked over another through a mask, as one material. | `core/Combine.h` |

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
