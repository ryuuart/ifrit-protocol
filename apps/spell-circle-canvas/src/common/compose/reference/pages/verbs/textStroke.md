---
kind: verb
library: SigilCompose
name: textStroke
qualified: sigil::compose::Element::textStroke
header: sigilcompose/core/verbs/TextStyle.h
group: Paint
python: sigil.compose.Element.textStroke
status: stable
example: textStroke_verb
---

# textStroke

Strokes the GLYPHS, under their fill: engraved display type, an outlined
label, a caption that has to survive over an image.

It is not [`stroke`](stroke.md), which dresses the node's BOX outline
and is a different thing entirely. This one thickens the letterforms.

<!-- example: textStroke_verb -->

## Syntax

```cpp
Element& textStroke(float width, SurfacePaint paint);
```

```python
def textStroke(self, width: float, paint: SurfacePaintLike) -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `width` | The stroke width in pixels, centred on the letterform's edge. | A number |
| `paint` | What the stroke is painted with. | [`SurfacePaint`](../types/SurfacePaint.md), including `Fill::currentInk()` |

The stroke settles to one comparable `Fill` on the node, so a static
paint collapses onto it and a live one strokes with nothing.

## Description

**The pass is beneath whatever fills the letterforms**, so the stroke
thickens the glyph outward from its edge and never covers the face of
it.

**It composes with the rest of the text surface**: with
[`textFill`](textFill.md), which paints the fill above it; with the
style's own underlays and overlays, which it joins rather than replaces;
and with the fx tracks, which carry every pass along as a glyph moves.

A caption over a photograph usually wants this and a dark outline
rather than a box behind the words: the outline follows the letterforms,
so it costs no rectangle and survives whatever is underneath.

## Examples

- `reference/examples/textStroke_verb.cpp` — one word over a busy
  ground, plain and outlined.
- `reference/examples/textStroke_verb.py` — the same picture in Python.

## See also

[`textFill`](textFill.md), [`stroke`](stroke.md) for the node's
boundary, `boundary` for dressing a node with the outline of its glyphs,
and [`text`](../elements/text.md).
