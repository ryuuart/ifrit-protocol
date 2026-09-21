---
kind: verb
library: SigilCompose
name: padding
qualified: sigil::compose::Element::padding
header: sigilcompose/core/verbs/Box.h
group: Size and spacing
python: sigil.compose.Element.padding
status: stable
---

# padding

The air INSIDE the node's box, between its edge and its content. Zero on
every side when unstated.

## Description

**Four spellings, one field.** One length is all four sides. Two are one
length across and one down. Four are a length per side, clockwise from
the left. An [`Edges`](../types/Edges.md) names the sides instead, and a
side it leaves unnamed is zero.

```cpp
box().padding(12);                              // all four
box().padding(12, 6);                           // across, down
box().padding(1, 2, 3, 4);                      // left, top, right, bottom
box().padding({.top = 2, .left = 1});           // by name; the rest zero
```

**The per-side verbs write ONE side and leave the other three as they
stand**, so `padding(5).paddingLeft(40)` is five on three sides and forty
on the left. They are `Element::paddingTop`, `Element::paddingRight`,
`Element::paddingBottom` and `Element::paddingLeft`.

**Padding is inside the stated size unless `boxSizing` says otherwise.**
Under `BoxSizing::BorderBox`, which is the default, a `width(100)` box
with `padding(10)` leaves 80 for its content; under
`BoxSizing::ContentBox` the same box measures 120 across.

Every length is a [`Dimension`](../types/Dimension.md), so a bare number
is pixels, `pct()` is of the parent, and `1_em` follows the type the
padding surrounds.

## See also

[`margin`](margin.md), [`inset`](inset.md), [`Edges`](../types/Edges.md),
[`display`](display.md).
