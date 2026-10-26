#pragma once

/** @file
 * @ingroup core-compute
 *
 * DEGREES AND RADIANS, the one rounding between them, and an angle that
 * names its own measure.
 *
 * An angle crosses this repository as a number a caller wrote in degrees
 * and a trigonometric function reads in radians, and every library that
 * carries it has to scale it the same way or the same drawing lands in
 * two places. The conversion is here, below all of them, for the same
 * reason the mixers are: it is arithmetic several libraries agree on to
 * the bit. So is the reading of an angle written as CSS writes one,
 * `"45deg"`, `"1rad"`, `"50grad"` or `"0.25turn"`, so that the text means
 * the same angle on every slot that takes it.
 */

#include <charconv>
#include <cmath>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>

/** DEGREES AND RADIANS, converted through one rounding. The constants are
 *  the correctly rounded floats of the exact ratios, and the two verbs
 *  are the only multiplication by them anything in this tree performs, so
 *  a pen, a contour and a shader's CPU twin place the same angle at the
 *  same point. */
namespace sigil::core::angle {

/** Degrees to radians, written as the correctly rounded float of the
 *  exact value and not as a quotient of a rounded pi: pi / 180 computed
 *  from a rounded pi lands an ulp away from the nearest float to the
 *  exact ratio, and an angle scaled by it drifts by that ulp. */
inline constexpr float kDegreesToRadians = 0.017453293f;
/** Radians to degrees, rounded independently for the same reason. */
inline constexpr float kRadiansToDegrees = 57.29578f;

/** Degrees → radians — the constant above as the verb that reads at a
 *  call site: `std::cos(angle::radians(bearing))`.
 *
 *  A pair of one-line functions rather than a note telling every caller
 *  to multiply, because the multiply IS the thing that gets respelled: a
 *  hand-written `degrees * 3.14159f / 180.0f` rounds twice, and a
 *  hand-written `degrees / 57.29578f` is a divide by a rounded
 *  reciprocal, which is a third answer again. These are the one
 *  rounding. */
inline constexpr float radians(float degrees) {
  return degrees * kDegreesToRadians;
}
/** Radians to degrees, through the same single rounding. */
inline constexpr float degrees(float radians) {
  return radians * kRadiansToDegrees;
}

/** The measures an angle is written in: CSS's four angle units. */
enum class Unit {
  /** A whole turn is 360. */
  Degrees,
  /** A whole turn is two pi. */
  Radians,
  /** A whole turn is 400. */
  Gradians,
  /** A whole turn is 1. */
  Turns,
};

/** AN ANGLE THAT SAYS WHICH MEASURE IT IS IN.
 *
 *  A bare number does not: the same `45` is degrees on one surface and
 *  radians on another, because each surface keeps the meaning its own
 *  convention gives a number. An `Angle` carries its unit beside its
 *  number, so a slot that reads one converts it to whatever that slot
 *  counts in, and it reads the same whichever slot is handed it.
 *
 *  The number is kept as written and converted only when a slot reads
 *  it, so an angle written in the measure a slot counts in reaches that
 *  slot unchanged: `Angle::degrees(45).inDegrees()` is exactly 45, and
 *  `Angle::radians(1).inRadians()` exactly 1. A conversion between
 *  degrees and radians goes through `angle::radians` and
 *  `angle::degrees`, the one rounding above. */
struct Angle {
  /** The number as written, in `unit`. */
  double value = 0.0;
  /** The measure `value` is written in. */
  Unit unit = Unit::Degrees;

  /** @p value degrees. */
  static constexpr Angle degrees(double value) {
    return {value, Unit::Degrees};
  }
  /** @p value radians. */
  static constexpr Angle radians(double value) {
    return {value, Unit::Radians};
  }
  /** @p value gradians, 400 to the turn. */
  static constexpr Angle gradians(double value) {
    return {value, Unit::Gradians};
  }
  /** @p value whole turns. */
  static constexpr Angle turns(double value) { return {value, Unit::Turns}; }

