# SigilGeometry — the kit

The chapter on the shelf over the tiers: the closed and open
silhouettes, the corner treatments, the shapers over the deviation
seam, the hatch door, the sweep's cross-sections, a figure's divisions
and the solids — and the contract every value on the shelf keeps.
`README.md` beside the library is the front page.

**`kit`** — `SigilGeometryKit`, the shelf. Stock values over the tiers
beneath, in `sigil::geometry::shapes`.

- **`kit/Generators.h`** — the closed silhouettes: `svg()` (an SVG path-d
  string parsed once, its bounds mapped onto the box), `polygon()`,
  `star()` (with `waist`, which bows each arm edge inward the way an
  engraved star narrows), `circle()` (winding, start point, a concentric
  `inset`, and `uniform` for the true CIRCLE on a box that is not square,
  where the default is the box's oval), `annulus()` — with `ring()`, the
  same ring stated as its own pixel width so it keeps that width when the
  box changes, and a concentric `dot` at the centre, which is one outline
  and not two marks — `squircle()`, `blob()` (seeded, so the same seed is
  the same blob every run), `arc()` (open), `sector()` (closed and
  fillable, with an inner radius for the donut slice), `parallelogram()`,
  `arrow()` (whose `headSpan` is the head's own size across, so a fan of
  arms of different lengths carries heads of one size rather than five)
  and `chevron()` (the wide flat V, with outrigger bars).

  Two shapes people reach for that are already here: the DIAMOND with its
  points on the box's edges is `polygon(4)`, since a polygon's first
  vertex is up and its vertices step round the box's own ellipse —
  `polygon(4, 45)` is the square, a different figure at a different size —
  and the true circle in an oblong box is `circle()` with `uniform`.
- **`kit/Curves.h`** — the open silhouettes, evaluated in a unit frame and
  scaled onto the box's half-extents so a curve keeps its proportions when
  the box changes: `parametric()` raw and keyed, `lissajous()`,
  `harmonograph()` (a Lissajous whose amplitudes decay, with precession),
  `rose()`, `spiral()` (Archimedean or logarithmic) and `trochoid()`.
- **`kit/Corners.h`** — the two wrappers over any silhouette and the two
  shapes a frame is cut to. `rounded()` rounds every sharp corner;
  `shaped()` runs a `path::Shaper` over the outline, so a torn or wobbled
  edge is decided ONCE where the shape is asked for rather than re-run as
  a path effect on every mark the figure carries — which is also what
  makes the fill, the keyline and the glow agree on where the edge went.
  `chamfered()` and `notched()` both take a per-`Corner` mask, because a
  cut on one diagonal is the common case and no single radius says it. A
  treatment of ZERO is a square corner, not a cut of no length: the two
  vertices it would otherwise emit stand on top of each other, and
  `rounded()` over that path finds no corner to round there. `Chamfered`
  also carries a `radius` for the corners its mask does NOT name —
  "rounded except where cut", the machined-panel rule, which a rounding
  wrapped round a chamfer cannot say because it would round the cut too —
  and a `cutRise` for the cut that is not at 45°.
- **`kit/Silhouettes.h`** — the 2D shelf, including all four:
  `Corners.h`, `Curves.h`, `Generators.h` and `Hatches.h`.
- **`kit/Shapers.h`** — `shapers::`, the stock over the deviation seam:
  `Wave` (also the braid primitive — strands that oscillate trade sides,
  and where they trade sides they cross), `Zigzag`, `Square`, `Jitter`,
  `Offset`, `Rounded` and `Chamfer`, with a factory each. `Wave` answers
  BOTH seams: `shape()`/`bleed()` make it a shaper and
  `across()`/`max()` make it a `path::Profile`, so the oscillating width
  law is the same `shapers::wave` call rather than a second one. As a
  profile it is ZERO-MEAN and therefore a strand centreline rather than
  a band width. Both stand in `shapers::` and not in `path::profile`,
  because a kit composes over a seam and does not grow it.
- **`kit/Hatches.h`** — `hatchOutline()`, a silhouette filled with lines
  as one path: the outline narrowed by `operations::offset`, flattened, run
  through `path::lattice` and joined up. It is a door rather than a
  construction — a caller holding an `SkPath` should not have to flatten
  it into rings itself, and that is why the fill has next to no adoption
  while three drawings fake it by clipping a line field. What comes back
  are CENTRELINES, so every mark can be walked, banded to a width or
  drawn along with a tool; `path::lattice` is the same fill as marks, for
  a caller that wants to do any of that itself.
- **`kit/Sections.h`** — `sections::`, the two unit cross-sections a
  sweep carries: `circle()` (open, its seam point duplicated so the swept
  u reaches 1) and `line()` (a unit-width segment, a flat band once
  swept). `SweepOptions::scale` sizes both, so neither takes a radius or
  a width, and an outline that is not one of these reaches a sweep
  through the sweep's own `pop::profile::fromPath()`.
- **`kit/Divisions.h`** — a figure's divisions as ONE multi-contour path:
  `ticks()` walks a division count around a `PolarFrame` (with a longer
  mark every N), `arcs()` walks the same count as CLOSED segments of the ring
  itself, `chords()` walks a polygon's sides. One path rather than N
  drawn things, because a divider ladder is static geometry with one
  style — the exception is per-mark animation, which needs its own
  keyed items. Each has a comparable `Silhouette` form (`TicksShape`,
  `ArcsShape`, `ChordsShape`) for a consumer that shapes a box with it.
  A `Ticks` carrying a `markPx` turns its ladder from open lines into
  CLOSED marks — the node, the lozenge, the bar — which is geometry
  rather than a weight the paint decides, so it fills, takes a gradient
  across its own width and unions with its neighbours; `arcs()` is the
  curved sibling of that, whose marks follow the ring and fatten with
  radius the way a segment of a dial does.
- **`kit/Solids.h`** — the 3D shelf, in `sigil::geometry::mesh` because
  what it makes is a `Mesh`. Two of them LIFT another currency:
  `extrude()` raises a filled path into a solid (caps earcut-triangulated
  with holes intact, walls swept from the flattened contours) and
  `revolve()` lathes a profile polyline around +y. Most of the rest are
  the named surfaces — `torus()`, `superellipsoid()`, `cylinderPanel()` —
  each one `mesh::grid()` evaluated through a formula anyone could have
  written, which is why they are a shelf and not the currency. `box()` is
  the one that is not a sheet: flat normals and hard corners, so every
  face carries its own four vertices, its own outward normal and its own
  UV square, and any of the six can be dropped — the underside a camera
  never gets beneath is a sixth of the triangles for nothing. Its
  `sideShade` darkens the four side faces against the top and bottom,
  which is what makes a field of boxes read as blocks rather than as one
  surface; at its default of 1 it shades nothing and the mesh carries no
  colour at all. `platonic()` is the other one that is not a sheet: the
  five regular solids from one generator, since a solid is a corner table
  and a rule for finding the faces over it — the plane through a corner
  and two of its neighbours is a face plane, and what stands furthest
  along one is the face. Every triangle carries its face's index in the
  `"Id"` lane, so a dodecahedron reads as twelve pentagons through
  `mesh::faceCount()` and its neighbours; `sharedVertices` picks between
  the hard-cornered solid, which is what a die or a machined part wants,
  and the welded corner cage, where two faces that meet name the same two
  corners and an edge is a pair of indices. The cube's hard-cornered form
  IS `box()`, so that is what it answers.

Every value here has `path(SkSize)`, `operator==` and `operator()`, and
that is the whole contract: a consumer that caches drawings prunes on the
equality, and a consumer that wants a plain path-over-size function gets
one from the call operator. Your own generator written the same way has
the same standing — the kit is stock, never privileged, and equal values
must draw identical paths at every size. A hand-rolled
`shapes::OutlineFunction` is the escape hatch beside them, and the size is
offered to it rather than demanded: one that draws the same path whatever
the box is takes `[] { return p; }`.
