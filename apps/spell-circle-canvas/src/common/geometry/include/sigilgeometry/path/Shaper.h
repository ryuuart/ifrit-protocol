#pragma once

/** @file
 * THE ONE WAY GEOMETRY DEVIATES: a comparable `SkPath -> SkPath` value.
 *
 * A shaper bends ONE CONTINUOUS MARK — a wave, a zigzag, a jitter, an
 * offset. Building a mark out of repeated CELLS instead is a pattern, a
 * different kind, and the two are named apart because they compose
 * differently.
 *
 * Comparable is the point: a consumer that caches drawings can prove two
 * frames asked for the same deviation and keep the recording it already
 * has. `ops::PathOp` is the incomparable sibling, for a one-off chain
 * nothing has to prune against.
 */

#include <include/core/SkPath.h>

#include <any>
#include <concepts>
#include <functional>
#include <utility>

namespace sigil::geometry::path {

/** A shaper value: `SkPath shape(const SkPath &) const`, plus equality.
 *
 *  It bends ONE CONTINUOUS MARK — a wave, a zigzag, a jitter, an offset —
 *  and that is the whole of the geometry-deviation vocabulary. Building a
 *  mark out of repeated CELLS instead is a pattern, which is a brush kind
 *  rather than a shaper; the two are named apart because they compose
 *  differently.
 *
 *  SkPath in, SkPath out: dash and width are path operations, so nothing
 *  richer is needed. `bleed()` is optional and declares how far the
 *  deviation reaches (a wave's amplitude), so the paint cull can grow by
 *  it and a cached picture is not truncated.
 *
 *  There are deliberately no sugar methods over this seam. Stock shapers
 *  are ordinary kit values, peers of anything you write — which is what a
 *  seam is for. */
template <typename S>
concept ShaperScheme =
    std::equality_comparable<S> && requires(const S& s, const SkPath& p) {
      { s.shape(p) } -> std::convertible_to<SkPath>;
    };

/** Type-erased comparable shaper. */
class Shaper {
 public:
  template <ShaperScheme S>
  Shaper(S scheme)  // NOLINT: implicit by design (.shaped(myWave))
      : m_bleed([&] {
          if constexpr (requires {
                          { scheme.bleed() } -> std::convertible_to<float>;
                        })
            return (float)scheme.bleed();
          else
            return 0.0f;
        }()) {
    m_held = scheme;
    m_equals = [](const std::any& a, const std::any& b) {
      return std::any_cast<const S&>(a) == std::any_cast<const S&>(b);
    };
    m_shape = [s = std::move(scheme)](const SkPath& p) { return s.shape(p); };
  }
  Shaper() = default;

  SkPath shape(const SkPath& p) const { return m_shape ? m_shape(p) : p; }
  float bleed() const { return m_bleed; }
  bool operator==(const Shaper& o) const {
    if (!m_equals || !o.m_equals) return !m_equals && !o.m_equals;
    return m_held.type() == o.m_held.type() && m_equals(m_held, o.m_held);
  }

 private:
  float m_bleed = 0.0f;
  std::function<SkPath(const SkPath&)> m_shape;
  std::any m_held;
  std::function<bool(const std::any&, const std::any&)> m_equals;
};

}  // namespace sigil::geometry::path
