# Wet glass console

A dark telemetry console stands under a tilted sheet of glass that has
been rained on. Its page keeps changing — traces sweep across their
charts, readings tick over, a calibration strip of fine lines sits along
the bottom — and the water on the glass is made of real bodies that see
it: every bead is a small lens of water that magnifies and bends the
lines and numbers beneath it, a few rivulets break loose and run down
the sheet, swallowing the beads in their way and leaving a thin wet
trail that drains away behind them, and an arc the wiper cleared shows
the page sharp and undisturbed between two ridges of left-behind water.
The eye should read the water by what it does to the page and by the
cool highlights it catches, not by any colour of its own. Transmission
at the index of water bends the page through each bead and trail;
transmission at the index of glass carries it through the sheet; a rect
light and the kit environment put the highlights on the water; a cool
key light throws the shadow of the glass's mount across the page.

## What it shows

- **The page.** A Compose page the console's face wears: a dark ground
  that takes the light, and on it three live
  traces scrolling across faint grids, two large readings and a row of
  channel values that change as the loop runs, and a calibration strip
  of line pairs growing finer toward one end beside a ramp of greys. The
  content is flat, so it shows as the console's own light.
- **The glass.** A thin sheet of clear glass over the page, its top
  leaning out toward the eye, held in a dark metal mount whose shadow the
  key throws across the page. It bends the page very little, and it is
  coated against reflection, as a console's cover is, so the room lies
  faintly on it and brightly on the water.
- **The beads.** Water caps standing on the glass, from pinheads to
  lenses wide enough to cover a digit, some drooping as water on an
  upright pane does. Each is a convex body of water, so the page seen
  through it is enlarged toward its centre and squeezed toward its rim,
  and bigger beads, carrying a longer path through the water, bend
  harder. Over the dark ground a bead reads dark, the room's windows
  bright along its rim and the panel a highlight near its crown; over a
  digit or a trace it fills with it, enlarged.
- **The rivulets.** Three runs of water that start near the top in turn
  and slide down the sheet, each a swept tube flattened onto the glass,
  thin behind and swelling into a heavy drop at its head. The beads in
  their path are taken up as the head passes and condense again once the
  trail has drained.
- **The wiped band.** An arc of glass a wiper cleared, where no bead
  stands; its two edges are low ridges of water the blade pushed aside,
  and a few faint streaks lie inside it where the blade was worn.

## How it is built

- `ctx.page` makes the page; `page.render(tree)` paints a new tree every
  frame and `page.surface()` is what the console's face wears. The traces
  are `path::curveThrough` outlines stroked by `compose::brush::ribbon`,
  a wide soft pass under a sharp one; the numbers are text that changes
  with the time.
- The glass is a `geometry::mesh::box` a few units deep whose surface
  states `transmission`, `ior` 1.5, a small `thickness` and a lowered
  `specular`.
- The beads are `geometry::mesh::revolve` caps, stood at points by
  `geometry::mesh::pop::on(cloud).instances(stamp)` with a `"size"` lane,
  in three sizes of body, each with the `thickness` its size of drop
  carries light through, at `ior` 1.33.
- A rivulet is `geometry::mesh::pop::on(points).sweep(profile)` carrying
  a flattened arch along a Catmull-Rom rail down the glass, its size
  tapered toward the head; its head is one more bead.
- The wipe ridges and streaks are the same sweep along arcs of the
  wiper's circle.
- The key is a `world::directionalLight` casting shadows. The highlights
  come from a `world::rectLight` that drifts slowly from side to side,
  linked with `lightLink` to the collection the glass and the water join,
  so it lights them and leaves the page dark, and from a
  `world::kit::environmentMap` of a dim room with cool windows round it.

## What to look at

The loop is twelve seconds. At the capture moment the left rivulet's
head is crossing the large humidity reading, the beads in its path
already taken up, and the drops on the digits hold pieces of them
enlarged; the middle rivulet has run almost down to the mount, its trail
the full height of the glass, and the right one is just letting go at
the top. Watch a single large bead as a trace runs beneath
it: the line bulges as it enters, slides fast through the middle and
pinches at the rim. Inside the wiped arc the same trace runs straight
and sharp. The panel light's reflection slides across every bead
together as it drifts.

One layer of refraction: a bead shows the page as it stands behind the
glass and never the glass sheet or another bead, and nothing is cast
through the water — no caustics, and the water throws no shadow. The
beads stand still; only the rivulets and what they take up move.
