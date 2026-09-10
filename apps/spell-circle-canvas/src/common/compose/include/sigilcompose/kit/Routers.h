#pragma once

/** @file
 * The connector routers — Router values for connector().
 *
 * A Router answers the routed path between two endpoint rects, and a
 * RailRouter the same over an ordered run of anchor points. There is no
 * enum of route kinds: these are the stock values, and a caller's own
 * value — or its own callable — is a peer of them.
 *
 * Every value here is COMPARABLE: two routers built from the same
 * parameters compare equal, so a re-described connector or rail prunes
 * and keeps the recording it already made. A raw callable handed to
 * `connector()` or `rail()` is the escape hatch and never prunes.
 *
 * The routed path arrives as the connector's `PaintContext::outline`, so
 * any PathFormat or ContourWalk foreground dresses it.
 */

#include "sigilcompose/Compose.h"

namespace sigil::compose::routers {

/** Where an orthogonal leg takes its turn. `MidX` is the Z every node
 *  graph editor defaults to (half way over, one vertical run, half way
 *  in); `HFirst` and `VFirst` are the two Ls — bend AT the target column
 *  (horizontal out of the source first) or AT the source column (vertical
 *  first). A circuit trace bends at the target column; a flowchart drops
 *  out of the source first. */
enum class Bend { MidX, HFirst, VFirst };

/** WHAT A ROUTE GIVES UP TO THE MARKS THAT STAND ON IT.
 *
 *  A route dressed with a STAMPED brush — `brush::Pattern`'s tiles, a
 *  `brush::Scatter`'s cells — is not a line the marks are laid over: the
 *  marks ARE the route, and a leg whose length is not a whole number of
 *  tiles either stretches them or leaves a seam where the count runs out.
 *  Two numbers say the whole of it, and they are props on the routers
 *  that already exist rather than a family of their own — any orthogonal
 *  route can be laid out for a stamp, and the same route stroked is the
 *  same route.
 *
 *  `endInset` is how much of each TERMINAL leg the route gives up, so the
 *  first tile stands clear of the thing the route leaves rather than
 *  under it. `advance` is the tile pitch: every corner the bend policy
 *  puts on a leg is moved to the nearest whole number of advances from
 *  that leg's start, so the leg carries an exact count. A corner treatment
 *  of its own (`brush::Pattern::cornerLength`) then reserves the elbow's
 *  room out of the two legs it joins, and the side runs come out exact.
 *
 *  Both zero — the default — is every route that is STROKED rather than
 *  stamped: a stroke has no pitch and nothing standing at its ends. */
struct Stamp {
  float advance = 0.0f;
  float endInset = 0.0f;
  bool operator==(const Stamp&) const = default;
};

/** Straight center-to-center line — the connector default, as a named
 *  value for symmetry. */
Router straight();

/** Orthogonal (Manhattan) route: horizontal out of the source, one
 *  vertical run at the midpoint, horizontal into the target. A
 *  positive @p cornerRadius rounds the two turns. */
Router orthogonal(float cornerRadius = 0.0f);

/** Orthogonal route with a bend policy: where the overload above always
 *  bends at midX (a Z), this one also spells the two Ls — see `Bend`.
 *  Collinear points collapse, so an axis-aligned pair emits ONE segment
 *  rather than three with zero-length ends, and the corner is either
 *  rounded (@p cornerRadius, SkCornerPathEffect) or cut at 45°
 *  (@p chamferCut — `geometry::path::ops::chamferCorners`). The two
 *  are alternatives:
 *  chamfer wins when both are set.
 *
 *  The zero-argument `orthogonal()` is NOT this function with defaults. It
 *  emits its degenerate verbs verbatim, and that output is frozen because
 *  existing routes depend on it byte for byte; this is the spelling to
 *  reach for in new code.
 *
 *  @p stamp lays the route out for a stamped brush — see `Stamp`. */
Router orthogonal(Bend bend, float cornerRadius = 0.0f, float chamferCut = 0.0f,
                  Stamp stamp = {});

// ---------------------------------------------------------------------------
// Rail routers (rail(): an ordered run of anchor points → the line's path)

/** The RAIL spelling of the orthogonal family: `rail(stops,
 *  routers::manhattan())`. `orthogonal()` cannot be used here — it is a
 *  pairwise Router and `rail()` takes a RailRouter over the whole anchor
 *  run.
 *
 *  Each consecutive anchor pair runs H/V legs per @p bend; collinear
 *  points collapse, so axis-aligned anchors thread as single clean
 *  segments; corners round with @p cornerRadius or cut at 45° with
 *  @p chamferCut, and chamfer wins when both are set. @p stamp lays the
 *  run out for a stamped brush — see `Stamp`. */
RailRouter manhattan(Bend bend = Bend::MidX, float cornerRadius = 0.0f,
                     float chamferCut = 0.0f, Stamp stamp = {});

/** Adapts any pairwise Router into a RailRouter: consecutive anchors are
 *  routed pairwise (each anchor as a point rect, so center-to-center
 *  routers see the anchor itself) and the legs stitch into ONE contour —
 *  terminal caps and casings fire once at the run's ends, not at every
 *  waypoint. Junction moves are dropped, zero-length segments collapse
 *  and exactly-collinear line runs merge; curve legs (arc()) ride
 *  through untouched. */
RailRouter fromPairwise(Router router);

/** Straight polyline through the waypoints; a positive @p cornerRadius
 *  rounds every turn (SkCornerPathEffect). */
RailRouter polyline(float cornerRadius = 0.0f);

/** The metro-map router: each leg runs a 45° diagonal for the shorter
 *  delta, then finishes straight — every segment ends up horizontal,
 *  vertical, or diagonal (octilinearity, the schematic-map convention);
 *  @p cornerRadius rounds the turns. */
RailRouter octilinear(float cornerRadius = 8.0f);

/** The orbit router: when two consecutive anchors sit at (nearly) the same
 *  radius from `center`, the leg follows the CIRCLE between them — the
 *  short way around — instead of chording across. Radius-changing legs
 *  stay straight spokes. This is how a skill-tree or orbital diagram
 *  reads, where nodes live on concentric rings and their in-ring links are
 *  arcs rather than chords. `tolerance` is the radius-match slack as a
 *  fraction of the radius. */
RailRouter orbit(SkPoint center, float tolerance = 0.05f);

/** Circular-ish bow between the centers: the route's midpoint bulges
 *  off the chord by @p bulge × chord-length (sign picks the side). */
Router arc(float bulge = 0.25f);

}  // namespace sigil::compose::routers
