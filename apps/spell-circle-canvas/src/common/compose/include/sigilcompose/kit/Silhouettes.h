#pragma once

/** @file
 * The silhouette shelf, spelled where compose spells it: the geometry
 * kit's stock generators under `shapes::`, plus the two adaptors that
 * only mean anything against a node — the ones that REPLACE the outline a
 * decoration runs on.
 *
 *     .shape(shapes::star(5))
 *     .decorate(shapes::onEdges(Edge::Top, PathFormat{…}))
 *
 * The generators themselves know nothing of a node: they are values with
 * `path(SkSize)` and `operator==`, and `Element::shape()` takes any of
 * them, or any of yours built the same way. What lives here instead of in
 * geometry is what reads a PaintContext.
 */

#include <sigilcompose/Compose.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Edges.h>

#include <string>
#include <utility>
#include <vector>

namespace sigil::compose::shapes {

/** The stock catalog, which is the geometry library's — every generator,
 *  curve and corner treatment, comparable and callable over a size. */
using namespace ::sigil::geometry::shapes;

/** Which box edges a per-edge treatment applies to. */
using Edge = geometry::path::Edge;
using geometry::path::has;

/** Decoration adaptor: runs @p inner with the PaintContext outline
 *  replaced by the selected edges — any primitive (PathFormat,
 *  ContourWalk, custom programs) becomes a per-edge treatment. */
struct EdgeSlice {
  Edge mask = Edge::All;
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

inline EdgeSlice onEdges(Edge mask, Decoration inner, float step = 3.0f) {
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

/** The sub-contours of a resolved outline that face the selected box
 *  edges — the arithmetic `onEdges` runs, for a consumer that wants the
 *  path rather than the decoration. */
using geometry::path::edges;

}  // namespace sigil::compose::shapes
