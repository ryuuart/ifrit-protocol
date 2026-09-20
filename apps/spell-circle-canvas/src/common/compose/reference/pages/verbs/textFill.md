---
kind: verb
library: SigilCompose
name: textFill
qualified: sigil::compose::Element::textFill
header: sigilcompose/core/Element.h
group: Paint
python: sigil.compose.Element.textFill
status: stable
example: textFill_verb
---

# textFill

Paints the GLYPHS with a material, mapped to text-metric space: the
material's unit square lands with x across the widest line and y from
the first line's cap top to the last line's baseline.

That mapping is what makes a chrome wordmark work at any size — author
the ramp once in the unit square and its horizon crosses the capitals
whatever the type size, with no hand-positioned gradient.

<!-- example: textFill_verb -->

## Syntax

```cpp
Element& textFill(material::skia::Paint paint);
```

```python
def textFill(self, paint: material.Paint) -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `material::skia::Paint` | A shader authored as a value: ramps, blends, sprites, recipes, SkSL. | [`material::skia::Paint`](../../VALUES.md#the-surface) — `Paint::linearUnit`, `Paint::radialUnit`, `Paint::recipe` and the rest |

## Description

**It supersedes the style's foreground paint.** Whatever colour the
leaf's type carried, the glyphs are painted with this.

**A live paint re-resolves per frame**, so a ramp bound to an output
moves under the letters without re-shaping them.

**It composes with the fx tracks.** A letter in flight is painted with
the metric material exactly as a resting one is, so a chrome wordmark
can also be a staggered entrance.

The cap top is read from the face's real metrics rather than from the
line's ascent, which is what keeps the horizon on the capitals across
two sizes and two faces.

## Examples

- `reference/examples/textFill_verb.cpp` — one unit-square ramp
  painting the same word at two sizes.
- `reference/examples/textFill_verb.py` — the same picture in Python.

## See also

[`textStroke`](textStroke.md) for the pass under it,
[`fill`](fill.md) for the node's box, [`ink`](ink.md) for the colour
that inherits, and [`text`](../elements/text.md) for the leaf itself.
