# Ceramic glaze

Three thrown jars stand on a dark slab with a row of incised tiles in front
of them, each pair in one glaze — celadon, oxblood and amber — and the eye
should read every glaze as a layer of glass over a body: a clear coat whose
reflections of a soft key and a studio sky break along a network of crazing,
over colour that runs pale where the glaze thins on the rims and ridges and
pools deeper and darker in the throwing grooves, the carved lines, the
incisions and the roll above the foot, which is left bare, rough and dull.
The crazing lives in the coat's own normal, so it shows in what the coat
reflects as the jars turn; the pooling is a mask the profile's own curvature
writes into the base colour, and the bare foot is the same page stating no
coat and a rougher surface.

## What it shows

- **Three glazes on one form.** Celadon on grey stoneware, oxblood on
  porcelain and amber on buff clay, each on the same thrown jar and the same
  incised tile.
- **The coat and its crazing.** Every glaze is a clear coat over its colour.
  The crazing is a network of cells, each tipped a little its own way with a
  groove at every crack, in the coat's own normal. The jars turn and the key
  glides, so the coat's reflections of the key and the studio's soft boxes
  break and step from cell to cell.
- **Ink in the cracks.** The same network takes up ink in the base colour:
  faint on the celadon, almost none on the oxblood, dark on the amber, where
  a finer second network runs gold between the wide cracks.
- **Pooling.** The glaze is pale where it runs thin and deep where it
  gathers: in the throwing rings, in the pair of grooves round the shoulder,
  in the roll just above the foot, on the floor inside and in the tile's
  incisions. The oxblood shows it most, white at the rim and nearly black in
  the grooves.
- **The bare foot.** Below a wavering glaze line with a few runs reaching
  down, the clay is bare: rough, grained, and under no coat. The tiles' edges
  and the kiln stilts they lean on are the same clay.

## How it is built

- **The jar** is `geometry::mesh::revolve` over a profile drawn through a
  few points with `path::curveThrough` and spaced evenly along its length with
  `path::resample`, so the page's rows lie evenly over the jar. The throwing
  rings and the shoulder grooves are real geometry, pressed into the profile
  along its normal.
- **The glaze's depth** comes from that profile: its curvature along the
  length, how far each point faces up, and the roll above the glaze line.
  It is a `material::linearGradient` painted once into a narrow strip as
  tall as the page and repeated across it, and the glaze's middle and pooled
  colours are layers masked by that strip.
- **The crazing** is `path::voronoi` over `path::sample` sites, repeated
  across the page's width so the network closes round the jar. Each cell is
  a Compose shape filled with a `material::linearGradient` in pixels and
  stroked at its edge. It is painted once into a texture scene
  (`Scene3DContext::textureScene`) as a height and once as an ink mask. The
  height goes through `material::surface::normalFromHeight` into a second
  texture scene, which is the coat's normal.
- **The tile** is two `geometry::mesh::box` bodies: the top face wears the
  page, and the four sides wear the clay. Its incisions are Compose strokes
  painted three ways: sharp as a height for the surface's normal, softened
  as the glaze's depth, and softened again for the normal of the coat that
  fills them, blended with the crazing by `material::surface::blendNormals`.
- **The pages** (`Scene3DContext::page`) paint each surface's maps: the
  glaze as `Material::surface` with `.clearcoat` and `.clearcoatRoughness`,
  the foot as bare clay with a higher roughness and a grain normal. A page
  paints the coat's weight and roughness but carries no normal of the coat's
  own, so the sketch lays the painted coat normal into the page surface's
  `material::surface::kClearcoatNormalSlot` itself.
- **The light**: one `world::rectLight` casting shadows, high behind the
  jars and gliding across them, a `world::hemisphereLight` sky, and a
  `world::kit::environmentMap` studio of a broad soft box and a narrow strip.

## What to look at

- The amber jar's upper right, where the key's reflection is broken into
  cells and stepped at each crack.
- The pair of grooves round each jar's shoulder, darker than the body, and
  the oxblood's white rim against its darker throwing rings.
- The tiles leaning toward the eye: their incisions are filled with darker
  glaze, and the key's reflection crosses their crazing as it glides.
- The foot ring under each jar: dull bare clay against the glossy glaze, and
  the runs that reach below the glaze line.

The loop is sixteen seconds: each jar turns once, and the key glides across
the back and returns.
