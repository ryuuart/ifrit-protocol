---
kind: verb
library: SigilCompose
name: fill
qualified: sigil::compose::Element::fill
header: sigilcompose/core/verbs/Paint.h
group: Paint
status: stable
example: fill_verb
---

# fill

Paints the node's own box. It is the ground the node stands on: under
its content, under its children, and over whatever
[`background`](background.md) put beneath it.

A colour, a gradient, a whole authored paint, a transition between two
of them, or a live binding whose value IS the node's colour.

<!-- example: fill_verb -->

## Syntax

```cpp
Element& fill(motion::Animatable<Fill> colour);
Element& fill(material::skia::Paint paint);
Element& fill(material::Color colour);
template <typename P> Element& fill(P&& surface);   // a SurfacePaint
Element& fill(material::skia::Paint paint, PaintAnchor anchor,
              BackgroundOrigin origin = BackgroundOrigin::BorderBox);
```

```python
def fill(self, value: SurfacePaintLike, anchor: PaintAnchor = ...,
         origin: BackgroundOrigin = ...) -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `Fill` | Nothing, a colour, a shader, or a reference the tree resolves at paint. | [`Fill`](../types/Fill.md) |
| `motion::Animatable<Fill>` | The same, at rest, in transition, or bound to a live output. | [`motion::Animatable`](../../VALUES.md#motion-over-a-value) |
| `material::skia::Paint` | A shader authored as a value: ramps, blends, sprites, recipes, SkSL. | [`material::skia::Paint`](../../VALUES.md#the-surface) |
| `material::Color` | A solid colour, without the `Fill::color` ceremony. | `hexColor(0xRRGGBB)`, or the four channels |
| `SurfacePaint` | A component's surface property: any of the above, or empty. | [`SurfacePaint`](../types/SurfacePaint.md) |
| `PaintAnchor` | Which box the paint's unit square maps onto. | `PaintAnchor::OwnBox`, `CanvasBox` |
| `BackgroundOrigin` | Which of the box's rectangles the paint begins at — CSS's background-origin. | `BackgroundOrigin::BorderBox`, `PaddingBox`, `ContentBox` |

In Python the parameter is `SurfacePaintLike`, which additionally
accepts a `"#rrggbb"` or `"#rrggbbaa"` string, a three- or four-number
sequence, `material.Color`, `material.Paint`, `material.Material`,
`compose.SurfacePaint`, a custom property reference, a bound or
transitioned fill, and `None` for no fill at all.

## Description

**The anchor decides which box the paint is stretched over.** `OwnBox`
is the default and is the node's own box. `CanvasBox` maps the unit
square onto the whole canvas instead, so several boxes show slices of
one field and moving one of them moves the slice it shows — a run of
cards under one gradient, with no per-card arithmetic. A fill does not
inherit, so `DeclaringBox` is `OwnBox`: the element that stated the fill
is the one painting it.

**The origin decides where inside that box the paint begins.**
`ContentBox` starts it inside the node's padding. The painted AREA never
moves with it: CSS's background-clip is not adopted here, because
`ink` covers painting the text with a paint and `inset`, `overflow` and
the decoration slots cover the rest. A border in this library is a
stroke dressing the boundary rather than a box lane, so `PaddingBox`
names the same rectangle `BorderBox` does until one exists.

**A static paint collapses to a fill.** Handing over a
`material::skia::Paint` that reads nothing live stores the shader it
resolves to, so it caches and prunes on exactly the path a colour does.
A live paint re-resolves per frame instead.

**A fill may be written as a REFERENCE the tree answers.**
`Fill::currentInk()` is the ink in force where the node is painted —
CSS's `currentColor`, the same colour the text under that node is set in
— and `Fill::var(name)` is a custom property. Both are resolved at paint
against the node's own context, so one component reads differently under
two panels with no argument passed down.

**The binding form is for a colour that IS a value.** A level meter
whose hue is the level, a readout that reddens: write the fill into a
bound output from the same step that computes the number.

```cpp
choreograph::Output<float> level;
choreograph::Output<Fill> bar;
ticker.add([&] { level = value; bar = Fill::color(ramp(value)); });
box().scaleX(motion::bind(&level)).fill(&bar);
```

What does not exist is deriving one from the other at the binding site:
the shaping chain maps numbers to numbers, so compute the fill where the
number is computed.

**Neither a tile nor a pattern is a fill.** A pattern's bake is its
identity, and one minted inside a describe has no bake in it and
re-renders its tile every frame. Hold the pattern where assets are held
and fill with what it bakes. Both overloads are deleted rather than
absent, so the error names the rule.

## Examples

- `reference/examples/fill_verb.cpp` — a colour, a paint and the ink in
  force, on three swatches.
- `reference/examples/fill_verb.py` — the same picture in Python.

## See also

[`ink`](ink.md) for the colour that INHERITS, [`background`](background.md)
for the slot beneath the fill, [`overlay`](overlay.md) for the one above
it, [`stroke`](stroke.md) for the boundary, and [`ink`](ink.md) for the
glyphs.
