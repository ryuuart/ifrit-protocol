# Colour, fill, paint and material

One page for the question every author arrives with: *there are four
things here that all mean "what colour is this" — which one do I pass?*

The short answer, before the detail:

- **Everything a surface is painted with is a `Fill`.** It is the
  comparable slot the reconciler stores a node's paint in: nothing, a
  colour, a `material::Material`, or a reference to a colour the tree
  supplies — the ink in force, or a custom property. The references are
  the part only a cascade can mean; the rest is SigilMaterial's material.
- **A material converts to a fill implicitly**, so a component declares
  one `Fill` property and its caller writes a colour, a gradient, layers,
  an image or a program as it holds it. No verb anywhere turns a colour
  into a material.
- **A fill that moves is `motion::Animatable<Fill>`**, which the `fill`
  verb takes beside the plain value.

## The lattice

```
                motion::Animatable<Fill>            a fill that moves
                            |
                      compose::Fill                 the top
         - Kind::None                 - Kind::Paint
         - Kind::Color                        |
         - Ref::CurrentInk            material::Material
         - Ref::Var                     - a base: a colour, a gradient,
              |                           an image, noise, a program
              |                         - layers over it, each blended,
              |                           faded and masked
              |                         - effects around it
              |                                |
      material::Color                 material::Color fields
              |                                |
              |                    the reasoning spaces:
    Oklab   Oklch   Lab   LinearRgb   never painted from directly
```

### One paragraph per node

**`motion::Animatable<Fill>`** — a fill that may be a plain value, a
described motion (`motion::animate` over a `motion::Tween<Fill>`), or a
live value made by `motion::animatable` and compared by the IDENTITY of
the cell it reads. That identity comparison is what lets a live fill
declare volatility without defeating the prune.

**`compose::Fill`** — nothing, a colour, a material, or a REFERENCE the
tree resolves where the mark lands. It compares as the paint the executor
lowers its material to does, by recipe, so the same gradient described
again is an equal fill; and it holds the material and that lowering once
and shares them, so a fill costs a node a colour and a pointer. A
component declares one `Fill` property and its caller writes whichever it
holds. See [Fill](pages/types/Fill.md).

**`material::Material`** — the paint model: a base, the layers over it
and the effects around it, each layer blended, faded and masked. It
carries the volatility tier — static, geometry, live — that decides what a
node's paint costs; `Fill::needsFrame` asks it. What a renderer draws it
with is that renderer's executor, read inside this library's sources and
never in its headers. See
[Material](../../material/reference/pages/types/Material.md).

**`material::Color`** — four straight sRGB floats, and the one colour
class. A Skia colour converts to it implicitly, field for field. See
[Color](../../material/reference/pages/types/Color.md).

**`Oklab`, `Oklch`, `Lab`, `LinearRgb`** — the spaces a colour is
reasoned about in: interpolated, measured, lifted, fitted. Each has a
round trip to and from a colour, and nothing paints from one directly.

**`material::Filter`** stands beside the lattice rather than in it.
A material shades a shape; a filter works on a layer that is already
drawn. A material's own effects are filters, which `fill` places around
the node. See [Filter](../../material/reference/pages/types/Filter.md).

## Why the containment runs this way

A material cannot be the top, and the reason is worth knowing because the
instinct behind the question — *there should be one thing I can pass
anywhere* — is right. There is one such thing; it is `Fill`.

**A reference is not a value.** `Fill::currentInk()` reads the ink in
force where the node lands, and `Fill::var(name)` reads a custom property
from the nearest ancestor that set it. A material carries a base, layers
and effects — there is nowhere in it to put "ask the tree".

**A flat colour would carry a program.** A flat material is kept as the
colour it is, so a solid never reaches a shader: it has no coordinates
and nothing to resolve.

**Equality would get worse.** A colour fill compares in a handful of
scalar comparisons and never reaches a material's. A material compares
its base, every layer and every effect. Every node's paint prune would
move from the first to the second.

**Motion is typed on the fill.** The animation family is
`motion::Animatable<Fill>` and its described and live forms. A
material animates SCALARS inside itself, binding one float field at a
time — a different mechanism with different equality.

## The rule for a colouring parameter

> **Every parameter that decides what colour pixels are takes the widest
> union its slot can honour. A narrower one is a documented decision with
> its reason on the line, never an omission.**

Three unions name the three roles, and in Python they are spelled:

