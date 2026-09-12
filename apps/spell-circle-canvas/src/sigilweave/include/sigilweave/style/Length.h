#pragma once

/** @file
 * @ingroup shaping
 *
 * `Length` — a distance in pixels, or one stated against a size that is
 * not known where it is written: the type size it is set at, the root size
 * of the tree it hangs under, the line it sits on. With `em()`, `rem()`,
 * `lh()` and the `_em` / `_rem` / `_lh` suffixes that spell one.
 */

#include <cstdint>

namespace sigil::weave {

/** A DISTANCE THAT MAY BE STATED AGAINST A SIZE IT DOES NOT CARRY.
 *
 *  A style written once and set at several sizes states its distances as
 *  MULTIPLES rather than as pixels: half the type size is one distance at
 *  13 px and another at 48, and a length that had baked the pixels in
 *  would be right at one size and wrong at every other.
 *
 *  PIXELS ARE IMPLICIT, so a plain number already is a length — `13` is
 *  thirteen pixels and takes no suffix. The three relative units name what
 *  they are multiples of, and a `Length` carries nothing else: WHO resolves
 *  one is whoever knows that number, which is the whole reason to keep the
 *  unit and leave the resolution out. */
struct Length {
  /** What the value is a multiple of. */
  enum class Unit : uint8_t {
    Px,   ///< the target canvas's own pixels — absolute, resolved already
    Em,   ///< the type size the length is resolved against
    Rem,  ///< the root size of the tree the length hangs under
    Lh,   ///< the line height the length is resolved against
  };

  float value = 0;
  Unit unit = Unit::Px;

  /** Zero pixels. */
  constexpr Length() = default;
  /** Pixels, implicitly: a plain number IS a length, so a call site that
   *  writes `13` needs no suffix and no cast. */
  constexpr Length(float px) : value(px) {}
  /** A value in a named unit; `em()`, `rem()` and `lh()` spell the three
   *  relative ones more readably than the enumerator does. */
  constexpr Length(float amount, Unit in) : value(amount), unit(in) {}

  bool operator==(const Length&) const = default;

  /** Whether resolving it needs a size it does not carry. */
  [[nodiscard]] constexpr bool relative() const { return unit != Unit::Px; }
};

/** Multiples of the type size the length is resolved against. */
[[nodiscard]] constexpr Length em(float multiple) {
  return {multiple, Length::Unit::Em};
}
/** Multiples of the root size of the tree the length hangs under. */
[[nodiscard]] constexpr Length rem(float multiple) {
  return {multiple, Length::Unit::Rem};
}
/** Multiples of the line height the length is resolved against. */
[[nodiscard]] constexpr Length lh(float multiple) {
  return {multiple, Length::Unit::Lh};
}

/** The three relative units as suffixes — `1.5_em`, `2_rem`, `0.5_lh` —
 *  each in both the floating and the integral spelling, so `2_em` and
 *  `2.0_em` are one length. There is no `_px`: a plain number already is
 *  pixels. */
inline namespace literals {

constexpr Length operator""_em(long double multiple) {
  return em(static_cast<float>(multiple));
}
constexpr Length operator""_em(unsigned long long multiple) {
  return em(static_cast<float>(multiple));
}
constexpr Length operator""_rem(long double multiple) {
  return rem(static_cast<float>(multiple));
}
constexpr Length operator""_rem(unsigned long long multiple) {
  return rem(static_cast<float>(multiple));
}
constexpr Length operator""_lh(long double multiple) {
  return lh(static_cast<float>(multiple));
}
constexpr Length operator""_lh(unsigned long long multiple) {
  return lh(static_cast<float>(multiple));
}

}  // namespace literals

}  // namespace sigil::weave
