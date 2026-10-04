---
kind: type
library: SigilCompose
name: Fill
qualified: sigil::compose::Fill
group: Paint
status: stable
---

# Fill

What one surface is painted with: no paint at all, a flat colour, a
`material::Material`, or a REFERENCE the tree resolves where the mark lands.
It is the top of the colouring lattice — the one value a component
declares and every verb that paints a surface takes — and the references
are the part of it only a cascade can mean.

A `material::Material` converts to a `Fill` implicitly, so a caller
writes a gradient, layers, an image or a program as it holds it. This carries
the material's base paint and layers, including authored shaders. Pass the
Material directly to `fill` or `ink` to retain its surface response and effects.
A fill that moves is `motion::Animatable<Fill>`.

## Anatomy

Two small enumerations, two numbers and a material.

`Fill::kind` is which of three things the fill is. `Fill::Kind::None`
paints nothing and is a value, not an absence — a slot holding it is
answered, not skipped. `Fill::Kind::Color` carries `Fill::colorValue`,
four straight sRGB floats. `Fill::Kind::Paint` carries a
`material::Material`, read through `Fill::material`; the fill holds it
once, beside the paint the executor lowers it to, and shares both, so a
fill costs a node a colour and a pointer however much a material grows.
`Fill::needsFrame` answers whether that material is live or reads the
box it lands on — one a slot measured without a frame cannot hold as it
stands.

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
| `Fill::color(colour)` | C++ | a flat `material::Color` |
| `Fill{material}`, or a `material::Material` where a fill is taken | C++ | the material, implicitly |
| `Fill::none()` | C++ | the value that paints nothing |
| `Fill::currentInk()` | C++ | the ink in force where the mark lands |
| `Fill::var(reference)` | C++ | the colour a custom property holds |
| `Fill::var(name)` | C++ | the same, interning the name through `compose::var` — C++ only, since Python's `Fill.var` takes the reference alone |
| `material::hexColor(0x1f2933)` | C++ | a packed sRGB integer, constexpr, as the `material::Color` a `Fill::color` takes |
| `compose::toFill(material)` | C++ | the static collapse of a material — a flat one is its colour, a static one is itself, and one that needs a frame is nothing |
| `"#1f2933"` | Python | a CSS colour string, implicitly |
| `(0.12, 0.16, 0.20)` | Python | a 3-tuple of unit floats, implicitly |
| `(0.12, 0.16, 0.20, 0.5)` | Python | a 4-tuple, the fourth being alpha |
| `[0.12, 0.16, 0.20]` | Python | a list of unit floats, implicitly |
| `material.Color(0.12, 0.16, 0.20)` | Python | the one colour class, implicitly |
| `material.rgb(0x1f2933)` | Python | the packed integer, which Python spells as a colour rather than as a fill |
| `compose.var("gutter")` | Python | a custom-property reference, implicitly — the colour the nearest ancestor set under that name |
| `compose.Fill.color(...)`, `compose.Fill.currentInk()` | Python | the named constructors, each under its own name |
| `compose.Fill.var(compose.var("gutter"))` | Python | the reference form, which is the only one Python's `Fill.var` takes |
| `material.Paint.linearGradient(...)`, a `material.Material` | Python | a paint or a material, implicitly |
| `None` | Python | `Fill::none()` |

In Python the whole of that column is the union `FillLike`, and a
parameter that takes a fill takes every row of it.

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `Element::fill` | verb | SigilCompose |
| `Element::ink` | verb | SigilCompose — a colour is the inherited ink lane, a paint the ink's paint |
| `Text::textStroke` | verb | SigilCompose |
| `PathFormat::strokeFill` | field | SigilCompose |
| `kit::Well::ground`, `kit::Board::ground`, `kit::Table::swatches` | field | SigilCompose — every component property that paints an area |
| `Line::fill`, `Line::Companion::fill` | field | SigilCompose |
| `Scrim::fill` | field | SigilCompose |
| `Sheet::rule` | field | SigilCompose |
| `lines::presets::cased`, `triple`, `arrow`, `railway`, `wavy` | function | SigilCompose |
| `lines::presets::hatch`, `crosshatch`, `radialHatch`, `concentric` | function | SigilCompose |
| `motion::Animatable` | type | SigilMotion — a fill that moves is `motion::Animatable<Fill>` |

An `Element::ink` given a reference leaves the ink where it was: the ink
is what a fill REFERS to, so a reference in that slot would have nothing
to read.

## Also returned by

| What | Kind | Library |
| --- | --- | --- |
| `compose::resolveRef` | function | SigilCompose — a reference resolved against a paint context |
| `compose::toFill` | function | SigilCompose — a static material paint collapsed |
| `compose::resolveFill` | function | SigilCompose — a paint or a fill for THIS frame, references resolved and live values sampled |

## Description

The reconciler keeps a node's paint as a `Fill`, and `Element::fill`
routes a static material paint through `compose::toFill`: a flat paint
becomes its colour, so a flat colour on a static node costs a prune, not
a resolve. A live or geometry-dependent paint is kept whole on the node
and the painter resolves it against the frame it is drawn at.

Equality is structural and includes the reference, so two nodes both
painted `Fill::currentInk()` compare equal even where they resolve to
different colours — which is what lets a recoloured ancestor recolour a
subtree without re-describing it. A paint compares by its recipe, so the
same gradient described again is an equal fill and its node prunes.

## See also

- `core/Paint.h` — the header: `Fill`, `Corners`,
  `PaintContext`, `resolveRef`, `frameOf`, `toFill`, `resolveFill`
- [Material](value:sigil::material::Material) — the material a fill
  carries, in SigilMaterial
- The colour chapter on the [SigilCompose](doxygen:SigilCompose) site —
  the lattice whole, and which of the three spellings of the current ink
  is which
- [Color](value:sigil::material::Color) — the one colour value, in
  SigilMaterial
