---
kind: type
library: SigilCompose
name: Edges
qualified: sigil::compose::Edges
group: Size and spacing
status: stable
---

# Edges

Four lengths, one per side, each saying which side it is. It is what
`padding`, `margin` and `inset` take when the positional shorthand would
leave a reader counting commas.

```cpp
box().padding({.top = 8, .left = 12});
box().inset({.right = 0, .bottom = 0});
```

## Anatomy

`Edges::top`, `Edges::right`, `Edges::bottom` and `Edges::left`, in that
order, each a [`Dimension`](Dimension.md). The order is CSS's, and it is
also the order a designated initialiser must be written in — C++ refuses
`{.left = 1, .top = 2}` because the declaration puts `top` first. The
positional shorthands beside it run in the same order, so the two
spellings say the same thing in the same sequence.

**A side left unnamed is UNSTATED, and each verb reads that as its own
default.** For `padding` and `margin` an unstated side is zero. For
`inset` it is unpinned — `autoDimension()` — so the node's own size, or
the opposite inset, sizes it there rather than stretching it to the
parent's edge. `{.left = 12}` on a padding pads one side; the same
braces on an inset pin one edge and leave the other three free.

## See also

[`padding`](../verbs/padding.md), [`margin`](../verbs/margin.md),
[`inset`](../verbs/inset.md), [`Dimension`](Dimension.md).
