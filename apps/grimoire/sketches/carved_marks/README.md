# Carved marks

One letter die, `OB8`, worked into four real bodies lying on a slate table:
cut into a block of limestone, raised and chased into a bronze plate,
impressed into a glazed porcelain tile whose impression is gilt, and pierced
clean through a brass plate whose under-stroke is sunk and filled with
enamel. A single low
key light rakes across the table and travels from one side to the other, so
the marks are read from their shadows: the walls of the stone's cuts throw
shade onto the cut floors and the worn shoulders roll into them, the pores
and tool passes in the stone catch the grazing light, the bronze letters
stand off their plate and the chasing round them darkens and flashes as the
light turns, and on the porcelain one held height changes the glaze into
gold — its colour, its metalness and its roughness — so the impression reads
as a continuous surface that is ceramic on the top and metal in the hollow.

## What it shows

- **Limestone, cut.** A block of warm cream stone with the die cut through
  its top as real incisions: vertical walls that throw shade across the cut
  floors, floors chiselled into a V and darkened with dust, and soft worn
  shoulders that roll down into every cut. The face is bedded with faint
  strata, mottled and pored, worn into a slow swell; the pressure ribbon and
  its fibre passes are pressed shallowly into it under the letters.
- **Bronze, raised and chased.** A bronze plate clouded with green-brown
  patina, its field dimpled by the hammer, with the die standing off it as
  polished solids. A chased groove follows every letter's edge and the
  ribbon is chased into the field; patina gathers in the chasing, which is
  rougher and less metallic than the field.
- **Porcelain, impressed and gilt.** A glazed tile with the die, a border
  rule and the ribbon pressed into it. One depth map drives everything: the
  colour turns from glaze to gold, metalness from none to full, roughness
  from glaze to burnished gold, and the glaze's clear coat lifts away, so the
  hollow is metal and the top is ceramic on one continuous surface.
- **Brass, pierced and enamelled.** A bright brass plate with the die cut
  through its whole thickness as a stencil: three bars left standing across
  the letters hold every counter to the plate, so the slate and the shadow
  of each letter show through the holes. The pressure ribbon is sunk into
  the face as a recess with real walls and dark green enamel laid in its
  floor.
- **One key that travels.** A low warm light starts at the left in front of
  the table, swings round behind the bodies to the right and returns over a
  sixteen-second loop, rising a little as it passes behind. The room's lamps
  turn with it, so the bronze's reflections travel too. The stone's shadow
  reaches across the table to the other bodies at the lowest angles.

## How it is built

- `sketch::Page` — four pages, one per painted face and one for the table. Each is a
  single box filled with a material whose surface Compose paints as maps;
  World's lights, shadows and environment shade them.
- `compose::TextureScene` — one depth map per face, white deepest, read back
  as `material::image`. It feeds `material::surface::normalFromHeight`,
  blended over noise normals with `material::surface::blendNormals`, and is
  the mask that layers grime, patina or gold over the base colour.
- `compose::brush::ribbon` — the pressure ribbon and the fibre passes, swept
  along held outlines into each depth map; `material::Filter::blur` softens
  the shoulders.
- `compose::relief` — the chiselled V on the stone's cut floors, painted
  inside the letters on the same page the skin wears.
- `weave::ParagraphLayout::glyphOutline` — the die's contours, placed once
  per face; the same outline cuts the stone, raises the bronze and is drawn
  into every depth map, so geometry and maps agree.
- `geometry::mesh::extrude` — the stone's body and the skin the letters are
  cut through, the bronze plate and its raised letters, and the porcelain
  tile; each face wears its page and its walls a plain surface. The brass
  wears plain metal and enamel surfaces with no page: its marks are all
  geometry.
- `geometry::path::Outline::subtracted` — the stencil: the letters less a
  bar across each row of counters, cut from the brass plate;
  `geometry::path::band` turns the under-stroke's pressure line and width
  into the outline of the enamelled recess.
  `geometry::shapes::rounded` rounds the slabs' corners.
- `world::directionalLight` with `castShadow` and three cascades is the key;
  `world::hemisphereLight` a dim fill; `world::kit::environmentMap` the room,
  turned with the key through `EnvironmentOptions::rotation`.

## What to look at

- The stone's cut walls: one side of every letter in shade, the opposite
  floor lit, and the side that is shaded changing as the key passes.
- The worn shoulders at the cut edges and the pores and strata on the face,
  which show only while the light grazes them.
- The bronze letters' shadows on the plate and the chased grooves round
  them, which flash as the reflected lamps cross.
- The porcelain impression: gold in the hollow under the clear glaze on the
  top, with no seam between the two.
- The stone's long shadow crossing the porcelain tile at the start of the
  loop.
- The brass stencil: the slate seen through every letter, the bars across
  the counters, and the letters' own shadows falling through the holes onto
  the table as the key moves.
