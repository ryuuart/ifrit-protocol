---
kind: verb
library: SigilCompose
name: rect
qualified: sigil::compose::Element::rect
header: sigilcompose/core/verbs/Placement.h
group: Flow and placement
python: sigil.compose.Element.rect
status: stable
---

# rect

Place an absolute node on a parent-space RECT — the peer of `centerAt`,
for when the box is already known.

## Description

**Exactly `left(r.fLeft).top(r.fTop).width(r.width()).height(r.height())`.**
It calls those four setters, so it writes the same four layout fields,
prunes identically, and cannot drift from the longhand. Right and bottom
stay unpinned.

**A primitive for placing content whose coordinates you already have**,
typically because they were measured off a reference. When a position is
a RELATIONSHIP instead — "inside its parent", "next to that one", "as
wide as the column" — flex and `inset` express it and this does not.

```cpp
g.children({box().rect(panelBox).fill(ink)});
g.children({text(u8"…", st).at({panelBox.fLeft + 16, panelBox.fTop})});
```

**Pixels only, and no right or bottom pin.** Percentage insets,
`autoDimension()` sides and right/bottom pinning are different intents
and keep the longhand. `geometry::path::centred()` builds the rect for
the centre-and-size case.

## See also

[`at`](at.md), the half of this that carries no box;
[`cover`](cover.md); `centerAt`.
