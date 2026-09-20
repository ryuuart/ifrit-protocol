# Colour, fill, paint and material

One page for the question every author arrives with: *there are four
things here that all mean "what colour is this" — which one do I pass?*

The short answer, before the detail:

- **A colour boils down to a `Fill`.** Not to a material. `Fill` is the
  comparable slot the reconciler stores a node's paint in, and it is
  where a colour, a gradient shader, and a reference to the ink in force
  all end up.
- **A material enters a surface paint through a recipe paint, and never
  the other way.** `material::skia::Paint::recipe` wraps a
  `material::Material` into a paint; there is no leaf of a material that
  holds a paint, and no verb anywhere that turns a colour into a
  material.
- **The single top is `SurfacePaint`**, in Compose's kernel. Everything
  else converts into it.

## The lattice

```
                    compose::SurfacePaint            the top
                            |
              +-------------+--------------+
              |                            |
   motion::Animatable<Fill>        material::skia::Paint
     - a plain Fill                  - solid, linear, radial,
     - a Transitioned<Fill>            conical, sweep
     - an Output<Fill>, live         - linearUnit, radialUnit, glowUnit
              |                      - image, buffer
              |                      - sksl, shader
       compose::Fill                 - blend
         - Kind::None                - recipe
         - Kind::Color                        |
         - Kind::Shader                       |
         - Ref::CurrentInk           material::Material
         - Ref::Var                   - a recipe, and its bytes
              |                       - bindings, slots, settings
        an SkColor4f                          |
              |                        material::Color fields
      material::Color                         |
              |                    the reasoning spaces:
    Oklab   Oklch   Lab   LinearRgb   never painted from directly
```

### One paragraph per node

**`compose::SurfacePaint`** — a two-branch variant: a
`motion::Animatable<Fill>` or a `material::skia::Paint`. It is what a
component prop asks for when it means "colour this surface, however the
caller likes", and every branch converts into it implicitly.
`SurfacePaint::apply` is the one place that decides which fill overload a
branch reaches. See [SurfacePaint](pages/types/SurfacePaint.md).

**`motion::Animatable<Fill>`** — a fill that may be a plain value, a
`motion::Transitioned<Fill>`, or a live `choreograph::Output<Fill>*`
compared by the output's IDENTITY. That identity comparison is what lets
a bound fill declare volatility without defeating the prune.

**`compose::Fill`** — nothing, a colour, a Skia shader, or a REFERENCE
the tree resolves where the mark lands. Five scalars, compared in one
line. See [Fill](pages/types/Fill.md).

**`material::skia::Paint`** — this project's paint model as one Skia
shader: solids, ramps in node px and in the unit square, images, buffers,
raw SkSL, raw shaders, blends, and recipe instances. It carries the
volatility tier — static, geometry, live — that decides what a node's
paint costs. See
[Paint](../../material/reference/pages/types/Paint.md).

**`material::Material`** — a recipe instance: the recipe, the parameter
bytes, the live bindings, the slots, the instance settings. One KIND of
paint, beside solid and gradient and image, reached by being wrapped in
`material::skia::Paint::recipe`. See
[Material](../../material/reference/pages/types/Material.md).

**`material::Color`** — four straight sRGB floats, and the one colour
class. A Skia colour converts to it implicitly, field for field. See
[Color](../../material/reference/pages/types/Color.md).

**`Oklab`, `Oklch`, `Lab`, `LinearRgb`** — the spaces a colour is
reasoned about in: interpolated, measured, lifted, fitted. Each has a
round trip to and from a colour, and nothing paints from one directly.

**`material::skia::Effect`** stands beside the lattice rather than in it.
A paint shades a shape; an effect filters a layer that is already drawn.
See [Effect](../../material/reference/pages/types/Effect.md).

## Why the containment runs this way

A material cannot be the top, and the reason is worth knowing because the
instinct behind the question — *there should be one thing I can pass
anywhere* — is right. There is one such thing; it is `SurfacePaint`.

