# Glass forms

Cut panes, a lens and a liquid cell stand on a dark table in front of a
live page, so everything behind each one is seen bent, tinted and
magnified through it while the page keeps moving. The page is a sheet of
calibration paper — a fine ruled grid, coloured rules, lettering sliding
along two rails, an etched scale and a target — painted by Compose and
lit by the scene. A soft square, a lens-shaped pane and an annulus are
thin solids with rounded edges, flat in the middle and curved at the
rim, so the page shows through them straight and breaks only where the
edge normal turns; the lens-shaped pane disperses, the annulus is green
glass. A round magnifier over the scale enlarges it. The liquid cell
holds a warm amber liquid with a flat top that climbs the walls in a
meniscus, its level rising and falling against an enamelled graduation,
a cobalt rod standing in it broken at the surface, and a square of heat
haze shimmering above it. A smoked cover darkens a band of the page, and
a rained-on pane hangs before the page's upper corner, its beads and one
running rivulet bending the paper and the drifting rules behind them. A
low key light casts the shadows of the stands, the rod and the plinth
across the table and the page, and the studio's soft boxes give the
glass something to reflect.

## What it shows

Left to right along the table:

- **the liquid cell**: a clear glass cell on a black plinth holding warm
  amber liquid at an index of 1.36. Its top is flat across the middle
  and climbs each wall in a meniscus; its level rises and falls against
  a graduation enamelled on the front glass. A cobalt rod stands in it,
  seen shifted where it enters the liquid, and a square of heat haze
  above the cell shimmers the paper behind it;
- **a soft square** of clear glass at 1.5, its faces flat and its rim
  turned through a half-round, so the page is straight through the middle
  and drawn inward at the edge;
- **a lens-shaped pane** of glass that disperses: every edge seen through
  its rim fringes red and blue;
- **an annulus** of green glass, its two rounded rims bending the page in
  rings, the green deepening where the light crosses more of it;
- **a magnifier** on a brass stand, its biconvex lens enlarging the
  etched target and millimetre scale behind it.

Above them, a **smoked cover** stands close in front of the page and
darkens the lower rail of lettering where it passes behind, and a **wet
pane** hangs on two brass clips before the page's upper left corner:

- **the beads** are water caps standing on the pane, from pinheads to
  lenses a grid square wide, some drooping as water on an upright pane
  does. Each is a convex body of water at an index of 1.33, so the grid
  and the drifting upright rule seen through it are enlarged toward its
  centre and squeezed toward its rim, and a bigger bead, carrying a longer
  path through the water, bends harder;
- **the rivulet** breaks loose near the top of the pane once a loop and
  runs down it, faster as it gathers water, taking up the beads in its
  lane and leaving a thin trail that drains away behind it while the
  beads form again.

The page carries a pale-blue calibration grid, a red and a blue rule
across it, four coloured upright rules drifting across it, a large line
of lettering sliding left behind the forms and a smaller line of
refractive indices sliding right above them, the etched plate, and a
two-line caption. The loop is sixteen seconds and closes on itself; the
eye drifts a little left and right, so the page slides through every
glass from two causes at once.

## How it is built

- The page is a `sketch::Page` (`ctx.page`, `page.render(tree)` each
  frame, `page.surface()` on an upright `gm::quad`): a Compose tree whose
  inks and paper state a surface response, so the scene's key light, fill
  and environment light it and the stands' shadows fall on it. The grid
  is repeating `material::linearGradient` layers; the rails are text set
  twice end to end in a clipped box, measured once with
  `compose::intrinsicSize`.
- The table is a second page of slate: `material::noise` layers and a
  normal from `material::surface::normalFromHeight`.
- The square, the lens-shaped pane and the smoked cover are
  `gm::loft` through outlines inset along a half-round, so their faces
  are flat and their rims curved; the annulus and the magnifier are
  `gm::revolve` of a rounded and a biconvex profile; the liquid is
  `gm::extrude` of its outline with the meniscus drawn into its top edge,
  remade at each level; the cell's walls are `gm::extrude` of a U.
- Every glass is `material::from(white).surface({...})` with
  `transmission`, `ior` and `thickness`; the annulus, the liquid and the
  cover add `attenuationColor` and `attenuationDistance`, the
  lens-shaped pane `dispersion`. The haze is a colourless surface whose
  `distortion` is `material::surface::normalFromHeight` over a
  `material::noise`, differentiated on the device, and whose
  `distortionStrength` breathes. The
  cell's front glass is a black `PlusLighter` surface that writes no
  depth: it shows only the room it reflects.
- The beads are two instanced bodies, one per cap shape: a cloud of bead
  centres stamped with a `gm::revolve` cap through
  `gm::points::instances`, each copy scaled by its "size" lane and
  carrying it, so the bead material's `thicknessAttribute` makes every
  copy's optical depth its own radius. The rivulet's trail is
  `gm::pop::on(rail).sweep` of an arch along its path with a taper that
  thins it as it drains, rebuilt to where the head has reached, and the
  head is one drooping cap.
- Light: one `world::directionalLight` key that casts shadows, a
  `world::hemisphereLight` fill and a `world::kit::environmentMap` of
  three soft boxes that the glass reflects.

## What to look at

- The square's middle against its rim: straight lines run through it
  unbroken and bend only where the edge turns.
- The lens-shaped pane's rim, where the drifting upright rules split into
  colour.
- The magnifier: the ticks under it several times wider than those beside
  it.
- The cell as its level moves: the meniscus rides up and down the
  graduation, the lettering behind it is tinted amber below the surface
  and plain above.
- The wet pane: the yellow upright rule and the grid swelling inside every
  bead, harder in the bigger ones, and the clear lane the rivulet leaves
  until the beads come back.

Each glass sees the opaque scene behind it, never another glass: a form
seen through another shows what stands behind both. Nothing is cast
through the glass — no caustics, no coloured shadows — and the glass
casts no shadow. The thicknesses are optical depths chosen for the bend
they give, not the bodies' measured depths. Nothing is interactive.
