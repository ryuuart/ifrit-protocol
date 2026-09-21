---
kind: verb
library: SigilCompose
name: decorationOutline
qualified: sigil::compose::Element::decorationOutline
header: sigilcompose/core/verbs/Decoration.h
group: Paint
python: sigil.compose.Element.decorationOutline
status: stable
---

# decorationOutline

Which outline this node's decorations follow: its own shape, as CSS's
`box-shadow` follows the box; the outline of its GLYPHS on a text leaf,
as `text-shadow` does; or the silhouette of what it DREW, as
`filter: drop-shadow` does. Under that last one a second value says how
much paint counts as ink.

## Description

```cpp
text(u8"CHROME", heavy).decorationOutline(Boundary::Glyphs).layerStyle(kit::y2kChrome());
image(cutOut).decorationOutline(Boundary::Coverage).layerStyle(kit::y2kChrome());
image(photo).key("fig").decorationOutline(Boundary::Coverage, 0.35f);
text(body, bodyStyle).flowAround("fig", 12);
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

**The coverage is the tolerance the silhouette is cut at**, a fraction
of full opacity clamped to [0, 1] and read under `Boundary::Coverage`
alone. The default is half, the rule an unantialiased rasteriser uses —
the paint reached at least half the pixel — so the traced edge is where
the drawn edge is. It is the dial a soft edge needs: lower it and a
wash, a feathered cut-out or a glow becomes silhouette; raise it and
only the solid core does.

**Everything that asks this node for its coverage reads the same
number** — its own decorations, and any text flowing around it.

## See also

[`stroke`](stroke.md), [`shape`](shape.md),
[`flowAround`](flowAround.md), `Boundary`.
