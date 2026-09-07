# Merge-readiness review — SigilGeometry path ops (sub-pass)

Read-only pass over `src/common/geometry/path` (delegated by the
geometry+material+world reviewer; delivered directly). Paths relative to
`apps/spell-circle-canvas/`. Category in brackets: 1 correctness,
2 API, 3 README drift, 4 comment rules, 5 leftovers, 6 duplication,
7 file size, 8 test gaps. SigilMaterial was NOT reviewed by any pass.

## Findings, most severe first

- blocker | `src/common/geometry/path/Ops.cpp:354` | `roundCorners` default branch builds into a fresh `SkPathBuilder dst`, and `SkCornerPathEffectImpl::onFilterPath` never sets a fill type, so an even-odd source (a donut, a glyph with counters) comes back `kWinding` and fills solid | carry `path.getFillType()` into the builder, as the `selectedCorners` branch at Ops.cpp:211 does | `SkPathBuilder dst(path.getFillType());` [1]
- blocker | `include/sigilgeometry/path/Cells.h:34` | declares `enum class Edge : uint8_t {Clamp,Wrap,Constant}` in `sigil::geometry::path`, a redefinition of `enum class Edge {Top,Right,Bottom,Left,All}` at `path/Edges.h:26` in the same namespace — the two public headers can never be included in one TU | rename Cells.h's to `Boundary` (or nest it inside `Cells`) [2]
- blocker | `include/sigilgeometry/Geometry.h:9` | says "Every public header of SigilGeometry in one include" but lists 8 of the 29 `path/` headers — Arrange, Band, Cells, Crossings, Direction, Edges, Extremes, Fit, Frame, Hull, Interpolate, Lattice, Neighbours, Profile, Scatter, Segments, Shaper, Stride, Symmetry, Tidy, Trace, Triangulate and kit/Hatches.h are absent | list every public header after fixing the `Edge` collision, or restate what the file is [3]
- should-fix | `path/Polyline.cpp:246` | `sample()` steps uniformly in the parameter and emits `count + 1` points, while Polyline.h:74 and README.md:366 say "`count` points spaced evenly by arc length"; PolylinesTest.cpp:97 asserts 65 points for 64 | reword both docs to "count + 1 points spaced evenly in the parameter", or make the code arc-length-uniform [3]
- should-fix | `path/Extremes.cpp:164` | when `SkConic::chopAt` fails, `out[1]` stays the whole conic and `out[0]` keeps the original end, so the split emits the curve twice | on chop failure emit the piece once and stop splitting it (single-piece signal, `continue` in `extremes()`) [1]
- should-fix | `path/Tidy.cpp:63` | the duplicate-node guard `kept.size() + 1 < pieces.size()` is order-dependent: a zero-length last piece is never dropped, an identical first piece is, and an all-degenerate contour disappears | guard on the surviving count: `kept.size() + remaining > 2` [1]
- should-fix | `path/Polyline.cpp:482` | `catmullRom` never sets `out.closed` and never wraps the seam chord, so a closed ring of controls comes back open and cut | `out.closed = controls.closed;` and index `p1/p2/p3` modulo `n` when closed [1]
- should-fix | `path/Ops.cpp:385` | `Zigzag` phases on `i / n`, right for a closed contour but leaving an open contour's endpoint off the source curve, which `Contour::displace` promises the opposite of | `denom = samples.closed ? n : n - 1` [1]
- should-fix | `path/Scatter.cpp:433` | the `hold` comment says a pushed-out point is put back, but the code clamps to the bounding box; the restore is at Scatter.cpp:443 outside the loop | move the sentence to the restore site [4]
- should-fix | `path/Scatter.cpp:115` | `nextBase` is documented as the next prime but falls through to `base + 2` past 43, so a Halton scatter with `parameter` above 43 gets two correlated axes | extend the table or state the fallback and clamp `parameter` [4]
- should-fix | `path/Band.cpp:127` | "so two concentric ring spines came out as a filled disc" — past-defect narrative | state the constraint: a single chain closed once bridges between contours with a filled chord [4]
- should-fix | `path/Crossings.cpp:322` | "is what the exact form replaced" — history | drop the clause [4]
- should-fix | `path/Band.cpp:199` | "If that ever bites, the honest fix is a `constant()` query on the Profile seam" — a TODO in prose | delete the sentence [4]
- should-fix | `include/sigilgeometry/path/Ops.h:21` | "Distorts run over the Geometry.h resampling currency" cites a file and is wrong (`Sampled`/`resample` live in Polyline.h) | "Distorts run over the resampled-polyline currency, so they respect contours and closure" [4]
- should-fix | `path/Noise.cpp:12` | `path::valueNoise` is a byte-for-byte copy of `core::noise::valueNoise` (`sigilcore/compute/Field.h:159`) | `return core::noise::valueNoise(seed, p.x, p.y, p.z);` [6]
- should-fix | `path/Crossings.cpp:80` | an anonymous `flatten(const SkPath&) -> Flat` re-implements flattening plus a cumulative length table beside `path::flatten` (Polyline.cpp:193) and `Polyline::length()` | build `Flat` from a `Polyline` and a prefix sum [6]
- should-fix | `path/Symmetry.cpp:52,37`, `Symmetry.h:51`, `Frame.h:129` | hand-spelt `180/π`, `6.28318531f`, `0.01745329251994329577f` where Numeric.h owns `degrees()`, `kTau`, `kDegToRad` | use the library's constants [6]
- should-fix | `path/Polyline.cpp:485` | `Sampled lerp(const Sampled&, const Sampled&, float)` is declared, defined, referenced by nothing, untested | delete or cover it [5]
- should-fix | `path/Polyline.cpp:93` | `Polyline::centroid()` is referenced nowhere (only `Sampled::centroid` is called), untested, absent from the README | delete, or document and test [5]
- should-fix | `path/Ops.cpp:444` | `chamferCorners` has no test (curve pass-through, half-leg clamp, closing vertex, straight-through vertex) | a suite in OpsTest per documented behaviour [8]
- should-fix | `path/Ops.cpp:570` | `displaceSquare` has no test (whole number of periods; closed mark not meeting itself mid-step) | assert the period count on a closed circle [8]
- should-fix | `include/sigilgeometry/path/Extremes.h:33` | `Where::MaxCurvature` is never exercised (NodesTest covers Axis and Inflection) | a quad/cubic with a hand-known curvature peak [8]
- should-fix | `include/sigilgeometry/path/Neighbours.h:175` | `relax()`/`Relaxation` have no direct test; "two coincident points are left alone" and `hold` are unasserted | add the two cases [8]
- should-fix | `include/sigilgeometry/path/Tidy.h:30` | `TidyOptions::duplicates` has no test, which is why the trailing-degenerate defect went unnoticed | a zero-length piece at front, middle and end [8]
- should-fix | `path/test/BlendTest.cpp:56` | the move into path/test dropped `Blend.OklabMidGrayIsPerceptual`; `blend::detail::lerpOklab` is covered by nothing | restore the case [8]
- should-fix | `path/Crossings.cpp:124,219` | `changesSides` probes a fixed 3 px and duplicates are merged within a fixed 1.5 px box, neither scaled to the geometry, so tiny figures miss or merge crossings | derive both from the flatten step [1]
- should-fix | `include/sigilgeometry/path/Fit.h:39`, README.md:427 | "a closed run is fitted as one loop, so the seam is a node like any other", but Fit.cpp:229 appends the first point and fits an open run, so the seam is a corner | reword, or make the fit wrap [3]
- should-fix | `include/sigilgeometry/path/Ops.h:205,214` | `PuckerBloat`/`Twirl` documented "about the shape's centroid" but `overSamples` (Ops.cpp:52) warps each contour about its own centroid | reword, or compute one centroid over the whole path [3]
- should-fix | `include/sigilgeometry/path/Lattice.h:29` | `LatticeOptions` is the only options struct in the leaf without `operator==` | add the defaulted comparison [2]
- should-fix | `include/sigilgeometry/path/Ops.h:186` | `Roughen` carries `uint32_t seed` and a `Source` but no `parameter`, so Halton/Stratified sources are unusable; `Distribution` spells the pair as `uint64_t seed` + `uint32_t parameter` | make the two agree [2]
- should-fix | `include/sigilgeometry/path/Neighbours.h:80` | `within` has a `glm::vec2` overload, `nearest`/`nearest(p,k)`/`nearestOther` do not | give the family one overload set [2]
- should-fix | `path/Neighbours.cpp:79` | a non-finite coordinate makes the bounds NaN and `(int)std::floor(NaN / cell)` is undefined | skip non-finite points; fall back to `{1,1,1}` [1]
- should-fix | `path/Scatter.cpp:216` | when the lattice cell count exceeds `cap * kAttemptsPerPoint`, `latticePoints` answers an empty vector instead of fewer points (thin diagonal sliver at `Rate::Count`) | drop the early return; rely on the cap break at 237 [1]
- should-fix | `path/Triangulate.cpp:142` | the degenerate-Voronoi branch is all-pairs and its comment claims affordability, but a thousand collinear points reach it | cap the degenerate path at a stated count, or drop the claim [4]
- should-fix | `path/Hull.cpp:84` | the ring walk breaks on a vertex with no outgoing boundary edge yet pushes the partial chain with `closed = true` | keep a ring only when the walk returned to `start` [1]
- should-fix | `path/Segments.cpp:199` | `startedAt` guards on `contour.segments.size() < 2`, but a one-segment closed contour has two cycle pieces and is restartable | build the cycle first, guard on `cycle.pieces.size()` [1]
- should-fix | `path/Ops.cpp:50` (also `Ops.cpp:446,571`, `Edges.cpp:34`, `Band.cpp:141,208`) | `overSamples`, `chamferCorners`, `displaceSquare`, `edges`, `profileOffset`, `bandRegionImpl` all build into a default `SkPathBuilder`, dropping the fill type — the `roundCorners` blocker's class on every rebuilding operator | seed each builder from the source path [1]
- should-fix | `path/Ops.cpp` (604 lines) | four subjects: pathops booleans, the offset family, the corner treatments, the resample distorts | `Ops.cpp`, `Corners.cpp`, `Distorts.cpp` [7]
- should-fix | `path/bench/PathBench.cpp` (766 lines) | one bench file over thirteen subjects | split as path/test is [7]
- nit | `path/Ops.cpp:499` | "NOT named `emit`: this header reaches Qt TUs" sits in a .cpp with no Qt | delete [4]
- nit | `path/Ops.cpp:16` | `#include <sigilcore/compute/Noise.h>` unused | delete [5]
- nit | `path/Symmetry.cpp:39` | `spokes == 1` inside a branch that established `spokes > 1` | drop the dead disjunct [5]
- nit | `path/Scatter.cpp:50` | `Inside` holds a raw `const Region*`; `Inside(Region::of(path))` dangles | delete the rvalue path or hold by value [2]
- nit | `path/Trace.cpp:42` | `minSpeed` (a field-magnitude bound) is also compared against `move`, an average of unit vectors | a separate agreement threshold [1]
- nit | `path/Polyline.cpp:246` | `sample()` with `closed = true` duplicates the seam vertex; `subdivide` (441) enforces the opposite | drop the last point when closed [2]
- nit | `include/sigilgeometry/path/Polyline.h:35` | `length()`, `centroid()`, `reverse()` absent from README.md:364-383 | list or drop [3]
- nit | `include/sigilgeometry/path/Arrange.h:61` | `onEllipse()` public, absent from README.md:590 | add [3]
- nit | `include/sigilgeometry/path/Ops.h:84` | `keepCompatible` silently ignores `position` and `step` (Ops.cpp:318) | say so [3]
- nit | `include/sigilgeometry/path/Frame.h:128` | member `Frame::radians(float deg)` (frame degrees to screen angle) beside free `path::radians(float)` (a factor) | `screenRadians` [2]

## Counts

- blocker: 3
- should-fix: 36
- nit: 10
