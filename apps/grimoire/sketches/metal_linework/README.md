# Metal linework

Plated conductor traces lie on a dark, rough glass-weave substrate, and
the eye should see each trace as brushed metal: its highlight is
stretched by a brushing that runs along the trace, and it slides as the
key light swings, turning round every fillet of a meander and round a
spiral coil as a spun disc's does, because the brushing follows each
trace's own direction. Copper and silver runs stand a little proud of
the board with soft shoulders, annular vias stand on their pads as real
plated rings that throw short shadows, solder beads at the connector
pads catch a bright point each, and one silver line carries a pulse of
light that runs its length and blooms. The effects that make it are
material keylines whose relief and brushing are cut from the band each
trace covers, a page whose height map becomes the board's and the pads'
normal, roughness that parts the polished plating from the rough
substrate, revolved rings and superellipsoid beads under a swinging key
and a soft panel the plating mirrors, and an emissive stamp chain riding
one spine with a bloom pass.

## What it shows

A small board seen from the front and above: a four-line silver bus that
runs from a row of connector pads, drops through a forty-five degree bend
and ends in a row of vias; a broad copper power run with its own bends; a
copper meander whose U-turns show the brushing turning with the trace; a
copper spiral coil wound out from a via; a few lone vias; and two lines of silkscreen lettering. The key light rides
a slow arc across the front of the board and back, and a warm panel hangs
behind it where the plating mirrors it. The pulse travels the longest bus
line once a loop.

## How it is built

- **The spines** are corner lists filleted into dense polylines, written
  as SVG for Compose and lifted to 3D for the pulse, so every layer reads
  the same routes.
- **The traces** are material keylines along the spines,
  `stroke(plating, {.width, .cap = Round, .join = Round})`: the plating
  is metallic and polished, its `relief` raises the band the keyline cuts
  on a soft shoulder, and `brushed` runs its anisotropy along the
  keyline's tangent, so the brushing turns with every bend without a map
  of its own.
- **The height page** (pads as discs, drills as pits, a `material::noise`
  grain stretched both ways for the weave) feeds
  `material::surface::normalFromHeight` for the substrate and the pads.
  Pads carry no brushing.
- **The colour page** (`sketch::Page`) fills the substrate rough and dark,
  lays the traces and the pads over it, and sets the silkscreen caption
  as flat ink on the substrate.
- **The bodies**: vias are `gm::revolve`d rings standing on their pads,
  solder beads are `gm::superellipsoid`s, the board is a `gm::box` slab
  under the page.
- **The pulse** is `gm::pop::on(spine).window(head, width).instances(...)`
  of a small elongated bead with emission, carried along the spine as the
  window's head advances, and `world::bloom` spreads it.
- **The light**: a directional key on an arc casting shadows, a
  `world::rectLight` panel behind the board shown by
  `world::kit::visibleLight`, and `world::kit::environmentMap` for what
  the plating reflects elsewhere.

## What to look at

- The highlight on the meander: it is a line across each run and turns
  through the U-turns with the trace instead of staying parallel to one
  axis.
- The coil: its highlight is a streak through the centre, pointing
  toward the light, that swings round as the key moves.
- The forty-five degree legs of the bus: their highlight lies at a
  different angle from the straight legs beside them.
- The vias' rims and the solder beads, each with a point of light and a
  shadow toward the camera's side when the key is low.
- The pulse: a short bright segment with a soft glow, travelling the bus
  line and lighting nothing but itself.
