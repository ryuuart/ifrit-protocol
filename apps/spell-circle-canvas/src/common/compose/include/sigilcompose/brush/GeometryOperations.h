#pragma once

/** @file
 * SigilCompose geometry ops — the one mechanism door for deviating an
 * outline: the GeometryOperation that a comparable shaper and a raw path
 * callable both convert to.
 */

#include <include/core/SkPath.h>
#include <sigilgeometry/path/Operations.h>
#include <sigilgeometry/path/Shaper.h>

#include <any>
#include <functional>
#include <utility>

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

/** THE ESCAPE HATCH a restyle carries: a path→path callable, which is
 *  `geometry::path::operations::PathOperation` — the recipe step every distort
 * in that catalogue converts to, chained with
 * `geometry::path::operations::chain`. It can do anything, and it can never
 * prune: an incomparable callable compares conservatively unequal, so a node
 * wearing one re-records every render (memo the host, or keep it
 * pointer-stable).
 *
 *  Reach for it only when no `geometry::shapes` value and no shaper you
 *  could write yourself can say what you mean — a shaper is a comparable
 *  struct with `SkPath shape(const SkPath &) const` and writing one is
 *  four lines.
 *
 *  What `brush::restyle()` carries: EITHER a comparable `Shaper`, so a
 *  restyle of a stock shaper still prunes, OR a raw path callable, which
 *  is conservatively unequal forever. A bare lambda literal must be
 *  assigned to a `geometry::path::operations::PathOperation` first, because two
 *  user-defined conversions do not chain.
 *
 *  Nothing else takes one: `Brush`'s pipeline and its per-layer suffixes
 *  are plain `Shaper` lists. */
class GeometryOperation {
 public:
  GeometryOperation(
      geometry::path::operations::PathOperation fn)  // NOLINT: escape hatch
      : m_apply(std::move(fn)) {}
  /** Any shaper VALUE, directly — `restyle(shapers::Wave{...}, decoration)`.
   * The hop through Shaper cannot be implicit (two user-defined conversions do
   * not chain), so it is spelled here once. */
  template <geometry::path::ShaperScheme S>
  GeometryOperation(S scheme)  // NOLINT: implicit by design
      : GeometryOperation(geometry::path::Shaper(std::move(scheme))) {}
  /** A Shaper IS a geometry op — the seam value under its taught name. */
  GeometryOperation(geometry::path::Shaper s);  // NOLINT: implicit by design

  SkPath apply(const SkPath& p) const { return m_apply ? m_apply(p) : p; }
  float bleed() const { return m_bleed; }
  bool operator==(const GeometryOperation& o) const {
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
