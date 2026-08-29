#pragma once

/** @file
 * SigilCompose derive family — content that asks where a keyed node landed:
 * connector and rail, with Router and RailRouter as their pluggable seam;
 * Anchor, a normalized point on a keyed node's bounds; band, a shape swept
 * out by an authored or borrowed spine; bandPointAt, the one statement of
 * the band's across sign; and the `derive::` namespace that gathers the
 * family under one name.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sigilcompose/Element.h"
#include "sigilcompose/Shape.h"
#include "sigilcompose/Stroke.h"

namespace sigil::compose {

/** A relationship as a first-class element: a path routed between two
 *  keyed nodes' resolved bounds, stroked by the connector's foreground
 *  decorations (attach a PathFormat — the routed path arrives as
 *  PaintContext::outline). Straight line by default; supply a router
 *  for anything else. Position it absolute().inset(0) over the nodes
 *  it connects.
 *
 *  `gap` is `Anchor::gap` under another name: it pulls each END of the
 *  routed path back along itself by that many px, clamped so short routes
 *  keep a visible run. Routes run centre to centre, and a node's box can
 *  be much larger than its visible shape — under `sdf::` chrome, for
 *  instance — so the gap is how a wire stops at the glow instead of
 *  piercing it. */
using Router = std::function<SkPath(const SkRect& from, const SkRect& to)>;
Element connector(std::string_view fromKey, std::string_view toKey,
                  Router router = {}, float gap = 0.0f);

/** A rail endpoint/waypoint: a NORMALIZED point on a keyed node's resolved
 *  bounds ((0,0)=top-left, (1,1)=bottom-right — the binding form tldraw and
 *  Excalidraw both converged on; never absolute coordinates, so rails
 *  survive layout, drag, and reflow). `gap` pulls a TERMINAL anchor back
 *  along its segment (breathing room at the ends; ignored on waypoints). */
struct Anchor {
  std::string nodeKey;
  SkPoint norm = {0.5f, 0.5f};
  float gap = 0.0f;
  bool operator==(const Anchor&) const = default;
};

/** Routes an ordered run of resolved anchor points into the rail's path —
 *  stock ones in <sigilcompose/Routers.h> (polyline, octilinear); write your
 *  own for anything else. Straight polyline when omitted. */
using RailRouter = std::function<SkPath(std::span<const SkPoint>)>;

/** The component that IS a line: a path threaded through an ordered span of
 *  anchors (a transit line through its stations, a wire through ports),
 *  resolved in the derive phase and re-routed whenever an anchored node
 *  moves. The routed path becomes PaintContext::outline, so PathFormat
 *  strokes, ContourWalk stamps and span masks all dress it — a rail with
 *  `.mask(by::spans(spans::upTo(with(1.0f, {800ms}))))` DRAWS ITSELF.
 *  Position it absolute().inset(0) over the nodes it threads, as with
 *  connector(). */
Element rail(std::vector<Anchor> anchors, RailRouter router = {});

/** A BAND: the shape a spine sweeps out at a given width across it.
 *
 *      band(shapes::circle(), across(22)).inward().fill(brass)
 *      band(around("dial"), across(14)).stroke(spans::edges(6), rule)
 *
 *  It is an ordinary element in every way that matters — it lays out,
 *  hosts children, fills, clips and takes stroke passes like any other
 *  shape. What it adds is that its shape is DERIVED: it owns an
 *  (along, across) space over its spine, `along` a fraction of arc length
 *  and `across` px on the normal (see bandPointAt for the sign), and
 *  `across(...)` takes a Profile, so a taper is the same value a strand
 *  or a ribbon width uses.
 *
 *  IT DOES NOT HIT-TEST AS ITS SHAPE. Hit testing consults the node's own
 *  shape value, and a band's silhouette is derived rather than set there,
 *  so a band hits as its LAYOUT BOX — a wider region than the mark you can
 *  see.
 *
 *  An authored spine is a `Shape`, exactly like shape()'s value: a
 *  comparable generator (any `shapes::` value) prunes; a raw callable is
 *  the escape hatch that never compares equal — memo() such a node, or
 *  hold the Shape value stable, to prune it. A borrowed spine
 *  (`around(key)`) is a comparable value and prunes on its own.
 *
 *  Formation is explicit: `.centered()` (the default) straddles the
 *  spine, `.outward()` and `.inward()` take one side. The spine is guide
 *  DATA, never an element — a path participates as an element's shape, as
 *  borrowed geometry (`around(key)`, resolved in the derive phase), or as
 *  pure guide data in no tree, and this is the third case.
 *
 *  The profile's `max()` is what the paint cull grows by, so a band whose
 *  width varies is never silently clipped. */
