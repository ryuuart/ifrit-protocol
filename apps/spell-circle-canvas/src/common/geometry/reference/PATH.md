# SigilGeometry — the path tier and its headers

The chapter on the 2D tier: `path`, the leaf every other feature
stands on, and `path/blend`, the shape interpolation over it, header by
header. `README.md` beside the library is the front page;
`MESH.md` is the 3D tier, `POP.md` the point operators and `KIT.md` the
shelf over both.

**`path`** — `SigilGeometryPath`, the leaf. Thirty headers that
depend on nothing else in the library: Skia, glm, SigilCoreCompute, whose
seeded mixers the value-noise field and the scatter's stream are built
on, and CDT, the Delaunay triangulator, read in one source file and named
in no header.

- **`path/Polyline.h`** — the resampling core. `Polyline` (its points, its
  closure and its `lane`, one scalar riding each vertex) with `length()`,
  `centroid()` (length-weighted over the edges), `signedArea()` and
  `reverse()` on it, and `flatten()`, `sample()` to walk a parametric
  curve evenly IN ITS PARAMETER (`resample()` is the arc-length one, and
  is taken over a polyline `sample()` has already produced), `Sampled`
  and `resample()`, `bestAlignment()`/`applyAlignment()` for matching two
  closed contours, `toPath()` to rebuild (optionally through Catmull-Rom
  cubics), `smoothThrough()` to rebuild as a curve the points STEER — one
  quadratic per interior point, through the midpoint of every edge and
  never outside the hull of the points, so a coastline given a dozen
  points or a brush centreline given four reads as one stroke rather
  than a chain of chords — and `lerp()`. Beside them the two resamplings
  keyed to a SPACING: `subdivide()`, every edge cut into equal steps no
  longer than the spacing with every source vertex kept, and
  `catmullRom()`, the same cut through the curve the controls lie on,
  blended toward the chords by an amount so a hand-placed chain does not
  bow further than the hand meant. Both carry the lane. And what a
  polyline answers about the area it bounds: `bounds()` and `bounds()`
  over a set, `contains()` (the even-odd ray test on one ring, which
  joins the ends whether or not the polyline says it is closed) with
  `containsEvenOdd()` over a set of them, and `edgeCrossings()`, where a
  segment crosses the edges, nearest its start first.
- **`path/Segments.h`** — the outline read as SEGMENTS: one `Segment` per
  drawn piece (`Line`, `Quad`, `Conic`, `Cubic`) with its points in
  drawing order and the conic's weight beside them, `segments()` in and
  `toPath()` out, round-tripping a path verb for verb and weight for
  weight. A closed contour's closing line is the CLOSURE and not a
  segment, so a rectangle is three pieces and comes back as the four
  points it was stored as. This is the reading that can answer a question
  about a NODE, which `Polyline` (which throws the curves away) and
  `Contour` (which sees only arc length) cannot: where a cubic turns,
  which handles lie on their chord, whether two outlines have the same
  nodes in the same order. `compatible()` is that last question and
  names the FIRST REASON a pair is not — contour count, segment count,
  a moved start point, or a genuinely different kind of piece — so a
  caller is told what to repair instead of silently getting a
  resampling. `reversed()`/`reverse()` and `startedAt()`/`startAt()` are
  the two rewrites that leave the drawn shape exactly where it is and
  change only which way the pen went and which node it went from.
- **`path/Extremes.h`** — nodes put where a curve TURNS. `Where` names
  the three turns worth a node — the axis extremes, a cubic's
  inflections, the points of maximum curvature — and `minDepthPx` is the
  one dial that says how shallow a turn may be and still earn one, since
  a bulge of half a pixel is a rounding artefact rather than a feature of
  the drawing and a node there is a node that will wander. `extremes()`
  is the operation and the drawn curve does not move under it: a node is
  inserted by splitting a piece into two of its own kind.
  `extremeNodes()` answers where they would go without putting them
  there. The solvers are Skia's own, in
  `src/core/SkGeometry.h` — a private header that ships beside the static
  archive, read in ONE translation unit and no other, because each of
  them is a page of well-known cubic arithmetic and a second spelling
  would be a second place for a rounding to differ.
