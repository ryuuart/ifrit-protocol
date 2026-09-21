---
kind: verb
library: SigilCompose
name: contentFlowAround
qualified: sigil::compose::Element::contentFlowAround
header: sigilcompose/core/verbs/TextStyle.h
group: The text leaf
python: sigil.compose.Element.contentFlowAround
status: stable
---

# contentFlowAround

Flow this paragraph around the keyed node, with `margin` px of
standoff.

## Description

**A target that declares a SILHOUETTE is subtracted by that outline** —
a `shape()`, or a routed connector or rail — concavities and holes
included, so text runs into the notch of a star and through the ring of
an annulus.

**A target that declares none is subtracted by its BOX**, which is the
whole of what it occupies. Corner radii round the fill rather than the
outline and do not count as a silhouette; a coverage
[`decorationOutline`](decorationOutline.md) does, and that verb's coverage is the
tolerance it is cut at.

**The margin is the same standoff from whichever edge is being
subtracted.**

**Resolved as a bounded second layout pass**, so a target that moves
re-cuts the lines under it. A reference to self or a descendant is
ignored — the cycle guard. Call repeatedly to weave around several
elements.

## See also

[`decorationOutline`](decorationOutline.md), `key`,
`Composer::bounds`.
