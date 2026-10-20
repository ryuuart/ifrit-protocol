#pragma once

/** @file
 * Internal to the kernel — the outlines and curves a node resolves once and
 * keeps against what they were resolved from: its glyph outline, the
 * silhouette its coverage traced to and the held parts it was composed
 * from, its custom shape's outline, and its motion path's measured table.
 */

#include <include/core/SkPath.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>
#include <sigilgeometry/path/Contour.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "description/ComposeInternal.h"
#include "paint/HeldCoverage.h"

namespace sigil::compose::detail {

/** THE OUTLINES A NODE KEEPS: one part of the retained node, read off the
 *  instance by each field's own name. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct OutlineCaches {
  // Resolved custom-outline cache: generators (blobs, rounded stars) can be
  // arbitrarily expensive — resolve once per (description, size). Description
  // pointer identity keys invalidation: every patch swaps the description.
  // Element::decorationOutline(Boundary::Glyphs): the union of this text's
  // glyph outlines at the placement its layout produced, resolved once per
  // layout because a decoration asked for it and never otherwise.
  SkPath glyphOutline;
  uint32_t glyphOutlineRev = ~0u;
  // Element::decorationOutline(Boundary::Coverage): the silhouette of what this
  // node's layer drew, traced off that layer's alpha, in the node's own
  // space. Re-traced when the layer that produced it is invalidated —
  // `paintDirty`, which every content, prop and layout change raises on
  // the node and on every ancestor — and on every frame of a subtree whose
  // volatility means nothing about it is cached. The size it was traced at
  // is kept beside it because a resized box is a different silhouette and
  // is the one invalidation that must not depend on a flag.
  SkPath coverageOutline;
  SkSize coverageOutlineSize = {-1.0f, -1.0f};
  /// the rect the trace's raster covered — the node's paint bounds, which
  /// carry the ink standing outside the box and the offset that implies
  SkRect coverageOutlineBounds = SkRect::MakeLTRB(-1, -1, -1, -1);
  float coverageOutlineScale = -1.0f;  ///< the device scale it was traced at
  float coverageOutlineThreshold = -1.0f;  ///< the tolerance it was cut at
  /** The node's OWN paint traced alone, for a silhouette composed from
   *  held parts while only its children move: keyed as the whole trace
   *  is, and re-traced when the node's own paint is invalidated. */
  struct OwnCoverage {
    SkPath outline;
    SkRect bounds = SkRect::MakeLTRB(-1, -1, -1, -1);
    float scale = -1.0f;
    float threshold = -1.0f;
  };
  OwnCoverage coverageOwn;
  /** The held parts the silhouette above was composed from — the own
   *  trace and each moving child's, each with where it stands — or none
   *  where it was traced whole. A decoration cast from the silhouette
   *  casts from these, so what it blurs is held with them. */
  std::vector<CoveragePart> coverageParts;
  // The silhouette a custom shape resolved to, and the shape VALUE it came
  // from. The description it was read off is not an identity: a
  // description freed and rebuilt can land on the address the old one
  // had, and the drawing behind it is a different one. A shape that
  // prunes compares equal to itself and hits; a lambda compares equal to
  // nothing and re-resolves, which is what a lambda already costs.
  SkPath outlineCache;
  SkSize outlineCacheSize = {-1.0f, -1.0f};
  Shape outlineCacheShape;

  /** THE MOTION-PATH TABLE — `Element::travel()`'s measured curve.
   *
   *  Cached against the two INPUTS that determine it, never behind a dirty
   *  flag: the `Shape` VALUE (so a comparable scheme keeps its table across
   *  describes, and the raw-callable escape hatch re-measures exactly as it
   *  re-describes) and the SIZE it was resolved at, which is the parent's
   *  box, so a relayout re-measures. Comparing against what the table was
   *  built FROM cannot go stale the way a "have I run yet" flag can. */
  struct MotionCache {
    Shape shape;                // the value this table was built from
    SkSize size{-1.0f, -1.0f};  // …at this parent size
    std::vector<geometry::path::Contour> contours;
    float total = 0;      // the arc length across all of them
    bool closed = false;  // every contour closed → t WRAPS
  };
  std::unique_ptr<MotionCache> motion;
};

}  // namespace sigil::compose::detail
