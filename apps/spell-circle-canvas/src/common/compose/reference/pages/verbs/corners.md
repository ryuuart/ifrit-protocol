---
kind: verb
library: SigilCompose
name: corners
qualified: sigil::compose::Element::corners
header: sigilcompose/core/verbs/Shape.h
group: Shape, corners and clipping
python: sigil.compose.Element.corners
status: stable
---

# corners

Round the node's corners, per corner, in pixels — CSS
`border-radius`. Square when unstated.

## Description

**It is the cheap path.** A rounded box clips with `clipRRect` and
strokes as a round rect, where a general [`shape`](shape.md) has to
build a path and work against that. Reach for a shape when the outline
is not a rounded rectangle, and for this when it is.

**A shape overrides it outright.** The two are not composed: a node
that states both is shaped, and its corner radii are unread.

**Corner radii are not a silhouette.** Text flowing around the node is
cut by its box, and a coverage boundary is read off what it drew; the
radii round the fill.

## See also

[`shape`](shape.md), [`clip`](clip.md), the
[`Corners`](../types/Corners.md) value.