- **`path/Tidy.h`** — nodes TAKEN AWAY. `tidy(path, tolerance)` drops a
  node standing on a straight run, merges nodes sitting on top of one
  another and turns a curve whose handles lie on its own chord into a
  line; the tolerance is the whole measure, being how far the outline may
  move where a node goes. The word `simplify` is taken by the boolean
  family's self-intersection cleanup, which is a different operation on a
  different thing. What is deliberately absent is a font editor's other
  half — setting each node's smooth-or-corner mode — because nothing here
  carries a node type and inventing one to serve one operator would put a
  font editor's model into a drawing library.
- **`path/Fit.h`** — a dense RUN OF POINTS as few cubics.
  `fitCurve(points, tolerance)` — and the same over a `Polyline`, which
  when closed is fitted from its seam round to its seam and then closed,
  so the seam is the one node the fit is pinned at and every other falls
  where the tolerance puts it — answers the fewest cubics that hold every point within the
  tolerance, by Schneider's rule: fit one cubic by least squares with
  the ends' own directions as the tangents and the chord lengths as the
  first guess at each point's parameter, improve those parameters
  against the fitted curve by Newton-Raphson, and split at the worst
  point when it is still further off than the tolerance. Fewer than two
  points is an empty path, exactly two is the line between them, and a
  run that stands still is one point, since it has no direction to fit.
  This is what a tracer, a stylus, a sampled field line or a decoded
  stroke hands its points to, and it is the opposite direction from
  `toPath(sampled, smooth)`, `smoothThrough` and `catmullRom`, which
  build a curve through or around a set of controls one piece per
  control: there the point count IS the node count, here the point count
  is the input and the node count is the answer.
- **`path/Interpolate.h`** — the exact in-between of two outlines that
  pair, node for node and curve for curve, or nothing at all when they do
  not — and `compatible()` says which of the reasons it was. `path/blend`
  is the other interpolation and a different one: it takes any two
  outlines, resamples both and matches them up, which is what a blend
  tool does and what a pair of masters must never need.
- **`path/Direction.h`** — which way round an outline is drawn, and the
  two things that travel with it. `nesting()` answers, per ring, how many
  rings enclose it and which encloses it most tightly — the even-odd
  reading, and the one piece of arithmetic an extrusion's caps, a hole
  test and a winding fix all need. `direction()` is the whole operation:
  `Winding` puts outers one way and holes the other, `orderContours`
  brings the outers first, and `resetStart` starts every closed contour
  at its bottom-left node. Three switches rather than three functions
  because they exist for one reason — an outline whose winding is fixed
  but whose contours arrive in another order still interpolates into a
  tangle. The sign convention is `Polyline::signedArea`'s: in Skia's
  y-down space a positive area is a CLOCKWISE ring, so a winding test
  copied from a y-up source reads inverted here.
- **`path/Stride.h`** — the even-spacing walk for a curve that arrives one
  piece at a time. `Stride::advance(length, spacing, land)` answers the
  fractions of the piece the walk lands at and carries the distance still
  owed across pieces, so a stroke sampled as a device reports it and the
  same stroke sampled whole put their marks in the same places. It names
  no point: the caller owns the geometry and interpolates whatever rides
  on it.
