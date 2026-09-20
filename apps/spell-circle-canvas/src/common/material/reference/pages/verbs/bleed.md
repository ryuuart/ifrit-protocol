---
kind: verb
library: SigilMaterial
name: bleed
qualified: sigil::material::skia::Paint::bleed
group: Uniforms and layer properties
status: stable
---

# bleed

## Description

How far this paint's node paints beyond its own box, in px, declared
with the same word a decoration uses.

It is needed when a node's outline escapes its layout rect — a shape
silhouette larger than the box, which is legal — because the cached
picture or texture is culled to the box plus whatever reserve was
declared, and paint outside that is simply cut off.

It is read by the recording cull only: it moves no pixels itself, and
the default of zero changes nothing. It participates in equality, since
a changed reserve has to force a re-record.

## See also

- [Paint](../types/Paint.md) — the value this sits on
- [`worldSpace`](worldSpace.md) — the other property the record reads
