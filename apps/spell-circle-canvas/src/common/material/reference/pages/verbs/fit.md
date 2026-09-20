---
kind: verb
library: SigilMaterial
name: fit
qualified: sigil::material::skia::Paint::fit
group: Uniforms and layer properties
status: stable
---

# fit

## Description

Stretch the source, cover the box with it, or sit it inside. It is
meaningful on `Paint::image` and `Paint::buffer` — the kinds that have a
source with a size of its own — and warned and ignored elsewhere,
matching `Paint::offset`'s guardrails.

A stated fit makes the paint GEOMETRY-DEPENDENT: it needs the box to
answer, so it resolves when its node records and re-records when layout
changes the box. Between layouts it caches like any static fill. A bound
pan still composes — the fit decides the mapping and the pan
post-translates it, exactly as it does over the recipe's own local
matrix.

Resolved with no box in reach — `Paint::asShader`, a standalone
decoration — it degrades to `Fit::Native`, the same way a world-space
paint degrades to node-local. The flag is recipe and joins
`Paint::operator==`.

`Fit` itself is the frame question, which the source's own pixel
dimensions cannot answer and a local matrix authored beside them cannot
either, because neither knows how big the box will be. `Fit::Native` is
the absence of the question: source pixels at their own size, placed by
whatever the local matrix says, which is what an atlas sprite and a
tiled texture both want. The other three are resolved against the box
when the node records, and they SUPERSEDE the local matrix — a sprite's
sub-rect and a fit are two different mappings and only one of them can
decide.

## See also

- [Paint](../types/Paint.md) — the value this sits on, and where `Fit`'s
  four members are listed
- [`offset`](offset.md) — the pan that post-translates the fit
- [`worldSpace`](worldSpace.md) — the other geometry-tier flag
