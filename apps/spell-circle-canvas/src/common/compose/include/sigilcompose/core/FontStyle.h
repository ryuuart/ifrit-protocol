#pragma once

/** @file
 * @ingroup compose-core
 *
 * `FontStyle` — CSS's `font-style` as a value: upright, italic, or an
 * oblique lean in degrees.
 */

#include <cstdint>

namespace sigil::compose {

/** HOW THE TYPE LEANS — CSS `font-style`: `FontStyle::Normal`,
 *  `FontStyle::Italic`, or `FontStyle::oblique(degrees)`. A bare number
 *  is the oblique of that many degrees, so `fontStyle(14)` and
 *  `fontStyle(FontStyle::oblique(14))` are one statement. Degrees follow
 *  CSS's sign: POSITIVE LEANS RIGHT.
 *  @trap `Italic` is a different face, not a lean: the family's italic
 *  face, or its `ital` axis, and an oblique of 14 degrees only where the
 *  family has neither. An oblique is the face's `slnt` axis, so a face
 *  without one stands upright under it. */
struct FontStyle {
  /** Which of CSS's three keywords the value is. */
  enum class Kind : uint8_t { Normal, Italic, Oblique };

  /** Upright: no italic face and no lean. */
  static const FontStyle Normal;
  /** The family's italic. */
  static const FontStyle Italic;

  /** A lean of @p degrees, positive to the right; CSS's 14 when no angle
   *  is given. */
  [[nodiscard]] static constexpr FontStyle oblique(float degrees = 14.0f) {
    return FontStyle(degrees);
  }

  /** The oblique of @p degrees — a bare number is a lean. */
  constexpr FontStyle(float degrees)  // NOLINT: implicit by design
      : kind(Kind::Oblique), degrees(degrees) {}

  Kind kind = Kind::Normal;
  /** The lean under `Oblique`, positive to the right; 0 otherwise. */
  float degrees = 0.0f;

  bool operator==(const FontStyle&) const = default;

 private:
  constexpr explicit FontStyle(Kind named) : kind(named) {}
};

inline constexpr FontStyle FontStyle::Normal{FontStyle::Kind::Normal};
inline constexpr FontStyle FontStyle::Italic{FontStyle::Kind::Italic};

}  // namespace sigil::compose
