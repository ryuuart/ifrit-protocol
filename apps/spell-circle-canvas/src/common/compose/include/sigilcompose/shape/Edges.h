#pragma once

/** @file
 * SigilCompose per-edge extraction — the sub-contours of a resolved outline
 * that face a box edge, and the decorations that run on them: `onEdges`,
 * `inset` and the arrow.
 */

#include <include/core/SkPathBuilder.h>

#include <cstdint>

#include "sigilcompose/Compose.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::compose::shapes {

// ---------------------------------------------------------------------------
// Per-edge extraction

enum class Edge : uint8_t {
  Top = 1,
  Right = 2,
  Bottom = 4,
  Left = 8,
  All = 15,
};
constexpr Edge operator|(Edge a, Edge b) {
  // the type is a bit set; any union of enumerators is a valid value
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return Edge(uint8_t(a) | uint8_t(b));
}
constexpr bool has(Edge mask, Edge e) {
  return (uint8_t(mask) & uint8_t(e)) != 0;
}

/** Extracts the sub-contours of @p outline that face the selected box
 *  edges. Facing is classified against the outline's bounds center
 *  (diagonal split, so rounded-rect corner arcs divide naturally
 *  between their two edges). Exact geometry via SkContourMeasure
 *  segment extraction; @p step is the classification sampling length
 *  in px. */
SkPath edges(const SkPath& outline, Edge mask, float step = 3.0f);

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
 *  Positive `px` shrinks; negative grows. Implemented as a stroke-and-fill
 *  offset of the resolved outline, so it follows any silhouette — a
 *  chamfered panel, a star, a blob — not just rectangles. */
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

/** An arrow along +x, inscribed in the box: a shaft of `shaftFrac` of the
 *  height and a head of `headFrac` of the width. */
struct Arrow {
  float shaftFrac = 0.34f;
  float headFrac = 0.42f;
  bool operator==(const Arrow&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Arrow arrow(float shaftFrac = 0.34f, float headFrac = 0.42f) {
  return Arrow{shaftFrac, headFrac};
}

}  // namespace sigil::compose::shapes
