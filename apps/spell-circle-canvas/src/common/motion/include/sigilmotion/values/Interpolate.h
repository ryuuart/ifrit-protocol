#pragma once

/** @file
 * @ingroup motion-values
 *
 * `interpolate()`, THE ONE SEAM a value of any type moves through: where a
 * tween, an engine's animation or a timeline item stands between two
 * values of `T` a fraction of the way along. A type that adds, subtracts
 * and scales by a number — a float, a `Duration`, a `glm::vec2` or
 * `glm::vec3` — needs nothing; any other type states its own mix beside
 * its declaration, found by argument-dependent lookup, and every motion
 * over it reads that one body.
 */

#include <concepts>
#include <utility>

namespace sigil::motion {

/** A value that moves by arithmetic: it adds, subtracts and scales by a
 *  number, so the straight line between two of them is
 *  `start + (end - start) * amount` and a change can ride on top of a
 *  running motion as a difference. */
template <typename T>
concept Additive = requires(const T value, float amount) {
  { value + (value - value) * amount } -> std::convertible_to<T>;
};

namespace detail::interpolation {
/** The straight line, for every additive value. A type with its own
 *  `interpolate(start, end, amount)` in its namespace is found beside this
 *  one and wins, being exact where this is a template. */
template <Additive T>
T interpolate(const T& start, const T& end, float amount) {
  return start + (end - start) * amount;
}

/** The customisation point: a call spelled here sees the straight line
 *  above and whatever the value's own namespace declares. */
struct Interpolate {
  template <typename T>
    requires requires(const T& start, float amount) {
      { interpolate(start, start, amount) } -> std::convertible_to<T>;
    }
  T operator()(const T& start, const T& end, float amount) const {
    return interpolate(start, end, amount);
  }
};
}  // namespace detail::interpolation

/** WHERE A VALUE STANDS a fraction @p amount of the way from @p start to
 *  @p end — 0 is `start`, 1 is `end`, and a curve that overshoots hands a
 *  fraction outside the two. A colour mixes the way its own library says
 *  (`material::interpolate`, straight sRGB with alpha, as a CSS
 *  transition does); a type with no line between two values — none is
 *  declared and it does not add — cannot be animated, and says so at the
 *  call that tries. */
inline constexpr detail::interpolation::Interpolate interpolate{};

/** Whether a value of `T` can move: it has a line between two values. */
template <typename T>
concept Interpolable = std::invocable<const detail::interpolation::Interpolate&,
                                      const T&, const T&, float>;

}  // namespace sigil::motion
