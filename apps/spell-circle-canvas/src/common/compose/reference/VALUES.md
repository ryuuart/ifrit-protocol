# The values

What a verb ACCEPTS. A signature says `Element& fill(Fill colour)`, and
the question a reader actually has is what makes a `Fill` — so this page
is the index of the values the paint path takes, each with where one
comes from and what takes it.

It covers the first slice: the paint verbs and the elements around them.
The values of the other concerns — the dimensions, the shapes, the
layout schemes, the text styles — are named in their groups on
[the verb index](VERBS.md) and get their rows here as their pages land.

A value's own page, under `pages/types/`, carries three sections that
nothing else in this reference can give: **Make one**, **Pass it to**
and **Also returned by**, each read off the whole tree rather than one
library. Until a value has a page, its row below is what this reference
claims about it. [COLOURING.md](COLOURING.md) is the chapter over the
four values that all mean "what colour is this", and is the page to
read before the four rows under *The surface*.

---

## The surface

| Value | What it is | Make one | Passed to |
|---|---|---|---|
| [`Fill`](pages/types/Fill.md) | Nothing, a colour, a shader, or a reference the tree resolves at paint. | `Fill::color`, `Fill::shader`, `Fill::none`, `Fill::currentInk`, `Fill::var`, `linearGradient`, `radialGradient`, `toFill` | `Element::fill`, `Text::textStroke`, every decoration's own paint |
| [`SurfacePaint`](pages/types/SurfacePaint.md) | A component's surface: a fill, a live fill binding, or a material. | Implicitly from a `Fill`, an animatable fill, a bound output, a transition, a paint or a recipe | `Element::fill`, `PathFormat::strokeFill`, the kit's wells and sheets |
| `material::skia::Paint` | A shader authored as a value: ramps, blends, sprites, recipes, SkSL. | `Paint::solid`, `Paint::linear`, `Paint::radial`, `Paint::sweep`, `Paint::linearUnit`, `Paint::image`, `Paint::recipe`, `Paint::blend` | `Element::fill`, `Element::ink` |
| `material::Material` | A recipe — a pattern described rather than a shader built. | SigilMaterial's own catalogue | `SurfacePaint`, and `Paint::recipe` |
| `material::Color` | The one colour class: a colour in a stated space, convertible to Skia's. | SigilMaterial's colour vocabulary | Anywhere a colour is taken, through `material::skia::toSkColor` |
| `hexColor` | Not a type: the one colour SPELLING here, `0xRRGGBB` and an alpha as a Skia colour. | — | — |

## The marks

| Value | What it is | Make one | Passed to |
|---|---|---|---|
| `Decoration` | A type-erased mark: anything answering `paint(canvas, PaintContext)`. | Implicitly from `PathFormat`, `Shadow`, `Slice`, any scheme of your own, or a bare `PaintProgram` | `Element::background`, `Element::overlay`, `Element::foreground`, `Element::stroke` |
| [`PathFormat`](pages/types/PathFormat.md) | A stroke of the outline, formatted by data: width, paint, alignment, dashes, stamps. | `stroke(width, fill)`, `stroke(width)` for the ink in force, or the struct outright | The four decoration slots |
| [`Shadow`](pages/types/Shadow.md) | A soft drop shadow behind the outline, with an optional knockout. | `shadow(colour, offset, blur)` | The same slots, `background` first among them |
| `LayerStyle` | A named bundle of decorations applied together: `under` below the fill, `over` above. | The kit's presets, or the struct outright | `Element::layerStyle` |
| `Boundary` | Which outline a node hands its decorations: `Auto`, `Outline`, `Glyphs`, `Coverage`. | The enumeration itself | `Element::decorationOutline` |
| `Spans` | Which runs of the boundary a pass claims. | `spans::range`, `spans::upTo`, `spans::corners`, `spans::edges`, `spans::every`, `spans::at`, `spans::fit`, `spans::rest`, and `|` between any of them | `Element::stroke`, `Element::background` |

## The layer

| Value | What it is | Make one | Passed to |
|---|---|---|---|
| `material::skia::Effect` | A filter over pixels: blurs, glows, colour programs, recipes. | `Effect::blur`, `Effect::glow`, `Effect::filter`, `Effect::recipe`, `Effect::directionalBlur` | `Element::filter`, `Element::backdropFilter` |
| `Cache` | How a node's paint is held: `Auto`, `Picture`, `Texture`, `Group`, `None`. | The enumeration itself | `Element::cache` |
| `PaintContext` | What a paint program is handed: the box, the outline, the clock, the ink, the font, the properties. | The composer builds it; a program reads it | Every `PaintProgram` and every decoration |
| `PaintProgram` | A drawing on a canvas that names only the parameters it reads. | Any callable taking a prefix of `(SkCanvas&, const PaintContext&)` | `custom`, and `Decoration` |

## Motion over a value