Element band(Shape spine, Across width);
Element band(Around spine, Across width);

/** The band's own (along, across) space, addressable: `along` is a
 *  fraction of the spine's total arc length, `across` is px on the normal.
 *
 *  **Positive `across` is to the LEFT of travel**, which in screen space
 *  (y down) is OUTSIDE a clockwise path — SkPath's own direction for rects
 *  and circles, so `.outward()` exits the shape.
 *
 *  THIS IS THE ONE STATEMENT OF THAT CONVENTION for the whole library.
 *  `Profile::across`, `strand::offset`, `geometry::parallel`,
 *  `lines::Rail::across`, `kit::brush::shapers::offset` and
 *  `TextPath::offset` all mean this same side. Anything placing content on
 *  a band reads it here, so the placement and the band's own geometry
 *  cannot disagree. */
SkPoint bandPointAt(const SkPath& spine, float along, float acrossPx);

// ---------------------------------------------------------------------------
// The DERIVE family, gathered under one word

/** Everything that asks "where did that keyed node land, and give me more
 *  content because of it" — the DERIVE phase.
 *
 *  Its members are `flowAround`, `connector` and `rail` (with `routers::`
 *  as their pluggable seam), `band(around(key))`, `spans::fit(key)` and a
 *  decoration's `strand::from(key)`. Six spellings, one mechanism; the
 *  aliases below exist so it can be found under one name.
 *
 *  THE RULES THEY SHARE — one flat edge store, walked once per render:
 *
 *   1. **AN UNKNOWN KEY IS SILENT, across the whole family.**
 *      `flowAround("typo")`, `spans::fit("typo")`, `around("typo")`, a
 *      connector naming a node that is not in the tree — each resolves to
 *      nothing and draws nothing, with no diagnostic. A misspelled key
 *      looks exactly like a feature you did not write.
 *   2. **ONE SECOND PASS, cycle-guarded.** Backward influence inside a
 *      frame is this declared exception and nothing else: derive answers
 *      are computed from the FIRST layout and fed to at most one more
 *      pass. A borrow that would close a cycle is dropped, not chased.
 *   3. **The answer can lag by a frame** when the borrowed node's own
 *      geometry only settles during that layout, so a borrow taken on the
 *      very first frame may resolve against a not-yet-final rect.
 *   4. **Flat, not recursive.** Routed nodes and flowing text are flat
 *      lists in tree order plus a back-index from anchor key to routes, so
 *      a tree with no derived content pays nothing and `routesAt(key)`
 *      answers in time proportional to the routes at that node.
 */
namespace derive {
/** A relationship as an element — see connector() above. */
using sigil::compose::connector;
/** A path threaded through anchors — see rail() above. */
using sigil::compose::rail;
/** A spine borrowed from a keyed element — `band(derive::around("dial"),
 *  across(14))`. */
using sigil::compose::around;
/** The family's text member as a free verb: `derive::flowAround(el,
 *  "fig", 8)` == `el.flowAround("fig", 8)`. The method is the ergonomic
 *  form, since it chains; this exists so the whole family can be found
 *  under one name. */
inline Element flowAround(Element el, std::string_view key,
                          float margin = 0.0f) {
  el.flowAround(key, margin);
  return el;
}
}  // namespace derive

}  // namespace sigil::compose
