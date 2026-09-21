---
kind: verb
library: SigilCompose
name: overflow
qualified: sigil::compose::Element::overflow
header: sigilcompose/core/verbs/Shape.h
group: Shape, corners and clipping
status: stable
---

# overflow

What becomes of paint that leaves the node's own shape — CSS's
`overflow`, as far as a tree that never scrolls has one.
`Overflow::Visible` lets it through and is what a node that says nothing
gets; `Overflow::Clip` cuts the fill, the content and the children to
the shape.

## Description

**Decorations are NOT clipped.** They dress the outline, so outer
strokes, shadows and glows keep their reach on a clipped node. What the
clip bounds is the fill, the content and the children; hit-testing
still bounds the subtree.

**`Overflow::Clip` is exactly one mask, so the two spellings are one
machine.**

```cpp
element.overflow(Overflow::Clip);
element.mask(parts::surface() | parts::content() | parts::children(),
             by::shape(Region::own()));
```

It keeps a word of its own because it is also the cheap path: a rounded
box clips with `clipRRect`, where the general shape gate has to build a
path and clip against that.

## See also

[`shape`](shape.md), [`borderRadius`](borderRadius.md), [`mask`](mask.md).
