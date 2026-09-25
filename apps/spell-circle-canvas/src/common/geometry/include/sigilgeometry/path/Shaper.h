#pragma once

/** @file
 * @ingroup geometry-path
 *
 * THE ONE WAY GEOMETRY DEVIATES: a comparable `SkPath -> SkPath` value.
 *
 * A shaper bends ONE CONTINUOUS MARK — a wave, a zigzag, a jitter, an
 * offset. Building a mark out of repeated CELLS instead is a pattern, a
 * different kind, and the two are named apart because they compose
 * differently.
 *
 * Comparable is the point: a consumer that caches drawings can prove two
 * frames asked for the same deviation and keep the recording it already
 * has. `Shaper::incomparable` carries any path callable (an
 * `operations::PathOperation` chain among them) through the same seam, at
 * the price of never comparing equal — the one door a raw callable has.
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

  /** ANY PATH CALLABLE as a shaper — the escape hatch for a deviation no
   *  comparable value can say. A closure has no equality, so the result
   *  compares unequal to everything, ITSELF INCLUDED: a consumer that
   *  prunes on equality re-records whatever wears one, every time. That
   *  price is the point of the name; a shaper is a comparable struct with
   *  `SkPath shape(const SkPath &) const`, and writing one is four lines.
   *  `bleed` is how far the callable's result reaches past its input,
   *  which only the caller knows. */
  static Shaper incomparable(std::function<SkPath(const SkPath&)> operation,
                             float bleed = 0.0f) {
    Shaper out;
    out.m_bleed = bleed;
    out.m_shape = std::move(operation);
    out.m_incomparable = true;
    return out;
  }

  SkPath shape(const SkPath& p) const { return m_shape ? m_shape(p) : p; }
  float bleed() const { return m_bleed; }
  /** Whether this shaper can ever compare equal — false only for one
   *  made by `incomparable`. */
  bool comparable() const { return !m_incomparable; }
  bool operator==(const Shaper& o) const {
    if (m_incomparable || o.m_incomparable) return false;
    if (!m_equals || !o.m_equals) return !m_equals && !o.m_equals;
    return m_held.type() == o.m_held.type() && m_equals(m_held, o.m_held);
  }

 private:
  float m_bleed = 0.0f;
  bool m_incomparable = false;
  std::function<SkPath(const SkPath&)> m_shape;
  std::any m_held;
  std::function<bool(const std::any&, const std::any&)> m_equals;
};

}  // namespace sigil::geometry::path
