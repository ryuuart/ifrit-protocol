# SigilGeometry — the kit

The chapter on the shelf over the tiers: the closed and open
silhouettes, the corner treatments, the shapers over the deviation
seam, the hatch door, the sweep's cross-sections, a figure's divisions
and the solids — and the contract every value on the shelf keeps.
`README.md` beside the library is the front page.

**`kit`** — `SigilGeometryKit`, the shelf. Stock values over the tiers
beneath, in `sigil::geometry::shapes`.

- **`kit/Radial.h`** — THE TWO GENERAL SHAPES over a box.
  `radial(count, path::RadialOptions)` deals N vertices round the centre
  and joins them into a loop, chord by chord, or not at all with a
  `path::Mark` at each — the polygon, the star, a star polygon's chords,
  a tick ladder, dial segments, a ring of studs as ONE outline, a spiral
  of seeds. `ellipse(EllipseOptions)` is the one conic inscribed in the
  box with its sweep, its `Close` (`Open`, `Chord`, `Pie`), a hole as a
  ratio or a pixel `thickness`, a centre `dot`, a superellipse
  `exponent`, an `inset`, `uniform` for the true circle on an oblong box,
  and the `winding` and `start` a text baseline reads. `fitted(outline,
  FitOptions)` scales any outline into the box. Each is a `Radial`,
  `Ellipse` or `Fitted` value carrying the MODIFIERS every general shape
  has: `cornered(radius, CornerOptions)`, `distorted(shaper)` and
  `at(centre, radius)`, the shape drawn about a point.
- **`kit/Generators.h`** — the stock values over those two, each one line
  and each shorter at the call site than the general form it stands for:
  `polygon(sides, rotation)` and `star(points, innerRatio, waist)` over
  `radial` (the waist bows each arm edge inward the way an engraved star
  narrows); `circle()` (with an inset, or a winding and a start point),
  `annulus(inner)`, `ring(thickness, dot)` — the same ring stated as its
  own pixel width so it keeps that width when the box changes —
  `squircle(exponent)`, `arc(from, sweep)` (open) and `sector(from,
  sweep, inner)` (closed and fillable, with an inner radius for the donut
  slice) over `ellipse`; `svg(data)` over `fitted`. Beside them the
  one-offs that are no setting of either: `blob()` (seeded, so the same
  seed is the same blob every run), `parallelogram()`, `arrow()` (whose
  `headSpan` is the head's own size across, so a fan of arms of different
  lengths carries heads of one size rather than five) and `chevron()`
  (the wide flat V, with outrigger bars).

  Two shapes people reach for that are already here: the DIAMOND with its
  points on the box's edges is `polygon(4)`, since a polygon's first
  vertex is up and its vertices step round the box's own ellipse —
  `polygon(4, 45)` is the square, a different figure at a different size —
  and the true circle in an oblong box is `ellipse({.uniform = true})`.
- **`kit/Curves.h`** — the open silhouettes, evaluated in a unit frame and
  scaled onto the box's half-extents so a curve keeps its proportions when
  the box changes: `parametric()` raw and keyed, `lissajous()`,
  `harmonograph()` (a Lissajous whose amplitudes decay, with precession),
  `rose()`, `spiral()` (Archimedean or logarithmic) and `trochoid()`.
- **`kit/Corners.h`** — the wrappers over any silhouette and the two
  shapes a frame is cut to. `cornered()` treats every sharp corner by a
  radius — `CornerShape::Round` or `Bevel` — and `rounded()` is its
  default; `at()` draws any silhouette about a centre at a radius;
  `shaped()` runs a `path::Shaper` over the outline, so a torn or wobbled
  edge is decided ONCE where the shape is asked for rather than re-run as
  a path effect on every mark the figure carries — which is also what
  makes the fill, the keyline and the glow agree on where the edge went.
  `chamfered()` and `notched()` both take a per-`Corner` mask, because a
  cut on one diagonal is the common case and no single radius says it. A
  treatment of ZERO is a square corner, not a cut of no length: the two
  vertices it would otherwise emit stand on top of each other, and
  `cornered()` over that outline finds no corner to round there. `Chamfered`
  also carries a `radius` for the corners its mask does NOT name —
  "rounded except where cut", the machined-panel rule, which a rounding
  wrapped round a chamfer cannot say because it would round the cut too —
  and a `cutRise` for the cut that is not at 45°.
- **`kit/Silhouettes.h`** — the 2D shelf, including all four:
  `Corners.h`, `Curves.h`, `Generators.h`, `Hatches.h` and `Radial.h`.
