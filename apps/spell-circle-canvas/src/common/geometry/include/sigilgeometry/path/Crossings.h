#pragma once

/** @file
 * Where a set of paths cross each other, and which one is on top there.
 *
 * A crossing is DISCOVERED, never authored: `discoverCrossings` flattens
 * the paths and finds every PROPER crossing among them, numbered along
 * the boundary. "Proper" is load-bearing — coincident paths and endpoint
 * touches, such as a shared polygon vertex, are meetings rather than
 * crossings, and reporting them would put a knot at every corner of every
 * rectangle.
 *
 * `CrossingRule` is the comparable value that answers who passes over
 * whom, and `crossingPatch` is the region where two marks actually
 * overlap at one knot — the shape a consumer repaints to put one strand
 * back on top.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>

#include <any>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace sigil::geometry::path {

/** Who is on top. Read against a Crossing's `a`, which is always the
 *  LOWER strand index, so the question is well-posed: Over means strand
 *  `a` passes over strand `b`. */
enum class Order : uint8_t { Over, Under };

/** One discovered crossing. **Crossings are never authored** — they are
 *  found by path intersection and numbered along the boundary. */
struct Crossing {
  /** ORDINAL in the discovered list, 0-based: the crossings are sorted by
   *  `alongA` (position on the lower-indexed strand) and then numbered.
   *  It is NOT a coordinate in any parameterisation and NOT stable under
   *  a change of geometry — add a strand or move one and the same knot may
   *  take a different ordinal. This is the number `CrossingRule::except()`
   *  pins, which is exactly why pins are documented as positional. */
  size_t index = 0;
  /** Strand indices, always `a < b` — `b` is the one list order paints
   *  later, i.e. on top when nothing says otherwise. */
  size_t a = 0, b = 0;
  SkPoint at{0, 0};
  /** Where the crossing falls along each strand, as fractions of that
   *  strand's arc length. */
  float alongA = 0.0f, alongB = 0.0f;
  bool operator==(const Crossing&) const = default;
};

/** A crossing rule value: `Order decide(const Crossing &) const`, plus
 *  equality — one named required member on a comparable value, like every
 *  other seam here. Never a bare lambda: a rule is read live every frame,
 *  so it has to participate in reconciler equality or the node holding it
 *  can never prune. */
template <typename D>
concept CrossingScheme =
    std::equality_comparable<D> && requires(const D& d, const Crossing& c) {
      { d.decide(c) } -> std::convertible_to<Order>;
    };

/** The rule ladder, as ONE comparable value. Climb only as far as the
 *  composition needs:
 *
 *      crossing::alternate()                    // == sequence({Over, Under})
 *      crossing::sequence({Over, Over, Under})  // any repeating pattern
 *      crossing::pairs({{0,1},{1,2},{2,0}})     // dominance, cyclic allowed
 *      MyRule{}                                 // your own decide() value
 *
 *  and pin exceptions onto whatever you chose with `.except(i, order)`.
 *
 *  The default is LIST ORDER: later strands pass over earlier ones. That
 *  is what makes `layers` and `weave` formally one machine. */
class CrossingRule {
 public:
  CrossingRule() = default;
  template <CrossingScheme D>
  CrossingRule(D scheme)  // NOLINT: implicit by design (.crossing = MyRule{})
      : m_kind(Kind::Custom) {
    m_held = scheme;
    m_equals = [](const std::any& x, const std::any& y) {
      return std::any_cast<const D&>(x) == std::any_cast<const D&>(y);
    };
    m_decide = [s = std::move(scheme)](const Crossing& c) {
      return s.decide(c);
    };
  }

  static CrossingRule sequence(std::vector<Order> pattern) {
    CrossingRule r;
    r.m_kind = Kind::Sequence;
    r.m_pattern = std::move(pattern);
    return r;
  }
  static CrossingRule pairs(std::vector<std::pair<int, int>> dominance) {
    CrossingRule r;
    r.m_kind = Kind::Pairs;
    r.m_dominance = std::move(dominance);
    return r;
  }

  /** Pin ONE crossing, layered over whatever rule this already is.
   *
   *  **Pins are POSITIONAL**: the index is a position in the discovered
   *  order, so a stable RULE survives a geometry change and a pin does
   *  not — move a strand and pin 3 lands on a different meeting. Use
   *  rules while a composition is still moving, and pins only once it is
   *  settled and you are correcting one knot by eye.
   *
   *  Pins compose onto the base rule and never stack as separate
   *  entries: there is one `.crossing` field, and this is how it takes
   *  exceptions. */
  CrossingRule& except(size_t index, Order order) {
    for (auto& pin : m_pins)
      if (pin.first == index) {
        pin.second = order;
        return *this;
      }
    m_pins.emplace_back(index, order);
    return *this;
  }

