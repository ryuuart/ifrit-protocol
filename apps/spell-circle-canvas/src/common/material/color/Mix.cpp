/** @file
 * Two colours mixed in a named space, and the space read from its CSS
 * spelling.
 */

#include <sigilmaterial/color/Mix.h>

#include <cctype>
#include <cmath>

#include "Spaces.h"

namespace sigil::material {

namespace {

bool sameWord(std::string_view text, std::string_view word) {
  if (text.size() != word.size()) return false;
  for (size_t index = 0; index < text.size(); ++index)
    if (std::tolower((unsigned char)text[index]) != word[index]) return false;
  return true;
}

struct SpaceName {
  std::string_view name;
  ColorSpace space;
};

constexpr SpaceName kSpaceNames[] = {
    {"srgb", ColorSpace::Srgb},
    {"srgb-linear", ColorSpace::SrgbLinear},
    {"display-p3", ColorSpace::DisplayP3},
    {"rec2020", ColorSpace::Rec2020},
    {"a98-rgb", ColorSpace::A98Rgb},
    {"prophoto-rgb", ColorSpace::ProphotoRgb},
    {"xyz", ColorSpace::XyzD65},
    {"xyz-d50", ColorSpace::XyzD50},
    {"xyz-d65", ColorSpace::XyzD65},
    {"lab", ColorSpace::Lab},
    {"lch", ColorSpace::Lch},
    {"oklab", ColorSpace::Oklab},
    {"oklch", ColorSpace::Oklch},
    {"hsl", ColorSpace::Hsl},
    {"hwb", ColorSpace::Hwb},
};

/** The hue @p end reached from @p start along @p arc, as an absolute angle
 *  that may run outside [0, 360) so the mix walks it without folding. */
double hueAlong(double start, double end, HueArc arc) {
  double delta = std::fmod(end - start, 360.0);
  if (delta < 0.0) delta += 360.0;
  switch (arc) {
    case HueArc::Shorter:
      if (delta > 180.0) delta -= 360.0;
      break;
    case HueArc::Longer:
      // Two equal hues are a whole turn apart the long way round.
      if (delta == 0.0)
        delta = 360.0;
      else if (delta < 180.0)
        delta -= 360.0;
      break;
    case HueArc::Increasing:
      break;
    case HueArc::Decreasing:
      if (delta > 0.0) delta -= 360.0;
      break;
  }
  return start + delta;
}

}  // namespace

std::optional<ColorSpace> readColorSpace(std::string_view name) {
  for (const SpaceName& known : kSpaceNames)
    if (sameWord(name, known.name)) return known.space;
  return std::nullopt;
}

Color colorMix(const Color& first, const Color& second, float amount,
               ColorSpace space, HueArc arc) {
  spaces::Components start = spaces::toSpace(space, first);
  spaces::Components end = spaces::toSpace(space, second);
  const int hue = spaces::hueIndex(space);
  double* startValues[3] = {&start.first, &start.second, &start.third};
  double* endValues[3] = {&end.first, &end.second, &end.third};
  // A grey has no hue to walk from, so it takes the other colour's, which
  // keeps a mix from grey to a colour on that colour's hue.
  if (hue >= 0) {
    const bool startPowerless = spaces::hueIsPowerless(space, start);
    const bool endPowerless = spaces::hueIsPowerless(space, end);
    if (startPowerless && !endPowerless) *startValues[hue] = *endValues[hue];
    if (endPowerless && !startPowerless) *endValues[hue] = *startValues[hue];
  }
  const double fraction = amount;
  const double startAlpha = std::clamp(first.a, 0.0f, 1.0f);
  const double endAlpha = std::clamp(second.a, 0.0f, 1.0f);
  const double alpha = startAlpha + (endAlpha - startAlpha) * fraction;
  spaces::Components mixed;
  double* mixedValues[3] = {&mixed.first, &mixed.second, &mixed.third};
  for (int index = 0; index < 3; ++index) {
    if (index == hue) {
      const double from = *startValues[index];
      const double to = hueAlong(from, *endValues[index], arc);
      *mixedValues[index] = from + (to - from) * fraction;
      continue;
    }
    // Premultiplied, so a nearly transparent end lends its alpha and
    // almost none of its colour, as CSS mixes.
    const double premultiplied =
        *startValues[index] * startAlpha +
        (*endValues[index] * endAlpha - *startValues[index] * startAlpha) *
            fraction;
    *mixedValues[index] = alpha > 0.0 ? premultiplied / alpha : premultiplied;
  }
  return spaces::fromSpace(space, mixed, (float)alpha);
}

}  // namespace sigil::material
