---
kind: verb
library: SigilCompose
name: at
qualified: sigil::compose::Element::at
header: sigilcompose/core/verbs/Placement.h
group: Flow and placement
status: stable
---

# at

Pin an absolute node's top-left to a parent-space POINT and leave the
node to size itself from its content.

## Description

**Exactly `left(x).top(y)`** — the half of the placement longhand that
carries no box, written through those two setters so it can neither
describe a node the longhand could not nor drift from it.

**Two lengths in any unit, or a point in hand.** `at(x, y)` takes what
`left()` and `top()` take, so `at(pct(50), pct(50))` is a percent of the
parent's box; `at(SkPoint)` is the same pin for coordinates already
measured, in pixels.

The same qualification as [`rect`](rect.md) holds: it is for a position
already known, and one that is a RELATIONSHIP belongs to flex and
`inset`.

## See also

[`rect`](rect.md), [`cover`](cover.md), `centerAt`.