  /** This angle in degrees. */
  constexpr float inDegrees() const {
    switch (unit) {
      case Unit::Degrees:
        return (float)value;
      case Unit::Radians:
        return angle::degrees((float)value);
      case Unit::Gradians:
        return (float)(value * 0.9);
      case Unit::Turns:
        return (float)(value * 360.0);
    }
    return (float)value;
  }
  /** This angle in radians. */
  constexpr float inRadians() const {
    switch (unit) {
      case Unit::Degrees:
        return angle::radians((float)value);
      case Unit::Radians:
        return (float)value;
      case Unit::Gradians:
        return (float)(value * (std::numbers::pi / 200.0));
      case Unit::Turns:
        return (float)(value * (2.0 * std::numbers::pi));
    }
    return (float)value;
  }
  /** This angle in whole turns. */
  constexpr double inTurns() const {
    switch (unit) {
      case Unit::Degrees:
        return value / 360.0;
      case Unit::Radians:
        return value / (2.0 * std::numbers::pi);
      case Unit::Gradians:
        return value / 400.0;
      case Unit::Turns:
        return value;
    }
    return value;
  }

  /** Two angles are equal when they are written alike: the same number
   *  in the same measure. `90deg` and `0.25turn` name one direction but
   *  are not equal, since each converts with its own rounding. */
  constexpr bool operator==(const Angle&) const = default;
};

/** The name CSS gives @p unit: `deg`, `rad`, `grad` or `turn`. */
constexpr std::string_view unitName(Unit unit) {
  switch (unit) {
    case Unit::Degrees:
      return "deg";
    case Unit::Radians:
      return "rad";
    case Unit::Gradians:
      return "grad";
    case Unit::Turns:
      return "turn";
  }
  return "deg";
}

/** The unit CSS names @p name, in any case, or nothing where @p name is
 *  not one of `deg`, `rad`, `grad` and `turn`. */
constexpr std::optional<Unit> unitNamed(std::string_view name) {
  const auto same = [name](std::string_view word) {
    if (name.size() != word.size()) return false;
    for (size_t at = 0; at < word.size(); ++at) {
      char letter = name[at];
      if (letter >= 'A' && letter <= 'Z') letter = (char)(letter - 'A' + 'a');
      if (letter != word[at]) return false;
    }
    return true;
  };
  for (const Unit unit :
       {Unit::Degrees, Unit::Radians, Unit::Gradians, Unit::Turns})
    if (same(unitName(unit))) return unit;
  return std::nullopt;
}

/** THE ANGLE CSS TEXT @p text WRITES, or nothing where it writes none.
 *
 *  An angle is one finite number with its unit straight after it,
 *  `45deg`, `-1.5rad`, `50grad`, `0.25turn` or `1e1deg`, in any case,
 *  with blank space around it ignored. The unit is required: text
 *  carries its own measure, so a slot whose bare number means radians
 *  and one whose bare number means degrees read `"45deg"` alike, and a
 *  bare `"45"` is refused rather than read in whichever measure the slot
 *  happens to count in. */
inline std::optional<Angle> read(std::string_view text) {
  const auto blank = [](char letter) {
    return letter == ' ' || letter == '\t' || letter == '\n' ||
           letter == '\r' || letter == '\f';
  };
  while (!text.empty() && blank(text.front())) text.remove_prefix(1);
  while (!text.empty() && blank(text.back())) text.remove_suffix(1);
  // A LEADING PLUS is the number's, and from_chars reads only the minus.
  if (!text.empty() && text.front() == '+') {
    text.remove_prefix(1);
    if (!text.empty() && text.front() == '-') return std::nullopt;
  }
  if (text.empty()) return std::nullopt;
  double value = 0.0;
  const char* const last = text.data() + text.size();
  const std::from_chars_result number =
      std::from_chars(text.data(), last, value);
  if (number.ec != std::errc{} || !std::isfinite(value)) return std::nullopt;
  const std::optional<Unit> unit =
      unitNamed(std::string_view(number.ptr, (size_t)(last - number.ptr)));
  if (!unit) return std::nullopt;
  return Angle{value, *unit};
}

/** @p value as CSS text: its number as written and its unit's name,
 *  `"45deg"` or `"0.25turn"`, which `read` reads back to the same
 *  angle. */
inline std::string text(const Angle& value) {
  char digits[32];
  const std::to_chars_result written =
      std::to_chars(digits, digits + sizeof(digits), value.value);
  return std::string(digits, written.ptr) + std::string(unitName(value.unit));
}

}  // namespace sigil::core::angle
