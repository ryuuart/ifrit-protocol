---
kind: verb
library: SigilCompose
name: fill
qualified: sigil::compose::Element::fill
header: sigilcompose/core/Element.h
group: Paint
python: sigil.compose.Element.fill
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
Element& fill(SkColor4f colour);
template <typename P> Element& fill(P&& surface);   // a SurfacePaint
```

```python
def fill(self, value: PaintLike) -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `Fill` | Nothing, a colour, a shader, or a reference the tree resolves at paint. | [`Fill`](../types/Fill.md) |
| `motion::Animatable<Fill>` | The same, at rest, in transition, or bound to a live output. | [`motion::Animatable`](../../VALUES.md#motion-over-a-value) |
| `material::skia::Paint` | A shader authored as a value: ramps, blends, sprites, recipes, SkSL. | [`material::skia::Paint`](../../VALUES.md#the-surface) |
| `SkColor4f` | A solid colour, without the `Fill::color` ceremony. | `hexColor(0xRRGGBB)`, or the four channels |
| `SurfacePaint` | A component's surface property: any of the above, or empty. | [`SurfacePaint`](../types/SurfacePaint.md) |

In Python the parameter is `PaintLike`, which additionally accepts a
`"#rrggbb"` or `"#rrggbbaa"` string, a three- or four-number sequence,
`material.Color`, `material.skia.Paint`, `compose.SurfacePaint`, a custom
property reference, and `None` for no fill at all.

## Description

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
it, [`stroke`](stroke.md) for the boundary, and
[`textFill`](textFill.md) for the glyphs.
