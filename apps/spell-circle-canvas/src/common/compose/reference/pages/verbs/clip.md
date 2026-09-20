---
kind: verb
library: SigilCompose
name: clip
qualified: sigil::compose::Element::clip
header: sigilcompose/core/verbs/Shape.h
group: Shape, corners and clipping
python: sigil.compose.Element.clip
status: stable
---

# clip

Clip the fill, the content and the children to the node's own shape.

## Description

**Decorations are NOT clipped.** They dress the outline, so outer
strokes, shadows and glows keep their reach on a clipped node. What the
clip bounds is the fill, the content and the children; hit-testing
still bounds the subtree.

**It is exactly one mask, so the two spellings are one machine.**

```cpp
element.clip();
element.mask(parts::surface() | parts::content() | parts::children(),
             by::shape(Region::own()));
```

It keeps a word of its own because it is also the cheap path: a rounded
box clips with `clipRRect`, where the general shape gate has to build a
path and clip against that.

## See also

[`shape`](shape.md), [`borderRadius`](borderRadius.md), [`mask`](mask.md).
