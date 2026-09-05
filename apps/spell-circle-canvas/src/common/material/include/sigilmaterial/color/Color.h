#pragma once

/** @file
 * A straight-alpha sRGB colour that uploads as one float4, and the two
 * round trips a colour is reasoned about through: OKLab, under every
 * perceptual interpolation, and CIELAB, under every measured difference.
 * Beside them sRGB to linear and back, the mix that happens IN linear
 * light, relative luminance, and the ramp read on the CPU.
 */

#include <algorithm>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstdint>
#include <span>

namespace sigil::material {

/** A COLOUR VALUE SOMEONE ELSE'S LIBRARY SPELLS: four straight sRGB
 *  components named the way Skia names them. Nothing in reach but
 *  `SkColor4f` answers to it, and matching it by shape rather than by
 *  name is what lets the colour below be built from one without this
 *  library's leaf including a renderer's header. */
template <class C>
concept FourFloatColor = requires(const C& value) {
  { value.fR } -> std::convertible_to<float>;
  { value.fG } -> std::convertible_to<float>;
  { value.fB } -> std::convertible_to<float>;
  { value.fA } -> std::convertible_to<float>;
};

/** A colour as a shader receives it: four straight (not premultiplied)
 *  components in sRGB, one float each. Exactly four floats in memory, so a
 *  params struct holding one mirrors to bytes as a plain float4 uniform. */
struct Color {
  float r = 0, g = 0, b = 0, a = 1;

  constexpr Color() = default;
  constexpr Color(float red, float green, float blue, float alpha = 1)
      : r(red), g(green), b(blue), a(alpha) {}
  /** A FOUR-FLOAT sRGB COLOUR, field for field: same order, same straight
   *  alpha, so there is no transfer function, no premultiply and no clamp
   *  to get wrong — a channel above 1 survives.
   *
   *  It is implicit because the alternative is a spelled conversion at
   *  every call, and a conversion spelled by hand at each site is a place
   *  where a channel order or an alpha convention drifts from this one
   *  silently. This is the one place the mapping is written. */
  template <FourFloatColor C>
  constexpr Color(const C& other)  // NOLINT: the crossing is the point
      : r((float)other.fR),
        g((float)other.fG),
        b((float)other.fB),
        a((float)other.fA) {}

