#pragma once

/** @file
 * @ingroup weave-shaping
 *
 * `Length` — a distance in pixels, or one stated against a size that is
 * not known where it is written: the type size it is set at, the root size
 * of the tree it hangs under, the line it sits on, the width of its
 * figures. With `em()`, `rem()`, `lh()`, `ch()`, `pt()` and the suffixes
 * that spell one.
 */

#include <cstdint>

namespace sigil::weave {

/** A DISTANCE THAT MAY BE STATED AGAINST A SIZE IT DOES NOT CARRY — a
 *  multiple of the type size, of the root size, or of the line height,
 *  for a style written once and set at several sizes. PIXELS ARE
 *  IMPLICIT, so a plain number already is a length and takes no suffix. A
 *  `Length` carries the unit and nothing else: who resolves one is
 *  whoever knows the number it is a multiple of. */
struct Length {
  /** What the value is a multiple of. */
  enum class Unit : uint8_t {
    Px,   ///< the target canvas's own pixels — absolute, resolved already
    Em,   ///< the type size the length is resolved against
    Rem,  ///< the root size of the tree the length hangs under
    Lh,   ///< the line height the length is resolved against
    Ch,   ///< the advance of "0" in the face the length is resolved against
    Pt,   ///< printer's points, four to every three pixels — absolute
  };

  /** The pixels one point is: the PostScript point, 72 to the inch,
   *  against the 96-per-inch pixel CSS measures a length in. */
  static constexpr float kPointPx = 4.0f / 3.0f;
  /** What a `Ch` falls back to where the face carries no "0" — a half of
   *  the type size, which is the stand-in CSS names for that case. */
  static constexpr float kAssumedZeroAdvanceEm = 0.5f;

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

  /** Whether resolving it needs a size it does not carry. A point is a
   *  fixed count of pixels and carries everything it needs, so it is not
   *  one of these. */
  [[nodiscard]] constexpr bool relative() const {
    return unit == Unit::Em || unit == Unit::Rem || unit == Unit::Lh ||
           unit == Unit::Ch;
  }
  /** The pixels an ABSOLUTE length comes to; meaningless on a relative
   *  one, which needs the size it is stated against. */
  [[nodiscard]] constexpr float absolutePx() const {
    return unit == Unit::Pt ? value * kPointPx : value;
  }
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
/** Multiples of the advance of "0" in the face the length is resolved
 *  against — the width a column of figures is measured in. */
[[nodiscard]] constexpr Length ch(float multiple) {
  return {multiple, Length::Unit::Ch};
}
/** Printer's points, four to every three pixels. */
[[nodiscard]] constexpr Length pt(float amount) {
  return {amount, Length::Unit::Pt};
}

/** The named units as suffixes — `1.5_em`, `2_rem`, `0.5_lh`, `3_ch`,
 *  `12_pt` — each in both the floating and the integral spelling, so
 *  `2_em` and `2.0_em` are one length. There is no `_px`: a plain number
 *  already is pixels.
 *
 *  Inline, so the suffixes are visible wherever `sigil::weave` is, and
 *  named so a caller that wants only them can say
 *  `using namespace sigil::weave::literals`. */
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
constexpr Length operator""_ch(long double multiple) {
  return ch(static_cast<float>(multiple));
}
constexpr Length operator""_ch(unsigned long long multiple) {
  return ch(static_cast<float>(multiple));
}
constexpr Length operator""_pt(long double amount) {
  return pt(static_cast<float>(amount));
}
constexpr Length operator""_pt(unsigned long long amount) {
  return pt(static_cast<float>(amount));
}

}  // namespace literals

}  // namespace sigil::weave
