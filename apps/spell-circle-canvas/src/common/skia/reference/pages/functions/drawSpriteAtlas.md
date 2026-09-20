---
kind: function
library: SigilSkia
name: drawSpriteAtlas
qualified: sigil::skia::draw::drawSpriteAtlas
group: The draws Graphite drops
status: stable
---

# drawSpriteAtlas

`drawAtlas` on every backend, as one `drawVertices` quad list sampling
the promoted sheet.

## Description

The blend is how each sprite hits the DESTINATION — `kPlus` is the whole
colour model of an additive particle system, and routing it through the
element's `saveLayer` instead would composite the flattened field once
rather than accumulating overlaps.

A batch whose lanes disagree draws NOTHING: a short lane is a caller
bug, and reading past it is the failure the batch exists to make
impossible.

Like `sigil::skia::draw::drawLattice` this is ALWAYS decomposed and never
the native op: raster's own `drawAtlas` lowers to the same vertices
internally, and a recorded `drawVertices` replays on Graphite where a
recorded native atlas op would vanish.

## See also

- `draw/Direct.h` — the header: `drawSpriteAtlas`, `SpriteBatch`,
  `drawLattice`, `ready`
- [SpriteBatch](value:sigil::skia::draw::SpriteBatch) — the four lanes it
  takes
