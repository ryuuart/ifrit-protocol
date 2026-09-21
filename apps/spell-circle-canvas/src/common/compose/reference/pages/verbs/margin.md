---
kind: verb
library: SigilCompose
name: margin
qualified: sigil::compose::Element::margin
header: sigilcompose/core/verbs/Box.h
group: Size and spacing
python: sigil.compose.Element.margin
status: stable
---

# margin

The air OUTSIDE the node's box, between its edge and its siblings. Zero
on every side when unstated.

## Description

**The same four spellings as [`padding`](padding.md).** One length is all
four sides; two are one length across and one down; four are a length
per side, clockwise from the left; an [`Edges`](../types/Edges.md) names
the sides, and a side it leaves unnamed is zero.

```cpp
box().margin(12);
box().margin(12, 6);
box().margin(1, 2, 3, 4);
box().margin({.top = 2, .left = 1});
```

The per-side verbs are `Element::marginTop`, `Element::marginRight`,
`Element::marginBottom` and `Element::marginLeft`; each writes one side
and leaves the other three.

**A margin is outside the node's box**, so it displaces the siblings on
its line and never enters the node's own measured size. `boxSizing` says
nothing about it.

**Margins do not collapse.** This is a flex tree, not a block one: two
stacked children with `marginBottom(10)` and `marginTop(10)` stand 20
apart, as CSS flex layout also has it. `gap` on the container is the way
to say one length between every pair.

## See also

[`padding`](padding.md), [`inset`](inset.md), [`Edges`](../types/Edges.md).