**A reference is not a value.** `Fill::currentInk()` reads the ink in
force where the node lands, and `Fill::var(name)` reads a custom property
from the nearest ancestor that set it. A recipe instance carries bytes,
bindings and slots — there is nowhere in it to put "ask the tree".

**A flat colour would carry a program.** A material is a recipe pointer
plus bytes plus bindings plus slots plus a resolve memo, and a recipe
compiles one program per identity, target and variant. Today
`Paint::isSolid` short-circuits before any shader is built, precisely
because a solid has no coordinates and nothing to resolve.

**Equality would get worse.** `Fill` compares in five scalar
comparisons. `Material` compares by recipe identity, bytes, bindings
under SigilMotion's animatable rule, children by value and instance
settings. Every node's paint prune would move from the first to the
second.

**Motion is typed on the fill.** The animation family is
`motion::Animatable<Fill>` and its transitioned and output forms. A
material animates SCALARS inside itself, binding one float field at a
time — a different mechanism with different equality.

**A paint already is the general paint value.** A material is one leaf of
`material::skia::Paint`, beside solid, gradients, image, buffer, SkSL,
raw shader and blend. Inverting the containment would mean teaching a
material every one of those, which is a paint under another name.

## The rule for a colouring parameter

> **Every parameter that decides what colour pixels are takes the widest
> union its slot can honour. A narrower one is a documented decision with
> its reason on the line, never an omission.**

Three unions name the three roles, and in Python they are spelled:

| Role | Python union | What it accepts |
| --- | --- | --- |
| a flat colour value | `ColorLike` | `material.Color`, a CSS string, a 3- or 4-tuple or list of unit floats |
| a flat mark that may be a tree reference | `FillLike` | everything in `ColorLike`, plus `compose.Fill`, `compose.VarRef` and `None` |
| anything that can colour a surface | `SurfacePaintLike` | everything in `FillLike`, plus the transitions and outputs, plus `material.Paint` and `material.Material`, plus `compose.SurfacePaint` |

The narrowings that ARE modelled, with their reasons:

- **`Element::ink` takes a colour and not an animatable.** The ink
  inherits, so a bound ink would make the whole inheriting subtree
  volatile.
- **A kit rule takes a `Fill` and not a `SurfacePaint`.** A hairline is a
  colour; a well is a surface.
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

Two unrelated types are called `Paint`, and one module apart in Python.

| Name | What it is | Python |
| --- | --- | --- |
| Skia's paint | a style, a stroke width, a blend mode, a colour and the filter objects for ONE draw | `sigil.skia.Paint` |
| the material paint | what a surface is shaded WITH, as a comparable value that compiles to one shader | `sigil.material.Paint` |

`Element::fill` takes the material one. A raw Skia paint appears where a
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
paint model of its own; SigilMaterial owns the paint, the effect, the
colour and the ramp; SigilWeave owns the text paint, and links no
renderer's paint model, which is why it holds a material by pointer and a
Skia paint by value.

- `core/Paint.h` — `Fill`, `hexColor`, `PaintContext`, `resolveRef`,
  `frameOf`, `toFill`, `resolveFill`, `linearGradient`, `radialGradient`
- `core/SurfacePaint.h` — `SurfacePaint`
- `core/Element.h` — `fill`, `ink`, `textFill`, `textStroke`,
  `background`, `foreground`, `overlay`, `backdrop`, `effect`, `opacity`,
  `var`

## See also

- [Fill](pages/types/Fill.md), [SurfacePaint](pages/types/SurfacePaint.md)
- [Color](../../material/reference/pages/types/Color.md),
  [Material](../../material/reference/pages/types/Material.md),
  [Paint](../../material/reference/pages/types/Paint.md),
  [Effect](../../material/reference/pages/types/Effect.md),
  [Ramp](../../material/reference/pages/types/Ramp.md),
  [Palette](../../material/reference/pages/types/Palette.md)
- `README.md` beside this directory's library — the engine the values are
  read by
