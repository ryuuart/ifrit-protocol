---
kind: verb
library: SigilCompose
name: mask
qualified: sigil::compose::Element::mask
header: sigilcompose/core/verbs/Mask.h
group: Shape, corners and clipping
status: stable
---

# mask

Gate what this node paints, and where. A mask is a paint-phase reveal:
animating one never relayouts, and hit-testing keeps the UNMASKED
shape.

## Description

**Reach for the one-argument form first.**

```cpp
element.mask(by::spans(spans::upTo(motion::animate(motion::from(0.f).to(1.f),
                                                   {600ms}))));
element.mask(by::edge(90.f, motion::bind(&sweep)));
element.mask(by::shape(Region::path(seal)));
```

It is `mask(parts::all(), with)`, and a gate addresses only the paint
it CAN address: an arc-length window means something to the surface and
the marks and nothing to the children, so `parts::all()` with
`by::spans()` gates the boundary tracers and leaves the children alone.

**The granular form gates SOME of what the node paints.**

```cpp
panel.overlay(hazardStripes, "hazard")
     .foreground(bevelKeyline)
     .mask(parts::named("hazard"), by::edge(0.f, &armTime));
```

**Repeated calls APPEND, and overlapping selections INTERSECT.** Both
gates must pass on the overlap. Each mask carries its own animation
slots, so masks at three different rates on one node is a picture, not
a race: the intersection is recomputed exactly, per frame.

**Union is spelled INSIDE a gate, never across masks.** Combine spans
with `|`; two masks are two conditions, and stacking them can only ever
show less.

**The one thing a mask cannot express that a span pass can:** a span
pass CLAIMS its run and joins the overlap check. That check is
deliberately read against the unmasked boundary, so an overlapping
claim is a description-level mistake reported once, never one that
blinks in and out partway through a transition.

## See also

[`overflow`](overflow.md), which is one of these written short;
[`stroke`](stroke.md) for the pass form; `Gate`, `Parts`, `Region`.
