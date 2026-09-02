#pragma once

/** @file
 * SigilCompose geometry ops — the one mechanism door for deviating an
 * outline: the raw `ops::` callables and the GeometryOp that a comparable
 * shaper and a raw op both convert to.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPicture.h>
#include <sigilcompose/brush/Decorations.h>  // PathSample
#include <sigilcompose/brush/Lines.h>        // lines::displace (the wave op)
#include <sigilcompose/kit/Silhouettes.h>

#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "sigilcompose/Compose.h"

namespace sigil::compose {

// ---------------------------------------------------------------------------
// THE ONE MECHANISM DOOR
//
// Geometry deviation has one comparable seam — `Shaper`, any value with
// `SkPath shape(const SkPath &) const` and equality. Stock shapers are
// `geometry::shapes::`, and writing your own is a few lines; both prune.
//
// A raw lambda cannot be a Shaper, because a closure has no equality. That
// capability is still reachable, through exactly one door —
// `brush::restyle(op, decoration)` — and it is priced accordingly: it never
// prunes. Reach for it only when no comparable value can say what you mean.

namespace ops {

/** THE ESCAPE HATCH: a path→path geometry op as a raw callable — our
 *  SkPathEffect-shaped extension point, because Skia's own subclassing
 *  seam is sealed in its public API. It can do anything, and it can never
 *  prune: an incomparable callable compares conservatively unequal, so a
 *  node wearing one re-records every render (memo the host, or keep it
 *  pointer-stable).
 *
 *  Reach for it only when no `kit::brush::shapers` value and no shaper you
 *  could write yourself can say what you mean — a shaper is a comparable
 *  struct with `SkPath shape(const SkPath &) const` and writing one is
 *  four lines. Chain lambdas with chain(); apply to any decoration with
 *  `brush::restyle()`, which is the only thing that takes one. */
using PathOp = std::function<SkPath(const SkPath&)>;

/** Print the path's contour census — count, lengths, closedness, bounds —
 *  and pass the path through unchanged. Drop it in at any position in a
 *  pipeline to see what that stage was handed. Being a pass-through, it
 *  changes no pixels; being a raw op, it does cost the node its pruning
 *  while it is there. */
PathOp debug(const char* tag = "brush");

/** Chain raw ops left-to-right, each fed the previous one's output. The
 *  comparable equivalent is simply a list of shapers: `Brush::shaped()`
 *  appends to the shared pipeline, and each `.layer()` carries its own
 *  suffix list. */
PathOp chain(std::vector<PathOp> steps);

}  // namespace ops

/** What `brush::restyle()` carries: EITHER a comparable `Shaper`, so a
 *  restyle of a stock shaper still prunes, OR a raw `ops::PathOp`, which is
 *  conservatively unequal forever. A bare lambda literal must be assigned
 *  to an `ops::PathOp` first, because two user-defined conversions do not
 *  chain.
 *
 *  Nothing else takes one: `Brush`'s pipeline and its per-layer suffixes
 *  are plain `Shaper` lists. */
class GeometryOp {
 public:
  GeometryOp(ops::PathOp fn)  // NOLINT: escape hatch, never prunes
      : m_apply(std::move(fn)) {}
  /** Any shaper VALUE, directly — `restyle(shapers::Wave{...}, dec)`. The
   *  hop through Shaper cannot be implicit (two user-defined conversions
   *  do not chain), so it is spelled here once. */
  template <geometry::path::ShaperScheme S>
  GeometryOp(S scheme)  // NOLINT: implicit by design
      : GeometryOp(geometry::path::Shaper(std::move(scheme))) {}
  /** A Shaper IS a geometry op — the seam value under its taught name. */
  GeometryOp(geometry::path::Shaper s);  // NOLINT: implicit by design

  SkPath apply(const SkPath& p) const { return m_apply ? m_apply(p) : p; }
  float bleed() const { return m_bleed; }
  bool operator==(const GeometryOp& o) const {
    return m_equals && o.m_equals && m_held.type() == o.m_held.type() &&
           m_equals(m_held, o.m_held);
  }

 private:
  float m_bleed = 0.0f;
  std::function<SkPath(const SkPath&)> m_apply;
  std::any m_held;
  std::function<bool(const std::any&, const std::any&)> m_equals;
};

}  // namespace sigil::compose