  constexpr auto operator<=>(const Color&) const = default;
};

/** A colour from a packed 0xRRGGBB, with @p a as its alpha — the spelling
 *  a palette is authored in. */
Color rgb(uint32_t hex, float a = 1.0f);

/** A colour from HUE, SATURATION and VALUE — the wheel a palette is
 *  WALKED on, where `rgb()` is the one an authored palette is typed in.
 *
 *  Hue is in degrees and wraps, so a golden-angle walk (`i * 137.5`) or a
 *  hue driven by an angle needs no fold at the call site — and the fold is
 *  here rather than there because the sextant ladder underneath silently
 *  answers magenta for anything it does not recognise, which is what an
 *  unwrapped hue hands it. Saturation and value are clamped to the unit
 *  range for the same reason: outside it the ladder returns a colour, and
 *  the wrong one.
 *
 *  NOT A PERCEPTUAL SPACE, and it must not be used as one. `value` is the
 *  largest channel and nothing else: a full-value yellow and a full-value
 *  blue are nowhere near the same brightness, so a ramp built by moving
 *  `value` bends in lightness, and a hue sweep at fixed s and v reads as
 *  bands of unequal weight. Interpolate in OKLab (`lerpOklab`) and reach
 *  for this when the SEPARATION of hues is the point — a wheel, a run of
 *  chips, one hue's tone ladder read off s and v together. */
Color hsv(float hueDegrees, float saturation, float value, float a = 1.0f);

/** A colour in OKLab, the space every perceptual interpolation runs in:
 *  lightness, the green–red axis, the blue–yellow axis, and straight alpha
 *  carried along unchanged. */
struct Oklab {
  float L, a, b, alpha;
};

/** The sRGB transfer function inverted: an encoded component to linear
 *  light. Inputs outside the unit range pass through the curve as given. */
inline float srgbToLinear(float c) {
  return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

/** The sRGB transfer function: linear light to the encoded component,
 *  clamped to the unit range first because the curve is only defined
 *  there. */
inline float linearToSrgb(float c) {
  c = std::clamp(c, 0.0f, 1.0f);
  return c <= 0.0031308f ? c * 12.92f
                         : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

/** sRGB to OKLab: linearise, project onto the cone response, take cube
 *  roots, and rotate into Lab. Alpha is carried through untouched. */
inline Oklab toOklab(const Color& c) {
  const float r = srgbToLinear(c.r), g = srgbToLinear(c.g),
              b = srgbToLinear(c.b);
  const float l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b;
  const float m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b;
  const float s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b;
  const float l_ = std::cbrt(l), m_ = std::cbrt(m), s_ = std::cbrt(s);
  return {0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
          1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
          0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_, c.a};
}

/** OKLab back to sRGB, the exact inverse of toOklab() up to rounding;
 *  the result is clamped to the unit range component-wise. */
inline Color fromOklab(const Oklab& lab) {
  const float l_ = lab.L + 0.3963377774f * lab.a + 0.2158037573f * lab.b;
  const float m_ = lab.L - 0.1055613458f * lab.a - 0.0638541728f * lab.b;
  const float s_ = lab.L - 0.0894841775f * lab.a - 1.2914855480f * lab.b;
  const float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
  const float r = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
  const float g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
  const float b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
  return {linearToSrgb(r), linearToSrgb(g), linearToSrgb(b),
          std::clamp(lab.alpha, 0.0f, 1.0f)};
}

/** Perceptual interpolation: both ends to OKLab, a straight lerp of all
 *  four channels at @p t, and back. The endpoints round-trip, so t = 0 and
 *  t = 1 return the inputs up to the transfer function's rounding. */
inline Color lerpOklab(const Color& a, const Color& b, float t) {
  const Oklab la = toOklab(a), lb = toOklab(b);
  return fromOklab({la.L + (lb.L - la.L) * t, la.a + (lb.a - la.a) * t,
                    la.b + (lb.b - la.b) * t,
                    la.alpha + (lb.alpha - la.alpha) * t});
}

/** @p c scaled by @p k in every channel, at alpha @p a. The shading verb
 *  a highlight and a shadow are both written with: one colour, brighter
 *  or darker, at a chosen opacity. */
constexpr Color scale(Color c, float k, float a) {
  return {c.r * k, c.g * k, c.b * k, a};
}

/** @p c moved a fraction @p t toward @p target, at alpha @p a. Straight
 *  sRGB, not OKLab: the caller that wants a perceptual path spells
 *  `lerpOklab`. */
constexpr Color mixToward(Color c, Color target, float t, float a) {
  return {c.r + (target.r - c.r) * t, c.g + (target.g - c.g) * t,
          c.b + (target.b - c.b) * t, a};
}

/** @p a and @p b mixed a fraction @p t apart IN LINEAR LIGHT — each
 *  channel linearised, mixed, and encoded back. Alpha mixes as it is
 *  given, since it never went through the transfer function.
 *
 *  This is the mix that answers a question about QUANTITIES — how much
 *  pigment, how much light, how much of one exposure over another — and
 *  it is a different answer from `mixToward`'s. Half way between black
 *  and white in code values is #808080, which carries a fifth of white's
 *  light; half way in linear light is near #BCBCBC, which carries half.
 *  Neither is wrong: `mixToward` walks the numbers a file stores, this
 *  walks the light they stand for, and `lerpOklab` walks what an eye
 *  reports. Say which one the drawing means. */
inline Color mixLinear(const Color& a, const Color& b, float t) {
  auto channel = [t](float x, float y) {
    return linearToSrgb(srgbToLinear(x) + (srgbToLinear(y) - srgbToLinear(x)) * t);
  };
  return {channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b),
          a.a + (b.a - a.a) * t};
}

/** RELATIVE LUMINANCE — how much light the colour stands for, on the sRGB
 *  primaries, in linear light and on a 0..1 scale where 1 is white. It is
 *  the number a contrast ratio is built from and the one that shows a mix
 *  walked in the wrong space: two colours can carry the same luminance and
 *  look nothing alike, and two that look alike can differ by a factor of
 *  three. Alpha does not enter it. */
inline float luminance(const Color& c) {
  return 0.2126f * srgbToLinear(c.r) + 0.7152f * srgbToLinear(c.g) +
         0.0722f * srgbToLinear(c.b);
}

/** A colour in CIELAB under the D65 white point: lightness on 0..100, the
 *  green–red axis, the blue–yellow axis, and straight alpha carried
 *  along.
 *
 *  It stands beside `Oklab` and answers a different question. OKLab is
 *  the space to INTERPOLATE in — its ramps are even and its hues hold.
 *  CIELAB is the space to MEASURE in: it is what a published difference
 *  is quoted in, so a drawing that claims two colours are a stated
 *  distance apart has to be read here or it is claiming something else. */
struct Lab {
  float L, a, b, alpha;
};

/** sRGB to CIELAB: linearise, project onto CIE XYZ, divide by the D65
 *  white point, and through the cube-root ladder. Alpha is untouched. */
inline Lab toLab(const Color& c) {
  const float r = srgbToLinear(c.r), g = srgbToLinear(c.g),
              b = srgbToLinear(c.b);
  const float x = 0.4124564f * r + 0.3575761f * g + 0.1804375f * b;
  const float y = 0.2126729f * r + 0.7151522f * g + 0.0721750f * b;
  const float z = 0.0193339f * r + 0.1191920f * g + 0.9503041f * b;
  // The ladder is linear near black rather than a cube root all the way
  // down: the cube root's slope runs away at zero, and a difference read
  // through it there would be a difference in the arithmetic.
  auto f = [](float t) {
    return t > 216.0f / 24389.0f
               ? std::cbrt(t)
               : (24389.0f / 27.0f * t + 16.0f) / 116.0f;
  };
  const float fx = f(x / 0.95047f), fy = f(y), fz = f(z / 1.08883f);
  return {116.0f * fy - 16.0f, 500.0f * (fx - fy), 200.0f * (fy - fz), c.a};
}

/** CIELAB back to sRGB, the inverse of `toLab` up to rounding; the result
 *  is clamped to the unit range component-wise, so a Lab value outside
 *  the sRGB gamut comes back as the nearest colour that is inside it
 *  rather than as one that is not a colour. */
inline Color fromLab(const Lab& lab) {
  const float fy = (lab.L + 16.0f) / 116.0f;
  const float fx = fy + lab.a / 500.0f;
  const float fz = fy - lab.b / 200.0f;
  auto g = [](float t) {
    const float cube = t * t * t;
    return cube > 216.0f / 24389.0f ? cube
                                    : (116.0f * t - 16.0f) * 27.0f / 24389.0f;
  };
  const float x = g(fx) * 0.95047f, y = g(fy), z = g(fz) * 1.08883f;
  const float r = 3.2404542f * x - 1.5371385f * y - 0.4985314f * z;
  const float gr = -0.9692660f * x + 1.8760108f * y + 0.0415560f * z;
  const float b = 0.0556434f * x - 0.2040259f * y + 1.0572252f * z;
  return {linearToSrgb(r), linearToSrgb(gr), linearToSrgb(b),
          std::clamp(lab.alpha, 0.0f, 1.0f)};
}

/** HOW FAR APART TWO COLOURS ARE, as a straight distance in CIELAB — the
 *  1976 difference, which is the one a plain Euclidean reading of that
 *  space is. Around 2.3 is where a side-by-side pair stops matching.
 *  Alpha does not enter it. */
inline float deltaE(const Color& a, const Color& b) {
  const Lab la = toLab(a), lb = toLab(b);
  const float dL = la.L - lb.L, da = la.a - lb.a, db = la.b - lb.b;
  return std::sqrt(dL * dL + da * da + db * db);
}

/** ONE STOP OF A RAMP: a position in [0, 1] and its colour. A ramp is a
 *  vector of these, which every renderer turns into its own gradient. */
struct RampStop {
  float pos = 0.0f;
  Color color;
  bool operator==(const RampStop&) const = default;
};

/** THE RAMP READ ON THE CPU — the same ladder a renderer's gradient
 *  draws, for the caller that needs one colour out of it rather than a
 *  shader: a seeded scatter tinted by its own parameter, a legend chip, a
 *  measurement against the picture.
 *
 *  Straight sRGB between neighbouring stops, which is what the gradients
 *  here do; a ramp that is meant to be walked perceptually is
 *  `lerpOklab` between the two stops this finds. Stops are read in the
 *  order given and are expected to be ordered; @p t clamps, so outside
 *  the ramp is the end stop's flat colour rather than an extrapolation.
 *  An empty ramp answers transparent black. */
inline Color sampleRamp(std::span<const RampStop> stops, float t) {
  if (stops.empty()) return {0, 0, 0, 0};
  if (t <= stops.front().pos) return stops.front().color;
  if (t >= stops.back().pos) return stops.back().color;
  for (size_t i = 1; i < stops.size(); ++i) {
    const RampStop& hi = stops[i];
    if (t > hi.pos) continue;
    const RampStop& lo = stops[i - 1];
    const float span = hi.pos - lo.pos;
    // Two stops at one position are a HARD EDGE, which is what a ramp
    // says a band boundary with: the upper one wins, and dividing by the
    // zero between them would not have said anything.
    const float f = span > 0.0f ? (t - lo.pos) / span : 1.0f;
    return {lo.color.r + (hi.color.r - lo.color.r) * f,
            lo.color.g + (hi.color.g - lo.color.g) * f,
            lo.color.b + (hi.color.b - lo.color.b) * f,
            lo.color.a + (hi.color.a - lo.color.a) * f};
  }
  return stops.back().color;
}

}  // namespace sigil::material