| Value | What it is | Make one | Passed to |
|---|---|---|---|
| `motion::Animatable` | A value at rest, a value in transition, or a value bound to a live output. | Implicitly from the value; `motion::animate`, `motion::bind` | `Element::fill`, `Element::opacity`, every transform lane |
| `motion::Transition` | How a change is eased: a duration, a curve, a delay. | SigilMotion's own vocabulary | `Element::transition`, the second argument of `motion::animate` |

## The custom properties

| Value | What it is | Make one | Passed to |
|---|---|---|---|
| `VarRef` | A custom property's NAME as a value, interned once, so two references to one name compare as an integer. | `compose::var`, and `varName` reads the name back | `Element::ink`, `Fill::var`, `Dimension`, `VarTable::set` |
| `VarValue` | What a property HOLDS: a colour, or a length that resolves where it is read. | Either half outright — a `material::Color` or a `Dimension` | `VarTable::set`, and `Element::var` through its two overloads |
| `VarTable` | Every property in force at a node, nearest ancestor winning, as one comparable value. | `VarTable::set` per entry, or `VarTable::overlay` over another | `Element::varDefaults`, and `PaintContext` carries it to a program |

A reference stands in for a value until the tree resolves it, so it is
written where the value would be: `Element::ink(var("accent"))`, a
`Fill::var`, or a `Dimension` given a reference instead of a number. In
Python the reference is `compose.var` and the table is the dictionary
`Element.varDefaults` takes.

## The text values

| Value | What it is | Make one | Passed to |
|---|---|---|---|
| `Utf8` | Text, spelled either way: a `char` or `char8_t` string, a `std::string`, or a value that reads itself out as text. | Implicitly at the call site | `text`, `Text::textOverflow`, every kit property that takes words |
| `weave::TextStyle` | A TOTAL style: a leaf set in one inherits nothing. | `weave::textStyle` over a partial, or the struct outright | `text(utf8, style)`, `Text::spanStyle` |
| `weave::Type` | A PARTIAL: the fields it names override, the rest inherit. | The struct, field by field | `Element::font`, `Text::spanStyle`, `weave::RichText::add` |
| `weave::RichText` | Mixed-style text as a comparable value, so a re-described identical value prunes. | `weave::rich`, then `add` per run | `text(spans)` |

## What Python spells differently

`material.Color` is the one colour class, and a colour is accepted
wherever it is written as a string — `"#rrggbb"` and `"#rrggbbaa"` — as
a three- or four-number sequence, or as a colour value. The paint and
the effect are `material.Paint` and `material.Effect`.

A fill is anything in that list plus `compose.Fill`, a custom property
reference, a transitioned or bound value, a `material.Paint` and a
`compose.SurfacePaint`; the annotation for that whole union is
`SurfacePaintLike`, and the narrower ones under it are `FillLike` and
`ColorLike`. A dimension is a number, a string in the length grammar
`compose.parseDimension` reads, a `compose.Dimension`, a weave length or
a property reference; `compose.em`, `rem`, `lh`, `ch` and `pt` spell the
units that have no literal suffix in this language, beside `pct`, `pw`
and `ph`. A decoration is a
`compose.Decoration`, a `compose.PathFormat` or a `compose.Shadow`: a
scheme of your own is C++ only.

## Where they live

- `core/Paint.h` — `Fill`, with `Fill::color`, `Fill::shader`,
  `Fill::none`, `Fill::currentInk` and `Fill::var`; `hexColor`, the one
  colour spelling here; `Corners`; `PaintContext`, and the
  `KeyState` and `PromotionPolicy` it carries; `PaintProgram`;
  `StampCache`; `resolveRef`, `toFill`, `resolveFill` and `frameOf`; and
  the gradient fills `linearGradient` and `radialGradient`.
- `core/SurfacePaint.h` — `SurfacePaint`, the surface a component takes.
- `core/Var.h` — `VarRef`, the `var` that interns one, and the `varName`
  that reads it back.
- `core/Cascade.h` — `VarValue` and the `VarTable` that holds the
  properties in force.
- `core/Shape.h` — `Shape`, `MotionPath`, `Decoration` with the
  `DecorationScheme` it is built from and the `AnimatedDecoration`,
  `BleedingDecoration`, `ReachingDecoration`, `BlendingDecoration` and
  `BorrowingDecoration` a scheme declares itself by; `LayerStyle`; and
  `Boundary`.
- `core/Stroke.h` — `Spans` and the `spans` factories, with `Across` and
  `StrandPath`.
- `core/Mask.h` — `Gate` and `Parts`, the two halves of a mask.
- `core/Layout.h` — `Dimension` with `pct`, `pw`, `ph`,
  `autoDimension` and the `parseDimension` that reads one written as
  text, the `Edges` that name the four sides around a node, `Align`,
  `Justify`, and `Cache`.
- `core/Utf8.h` — `Utf8`.
- `brush/Decorations.h` — `PathFormat` and the `stroke` that makes one,
  `Shadow` and `shadow`, and `Slice`.
