#pragma once

/** @file
 * The masking family at paint. A mask is (selection, gate), and the gates
 * fall into two mechanical classes; the split is the whole implementation:
 *
 *  - SPANS cuts the BOUNDARY. It rewrites the path the selected
 *    outline-tracing outputs trace — the surface (fill + echoes) and the
 *    marks. Content and children do not trace a boundary, so a spans gate
 *    over them is not a picture and does nothing.
 *  - EDGE / SHAPE / COVERAGE cut the PLANE. They wrap the selected outputs
 *    in a canvas clip (edge, shape) or a kDstIn/kDstOut coverage layer
 *    (alpha and luma, each with its complement).
 *
 * Both classes intersect for free, and across each other: span sets
 * intersect as interval arithmetic, nested clips intersect by definition,
 * stacked kDstIn layers multiply coverage. Where a mask selects EVERYTHING
 * the plane gates are hoisted to wrap the whole node once rather than each
 * group — cheaper, and the only way a nested pair of antialiased clips
 * cannot compound its own edge.
 */

#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkRect.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "runtime/ComposeRuntime.h"

class SkCanvas;

namespace sigil::compose {

/** Does this span set claim the whole boundary? Then the boundary is
 *  untouched, and returning the source path unchanged is required, not an
 *  optimisation: a fully settled reveal must draw exactly the path it would
 *  have drawn with no mask on it at all, bit for bit, or adding a mask that
 *  is currently showing everything moves pixels. */
inline bool claimsEverything(const std::vector<Span>& show) {
  return show.size() == 1 && show[0].begin <= 1e-6f &&
         show[0].end >= 1.0f - 1e-6f;
}

/** Apply a resolved SHOW set to a boundary. `cut`, when asked for, says
 *  the geometry actually changed — which only the SURFACE needs, because
 *  only the surface has a cheap rrect to fall out of. Decorations always
 *  draw a path. */
inline SkPath gateOutline(const SpanArithmeticOperations* arith,
                          const SkPath& src, const std::vector<Span>& show,
                          bool* cut = nullptr) {
  if (claimsEverything(show)) return src;
  if (cut) *cut = true;
  if (show.empty()) return SkPath();
  return arith ? arith->spanPath(src, show) : src;
}

/** The shape a gated boundary was cut FROM — `PaintContext::silhouette` —
 *  or an empty path when the gate cut nothing and the boundary IS that
 *  shape. A span gate narrows the boundary to the run shown so far, and a
 *  partial run is OPEN: it bounds no area. An Inner- or Outer-aligned
 *  stroke clips to what the shape encloses, so on the run alone it clips
 *  to nothing while the run is one straight segment and to whatever the
 *  run's implicit closure encloses once it turns a corner. Carrying the
 *  uncut shape keeps an alignment meaning one thing at every fraction of a
 *  reveal: the half of the stroke's width inside — or outside — the shape,
 *  along the part of the boundary that is shown. */
inline SkPath gateSilhouette(const SkPath& src, const std::vector<Span>& show) {
  return claimsEverything(show) ? SkPath() : src;
}

/** THE BOUNDARY GATES of a node's masks, resolved once per paint: the SHOW
 *  set each boundary-tracing output is left with, as fractions of the
 *  whole boundary, and the arithmetic they were cut and intersected with.
 *
 *  Two outputs trace a boundary — the SURFACE (fill + echo re-stamps) and
 *  the MARKS (every decoration, every span pass) — so at most two cut
 *  paths exist, and in the overwhelmingly common case
 *  (`mask(by::spans(…))`, the whole node) they are the same path and are
 *  computed once. */
struct BoundaryGates {
  /** THE SPAN ARITHMETIC the boundary is cut and intersected with — the
   *  brush engine the node's stroke passes carry, or failing that the one
   *  its first spans gate carries. Absent only on a node that has neither,
   *  which then has nothing to cut. */
  const SpanArithmeticOperations* arithmetic = nullptr;
  /** The SHOW set each of surface / marks is left with, as fractions of
   *  the whole boundary — absent when no spans gate selects it. */
  std::optional<std::vector<Span>> surfaceShow, marksShow;
  /** …and the per-NAMED-mark refinement, for masks that address one
   *  label. */
  std::vector<std::pair<std::string, std::vector<Span>>> namedShow;

  /** The runs both sets cover; @p a unchanged where there is no
   *  arithmetic to intersect with. */
  [[nodiscard]] std::vector<Span> intersect(const std::vector<Span>& a,
                                            const std::vector<Span>& b) const {
    return arithmetic ? arithmetic->intersect(a, b) : a;
  }
};

/** Resolves each SPANS gate of @p masks (null where the node has none)
 *  against @p outline, the node's uncut boundary. @p gateValues are the
 *  masks' resolved animatable values in declaration order, each mask
 *  taking `valueCount()` of them. Stacked gates over one target narrow
 *  each other, never widen. */
BoundaryGates resolveBoundaryGates(detail::Instance& inst,
                                   const std::vector<Mask>* masks,
                                   const std::vector<float>& gateValues,
                                   const SkPath& outline);

/** THE PLANE GATES of a node's masks, opened and closed around the parts
 *  they select. `by::edge` and `by::shape` are canvas clips; `by::alpha`
 *  and `by::luma` are coverage layers through kDstIn, their complements
 *  through kDstOut.
 *
 *  One entry/exit pair. Region gates go on first (they commute with the
 *  coverage layers, so the order between the two kinds is free and this
 *  one keeps the layer pops simple); coverage layers are popped in
 *  reverse, each drawing its Material's alpha through kDstIn. */
class PlaneGates {
 public:
  /** The plane gates of @p masks (null where the node has none), with an
   *  edge gate's fraction read from @p gateValues. Everything named here
   *  is held by reference and must outlive the gates. */
  PlaneGates(Composer::Impl& impl, detail::Instance& inst, SkCanvas& canvas,
             const std::vector<Mask>* masks,
             const std::vector<float>& gateValues, const SkRect& bounds,
             const SkPath& outline, const PaintContext& paintContext);

  /** Does any plane gate select less than the whole node, so that each
   *  part opens the gates addressing it? */
  [[nodiscard]] bool granular() const { return granularPlane; }
  /** How many coverage layers stand open: what `leave` closes back to. */
  [[nodiscard]] size_t coverDepth() const { return coverStack.size(); }
  /** Opens every gate addressing @p part (a mark by its @p label), or every
   *  whole-node gate under @p wholeNode; answers the save count to
   *  restore to, or -1 where no gate opened. */
  int enter(bool wholeNode, Parts::Bits part, std::string_view label);
  /** Closes what `enter` opened: the coverage layers above @p coverBase,
   *  then the saves down to @p base. */
  void leave(int base, size_t coverBase);

 private:
  struct PlaneGate {
    const Mask* mask = nullptr;
    float fraction = 1.0f;  // Edge
  };
  Composer::Impl& impl;
  detail::Instance& inst;
  SkCanvas& canvas;
  const SkRect& bounds;
  const SkPath& fullOutline;
  const PaintContext& paintContext;
  std::vector<PlaneGate> plane;
  bool granularPlane = false;
  std::vector<SkPaint> coverStack;
};

}  // namespace sigil::compose
