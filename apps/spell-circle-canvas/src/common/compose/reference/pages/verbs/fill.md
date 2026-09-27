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

A colour, a material as SigilMaterial builds it — a base, layers over it
and effects around it — a whole authored paint, a transition between two
of them, or a live binding whose value IS the node's colour.

<!-- example: fill_verb -->

## Syntax

```cpp
Element& fill(motion::Animatable<Fill> colour);
Element& fill(material::Material material, PaintBox box = PaintBox::Element);
Element& fill(material::Color colour);
Element& fill(Fill fill, PaintBox box = PaintBox::Element);
```

```python
def fill(self, value: MotionFillLike, box: PaintBox = ...) -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `Fill` | Nothing, a colour, a material, or a reference the tree resolves at paint — what a component property hands on. A material in it is placed over the box as the material form below places it. | [`Fill`](../types/Fill.md) |
| `motion::Animatable<Fill>` | The same, at rest, as a described motion, or as a live value somebody writes. | [`motion::Animatable`](../../VALUES.md#motion-over-a-value) |
| `material::Material` | The whole model: its base and layers paint the box, and its effects dress the node's own layer — a `Filter::shadow` beneath the fill (a hard one as an echo of the fill and the text), an inside shadow, a `Filter::stroke` and a `Filter::bevel` over it; a pass that reads pixels runs over the node and its subtree as [`filter`](filter.md) does. | `material::from(base).layer(…).effects(…)`, a gradient, a noise, an image, a program, a sketch's own look |
| `material::Color` | A solid colour, without the `Fill::color` ceremony. | `hexColor(0xRRGGBB)`, or the four channels |
| `PaintBox` | The rectangle the paint's unit square is stretched over. | [`PaintBox`](../types/PaintBox.md): `Element`, `Padding`, `Content`, `Canvas` |

In Python the parameter is `MotionFillLike`, which additionally
accepts a `"#rrggbb"` or `"#rrggbbaa"` string, a three- or four-number
sequence, `material.Color`, `material.Paint`, `material.Material`,
a custom property reference, a bound or transitioned fill, and `None`
for no fill at all.

## Description

**The box decides which rectangle the paint is stretched over.**
`PaintBox::Element` is the default and is the node's own box.
`PaintBox::Canvas` stretches the unit square over the whole canvas
instead, so several boxes show slices of one field and moving one of them
moves the slice it shows — a run of cards under one gradient, with no
per-card arithmetic. A fill does not inherit, so `PaintBox::Subtree` is
the element's own box: the element that stated the fill is the one
painting it.

**`Content` starts the paint inside the node's padding** — CSS's
content-box origin. The painted AREA never moves with it: CSS's
background-clip is not adopted here, because `ink` covers painting the
text with a paint and `inset`, `overflow` and the decoration slots cover
the rest. A border in this library is a stroke dressing the boundary
rather than a box lane, so `Padding` names the same rectangle `Element`
does until one exists.

**A box has no text units.** `Glyph`, `Cluster`, `Word`, `Line` and
`Sentence` restart an [`ink`](ink.md) on each unit of a passage; handed
to a fill, one is refused, said once on the diagnostic stream, and the
paint is stretched over the element's own box. The box is part of the
fill's statement, so a later fill replaces it with its own, and a colour
or a surface with no picture to place takes the element's.

**A static material collapses to a fill.** Handing over a
`material::Material` that reads nothing live stores the shader it
resolves to, so it caches and prunes on exactly the path a colour does.
A live material re-resolves per frame instead.

**A fill may be written as a REFERENCE the tree answers.**
`Fill::currentInk()` is the ink in force where the node is painted —
CSS's `currentColor`, the same colour the text under that node is set in
— and `Fill::var(name)` is a custom property. Both are resolved at paint
against the node's own context, so one component reads differently under
two panels with no argument passed down.

**The live form is for a colour that IS a value.** A level meter whose
hue is the level, a readout that reddens: write the fill into a live
value from the same step that computes the number.

```cpp
motion::Animatable<float> level = motion::animatable(0.0f);
motion::Animatable<Fill> bar = motion::animatable(Fill::color(cold));
engine.timer([&] { level = value; bar = Fill::color(heat(value)); });
box().scaleX(level).fill(bar);
```

What does not exist is deriving one from the other at the binding site:
a `motion::Binding` maps numbers to numbers, so compute the fill where
the number is computed.

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
