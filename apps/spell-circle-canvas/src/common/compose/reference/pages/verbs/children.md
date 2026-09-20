---
kind: verb
library: SigilCompose
name: children
qualified: sigil::compose::Element::children
header: sigilcompose/core/Element.h
group: Children
python: sigil.compose.Element.children
status: stable
---

# children

What is in the node, in order. It is written last, after every verb
that says what is done to the node itself, so a description reads from
the node outward and then down.

## Description

```cpp
column().gap(9).children({
    heading(),
    each(rows, row),
    footer(),
});
```

**A run of the block is an element or the list `each()` made from a
range**, so one block mixes the two. Braces on a description mean this
and nothing else.

**The range form is for a container whose whole content is a
collection**, where the braced block would hold one `each()` and nothing
else. The children are appended in the range's own order.

**A second call APPENDS.** Children said in two blocks are one list, in
the order the blocks were written.

**On a text leaf the children are its marks and its slot mounts**, each
anchored to the rect the passage resolved for it.

## See also

[`mark`](mark.md) for a child anchored to a selection of text, `each`
for a run made from a range, and `key` for what a child is matched by
across describes.
