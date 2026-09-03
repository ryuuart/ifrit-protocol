#pragma once

/** @file
 * SigilCompose decoration adaptors — the two mechanisms that run a
 * decoration on ANOTHER OUTLINE than the node's own: `onEdges`, against
 * only the sub-contours facing chosen box edges, and `inset`, against a
 * concentric copy of the outline. Each rewrites `PaintContext::outline`
 * and delegates, so any primitive — a PathFormat, a ContourWalk, a
 * custom program — becomes a per-edge or a nested treatment.
 *
 *     .shape(geometry::shapes::star(5))
 *     .foreground(onEdges(geometry::path::Edge::Top, PathFormat{…}))
 *
 * The silhouettes themselves are SigilGeometry's
 * (`<sigilgeometry/kit/Silhouettes.h>`): values with `path(SkSize)` and
 * `operator==`, which `Element::shape()` takes.
 */

#include <sigilcompose/Compose.h>
#include <sigilgeometry/path/Edges.h>

#include <string>
#include <utility>
#include <vector>

namespace sigil::compose {

/** Decoration adaptor: runs @p inner with the PaintContext outline
 *  replaced by the selected edges — any primitive (PathFormat,
 *  ContourWalk, custom programs) becomes a per-edge treatment. */
struct EdgeSlice {
  geometry::path::Edge mask = geometry::path::Edge::All;
  Decoration inner{PaintProgram{}};
  float step = 3.0f;

  /** Forwarded, or an inner weave's strand::from(key) would never be
   *  registered for the derive pass (BorrowingDecoration). */
  std::vector<std::string> borrows() const { return inner.borrows(); }
  float reach() const { return inner.reach(); }

  void paint(SkCanvas& canvas, const PaintContext& ctx) const;
  bool isAnimated() const { return inner.isAnimated(); }
  /** Structural equality, so a static per-edge border prunes like any
   *  other decoration. Without it every `onEdges(...)` compares unequal
   *  and re-records its subtree on every describe, redoing the edge
   *  extraction — a contour walk with a binary search at each run boundary
   *  — for chrome that never changed. Any adaptor added beside this one
   *  needs the same operator for the same reason. */
  bool operator==(const EdgeSlice& o) const {
    return mask == o.mask && step == o.step && inner == o.inner;
  }
};

inline EdgeSlice onEdges(geometry::path::Edge mask, Decoration inner,
                         float step = 3.0f) {
  return EdgeSlice{mask, std::move(inner), step};
}

/** Runs a decoration against an INSET (or outset) copy of the node's
 *  outline — EdgeSlice's sibling, and the same trick: rewrite
 *  `PaintContext::outline` and delegate.
 *
 *  "The same bevel again, six pixels in" is the whole vocabulary of nested
 *  chrome, and without this every nested frame is either a second element
 *  or a bespoke decoration struct.
 *
 *  Positive `px` shrinks; negative grows. The offset follows any
 *  silhouette — a chamfered panel, a star, a blob — not just rectangles. */
struct Inset {
  float px = 0;
  Decoration inner{PaintProgram{}};

  /** Forwarded, or an inner weave's strand::from(key) would never be
   *  registered for the derive pass (BorrowingDecoration). */
  std::vector<std::string> borrows() const { return inner.borrows(); }
  float reach() const { return inner.reach(); }

  void paint(SkCanvas& canvas, const PaintContext& ctx) const;
  bool isAnimated() const { return inner.isAnimated(); }
  bool operator==(const Inset& o) const {
    return px == o.px && inner == o.inner;
  }
};

inline Inset inset(float px, Decoration inner) {
  return Inset{px, std::move(inner)};
}

}  // namespace sigil::compose
