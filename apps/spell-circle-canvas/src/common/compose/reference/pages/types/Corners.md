---
kind: type
library: SigilCompose
name: Corners
qualified: sigil::compose::Corners
group: Shape and edge
status: stable
---

# Corners

The four corner radii of a node's BOX, clockwise from the top left —
CSS `border-radius`. One radius rounds all four; four dress each corner
on its own. Each radius is a [`Dimension`](value:sigil::compose::Dimension). It is
the box-corner answer only: a shape that is not a box — a star, a
polygon, an outline you generated — rounds through
`shapes::rounded(shape, radius)` around its outline generator instead.

## Anatomy

`Corners::topLeft`, `Corners::topRight`, `Corners::bottomRight` and
`Corners::bottomLeft`, in that order, each a length defaulting to zero
pixels: pixels when bare; `em`, `rem`, `lh`, `ch`, a viewport unit or a
custom property against the node's font, the canvas and the properties
in force; and a `percent` of the node's own box, as CSS reads one — of
its width across and of its height down, so `50_percent` makes the box
an ellipse. `Corners::any` answers whether any of them is more than
nothing, which is the question the paint stage asks before it builds a
rounded path at all.

Zero is a square corner, not an absent one: a node that says
`.borderRadius({0})` is stating the drafting-plate square, and compares equal
to one that never named a radius.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `Corners()` | C++ | four square corners |
| `Corners(8)` | C++ | all four rounded by 8 px — implicit, so `.borderRadius({8})` is the whole call |
| `Corners(8, 8, 0, 0)` | C++ | top-left, top-right, bottom-right, bottom-left |
| `Corners(50_percent)`, `Corners(1_em)` | C++ | all four, in a length |
| `compose.Corners(8)` | Python | all four |
| `compose.Corners(8, 8, 0, 0)` | Python | the four in the same clockwise order |
| `compose.Corners("50%")`, `compose.Corners("1em")` | Python | any length the text grammar reads |
| `element.borderRadius(8)`, `element.borderRadius("50%")`, `element.borderRadius(8, 8, 0, 0)` | Python | the verb takes the radii directly, so the value need not be named |

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `Element::borderRadius` | verb | SigilCompose |

Several kit components carry a corner radius of their own as a plain
number — `Rows::swatchCorners` and the wells and plates that round
themselves — because one radius is all a component of that shape can
honour.

## Also returned by

Nothing returns one; it is an argument value.

## Description

Corners belong to the paint values rather than to the layout values,
because a radius changes what is drawn and never what is measured. A
rounded box lays out exactly as the square one does, and a child inside
it is not inset by the radius — which is what makes rounding a node free
to change late, and what makes a caller who wanted the inset say so with
padding.

The radii are structural, so a rounded static node prunes without a memo
like any other value.

## See also

- `core/Paint.h` — the header: `Corners`, `Fill`, `PaintContext`
- [PathFormat](value:sigil::compose::PathFormat) — the stroke that
  follows the rounded outline, and where its alignment puts the mark
- [Dimension](value:sigil::compose::Dimension) — the lengths each
  radius is written in
