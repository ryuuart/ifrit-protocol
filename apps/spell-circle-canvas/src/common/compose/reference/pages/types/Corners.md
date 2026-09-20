---
kind: type
library: SigilCompose
name: Corners
qualified: sigil::compose::Corners
group: Shape and edge
status: stable
---

# Corners

The four corner radii of a node's BOX, clockwise from the top left. One
number rounds all four; four numbers dress each corner on its own. It is
the box-corner answer only: a shape that is not a box — a star, a
polygon, an outline you generated — rounds through
`shapes::rounded(shape, radius)` around its outline generator instead.

## Anatomy

`Corners::topLeft`, `Corners::topRight`, `Corners::bottomRight` and
`Corners::bottomLeft`, in that order, in px, each defaulting to zero.
`Corners::any` answers whether any of them is positive, which is the
question the paint stage asks before it builds a rounded path at all.

Zero is a square corner, not an absent one: a node that says
`.corners({0})` is stating the drafting-plate square, and compares equal
to one that never named a radius.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `Corners()` | C++ | four square corners |
| `Corners(8)` | C++ | all four rounded by 8 px — implicit, so `.corners({8})` is the whole call |
| `Corners(8, 8, 0, 0)` | C++ | top-left, top-right, bottom-right, bottom-left |
| `compose.Corners(8)` | Python | all four |
| `compose.Corners(8, 8, 0, 0)` | Python | the four in the same clockwise order |
| `element.corners(8)`, `element.corners(8, 8, 0, 0)` | Python | the verb takes the radii directly, so the value need not be named |

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `Element::corners` | verb | SigilCompose |

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
- [Dimension](value:sigil::compose::Dimension) — the lengths a box is
  measured in, which a radius deliberately is not
