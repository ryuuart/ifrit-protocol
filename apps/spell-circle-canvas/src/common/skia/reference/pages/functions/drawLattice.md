---
kind: function
library: SigilSkia
name: drawLattice
qualified: sigil::skia::draw::drawLattice
group: The draws Graphite drops
status: stable
---

# drawLattice

`drawImageLattice` on every backend. Empty divisions stretch the whole
image, which is a plain `drawImageRect`.

## Why it exists

In this Skia, `graphite::Device` overrides `drawImageLattice` and
`drawAtlas` with empty bodies, so every such draw on a Graphite canvas
silently vanishes — as invisible nine-slice frames and instance stamps.

The forms here decompose on EVERY backend, and never call the native
ops — a picture recorded on a raster canvas must be able to replay on
Graphite, where a recorded native lattice or atlas op silently vanishes:
a lattice becomes per-cell `drawImageRect` over NinePatch alternating
bands, and an atlas becomes one `drawVertices` quad list sampling the
promoted sheet. The canvas's recorder gates only TEXTURE PROMOTION:
raster source images promote through the recorder's image provider,
which owns their reuse. A provider miss falls back to an uncached
upload.

## The bands

`sigil::skia::draw::detail::latticeEdges` computes them. Divisions split
`[0, sourceLength)` into alternating fixed and stretchable intervals
starting FIXED. Stretch bands share the leftover destination space; when
the destination is smaller than the fixed sum, fixed bands scale down
proportionally, which is Skia's own rule.

AN AXIS WITH NO DIVS IS ONE STRETCHABLE BAND, so it fills the
destination the way an image drawn to a rect does. The alternative
reading — one fixed band — would make the same lattice stretch or not
stretch depending on what the OTHER axis carries, since a lattice with
neither axis divided is a plain image draw.

`density` is SOURCE PIXELS PER DESTINATION UNIT for the fixed bands: a
frame drawn at twice the size it is used at declares 2 and its corners
land at half their pixel count, sharp on a 2x device instead of twice
the intended width. It scales the fixed bands only — the stretchable
ones absorb whatever is left either way.

## See also

- `draw/Direct.h` — the header: `drawLattice`, `drawSpriteAtlas`,
  `ready`, `SpriteBatch`
- [drawSpriteAtlas](page:SigilSkia/functions/drawSpriteAtlas) — the other
  draw Graphite drops
