---
kind: verb
library: SigilCompose
name: inset
qualified: sigil::compose::Element::inset
header: sigilcompose/core/verbs/Placement.h
group: Flow and placement
python: sigil.compose.Element.inset
status: stable
---

# inset

How far in from each edge of the parent's box this node's own edges
stand. Stating it takes the node OUT OF THE FLOW, as `absolute()` does.

## Description

**The same lengths as `padding`, in the same CSS order.** One is all
four edges; two are vertical then horizontal; three are top, both sides,
bottom; four run clockwise from the top; an
[`Edges`](../types/Edges.md) names them.

```cpp
box().inset(0);                            // fill the parent
box().inset(20, 10);                       // down, across
box().inset(2, 3, 4);                      // top, both sides, bottom
box().inset(2, 3, 4, 1);                   // top, right, bottom, left
box().inset({.top = 6, .left = 8});        // by name; the rest unpinned
```

**An unpinned side is not a zero side.** `autoDimension()` — and a side
an `Edges` leaves unnamed — says the node is not pinned there, so its own
width or height, or the opposite inset, sizes it. A zero pins the edge to
the parent's, which stretches the node. That is the difference between a
badge in a corner and a bar across the top.

**Pinning both opposite edges stretches the node between them**; pinning
one leaves the other free. The per-side pins `Element::left`,
`Element::top`, `Element::right` and `Element::bottom` write one edge
each and imply `absolute()` the same way.

**`cover()` is `inset(0)` and a flag.** A covering node given a size
afterwards goes back into the flow at that size; a node placed by
`inset` stays out of it.

Every length is a [`Dimension`](../types/Dimension.md), so a percent is
of the parent's box.

## See also

[`cover`](cover.md), [`at`](at.md), [`rect`](rect.md),
[`Edges`](../types/Edges.md).
