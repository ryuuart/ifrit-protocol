---
kind: element
library: SigilCompose
name: point
qualified: sigil::compose::point
header: sigilcompose/core/Factories.h
group: Containers
status: stable
common_verbs: [key, attribute, left, top, centerAt]
---

# point

A node with no extent: a place in its parent's box that carries a key
and facts and draws nothing. It is what a skeleton is made of — a port
on a card, a station on a map, an hour on a dial — placed by an
arranging operator or by hand, and built on by an adding operator.

## Syntax

```cpp
Element point();
```

## Parameters

None.

## Description

A point is out of the flow, so it takes no room beside its siblings, and
it is placed exactly as any absolute node is: with the placement
longhand, `point().key("out").left(pct(100)).top(pct(50))` for a port on
a card's right edge, or with a centre pin. It is zero by zero, so a
centre pin lands it on the point it names, and it is out of hit testing,
because it has no box to be hit in — the node under it answers.

What it carries is what makes it useful: a `key` another node routes to
or an operator finds it by, and `attribute` facts an operator reads. A
dial is twelve points each stating its hour under `layouts::Radial` told
the lane; a map's stations are points a rail threads.

## See also

[`box`](box.md) for a node with a box, `attribute` for the facts a point
carries, and `operators` for what places it and builds on it.