- **`path/Lattice.h`** — the scanline fill. `lattice()` lays parallel lines
  at an `angle`, a `spacing` apart with an optional `taper` opening or
  crowding each successive gap, and cuts them to the even-odd interior of
  a set of rings; each `LatticeMark` is a centreline. A hatch, a plotter
  fill and a mass of strokes are the same construction, so there is one.
  `origin` is where the ladder is measured from — the phase, which is
  what stops a fill crawling as the shape it fills animates; unset lays
  the first line half a gap inside the rings, which is what fills a shape
  whose place is not fixed. `shapes::hatchOutline` is the stock value
  over this and the offset, for a caller holding an `SkPath` rather than
  a set of rings.

  **`multigrid(families, MultigridOptions)` is the other reading of the
  same lines**: N families of parallel lines DUALISED into a tiling of
  rhombs, which is de Bruijn's construction and the one operation an
  aperiodic rhomb tiling is. Each `MultigridFamily` is a `normal`, a
  `spacing` and an `offset`; each crossing of two lines becomes a
  `MultigridRhomb` whose edges are the two families' normals, placed by
  counting how many lines of every family stand between the crossing and
  the origin. `multigridRing(count, offset)` is the evenly spread ring —
  a whole turn for an odd count and a half turn for an even one, which is
  the smallest turn giving that many distinct line directions — so five
  is the Penrose rhombs, four the Ammann-Beenker squares and 45-degree
  rhombs, three the rhombille, and families at angles of the caller's own
  choosing are the tiling those angles admit. `radius` is the only reach
  there is: the line indices that answer it are derived by carrying the
  reach back through the dual map, so no caller states an index range.
  **It is solved in double and it answers in double**, because a rhomb's
  place is a count of lines read off a crossing by a ceiling: a crossing
  that lands a hair on the wrong side of a line moves that rhomb a whole
  edge, so a grid rounded to float before it is dualised does not blur,
  it tiles differently. The corners come back as one `vertices` list
  welded at `tolerance`, so two rhombs meeting at a corner name the same
  vertex. **The offsets have to be regular and a singular set is
  refused**: a point that lines of three or more families run through has
  no count of those families' lines, so which side of the third line it
  is read on is settled by the last digit rather than by the geometry and
  the rhomb moves a whole edge with the answer. Offsets are regular when
  no point of the plane lies on three or more of the lines; a singular
  set answers an empty tiling the way families that cannot span the plane
  do. All-zero offsets are singular whatever the families, since line
  zero of each runs through the origin, and a ring of three families is
  singular exactly when its offsets sum to a whole number. For a longer
  ring the sum decides nothing — five families at a fifth each sum to one,
  are regular, and dualise into the tiling that is exactly fivefold about
  the origin, which is what a caller reaching for zero offsets wanted.
- **`path/Neighbours.h`** — the uniform grid, and the primitive under
  everything below it. `Neighbours` is built once from a set of points
  and copies them in, then answers `within()` (a radius, into a vector
  the caller owns so a loop of queries allocates once), `nearest()` (the
  nearest, or the k nearest in distance order), `nearestOther()` (the
  nearest that is not a named point — what every spacing measurement
  wants, since a point indexed with the set it is measured against is
  always its own nearest), `cellPoints()` and the allocation-free
  `forEachWithin()`. The cell size is the one dial and choosing none is
  the usual answer; the grid is bounded in cells, so a cell size given is
  a request the index may coarsen and `cell()` says what it used. The
  currency is three dimensions and flat points enter at `z` of zero, so a
  sheet costs exactly what a two-dimensional grid would. Beside it
  `relax()` and `Relaxation`: points pushed out of each other's radius,
  every pass over a fresh index, with an optional `hold` deciding where a
  moved point lands. It is a SNAPSHOT — a moved point set is a new index,
  not an update — which is what makes a relaxation a loop of rebuilds and
  each pass' answers consistent with one another.
- **`path/Scatter.h`** — filling a shape with points. `Region` is a set of
  rings read by the even-odd rule, built `of()` a path, a rect or a span
  of rings or as a `disc()`, and answering `bounds()`, `contains()` and
  `area()`; `Distribution` is where the points go, as one value with
  props — a `Spread` (`Random` independent draws, `Lattice` whose
  `jitter` runs from an exact grid to anywhere in the cell, `Poisson` a
  minimum separation), a `Rate` saying whether `amount` is a count, a
  density or a spacing, a seed and a `core::chance::Source`,
  `relaxIterations`, and `maxPoints` as a bound rather than a preference.
  `sample(region, distribution)` answers the points. `uniform()`,
  `poisson()`, `blueNoise()`, `grid()` and `jittered()` are stock values
  over that one type, not five functions: blue noise is any spread with
  the relaxation switched on, and an exact grid is a lattice with no
  jitter. A count is exact for `Random` and a CAP for the other two,
  which answer what their pitch fits.
