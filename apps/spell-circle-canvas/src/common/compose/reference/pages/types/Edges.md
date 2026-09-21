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
braces on an inset pin one edge and leave the other three free. Auto is
not a length the air around a node can take, so on `padding` and
`margin` a side written `autoDimension()` is zero as well.

**Braces with no names are ordinary aggregate initialisation**, filling
the fields from the front: `padding({2, 4})` is top 2, right 4, and
zero below and left — not the vertical/horizontal pair the positional
`padding(2, 4)` writes. Name the sides, or drop the braces. A single
braced length, `padding({12})`, is refused outright as ambiguous between
the one-length shorthand and this type.

## See also

[`padding`](../verbs/padding.md), [`margin`](../verbs/margin.md),
[`inset`](../verbs/inset.md), [`Dimension`](Dimension.md).