- **`kit/Shapers.h`** — `shapers::`, the stock over the deviation seam:
  `Wave` (also the braid primitive — strands that oscillate trade sides,
  and where they trade sides they cross), `Zigzag`, `Square`, `Jitter`,
  `Offset`, `Rounded` and `Chamfer`. Each is written as the value it is,
  `shapers::Wave{.amplitude = 6, .wavelength = 40}`, with no lowercase
  factory beside it: a shaper carries two or three dials whose names are
  the whole of what a reader needs, and a positional call hides them.
  `Wave` answers BOTH seams: `shape()`/`bleed()` make it a shaper and
  `across()`/`max()` make it a `path::Profile`, so the oscillating width
  law is that same value rather than a second one. As a profile it is
  ZERO-MEAN and therefore a strand centreline rather than a band width.
  Both stand in `shapers::` and not in `path::profile`, because a kit
  composes over a seam and does not grow it.
- **`kit/Hatches.h`** — `hatchOutline()`, a silhouette filled with lines
  as one path: the outline narrowed by `operations::offset`, flattened, run
  through `path::lattice` and joined up. It is a door rather than a
  construction — a caller holding an outline should not have to flatten
  it into rings itself, and that is why the fill has next to no adoption
  while three drawings fake it by clipping a line field. What comes back
  are CENTRELINES, so every mark can be walked, banded to a width or
  drawn along with a tool; `hatchMarks()` is the same fill over rings as
  marks, for a caller that wants to do any of that itself. `Hatch` is the
  ONE hatch value above this library — spacing, angle, taper, origin,
  inset and a `cross` pass at a right angle — which Draw's brush lays
  marks along and Compose's decoration strokes; what they add (the tool,
  the jitter, the ink and width) is theirs.
- **`kit/Sections.h`** — `sections::`, the two unit cross-sections a
  sweep carries: `circle()` (open, its seam point duplicated so the swept
  u reaches 1) and `line()` (a unit-width segment, a flat band once
  swept). `SweepOptions::scale` sizes both, so neither takes a radius or
  a width, and an outline that is not one of these reaches a sweep
  through the sweep's own `pop::profile::fromPath()`.
- **`kit/Divisions.h`** — a figure's divisions as ONE multi-contour
  outline, three stock values over `radial`: `ticks()` walks a division
  count around a `PolarFrame` (with a longer mark every N), `arcs()` walks
  the same count as CLOSED segments of the ring itself, `chords()` walks
  a polygon's sides. One outline rather than N drawn things, because a
  divider ladder is static geometry with one style — the exception is
  per-mark animation, which needs its own keyed items. Each takes its
  option struct (`Ticks`, `Arcs`, `Chords`) in two forms: with a frame,
  the outline drawn where the frame stands; alone, a `Radial` whose frame
  comes from the box it is asked for. `radialOf()` is the conversion to
  the general options. A `Ticks` carrying a `markPx` turns its ladder
  from open lines into CLOSED marks — the node, the lozenge, the bar —
  which is geometry rather than a weight the paint decides, so it fills,
  takes a gradient across its own width and unions with its neighbours;
  `arcs()` is the curved sibling of that, whose marks follow the ring and
  fatten with radius the way a segment of a dial does.
- **`kit/Solids.h`** — the 3D shelf, in `sigil::geometry::mesh` because
  what it makes is a `Mesh`. Two of them LIFT another currency:
  `extrude()` raises a filled outline into a solid `depth` thick (caps
  triangulated by `path::triangulate` with holes intact, walls swept from
  the flattened contours), `fill()` is one of those caps on its own — the
  outline flat at z = 0 — `loft()` skins a run of section rings, resampled
  to one count so a square can loft into a circle, and `revolve()` lathes
  a profile — points, or an outline's first contour — around +y. Most of the rest are the named surfaces — `torus()`, `superellipsoid()`, `cylinderPanel()` —
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

Every value here has `outline(size)` answering a `path::Outline` and
`operator==`, and that is the whole contract (`shapes::Silhouette`): a
consumer that caches drawings prunes on the equality, and
`shapes::outlineOf()` asks any value — or a bare callable — for its
outline, which is the one call a wrapper or a consumer that holds a plain
function needs. A silhouette is ONE value and a lowercase factory
spelling that value's fields as arguments, and the value is where the
documentation lives, parameter by parameter, so there is no second copy
of it to drift. Your own generator written the same way has the same
standing — the kit is stock, never privileged, and equal values must
draw identical outlines at every size. A hand-rolled
`shapes::OutlineFunction` is the escape hatch beside them, and the size is
offered to it rather than demanded: one that draws the same outline
whatever the box is takes `[] { return outline; }`.
