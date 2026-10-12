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

A vertex draw ignores the paint's antialiasing on every backend and
lights a pixel whose centre a sprite covers: a sprite narrower than a
pixel lights none, so a run of fine stamps breaks into dots, and one a
pixel or two wide lights one more or one fewer as it moves, so the run
beads. A sprite narrower than four device pixels, as the canvas stands
when the batch is drawn, carries its own edge instead — an inner quad
half a pixel inside its outline and an outer one half a pixel outside
it, its alpha falling to nothing across the pixel between — and lays
exactly its area as ink. One narrower than a pixel is spread over a pixel
and faded by the part of it it covers, so it reads its cell inside
itself rather than at the cell's rim, which for a round tip is no ink at
all. A wider sprite is drawn as it stands.

## See also

- `draw/Direct.h` — the header: `drawSpriteAtlas`, `SpriteBatch`,
  `drawLattice`, `ready`
- [SpriteBatch](value:sigil::skia::draw::SpriteBatch) — the four lanes it
  takes