- **`path/Triangulate.h`** — the Delaunay triangulation and its dual.
  `delaunay()` answers a `Triangulation`: the points actually triangulated
  (duplicates are one point), the triangles over them, what lies across
  each edge, and `circumcentre()`, `circumradius()` and `adjacent()`.
  `voronoi()` builds the dual CDT does not — one ring per point, each the
  bounds cut once per neighbour by their perpendicular bisector, which is
  why the bounds is an argument and not an option: the cells on the
  outside are unbounded until something closes them. A set too degenerate
  to triangulate still has a diagram and gets it.
- **`path/Hull.h`** — `hull(points, alpha)`, one function with one dial.
  At `alpha` of infinity — the default — it is the convex hull, by the
  monotone chain and no triangulation at all. Below that it is the alpha
  shape: triangles whose circumcircle is larger than the bound are not
  inside, so the outline reaches into concavities, and below the spacing
  of the points themselves the shape falls apart into islands — which is
  why the answer is a set of rings, wound so they read by the even-odd
  rule the rest of the library reads rings by.
- **`path/Trace.h`** — walking a vector field. `VectorField` is any
  `glm::vec2(glm::vec2)`; `streamline()` carries a seed along one by the
  classical fourth-order Runge-Kutta step and answers a `Polyline`, so
  `resample`, `subdivide`, `smoothThrough`, `catmullRom`, `toPath`, the
  lattice and the band all apply to a flow line unchanged.
  `TraceOptions` holds the step, the total length, an optional bounds,
  `bothWays` (which makes the seed the middle of the line rather than its
  start) and the speed below which a field has no direction. `flow()` is
  the one adapter this library ships, turning a `core::noise::Field` into
  a direction as an `Angle`, as its `Gradient` or as its `Curl` — three
  genuinely different pictures of one noise, since a gradient field never
  circulates and a curl field never converges.
- **`path/Symmetry.h`** — repeating a figure into its own copies.
  `Symmetry` is a rotation (`order`, `start`, `sweep`) about a `centre`, a
  reflection (`mirror`, `mirrorAngle`) through it, and a translation
  lattice (`cellU`, `cellV`, `repeatU`, `repeatV`) — three independent
  things that multiply, defaulting to the identity so each switches one
  repetition on without disturbing the others. `copies()` answers the
  matrices themselves, or the polyline, the point set or the path under
  each of them; the lattice is outermost, so drawing them in order lays a
  whole rosette down per cell. `wallpaper()` names the plane groups this
  value expresses EXACTLY — those whose point group is one rotation, or
  one rotation and one mirror, about a single centre. The groups built on
  glide reflections are not named rather than named and approximated.
- **`path/Cells.h`** — the substrate every cellular automaton and
  reaction-diffusion shares. `Cells<T>` is a rectangle of cells with a
  spare: `at()` reads and writes a cell, `read()` reads one through the
  `Boundary` rule (`Clamp`, `Wrap`, or a stated `Constant` outside), and
  `step(rule)` calls `rule(sheet, x, y)` for every cell, writing the
  spare and swapping at the end — so nothing a rule reads has been
  written by its own pass and the order the cells are walked in cannot
  change the answer. THE RULES ARE NOT HERE: a fire, a slime mould and
  Conway's life share this buffer and share nothing else, so a catalogue
  of rules would be a catalogue of somebody else's pictures.
- **`path/Contour.h`** — a path's sub-paths by arc length. `Contour::of()`
  splits a path (skipping zero-length contours); `length()`, `closed()`,
  `at()`, `around()`, `segment()`/`appendSegment()`, and `corners()`, which
  walks the contour in strides and bisects to each turn sharper than a
  threshold. Three constructions walk every contour of a path:
  `parallel()` (a curve a constant distance to the side, round outer
  joins and mitred inner ones), `displace()` (a sinusoidal or zigzag
  sideways wave, fitted to a whole number of cycles so both ends stay on
  the source curve) and `cornerWindows()` (the pieces within a radius of
  each corner, or everything but them).
- **`path/Pose.h`** — a contour addressed by distance, with the sideways
  direction and the end policy written down. `Pose` (position, unit
  tangent, the normal that tangent turned toward +y, and the distance the
  policy resolved to), `Wrap` (`Clamp` parks at the nearer end, `Around`
  comes round closed geometry), `poseAlong()` over one contour or over a
  list of them walked as ONE arc-length coordinate, and `totalLength()`
  and `closedThroughout()` over such a list.
