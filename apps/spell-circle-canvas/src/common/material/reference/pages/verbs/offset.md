---
kind: verb
library: SigilMaterial
name: offset
qualified: sigil::material::skia::Paint::offset
group: Uniforms and layer properties
status: stable
---

# offset

## Description

The resolved pair is treated as content scalars, so the node lifts to
content volatility while the pan is moving and is released once the
values provably hold still, letting ancestors cache again; the per-draw
scan re-declares it on the frame an externally-driven pan resumes.

It is meaningful on `Paint::image` and `Paint::buffer` paints only — the
kinds whose recipe carries a local matrix for the pan to translate. On
any other kind it is warned and IGNORED, matching `Paint::uniform`'s
guardrails.

It composes with the recipe's static matrix rather than replacing it:
the bound values post-translate, so a static phase origin and a bound
pan add. Either axis may be left empty to pan the other alone, and an
axis may be a shaped binding chain — a wrapped ramp is a conveyor, a
ping-pong is a rocking weave — so the arithmetic sits beside the pan it
drives.

The BINDING is recipe and participates in `Paint::operator==` as an
animatable does: a live axis by its output's identity, like a bound
fill; the values it resolves to belong to the system and never enter the
prune comparison.

## Is the pan the whole of what moves?

`Paint::boundOffsetOnly` is the question a retained runtime asks about a
pan, and the only one. A paint answering yes is two floats a consumer
can read back by pointer dereference and compare, so it rides a scalar
lane and prunes by value like a placed tile; one answering no changes in
ways no memo can read and is opaque. It is a PARTITION of the animated
paints, not a hint.

Yes covers the constant pan as well as the moving one — a placed tile is
a pan whose value happens not to change — because the consumer reads the
same two floats either way. What it excludes is a live uniform, `uTime`,
`uContentScale` and any animated slot or blend layer, including a NESTED
pan, which the layer-local channel cannot reach.

`Paint::boundOffsetValue` is the pan as of NOW: what each axis's
animatable reads as, zero for an axis that carries none. Every consumer
reads the pan through that one body, so the volatility release, the
per-draw scan and the paint itself cannot disagree about what the
current value is.

## See also

- [Paint](../types/Paint.md) — the value this sits on
- [`fit`](fit.md) — what the pan post-translates over
- [`worldSpace`](worldSpace.md) — the other layout-derived mapping
