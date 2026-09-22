---
kind: verb
library: SigilCompose
name: atRest
qualified: sigil::compose::Text::atRest
header: sigilcompose/core/Text.h
group: Content
status: stable
---

# atRest

This leaf as it stands at rest, as a SECOND leaf that can stand beside
it in one tree. It is the one verb that ends a chain rather than
continuing it: it hands back a leaf of the same kind by value, because a
rest pose is something to place beside the moving copy rather than a
state of it.

## Description

**The same content, style, measure and layout**, carrying nothing that
deviates or restyles a glyph at paint time: no `fx()` tracks, no span
restyles, and no children, since a text node's children are its marks
and its slot mounts and both are already on screen once.

**A slot's reserved RUN stays.** It is content, and it holds the same
space in the copy's paragraph, which is what keeps the two copies'
letters in the same places.

**The key takes `-rest` after it**, so both are addressable and both
prune; a keyless original leaves the copy keyless. The ink is left to
the caller, which is what [`ink`](ink.md) is for.

A rest pose is what a track's per-glyph deviation is measured against,
and `kit::restGhost` draws it under the moving copy.

## See also

`fx`, [`ink`](ink.md), `kit::restGhost`, `Composer::beatsOf`.
