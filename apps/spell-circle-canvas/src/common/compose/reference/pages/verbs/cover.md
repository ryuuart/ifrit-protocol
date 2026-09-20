---
kind: verb
library: SigilCompose
name: cover
qualified: sigil::compose::Element::cover
header: sigilcompose/core/verbs/Placement.h
group: Flow and placement
python: sigil.compose.Element.cover
status: stable
---

# cover

This node FILLS the box it stands in. CSS's own word: the node is taken
out of the flow and stretched to its parent's box, so a drawing, an
overlay, a scrim, a rail and a hit surface each say what they are
rather than how they are pinned.

## Syntax

```cpp
Element& cover();
```

```python
def cover(self) -> Element: ...
```

## Description

**It is `absolute()` and `inset(0)` in one sentence**, which is what it
was written as before it had a word. It also raises a placement state of
its own, and that state is what the two spellings do not share.

**A size stated after it puts the node back in the flow at that size.**
Filling the parent's box and holding a box of its own are the two things
a covering node can be, and a `width` or a `height` written after
`cover()` says which. A pin or an inset stated after it is a placement,
not a size, and stands.

A node that must fill only PART of the box states that part with
`inset()` instead. A node that must stand in the flow states
its size and says nothing here.

## See also

[`rect`](rect.md) and [`at`](at.md) for the two shorthands over the
placement longhand; `absolute`, `inset` and `centerAt` are the longhand
itself.
