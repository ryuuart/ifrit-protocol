#pragma once

/** @file
 * A straight-alpha sRGB colour that uploads as one float4, and the OKLab
 * round trip under every perceptual interpolation: sRGB to linear and
 * back, the linear-to-OKLab matrices, and `lerpOklab`.
 */

#include <algorithm>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstdint>

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

}  // namespace sigil::material