| Role | Python union | What it accepts |
| --- | --- | --- |
| a flat colour value | `ColorLike` | `material.Color`, a CSS string, a 3- or 4-tuple or list of unit floats |
| a fill | `FillLike` | everything in `ColorLike`, plus `compose.Fill`, `compose.VarRef`, `material.Material`, `material.Paint` as the base of a material, and `None` |
| a fill that may move | `MotionFillLike` | everything in `FillLike`, plus the transitions and outputs |

The narrowings that ARE modelled, with their reasons:

- **`Element::ink` takes a colour and not an animatable.** The ink
  inherits, so a bound ink would make the whole inheriting subtree
  volatile.
- **A component property takes a `Fill` and not an animatable.** It
  converts from a material in one step, which an animatable
  could not; a ground that moves is given to the component's element with
  `fill` directly.
- **A slot measured without a frame reads a paint that needs one as the
  ink in force.** A glyph outline, and a paired rule's rails, store one
  fill measured before any frame; a live paint, or one that reads the box
  it lands on, has no colour to give them there.
- **SigilWorld's fill takes a material and nothing else.** World shades:
  a 2D paint has no normal, no view vector and no light. Its verb shares
  a NAME with Compose's and takes the opposite set, on purpose.

## Three spellings of the current ink

CSS has one `currentColor`. This tree has three, in three libraries, and
a reader who knows one does not otherwise discover the other two.

| Spelling | Where | What it resolves to |
| --- | --- | --- |
| `Fill::currentInk()` | SigilCompose | the colour the nearest `Element::ink` set, read from `PaintContext::ink` at paint |
| `weave::Decoration::color` left transparent | SigilWeave | the resolved foreground's colour — and, for a highlight, that colour at quarter alpha, since an opaque one would hide the text |
| `weave::PaintLayer::paint` carrying a transparent colour | SigilWeave | the pass's own stroke, blur and offset, in the colour the text is set in — `PaintLayer::resolvedPaint` is the reading |

A mark that names no colour at all is already painted in the ink; these
are the spellings for a slot that DEMANDS a value.

## Which Paint is which

Two unrelated types are called `Paint`. Python spells both of them with
`skia.Paint` as the last two words, and the module in front is the whole
of the difference.

| Name | What it is | Python |
| --- | --- | --- |
| Skia's paint | a style, a stroke width, a blend mode, a colour and the filter objects for ONE draw | `sigil.skia.Paint` |
| the material paint | what the Skia executor lowers a material to — one comparable value that compiles to one shader | `sigil.material.Paint` |

`Element::fill` takes a material, and neither paint is spelled in
Compose's headers. A raw Skia paint appears where a
caller is drawing on a canvas directly — inside a paint program, a pen
program, or SigilWeave's own text passes, where `weave::PaintLayer::paint`
is deliberately the complete Skia vocabulary because a text pass wants
strokes, blurs and blenders that no paint model of ours mirrors.

`Color` has the same shape of collision and is resolved by merging rather
than by qualifying: `sigil.material.Color` is the one colour class, and a
Skia colour converts into it implicitly.

## Where each value is spelled

The vocabulary belongs to the library that owns it, and no library
re-exports another's. Compose places what a material paints and holds no
paint model of its own; SigilMaterial owns the material, the effect, the
colour and the ramp; SigilWeave owns the text paint, and links no
renderer's paint model, which is why it holds a material by pointer and a
Skia paint by value.

- `core/Paint.h` — `Fill`, `PaintContext`, `resolveRef`,
  `frameOf`, `toFill`, `resolveFill`
- `core/verbs/Paint.h` — `fill`
- `core/verbs/Cascade.h` — `ink`, `var`
- `core/PaintBox.h` — `PaintBox`
- `core/verbs/Decoration.h` — `background`, `foreground`, `overlay`
- `core/verbs/Effects.h` — `backdropFilter`, `filter`, `opacity`
- `core/verbs/TextStyle.h` — `textStroke`

## See also

- [Fill](pages/types/Fill.md), [PaintBox](pages/types/PaintBox.md)
- [Color](../../material/reference/pages/types/Color.md),
  [Material](../../material/reference/pages/types/Material.md),
  [Paint](../../material/reference/pages/types/Paint.md),
  [Filter](../../material/reference/pages/types/Filter.md),
  [Ramp](../../material/reference/pages/types/Ramp.md),
  [Palette](../../material/reference/pages/types/Palette.md)
- `README.md` beside this directory's library — the engine the values are
  read by
