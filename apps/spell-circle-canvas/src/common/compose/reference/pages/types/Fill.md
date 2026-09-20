---
kind: type
library: SigilCompose
name: Fill
qualified: sigil::compose::Fill
group: Paint
status: stable
---

# Fill

What one surface is painted with, in the form the reconciler stores: no
paint at all, a flat colour, a Skia shader, or a REFERENCE the tree
resolves where the mark lands. It is the cheap end of the colouring
lattice — five scalars that compare in one line — and it is what every
richer colouring value collapses to when it can.

A colour boils down to a `Fill`. It does not boil down to a material:
`Fill` is the slot a node's paint is pruned on, and a material reaches a
node by being wrapped in a paint and carried in a
[SurfacePaint](value:sigil::compose::SurfacePaint).

## Anatomy

Two small enumerations and three numbers.

`Fill::kind` is which of three things the fill is. `Fill::Kind::None`
paints nothing and is a value, not an absence — a slot holding it is
answered, not skipped. `Fill::Kind::Color` carries `Fill::colorValue`,
four straight sRGB floats. `Fill::Kind::Shader` carries
`Fill::shaderValue`, anything Skia can shade.

`Fill::ref` is where a colour fill READS its colour from. `Fill::Ref::None`
is the ordinary case: the colour is the value in hand. `Fill::Ref::CurrentInk`
is CSS's `currentColor` — the colour the nearest `Element::ink` set, which
is also the colour text under it is set in. `Fill::Ref::Var` reads the
custom property `Fill::varId` names, as the nearest ancestor that set it
left it. `Fill::references` answers whether either reference is in force,
and `compose::resolveRef` is what turns one into the colour it names,
against the `PaintContext::ink` and `PaintContext::vars` in force at the
node. Until then a `Fill::currentInk` stands as the root's black, so a
consumer reading the colour with no context in hand draws that rather
than nothing.

A reference is the one thing this type can carry that a material
structurally cannot, and it is why the lattice's bottom is not a poorer
version of its top.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `Fill::color(colour)` | C++ | a flat `SkColor4f` |
| `Fill::shader(shader)` | C++ | any `sk_sp<SkShader>`, gradients included |
| `Fill::none()` | C++ | the value that paints nothing |
| `Fill::currentInk()` | C++ | the ink in force where the mark lands |
| `Fill::var(reference)` | C++ | the colour a custom property holds |
| `Fill::var(name)` | C++ | the same, interning the name through `compose::var` — C++ only, since Python's `Fill.var` takes the reference alone |
| `compose::hexColor(0x1f2933)` | C++ | a packed sRGB integer, constexpr, as an `SkColor4f` a `Fill::color` takes |
| `compose::linearGradient(from, to, colours)` | C++ | a two-point ramp, as a shader fill |
| `compose::radialGradient(centre, radius, colours)` | C++ | a circular ramp, as a shader fill |
| `compose::toFill(paint)` | C++ | the static collapse of a material paint — a solid or a built shader, and nothing for a paint that needs a frame |
| `"#1f2933"` | Python | a CSS colour string, implicitly |
| `(0.12, 0.16, 0.20)` | Python | a 3-tuple of unit floats, implicitly |
| `(0.12, 0.16, 0.20, 0.5)` | Python | a 4-tuple, the fourth being alpha |
| `[0.12, 0.16, 0.20]` | Python | a list of unit floats, implicitly |
| `material.Color(0.12, 0.16, 0.20)` | Python | the one colour class, implicitly |
| `material.rgb(0x1f2933)` | Python | the packed integer, which Python spells as a colour rather than as a fill |
| `compose.var("gutter")` | Python | a custom-property reference, implicitly — the colour the nearest ancestor set under that name |
| `compose.Fill.color(...)`, `compose.Fill.currentInk()` | Python | the named constructors, each under its own name |
| `compose.Fill.var(compose.var("gutter"))` | Python | the reference form, which is the only one Python's `Fill.var` takes |
| `None` | Python | `Fill::none()` |

In Python the whole of that column is the union `FillLike`, and a
parameter that takes a fill takes every row of it.

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `Element::fill` | verb | SigilCompose |
| `Element::textStroke` | verb | SigilCompose |
| `SurfacePaint` | type | SigilCompose — the implicit constructor, which is how a fill reaches every slot that takes a surface paint |
| `PathFormat::strokeFill` | field | SigilCompose — through `SurfacePaint` |
| `Line::fill`, `Line::Companion::fill` | field | SigilCompose |
| `Scrim::fill` | field | SigilCompose |
| `Sheet::rule` | field | SigilCompose |
| `lines::presets::cased`, `triple`, `arrow`, `railway`, `wavy` | function | SigilCompose |
| `lines::presets::hatch`, `crosshatch`, `radialHatch`, `concentric` | function | SigilCompose |
| `motion::Animatable` | type | SigilMotion — a fill that moves is `motion::Animatable<Fill>` |

An `Element::ink` takes a colour rather than a fill: the ink is what a
fill REFERS to, so a reference in that slot would have nothing to read.

## Also returned by

| What | Kind | Library |
| --- | --- | --- |
| `compose::resolveRef` | function | SigilCompose — a reference resolved against a paint context |
| `compose::toFill` | function | SigilCompose — a static material paint collapsed |
| `compose::resolveFill` | function | SigilCompose — the same paint for THIS frame, bound outputs sampled |
| `SurfacePaint::resolve` | member | SigilCompose |
| `compose::linearGradient`, `compose::radialGradient` | function | SigilCompose |

## Description

The reconciler keeps a node's paint as a `Fill` wherever it can, because
`Fill` compares in five scalar comparisons and a material compares by
recipe identity, bytes, bindings, children and settings. That is the
whole reason the type exists and the reason `Element::fill` routes a
static material paint through `compose::toFill` rather than holding it
whole: a flat colour on a static node must cost a prune, not a resolve.
A live or geometry-dependent paint cannot collapse, so it is kept whole
on the node and the painter resolves it per frame.

Equality is structural and includes the reference, so two nodes both
painted `Fill::currentInk()` compare equal even where they resolve to
different colours — which is what lets a recoloured ancestor recolour a
subtree without re-describing it.

## See also

- `core/Paint.h` — the header: `Fill`, `Corners`, `hexColor`,
  `PaintContext`, `resolveRef`, `frameOf`, `toFill`, `resolveFill`,
  `linearGradient`, `radialGradient`
- [SurfacePaint](value:sigil::compose::SurfacePaint) — the widest
  colouring value, which a fill converts into
- The colour chapter on the [SigilCompose](doxygen:SigilCompose) site —
  the lattice whole, and which of the three spellings of the current ink
  is which
- [Color](value:sigil::material::Color) — the one colour value, in
  SigilMaterial