- **`path/Noise.h`** — `valueNoise()`, trilinear value noise over the
  integer lattice in [-1, 1], seeded: this library's own field, read at a
  POSITION rather than at an index. The per-index mixers it is built on —
  `hash(seed, i)` and the PCG family — are SigilCoreCompute's, included
  from `<sigilcore/compute/Noise.h>` and spelled `core::noise::`; a
  resource key and a text cache fold with the same arithmetic.
- **`path/Numeric.h`** — `kPi`, `kTau`, the degree/radian factors and
  `radians()`/`degrees()` over them (one rounding, where a hand-written
  `deg * 3.14159f / 180.0f` rounds twice), `bisect()` over a predicate
  and `wrap()` into a period.
- **`path/Arrange.h`** — namespace `arrange`. Where item i of n goes when
  a run of things is spread out: `Turn` (`Open` occupies both ends of an
  extent in n−1 steps, `Closed` takes n steps so the last stops short of
  the first), `step()` and `along()` over an extent in the caller's own
  unit — radians round a ring, arc length along a contour — and
  `onEllipse()`, the point at an angle on the ellipse at a centre with a
  radius per axis, with `onRing()` over it for item i's centre. The grid half is `Cell`, `cellAt()` (row-major index to column and
  row), `moduleSize()` (the module that fits columns by rows of itself and
  the gaps between them exactly into a container) and `cellRect()` (the
  rect a block of cells covers, swallowing the gaps it crosses; nothing is
  clamped to a column or row count, because whether landing outside is an
  error or a bleed is the caller's to know). Here rather than in a
  catalog of placements because a catalog is where this arithmetic gets
  spelled a second time, and two spellings of one ring round apart.
- **`path/Conic.h`** — the one curve family measured from a FOCUS rather
  than centred on a box: `r(v) = p / (1 + e cos v)`, with `Conic` carrying
  the focus, the semi-latus rectum, the eccentricity and where the near
  point lies. Eccentricity alone decides which of the four curves it is —
  circle, ellipse, parabola, hyperbola — and `v` is measured from the
  periapsis, so the same angle means the same place on any of them.
  `radiusAt()`, `at()`, `alongAt()` (the direction of travel, which is
  square to the radius on a circle and on nothing else), `outwardAt()` and
  `asymptoteDeg()` (the direction an open branch runs off along).
  `conicPath(conic, span)` samples a span of it, closing only when a
  closed conic goes the whole way round, and `ConicSpan::reach` BREAKS the
  contour where the curve leaves what is near enough to draw, since a path
  carrying a point a million units out is a path whose bounds and arc
  length are decided by somewhere nobody can see. It stands here rather
  than on the kit's shelf because it is not a silhouette in a box: what it
  is measured from is off centre, and a drawing that puts the thing at the
  focus in the middle of the ellipse has said something false.
- **`path/Skia.h`** — `toSk()` and `fromSk()` between `glm::vec2` and
  `SkPoint`, and `centre()` of an `SkRect`.
- **`path/Edges.h`** — narrowing an outline before something is drawn on
  it. `Edge` and `has()`, `edges()` (the sub-contours facing chosen box
  edges, classified against the bounds centre and cut by bisection at
  each run boundary) and `insetOutline()` (a mitred concentric copy;
  positive shrinks). Both take an outline and give an outline. Beside
  them `insetPolygon()` takes a polygon's vertices and gives them back
  moved inward ONE FOR ONE — every edge parallel to its source at the
  distance, inward read off the polygon's own winding — so a caller
  pairing each source corner with its moved one (a chamfer band, a lid
  on a plinth) keeps the correspondence an outline offset cannot give;
  a needle-sharp corner's mitre is capped at a stated number of
  distances, blunting the corner rather than dropping the vertex.
- **`path/Operations.h`** — path operators. Booleans over Skia's pathops
  (`unite` over a pair or over a whole stack — any range of paths: a
  vector, an array, a brace list, or a view that builds them as it is
  walked — `subtract`, `intersect`,
  `exclude`, `simplify`), the OFFSET and the CORNER ROUNDING — one
  operator each, since every side, every join and every selection either
  answers is a dial rather than a name of its own — and four distortions
  as parameter structs you apply on demand: `Roughen` (seeded jitter
  along the normal, each contour drawing from its own stream so adding
  one does not re-roll the others), `Zigzag`, `PuckerBloat`, `Twirl`.
  Each of the four compares dial for dial, so a description holding one
  can be asked whether it is the description it held last frame. None of
  them is a `sigil::geometry::path::ShaperScheme`: a distort is APPLIED,
  and the seam spelling of the same warps is the kit's
  `shapers::Jitter` and `shapers::Zigzag`.
  `PathOperation` and `chain()` compose them, `offsetBy()` adapts `offset` into
  a step. Beside them two treatments that are neither a boolean nor a
  distortion. `chamferCorners()` cuts every line-line corner with a
  straight bevel a stated distance along each leg — the 45-degree face a
  right angle takes, which Skia's corner effect cannot spell because it
  only rounds — clamping the cut to half of each leg so short legs
  degenerate to a diagonal rather than crossing over. It is a POLYLINE
  treatment: a contour holding any curve segment is copied through
  untouched, so a chamfer over an arc is a silent no-op on that contour.
  `displaceSquare()` walks a contour at a fixed wavelength and jumps it
  either side of its normal with vertical steps between — a battlement,
  a meander key, a stepped circuit trace — at a wavelength rounded so a
  whole number of periods fits, which is what keeps a closed mark from
  meeting itself mid-step.

  **STRIP JOINERY** is the third family here, over a `Strip` — a segment
  cut to a width, the piece a lattice, a trellis, a window bar and a
  Voronoi cage are all made of. `stripOutlines()` answers one closed
  contour per piece with its ends cut to the joints it stands in: a node
  is wherever ends meet, the ends there are put in order round it, and
  the seam between each neighbouring pair is the bisector of their two
  directions, so every piece is planed to the same face as the piece
  beside it. Two ends meeting give the corner mitre a picture frame is
  cut to, three or more give each piece a wedge — which is what a lattice
  node actually is — and an end that meets nothing is cut square across.
  `StripOptions::join` picks between the true mitre (bounded by
  `miterLimit`), a bevel that stops each end a half-width short, and a
  round that finishes each end with an arc of its own half-width about
  the node. `strips()` is those outlines united, the lattice as one
  silhouette with its joints closed.

  `stripLaps()` is the other joint: where two pieces CROSS rather than
  meet, which is the half-lap a lattice is held together by. Each
  `StripLap` names the two pieces, the point, each piece's direction and
  the fraction along it, and `halfSpan` — the other piece's width carried
  across at the angle the two cross, which is the seam the piece passing
  over shows, bounded by `lapLimit` so a grazing crossing does not run
  away. Which piece is on top is the caller's: a lattice's layer order is
  not a property of its geometry.

  **`offset(path, distance, OffsetOptions)` is one operator for what an
  outline offset, a concentric frame, a parallel rail and a bolder
  silhouette all are.** `join`, `cap` and `miterLimit` are Skia's stroker
  dials, carried rather than hardcoded. `position` is the dial that makes
  this one operator instead of a family, and it is CONTINUOUS: at 0 the
  answer is the single curve a distance to the LEFT of travel — which is
  `parallel`, exactly — at 1 the curve the same distance to the right,
  and at 0.5 the band that straddles the source, which for a filled
  shape is that shape grown by the distance (or shrunk, at a negative
  one). A band that reaches across the source encloses the source's own
  edge, so what is answered there is the source with the band added or
  taken away; a band lying to one side encloses nothing and is answered
  as itself. `keepCompatible` is the other spelling entirely: the
  source's own nodes are moved along their corner bisectors and their
  handles along their normals, so the answer has the nodes the source
  had, of the same kinds in the same order, and `compatible()` still
  answers `Yes` — which is what an outline offset in a set of masters
  needs and what an outline rebuilt by a stroker can never give. A
  needle-sharp corner's mitre is capped by `miterLimit`, blunting the
  corner rather than dropping the node.

  **`roundCorners(path, radius, CornerOptions)`** is Skia's corner effect
  with nothing set, and with any dial set it is the walk that effect
  cannot do: `minTurnDeg` leaves the shallow corners alone, `outwardOnly`
  reads the contour's own winding and leaves the reflex ones alone, and
  `visual` scales each radius by its corner's angle so every arc stands
  the same distance out from the vertex it replaced — an acute corner
  taking a smaller radius and an obtuse one a larger, which is what stops
  a shallow corner reading as barely rounded beside a sharp one cut by
  the same number. With a dial set it is a POLYLINE treatment: the
  selection and the correction are read off two straight legs, so a joint
  where either side is a curve passes through untouched.
- **`path/Shaper.h`** — `Shaper`, the COMPARABLE `SkPath -> SkPath` value,
  over the `ShaperScheme` concept (`shape()`, equality, an optional
  `bleed()` declaring how far the deviation reaches). It bends one
  continuous mark — a wave, a zigzag, a jitter, an offset. Comparable is
  the point: a consumer that caches drawings proves two frames asked for
  the same deviation and keeps the recording it has, which `operations::PathOperation`
  cannot answer.
- **`path/Profile.h`** — `Profile`, the comparable WIDTH LAW, over the
  `ProfileScheme` concept (`across(along)`, `max()`, equality). `max()`
  is what every cull and bleed is sized from; equality is required
  because a profile is read live. `PxKeyedProfileScheme` declares
  `alongIsPx` for a law keyed in px of arc length rather than in a
  fraction of it — which is what keeps a calligraphic pressure law from
  sliding along a mark as a reveal grows — and `acrossAt(along, lengthPx)`
  is the one call that converts. `path::profile::self()`,
  `path::profile::offset(px)`, `path::profile::taper(startPx, endPx)` and
  `path::profile::spans(upTo, widthsPx)` are the presets that read nothing but
  their own numbers — the boundary, the parallel, the linear run between
  two widths, and the stepped table, which does not interpolate across a
  boundary because what it describes is a measurement that changes at a
  place. Richer families (an oscillating width, a braid built on it) are
  the kit's.
- **`path/Band.h`** — `profileOffset()` walks one rail of a width law;
  `bandRegion()` walks both and closes them per contour, on
  `Formation::Center`, `Inner` or `Outer`. Every rail takes the
  real-vertex repair — arc outside a turn, miter inside — rather than the
  spur a sample-and-displace walk leaves inside every rectangle, whether
  the width is constant or varies; a constant one delegates to `parallel`,
  which answers the same rail to the bit.
  `sweptRegion()` is the OTHER construction of the same band and the two
  differ at a hard turn: it builds no rail, unioning the band's
  cross-sections instead, so the inside of a bend is overlap rather than
  a crossing and the outside is whatever `Sweep::join` says — `SweepJoin`
  being the point, the arc or the chord, the same decision a stroke's
  join is. That vocabulary is the reason it exists, and so is `SweepWidth`,
  a width read off a `SweepStation`: a pen NIB is widest where the spine
  crosses it, which is a function of the tangent that no profile keyed on
  arc length can express. A non-finite width pinches the band to the
  spine rather than deleting the whole mark.
- **`path/Frame.h`** — the two coordinate systems a figure is measured in.
  `PolarFrame` converts `(angle, radius)` into a point, a rect or an
  arc-length fraction IN THE DRAWING'S OWN CONVENTION: `Zero::North` or
  `East`, `Sense::CW` or `CCW`, plus an origin offset. That is the reason
  it is a value — written as a bare `polar()` helper the difference is a
  sign flip and a −90 that every call site repeats. `scaled`, `about` and
  `turned` derive a frame that keeps the convention it came from. `Grid`
  is the unit map: artefact units to canvas px through one scale, an
  origin and an optional snap, `constexpr` so a canvas constant can be
  declared in the artefact's units. `yScale` is the y axis as a multiple
  of that scale — −1 is the MATH FRAME, y counting up the page, which is
  what a plotted function or a surveyed elevation is measured in, and
  anything else is an anisotropic map; a rect comes back sorted either
  way. `centred` is the rect both are read
  through.
- **`path/Projection.h`** — the SPHERE laid onto a plane, and the rotation
  that turns the sphere before it is laid. `Spherical` is a direction in
  degrees (`lonDeg`, `latDeg` — right ascension and declination under the
  sky's names) with `direction()` and `of()` between it and a unit vector,
  `angleBetween()` for how far apart two of them stand and `offsetFrom()`
  for the great-circle step a spherical construction is made of — the
  horizon point at an azimuth, the pole of the circle a chart's own line
  is. `Projection` is the map: a `Scheme` (`Stereographic`,
  `Orthographic`, `AzimuthalEquidistant`, `Equirectangular`, `Mercator`),
  a `centre`, a `scale`, a `rollDeg` and a `Vantage`, with `at()` out,
  `from()` home, `angleFrom()` to cull by, and `radiusAt()`/`arcAtRadius()`
  for the law itself — the RADIUS on an azimuthal map, where the map is
  round and the law holds at every bearing, and the ORDINATE on a
  cylindrical one, where it does not. **The scheme is a field and not five
  functions** because the five differ in one line of arithmetic each and
  agree about the centring, the handedness, the turn and the way back, and
  a caller asking which of them a measured chart was drawn on has to hold
  two of them in variables and swap. `scale` is plane units per radian AT
  THE CENTRE, the one derivative all five share there, so changing the
  scheme leaves the middle of the map the size it was. The plane is y UP
  and isotropic: a chart measured at one number of degrees per centimetre
  across and another down is this under a `Grid`, since a per-axis scale
  here would turn a stereographic's circles into ellipses. `circleOf()` is
  what a stereographic alone can answer — every circle on the sphere is a
  circle on the plane, which is what lets an instrument's whole family of
  them be struck with a compass — and it answers nothing rather than an
  infinity for a circle passing through the point the projection is taken
  FROM, whose image is a straight line; `circleThrough()` beside it is the
  same circle reached the maker's way, through three of its points.
  `Rotation` is the sphere turned: `aboutX/Y/Z`, `zyz` (the three-angle
  form an epoch-to-epoch precession is published in), `then()`,
  `inverse()`, applied to a vector or to a `Spherical`. WHICH angles is
  the caller's — the astronomy of a precession, and a measured artefact's
  own departures from its law, belong beside the artefact.
- **`path/Crossings.h`** — where a set of paths cross each other and who
  is on top there. `discoverCrossings()` takes the strands as a span (or a
  brace list) and finds every PROPER crossing — coincident paths and
  endpoint touches are meetings, not crossings — and numbers them along the
  boundary. `CrossingRule` is the comparable
  answer: list order by default, `crossing::alternate()`,
  `crossing::alternateAlong()`, `crossing::sequence()`,
  `crossing::pairs()` for dominance (cycles
  legal, which is the impossible braid), your own `CrossingScheme`, and
  `except(i, order)` pinning one knot POSITIONALLY. The two alternating
  rules are not the same rule: `alternate()` alternates by DISCOVERED
  ORDINAL, which is arc length along one strand, so every other strand
  meets that numbering in whatever order it happens to;
  `alternateAlong()` is the knot-theoretic weave — walk ANY strand and
  the crossings run over, under, over — and it answers by sorting the
  passes, two per crossing, by strand and then by arc length. It
  therefore needs the whole set before it can answer any of it, which is
  what `CrossingRule::prepare(all)` and the `PreparedCrossingScheme`
  concept are for: a holder that discovers crossings calls it once per
  discovery, and what it works out stays outside equality because it is
  a function of the geometry rather than of the author. A {7/2}
  heptagram is the smallest figure that tells the two apart. `crossingPatch()` is
  the region two marks actually overlap at one knot, bounded by a
  `maxRadius` that is required for correctness rather than a margin:
  without it neighbouring lenses merge and one strand owns half the
  braid.

**`path/blend`** — `SigilGeometryPathBlend`, needs `path`.

- **`path/blend/Blend.h`** — shape interpolation modelled on Illustrator's
  blend tool: `Key`s expand into drawable `Step`s under `Options`
  controlling spacing (`Steps`, `Distance`, `SmoothColor`), an optional
  spine path, orientation, sample density and outline smoothing.
