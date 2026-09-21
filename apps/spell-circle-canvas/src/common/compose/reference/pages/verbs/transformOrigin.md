---
kind: verb
library: SigilCompose
name: transformOrigin
qualified: sigil::compose::Element::transformOrigin
header: sigilcompose/core/verbs/Transform.h
group: Transform
status: stable
---

# transformOrigin

The pivot every rotation, scale and skew turns about, and the point a
travelling node puts on its curve. CSS `transform-origin`: two lengths
in the node's own box and, optionally, a depth. The centre of the box,
in its plane, when unstated.

## Description

**A percentage is of the node's own box; every other length is
node-local pixels.** `pct(0)` is the left or the top edge and `pct(100)`
the right or the bottom, so a bar that grows rightward pins its left
edge with `transformOrigin(pct(0), pct(50))`. A pivot that is not a
fraction of this node's box is written in pixels: a window zoomed about
its own centre from inside a full-canvas overlay is
`transformOrigin(Dimension(centreX), Dimension(centreY))`. An em, a
custom property and a canvas percentage resolve against the node as they
do for every other `Dimension`, and the two axes need not share a unit.
The pivot may stand outside the box.

**A bare number is refused, and the refusal is a compile error.** Every
other verb that takes a `Dimension` reads a bare number as pixels, while
`0.5` here reads as readily as half the box: a pivot half a pixel from
the corner draws almost exactly what no pivot at all draws, so the
mistake would be silent. CSS makes the same refusal of a length without
a unit. The Python binding refuses a number with a `TypeError`, and
takes `'50%'` as it does everywhere.

**The third length is the pivot's distance in front of the plane**,
positive toward the viewer, and matters to the depth lanes alone: a face
that turns about a point behind itself is a cube's face. A depth has no
box to be a percentage of, so a percentage there is dropped with a
warning and the pivot stays in the plane.

**Paint-only.** The pivot moves no layout box; it changes where the
transform lanes turn, and at rest, with every lane at its identity, it
changes nothing at all.

`perspectiveOrigin` reads its two lengths the same way, against the box
of the node that declares the `perspective`.

## See also

`rotate`, `scale`, [`scaleX`](scaleX.md), `skewX`,
[`travel`](travel.md), `perspectiveOrigin`,
[`preserve3d`](preserve3d.md), and the
[`Dimension`](../types/Dimension.md) a length is written as.
