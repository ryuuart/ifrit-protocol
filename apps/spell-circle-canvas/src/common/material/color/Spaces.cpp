/** @file
 * The crossings between `Color` and every space CSS names, through the
 * matrices CSS derives from each space's primaries and white.
 */

#include "Spaces.h"

#include <array>
#include <cmath>
#include <numbers>

namespace sigil::material {

namespace {

using Vector = std::array<double, 3>;
using Matrix = std::array<Vector, 3>;

Vector times(const Matrix& matrix, const Vector& vector) {
  Vector result{};
  for (size_t row = 0; row < 3; ++row)
    for (size_t column = 0; column < 3; ++column)
      result[row] += matrix[row][column] * vector[column];
  return result;
}

Matrix product(const Matrix& left, const Matrix& right) {
  Matrix result{};
  for (size_t row = 0; row < 3; ++row)
    for (size_t column = 0; column < 3; ++column)
      for (size_t inner = 0; inner < 3; ++inner)
        result[row][column] += left[row][inner] * right[inner][column];
  return result;
}

Matrix inverse(const Matrix& matrix) {
  const auto& source = matrix;
  const double determinant =
      source[0][0] *
          (source[1][1] * source[2][2] - source[1][2] * source[2][1]) -
      source[0][1] *
          (source[1][0] * source[2][2] - source[1][2] * source[2][0]) +
      source[0][2] *
          (source[1][0] * source[2][1] - source[1][1] * source[2][0]);
  Matrix result;
  result[0] = {
      (source[1][1] * source[2][2] - source[1][2] * source[2][1]) / determinant,
      (source[0][2] * source[2][1] - source[0][1] * source[2][2]) / determinant,
      (source[0][1] * source[1][2] - source[0][2] * source[1][1]) /
          determinant};
  result[1] = {
      (source[1][2] * source[2][0] - source[1][0] * source[2][2]) / determinant,
      (source[0][0] * source[2][2] - source[0][2] * source[2][0]) / determinant,
      (source[0][2] * source[1][0] - source[0][0] * source[1][2]) /
          determinant};
  result[2] = {
      (source[1][0] * source[2][1] - source[1][1] * source[2][0]) / determinant,
      (source[0][1] * source[2][0] - source[0][0] * source[2][1]) / determinant,
      (source[0][0] * source[1][1] - source[0][1] * source[1][0]) /
          determinant};
  return result;
}

/** A white point from its chromaticity, at a luminance of one. */
Vector whiteAt(double x, double y) { return {x / y, 1.0, (1.0 - x - y) / y}; }

const Vector kD65 = whiteAt(0.3127, 0.3290);
const Vector kD50 = whiteAt(0.3457, 0.3585);

/** Linear RGB to XYZ for three primaries' chromaticities and a white:
 *  each primary's XYZ at unit luminance, scaled so the three sum to the
 *  white. This is how CSS derives every matrix it publishes. */
Matrix rgbToXyz(double redX, double redY, double greenX, double greenY,
                double blueX, double blueY, const Vector& white) {
  Matrix primaries{
      {{redX / redY, greenX / greenY, blueX / blueY},
       {1.0, 1.0, 1.0},
       {(1.0 - redX - redY) / redY, (1.0 - greenX - greenY) / greenY,
        (1.0 - blueX - blueY) / blueY}}};
  const Vector scale = times(inverse(primaries), white);
  for (auto& row : primaries)
    for (size_t column = 0; column < 3; ++column) row[column] *= scale[column];
  return primaries;
}

/** The Bradford adaptation from one white to another. */
Matrix adaptation(const Vector& from, const Vector& to) {
  const Matrix bradford{{{0.8951, 0.2664, -0.1614},
                         {-0.7502, 1.7135, 0.0367},
                         {0.0389, -0.0685, 1.0296}}};
  const Vector source = times(bradford, from);
  const Vector target = times(bradford, to);
  Matrix scale{};
  for (size_t index = 0; index < 3; ++index)
    scale[index][index] = target[index] / source[index];
  return product(inverse(bradford), product(scale, bradford));
}

struct Matrices {
  Matrix srgbToXyz, xyzToSrgb;
  Matrix displayP3ToXyz, xyzToDisplayP3;
  Matrix rec2020ToXyz, xyzToRec2020;
  Matrix a98RgbToXyz, xyzToA98Rgb;
  Matrix prophotoToXyzD50, xyzD50ToProphoto;
  Matrix d50ToD65, d65ToD50;
};

const Matrices& matrices() {
  static const Matrices built = [] {
    Matrices result;
    result.srgbToXyz = rgbToXyz(0.64, 0.33, 0.30, 0.60, 0.15, 0.06, kD65);
    result.displayP3ToXyz =
        rgbToXyz(0.680, 0.320, 0.265, 0.690, 0.150, 0.060, kD65);
    result.rec2020ToXyz =
        rgbToXyz(0.708, 0.292, 0.170, 0.797, 0.131, 0.046, kD65);
    result.a98RgbToXyz = rgbToXyz(0.64, 0.33, 0.21, 0.71, 0.15, 0.06, kD65);
    result.prophotoToXyzD50 = rgbToXyz(0.734699, 0.265301, 0.159597, 0.840403,
                                       0.036598, 0.000105, kD50);
    result.xyzToSrgb = inverse(result.srgbToXyz);
    result.xyzToDisplayP3 = inverse(result.displayP3ToXyz);
    result.xyzToRec2020 = inverse(result.rec2020ToXyz);
    result.xyzToA98Rgb = inverse(result.a98RgbToXyz);
    result.xyzD50ToProphoto = inverse(result.prophotoToXyzD50);
    result.d50ToD65 = adaptation(kD50, kD65);
    result.d65ToD50 = adaptation(kD65, kD50);
    return result;
  }();
  return built;
}

// Each transfer function is extended past the unit range by mirroring it
// through zero, as CSS extends them, so a component outside the gamut
// survives the round trip.

double srgbDecode(double component) {
  const double magnitude = std::abs(component);
  if (magnitude <= 0.04045) return component / 12.92;
  return std::copysign(std::pow((magnitude + 0.055) / 1.055, 2.4), component);
}

double srgbEncode(double component) {
  const double magnitude = std::abs(component);
  if (magnitude <= 0.0031308) return component * 12.92;
  return std::copysign(1.055 * std::pow(magnitude, 1.0 / 2.4) - 0.055,
                       component);
}

constexpr double kRec2020Alpha = 1.09929682680944;
constexpr double kRec2020Beta = 0.018053968510807;

double rec2020Decode(double component) {
  const double magnitude = std::abs(component);
  if (magnitude < kRec2020Beta * 4.5) return component / 4.5;
  return std::copysign(
      std::pow((magnitude + kRec2020Alpha - 1.0) / kRec2020Alpha, 1.0 / 0.45),
      component);
}

double rec2020Encode(double component) {
  const double magnitude = std::abs(component);
  if (magnitude <= kRec2020Beta) return component * 4.5;
  return std::copysign(
      kRec2020Alpha * std::pow(magnitude, 0.45) - (kRec2020Alpha - 1.0),
      component);
}

double a98Decode(double component) {
  return std::copysign(std::pow(std::abs(component), 563.0 / 256.0), component);
}

double a98Encode(double component) {
  return std::copysign(std::pow(std::abs(component), 256.0 / 563.0), component);
}

double prophotoDecode(double component) {
  const double magnitude = std::abs(component);
  if (magnitude <= 16.0 / 512.0) return component / 16.0;
  return std::copysign(std::pow(magnitude, 1.8), component);
}

double prophotoEncode(double component) {
  const double magnitude = std::abs(component);
  if (magnitude < 1.0 / 512.0) return component * 16.0;
  return std::copysign(std::pow(magnitude, 1.0 / 1.8), component);
}

template <class Function>
Vector each(const Vector& vector, Function function) {
  return {function(vector[0]), function(vector[1]), function(vector[2])};
}

constexpr double kKappa = 24389.0 / 27.0;
constexpr double kEpsilon = 216.0 / 24389.0;

Vector labFromXyzD50(const Vector& xyz) {
  auto ladder = [](double value) {
    return value > kEpsilon ? std::cbrt(value)
                            : (kKappa * value + 16.0) / 116.0;
  };
  const double x = ladder(xyz[0] / kD50[0]);
  const double y = ladder(xyz[1] / kD50[1]);
  const double z = ladder(xyz[2] / kD50[2]);
  return {116.0 * y - 16.0, 500.0 * (x - y), 200.0 * (y - z)};
}

Vector xyzD50FromLab(const Vector& lab) {
  const double middle = (lab[0] + 16.0) / 116.0;
  const double first = lab[1] / 500.0 + middle;
  const double last = middle - lab[2] / 200.0;
  auto unladder = [](double value) {
    const double cube = value * value * value;
    return cube > kEpsilon ? cube : (116.0 * value - 16.0) / kKappa;
  };
  const double y =
      lab[0] > kKappa * kEpsilon ? middle * middle * middle : lab[0] / kKappa;
  return {unladder(first) * kD50[0], y * kD50[1], unladder(last) * kD50[2]};
}

Vector oklabFromLinear(const Vector& linear) {
  const double longCone =
      std::cbrt(0.4122214708 * linear[0] + 0.5363325363 * linear[1] +
                0.0514459929 * linear[2]);
  const double middleCone =
      std::cbrt(0.2119034982 * linear[0] + 0.6806995451 * linear[1] +
                0.1073969566 * linear[2]);
  const double shortCone =
      std::cbrt(0.0883024619 * linear[0] + 0.2817188376 * linear[1] +
                0.6299787005 * linear[2]);
  return {0.2104542553 * longCone + 0.7936177850 * middleCone -
              0.0040720468 * shortCone,
          1.9779984951 * longCone - 2.4285922050 * middleCone +
              0.4505937099 * shortCone,
          0.0259040371 * longCone + 0.7827717662 * middleCone -
              0.8086757660 * shortCone};
}

Vector linearFromOklab(const Vector& lab) {
  const double longCone =
      lab[0] + 0.3963377774 * lab[1] + 0.2158037573 * lab[2];
  const double middleCone =
      lab[0] - 0.1055613458 * lab[1] - 0.0638541728 * lab[2];
  const double shortCone =
      lab[0] - 0.0894841775 * lab[1] - 1.2914855480 * lab[2];
  const double longCube = longCone * longCone * longCone;
  const double middleCube = middleCone * middleCone * middleCone;
  const double shortCube = shortCone * shortCone * shortCone;
  return {4.0767416621 * longCube - 3.3077115913 * middleCube +
              0.2309699292 * shortCube,
          -1.2684380046 * longCube + 2.6097574011 * middleCube -
              0.3413193965 * shortCube,
          -0.0041960863 * longCube - 0.7034186147 * middleCube +
              1.7076147010 * shortCube};
}

double wrapDegrees(double degrees) {
  double wrapped = std::fmod(degrees, 360.0);
  if (wrapped < 0.0) wrapped += 360.0;
  return wrapped;
}

Vector polarOf(const Vector& rectangular) {
  const double chroma = std::hypot(rectangular[1], rectangular[2]);
  const double hue = wrapDegrees(std::atan2(rectangular[2], rectangular[1]) *
                                 180.0 / std::numbers::pi);
  return {rectangular[0], chroma, hue};
}

Vector rectangularOf(const Vector& polar) {
  const double radians = polar[2] * std::numbers::pi / 180.0;
  return {polar[0], polar[1] * std::cos(radians), polar[1] * std::sin(radians)};
}

/** sRGB to HSL as CSS converts it: the hue in degrees, saturation and
 *  lightness as unit fractions. */
Vector hslOf(const Vector& rgb) {
  const double largest = std::max({rgb[0], rgb[1], rgb[2]});
  const double smallest = std::min({rgb[0], rgb[1], rgb[2]});
  const double lightness = (largest + smallest) / 2.0;
  const double spread = largest - smallest;
  double hue = 0.0, saturation = 0.0;
  if (spread != 0.0) {
    saturation =
        (lightness == 0.0 || lightness == 1.0)
            ? 0.0
            : (largest - lightness) / std::min(lightness, 1.0 - lightness);
    if (largest == rgb[0])
      hue = (rgb[1] - rgb[2]) / spread + (rgb[1] < rgb[2] ? 6.0 : 0.0);
    else if (largest == rgb[1])
      hue = (rgb[2] - rgb[0]) / spread + 2.0;
    else
      hue = (rgb[0] - rgb[1]) / spread + 4.0;
    hue *= 60.0;
  }
  if (saturation < 0.0) {
    hue += 180.0;
    saturation = -saturation;
  }
  return {wrapDegrees(hue), saturation, lightness};
}

/** HSL to sRGB as CSS converts it. */
Vector rgbOfHsl(const Vector& hsl) {
  const double hue = wrapDegrees(hsl[0]);
  const double reach = hsl[1] * std::min(hsl[2], 1.0 - hsl[2]);
  auto channel = [&](double offset) {
    const double sector = std::fmod(offset + hue / 30.0, 12.0);
    return hsl[2] -
           reach * std::max(std::min({sector - 3.0, 9.0 - sector, 1.0}), -1.0);
  };
  return {channel(0.0), channel(8.0), channel(4.0)};
}

Vector hwbOf(const Vector& rgb) {
  const double hue = hslOf(rgb)[0];
  return {hue, std::min({rgb[0], rgb[1], rgb[2]}),
          1.0 - std::max({rgb[0], rgb[1], rgb[2]})};
}

Vector rgbOfHwb(const Vector& hwb) {
  const double white = hwb[1], black = hwb[2];
  if (white + black >= 1.0) {
    const double grey = white / (white + black);
    return {grey, grey, grey};
  }
  Vector rgb = rgbOfHsl({hwb[0], 1.0, 0.5});
  for (double& channel : rgb) channel = channel * (1.0 - white - black) + white;
  return rgb;
}

float unit(float value) { return std::clamp(value, 0.0f, 1.0f); }

Color clampedColor(const Vector& rgb, float alpha) {
  return {(float)std::clamp(rgb[0], 0.0, 1.0),
          (float)std::clamp(rgb[1], 0.0, 1.0),
          (float)std::clamp(rgb[2], 0.0, 1.0), unit(alpha)};
}

/** Linear-light sRGB to a colour, brought inside the gamut by reducing
 *  its OKLCH chroma where it lies outside. */
Color fromLinear(const Vector& linear, float alpha) {
  // A colour that only grazes the edge by rounding is inside; anything
  // further out keeps its hue and lightness and gives up chroma.
  constexpr double kSlack = 1e-5;
  bool inside = true;
  for (double component : linear)
    if (!(component >= -kSlack && component <= 1.0 + kSlack)) inside = false;
  if (inside)
    return clampedColor(
        each(linear,
             [](double component) {
               return srgbEncode(std::clamp(component, 0.0, 1.0));
             }),
        alpha);
  const Vector lab = oklabFromLinear(linear);
  return fitToSrgb(
      oklchOf(Oklab{(float)lab[0], (float)lab[1], (float)lab[2], unit(alpha)}));
}

Vector asVector(const spaces::Components& components) {
  return {components.first, components.second, components.third};
}

spaces::Components asComponents(const Vector& vector) {
  return {vector[0], vector[1], vector[2]};
}

}  // namespace

namespace spaces {

Components toSpace(ColorSpace space, const Color& color) {
  const Vector rgb{color.r, color.g, color.b};
  switch (space) {
    case ColorSpace::Srgb:
      return asComponents(rgb);
    case ColorSpace::Hsl:
      return asComponents(hslOf(rgb));
    case ColorSpace::Hwb:
      return asComponents(hwbOf(rgb));
    default:
      break;
  }
  const Matrices& known = matrices();
  const Vector linear = each(rgb, srgbDecode);
  const Vector xyz = times(known.srgbToXyz, linear);
  switch (space) {
    case ColorSpace::SrgbLinear:
      return asComponents(linear);
    case ColorSpace::DisplayP3:
      return asComponents(each(times(known.xyzToDisplayP3, xyz), srgbEncode));
    case ColorSpace::Rec2020:
      return asComponents(each(times(known.xyzToRec2020, xyz), rec2020Encode));
    case ColorSpace::A98Rgb:
      return asComponents(each(times(known.xyzToA98Rgb, xyz), a98Encode));
    case ColorSpace::ProphotoRgb:
      return asComponents(
          each(times(known.xyzD50ToProphoto, times(known.d65ToD50, xyz)),
               prophotoEncode));
    case ColorSpace::XyzD65:
      return asComponents(xyz);
    case ColorSpace::XyzD50:
      return asComponents(times(known.d65ToD50, xyz));
    case ColorSpace::Lab:
      return asComponents(labFromXyzD50(times(known.d65ToD50, xyz)));
    case ColorSpace::Lch:
      return asComponents(polarOf(labFromXyzD50(times(known.d65ToD50, xyz))));
    case ColorSpace::Oklab:
      return asComponents(oklabFromLinear(linear));
    case ColorSpace::Oklch:
      return asComponents(polarOf(oklabFromLinear(linear)));
    default:
      return asComponents(rgb);
  }
}

Color fromSpace(ColorSpace space, Components components, float alpha) {
  const Vector values = asVector(components);
  const Matrices& known = matrices();
  switch (space) {
    case ColorSpace::Srgb:
      return {(float)values[0], (float)values[1], (float)values[2],
              unit(alpha)};
    case ColorSpace::Hsl:
      return clampedColor(rgbOfHsl(values), alpha);
    case ColorSpace::Hwb:
      return clampedColor(rgbOfHwb(values), alpha);
    case ColorSpace::SrgbLinear:
      return fromLinear(values, alpha);
    case ColorSpace::DisplayP3:
      return fromLinear(times(known.xyzToSrgb, times(known.displayP3ToXyz,
                                                     each(values, srgbDecode))),
                        alpha);
    case ColorSpace::Rec2020:
      return fromLinear(
          times(known.xyzToSrgb,
                times(known.rec2020ToXyz, each(values, rec2020Decode))),
          alpha);
    case ColorSpace::A98Rgb:
      return fromLinear(times(known.xyzToSrgb, times(known.a98RgbToXyz,
                                                     each(values, a98Decode))),
                        alpha);
    case ColorSpace::ProphotoRgb:
      return fromLinear(
          times(known.xyzToSrgb,
                times(known.d50ToD65, times(known.prophotoToXyzD50,
                                            each(values, prophotoDecode)))),
          alpha);
    case ColorSpace::XyzD65:
      return fromLinear(times(known.xyzToSrgb, values), alpha);
    case ColorSpace::XyzD50:
      return fromLinear(times(known.xyzToSrgb, times(known.d50ToD65, values)),
                        alpha);
    case ColorSpace::Lab:
      return fromLinear(
          times(known.xyzToSrgb, times(known.d50ToD65, xyzD50FromLab(values))),
          alpha);
    case ColorSpace::Lch:
      return fromLinear(
          times(known.xyzToSrgb,
                times(known.d50ToD65, xyzD50FromLab(rectangularOf(values)))),
          alpha);
    case ColorSpace::Oklab:
      return fromLinear(linearFromOklab(values), alpha);
    case ColorSpace::Oklch:
      return fromLinear(linearFromOklab(rectangularOf(values)), alpha);
  }
  return {0, 0, 0, unit(alpha)};
}

int hueIndex(ColorSpace space) {
  switch (space) {
    case ColorSpace::Hsl:
    case ColorSpace::Hwb:
      return 0;
    case ColorSpace::Lch:
    case ColorSpace::Oklch:
      return 2;
    default:
      return -1;
  }
}

bool hueIsPowerless(ColorSpace space, const Components& components) {
  constexpr double kNoChroma = 1e-6;
  switch (space) {
    case ColorSpace::Hsl:
      return components.second <= kNoChroma;
    case ColorSpace::Hwb:
      return components.second + components.third >= 1.0 - kNoChroma;
    case ColorSpace::Lch:
    case ColorSpace::Oklch:
      return components.second <= kNoChroma;
    default:
      return false;
  }
}

}  // namespace spaces

Color hsl(float hueDegrees, float saturation, float lightness, float alpha) {
  return spaces::fromSpace(
      ColorSpace::Hsl,
      {hueDegrees, std::clamp(saturation, 0.0f, 100.0f) / 100.0,
       std::clamp(lightness, 0.0f, 100.0f) / 100.0},
      alpha);
}

Color hwb(float hueDegrees, float whiteness, float blackness, float alpha) {
  return spaces::fromSpace(
      ColorSpace::Hwb,
      {hueDegrees, std::clamp(whiteness, 0.0f, 100.0f) / 100.0,
       std::clamp(blackness, 0.0f, 100.0f) / 100.0},
      alpha);
}

Color lab(float lightness, float a, float b, float alpha) {
  return spaces::fromSpace(ColorSpace::Lab,
                           {std::clamp(lightness, 0.0f, 100.0f), a, b}, alpha);
}

Color lch(float lightness, float chroma, float hueDegrees, float alpha) {
  return spaces::fromSpace(
      ColorSpace::Lch,
      {std::clamp(lightness, 0.0f, 100.0f), std::max(chroma, 0.0f), hueDegrees},
      alpha);
}

Color oklab(float lightness, float a, float b, float alpha) {
  return spaces::fromSpace(ColorSpace::Oklab,
                           {std::clamp(lightness, 0.0f, 1.0f), a, b}, alpha);
}

Color oklch(float lightness, float chroma, float hueDegrees, float alpha) {
  return spaces::fromSpace(
      ColorSpace::Oklch,
      {std::clamp(lightness, 0.0f, 1.0f), std::max(chroma, 0.0f), hueDegrees},
      alpha);
}

}  // namespace sigil::material
