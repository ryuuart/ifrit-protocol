---
kind: function
library: SigilSkia
name: ready
qualified: sigil::skia::draw::ready
group: The draws Graphite drops
status: stable
---

# ready

An image ready for Graphite: the cached or freshly promoted texture
(unchanged on raster canvases, and on images already on a texture). The
recorder's image provider owns reuse. When it cannot supply a texture, an
uncached upload is attempted; a failed upload returns the image it was
given.

## Description

The required properties are what the draw needs of the texture — a mip
chain, above all. A promotion that dropped the request would sample a
minified sheet from its top level alone, which is where a raster canvas
and a Graphite one stop agreeing.

A REQUIREMENT IS NEVER WORTH THE DRAW. A format that cannot carry a
generated chain promotes without one rather than not at all: a draw
left holding a raster image on a Graphite canvas is dropped for want
of a texture, so insisting would trade a sheet filtered from its top
level for no sheet at all.

## See also

- `draw/Direct.h` — the header: `ready`, `drawLattice`,
  `drawSpriteAtlas`, `SpriteBatch`
- [drawLattice](page:SigilSkia/functions/drawLattice) — a caller of it
- [drawSpriteAtlas](page:SigilSkia/functions/drawSpriteAtlas) — the other