  Order decide(const Crossing& c) const {
    for (const auto& pin : m_pins)
      if (pin.first == c.index) return pin.second;
    switch (m_kind) {
      case Kind::Sequence:
        if (!m_pattern.empty()) return m_pattern[c.index % m_pattern.size()];
        break;
      case Kind::Pairs:
        for (const auto& [over, under] : m_dominance) {
          if (over == (int)c.a && under == (int)c.b) return Order::Over;
          if (over == (int)c.b && under == (int)c.a) return Order::Under;
        }
        break;
      case Kind::Custom:
        if (m_decide) return m_decide(c);
        break;
      case Kind::ListOrder:
        break;
    }
    // List order: `b` is later in the list, so `a` is underneath.
    return Order::Under;
  }

  bool operator==(const CrossingRule& o) const {
    if (m_kind != o.m_kind || m_pattern != o.m_pattern ||
        m_dominance != o.m_dominance || m_pins != o.m_pins)
      return false;
    if (m_kind != Kind::Custom) return true;
    if (!m_equals || !o.m_equals) return !m_equals && !o.m_equals;
    return m_held.type() == o.m_held.type() && m_equals(m_held, o.m_held);
  }

 private:
  enum class Kind : uint8_t { ListOrder, Sequence, Pairs, Custom };
  Kind m_kind = Kind::ListOrder;
  std::vector<Order> m_pattern;
  std::vector<std::pair<int, int>> m_dominance;
  std::vector<std::pair<size_t, Order>> m_pins;
  std::function<Order(const Crossing&)> m_decide;
  std::any m_held;
  std::function<bool(const std::any&, const std::any&)> m_equals;
};

namespace crossing {
/** Over, under, over, under — the plain-weave rule, and formally just
 *  `sequence({Over, Under})`. Both spellings exist because they name two
 *  author intents over one machine. */
inline CrossingRule alternate() {
  return CrossingRule::sequence({Order::Over, Order::Under});
}
inline CrossingRule sequence(std::vector<Order> pattern) {
  return CrossingRule::sequence(std::move(pattern));
}
/** Strand DOMINANCE: `{{over, under}, …}`. Cycles are legal and are the
 *  point — `{{0,1},{1,2},{2,0}}` is the impossible-braid rule Penrose
 *  tilings and heraldic knots are full of. */
inline CrossingRule pairs(std::vector<std::pair<int, int>> dominance) {
  return CrossingRule::pairs(std::move(dominance));
}
}  // namespace crossing

/** Every crossing among a set of strand paths, numbered along the
 *  boundary (ascending by position on the lowest-indexed strand
 *  involved). Only PROPER crossings count: coincident strands and
 *  endpoint touches, such as a shared polygon vertex, are meetings rather
 *  than crossings, and reporting them would put a knot at every corner. */
std::vector<Crossing> discoverCrossings(const std::vector<SkPath>& strands);

/** The region where two strands' MARKS actually overlap at one crossing:
 *  the intersection of the two paths stroked to their own reach, reduced to
 *  the component containing `at` and bounded by @p maxRadius px around it.
 *
 *  Exact at any angle, which a disc is not — two marks meeting at 12° overlap
 *  in a long lens whose extent along each strand goes as reach/sin(theta),
 *  and a disc sized for the perpendicular case leaves the under-strand
 *  showing straight across the over-strand's mark.
 *
 *  `maxRadius` is not a safety margin, it is REQUIRED for correctness on any
 *  ordinary braid. Once reach/sin(theta) approaches the spacing between
 *  knots, neighbouring lenses touch and path ops merge them into ONE
 *  contour — at which point the first crossing's patch owns the whole run
 *  and the weave degenerates to "one strand on top" for half its knots.
 *  Pass half the arc distance to the adjacent crossing, so each knot can
 *  only ever claim its own half.
 *
 *  Falls back to a disc when the intersection is empty (degenerate or
 *  non-overlapping input). */
SkPath crossingPatch(const SkPath& a, float reachA, const SkPath& b,
                     float reachB, SkPoint at, float maxRadius);

}  // namespace sigil::geometry::path
