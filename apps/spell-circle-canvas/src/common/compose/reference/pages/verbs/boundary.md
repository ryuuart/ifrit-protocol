---
kind: verb
library: SigilCompose
name: boundary
qualified: sigil::compose::Element::boundary
header: sigilcompose/core/verbs/Decoration.h
group: Paint
python: sigil.compose.Element.boundary
status: stable
---

# boundary

What this node's decorations dress: its own shape, the outline of its
GLYPHS on a text leaf, or the silhouette of what it DREW.

## Description

```cpp
text(u8"CHROME", heavy).boundary(Boundary::Glyphs).style(kit::y2kChrome());
image(cutOut).boundary(Boundary::Coverage).style(kit::y2kChrome());
```

**A decoration was never about a box.** It is drawn across an outline,
and this is which outline it gets — so every layer style already
written works on letters, or around a cut-out, the moment that is the
outline, with no new preset and no second code path.

**The glyph outline is the placement's own.** It follows a wrapped
line, a mixed-style run's size, a path run's curve and a vertical
column's axis, because it is read off the placed glyphs rather than
measured again. On a node that is not text it means the node's shape,
which is what every node means by default.

**The coverage outline is read off the node's rendered layer** rather
than off any description of it, which is why it is the answer for a
cut-out, a clip or a mask — and why it is a staircase at the raster's
resolution, and costs a raster and a trace whenever the node's layer is
invalidated.

## See also

[`threshold`](threshold.md), which is the tolerance a coverage outline
is cut at; [`stroke`](stroke.md), [`shape`](shape.md), `Boundary`.
