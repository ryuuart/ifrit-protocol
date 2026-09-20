---
kind: type
library: SigilSkia
name: SpriteBatch
qualified: sigil::skia::draw::SpriteBatch
group: The draws Graphite drops
status: stable
---

# SpriteBatch

The sprites one atlas draw lays down, as one value.

Four lanes that must agree on their length, held together so the
agreement is the value's own business rather than four arguments and a
count the caller has to keep in step. `transforms` and
`sourceRectangles` are the draw — where each sprite lands and which cell
of the sheet it takes; the other two are optional lanes, and an EMPTY one
means the whole batch is untinted or uniformly scaled, which is the
common case and costs nothing to say.

## Anatomy

| Lane | What it holds |
| --- | --- |
| `sigil::skia::draw::SpriteBatch::transforms` | where each sprite lands: rotation, uniform scale and translation |
| `sigil::skia::draw::SpriteBatch::sourceRectangles` | which cell of the sheet each takes, in sheet pixels |
| `sigil::skia::draw::SpriteBatch::colors` | per-sprite tint, modulated onto the sheet; empty is untinted |
| `sigil::skia::draw::SpriteBatch::sizes` | per-sprite non-uniform scale; empty is uniform |

`sizes` is a per-sprite (x, y) scale MULTIPLIER on top of the transform's
uniform scale — the lane SkRSXform cannot carry, because it holds
(scos, ssin) and one scale by construction. A streaked particle is a
quad half its velocity long by `size` wide, and its aspect swings
across its life, which is what the lane is for: without it every such
study hand-builds the vertex buffer the atlas draw already builds
internally.

## Asking it

`sigil::skia::draw::SpriteBatch::size` is how many sprites the batch
draws, which is how many transforms it carries, and
`sigil::skia::draw::SpriteBatch::empty` whether it draws nothing.

`sigil::skia::draw::SpriteBatch::consistent` asks whether the lanes
agree. A required lane shorter than `transforms`, or a stated optional
lane shorter than it, is the one thing four parallel pointers could not
say — and the draw refuses rather than reading past the end of the short
one.

`sigil::skia::draw::SpriteBatch::slice` takes n sprites from an offset,
every stated lane taken along.

## See also

- `draw/Direct.h` — the header: `SpriteBatch`, `drawSpriteAtlas`,
  `drawLattice`, `ready`
- [drawSpriteAtlas](page:SigilSkia/functions/drawSpriteAtlas) — the draw
  a batch is handed to
