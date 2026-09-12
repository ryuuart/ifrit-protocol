#pragma once

/** @file
 * A straight-alpha sRGB colour that uploads as one float4, and the two
 * round trips a colour is reasoned about through: OKLab, under every
 * perceptual interpolation — with OKLCH, its polar form, where a hue and
 * a chroma are named — and CIELAB, under every measured difference.
 * Beside them sRGB to linear and back, the mix that happens IN linear
 * light, relative luminance, and the ramp read on the CPU.
 */

#include <algorithm>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstdint>
#include <span>
#include <vector>

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
 *  parameter struct holding one mirrors to bytes as a plain float4 uniform. */
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
 *  a palette is authored in, one hex integer per colour.
 *
 *  constexpr, because a palette is a list of constants and a constant
 *  that has to be built at run time is a constant the compiler cannot
 *  fold into the value that holds it. */
constexpr Color rgb(uint32_t hex, float a = 1.0f) {
  return {(float)((hex >> 16u) & 0xffu) / 255.0f,
          (float)((hex >> 8u) & 0xffu) / 255.0f, (float)(hex & 0xffu) / 255.0f,
          a};
}

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

/** THE LIGHT AN OKLab COLOUR STANDS FOR: the three linear-light sRGB
 *  components, before the transfer function and before any clamp.
 *
 *  It is separate from `fromOklab` because the numbers BEFORE the clamp
 *  are the ones that say whether the colour is a colour at all: a
 *  component below zero or above one means the sRGB primaries cannot mix
 *  it, and once it is clamped that fact is gone. */
struct LinearRgb {
  float r, g, b;
};

/** OKLab to linear light. */
inline LinearRgb linearOf(const Oklab& lab) {
  const float l_ = lab.L + 0.3963377774f * lab.a + 0.2158037573f * lab.b;
  const float m_ = lab.L - 0.1055613458f * lab.a - 0.0638541728f * lab.b;
  const float s_ = lab.L - 0.0894841775f * lab.a - 1.2914855480f * lab.b;
  const float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
  return {4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
          -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
          -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s};
}

/** Whether the sRGB primaries can mix this colour at all, to within the
 *  rounding a round trip through the transfer function leaves. */
inline bool inSrgbGamut(const Oklab& lab, float slack = 1e-4f) {
  const LinearRgb light = linearOf(lab);
  const float low = -slack, high = 1.0f + slack;
  return light.r >= low && light.r <= high && light.g >= low &&
         light.g <= high && light.b >= low && light.b <= high;
}

/** OKLab back to sRGB, the exact inverse of toOklab() up to rounding;
 *  the result is clamped to the unit range component-wise. */
inline Color fromOklab(const Oklab& lab) {
  const LinearRgb light = linearOf(lab);
  return {linearToSrgb(light.r), linearToSrgb(light.g), linearToSrgb(light.b),
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

/** OKLab IN POLAR FORM: the same lightness, the distance from grey, and
 *  the direction it lies in.
 *
 *  It holds exactly what OKLab holds and is a different thing to REASON
 *  with. `a` and `b` are two axes nobody can name; `chroma` is how
 *  colourful, `hueDegrees` is which colour, and those are the two words a
 *  harmony, a tone ladder and a hue-preserving lift are stated in. A
 *  chroma of zero is a grey, and its hue is arbitrary — the one place the
 *  round trip does not carry a number, since a grey has no direction. */
struct Oklch {
  float L, chroma, hueDegrees, alpha;
};

/** OKLab to its polar form. The hue is in degrees on [0, 360). */
inline Oklch oklchOf(const Oklab& lab) {
  float hue = std::atan2(lab.b, lab.a) * (180.0f / 3.14159265358979323846f);
  if (hue < 0.0f) hue += 360.0f;
  return {lab.L, std::sqrt(lab.a * lab.a + lab.b * lab.b), hue, lab.alpha};
}

/** The polar form back to OKLab. The hue wraps, so an angle walked past a
 *  full turn needs no fold at the call site. */
inline Oklab oklabOf(const Oklch& lch) {
  const float radians = lch.hueDegrees * (3.14159265358979323846f / 180.0f);
  return {lch.L, lch.chroma * std::cos(radians), lch.chroma * std::sin(radians),
          lch.alpha};
}

/** A colour read in OKLCH, through OKLab. */
inline Oklch toOklch(const Color& c) { return oklchOf(toOklab(c)); }

/** OKLCH back to sRGB, clamped component-wise the way `fromOklab`
 *  clamps. A colour outside what the primaries can mix comes back with
 *  its channels cut to the range — which moves its hue and its
 *  lightness, since three channels are cut by three different amounts.
 *  `fitToSrgb` is the reading that does not. */
inline Color fromOklch(const Oklch& lch) { return fromOklab(oklabOf(lch)); }

/** THE NEAREST COLOUR THE DISPLAY CAN SHOW AT THIS HUE AND LIGHTNESS:
 *  the chroma reduced until the colour is inside the sRGB gamut, and
 *  nothing else touched.
 *
 *  The difference from `fromOklch` is which fact survives. Cutting the
 *  channels keeps as much colour as it can and lets the hue drift, so a
 *  set of colours built by turning one hue comes back as a set at
 *  several hues and several lightnesses — which is exactly what a
 *  harmony, a tone ladder or a hue sweep exists to avoid. Reducing the
 *  chroma keeps the two numbers that were chosen and gives up the one
 *  the display cannot honour.
 *
 *  Found by halving rather than solved: the gamut is a solid with
 *  corners in this space, so there is no closed form for where a hue
 *  leaves it, and the boundary is crossed once along a ray of increasing
 *  chroma. Sixteen halvings put the answer well inside a single step of
 *  an eight-bit channel. */
inline Color fitToSrgb(const Oklch& lch) {
  Oklch fitted = lch;
  fitted.L = std::clamp(fitted.L, 0.0f, 1.0f);
  if (inSrgbGamut(oklabOf(fitted))) return fromOklab(oklabOf(fitted));
  float low = 0.0f, high = fitted.chroma;
  for (int step = 0; step < 16; ++step) {
    const float middle = (low + high) * 0.5f;
    Oklch probe = fitted;
    probe.chroma = middle;
    (inSrgbGamut(oklabOf(probe)) ? low : high) = middle;
  }
  fitted.chroma = low;
  return fromOklab(oklabOf(fitted));
}

/** THE SAME COLOUR AT A DIFFERENT ALPHA — `{c.r, c.g, c.b, a}`, and
 *  nothing else touched.
 *
 *  Kept separate from `scale` deliberately: replacing an alpha and
 *  scaling the colour channels are different operations, and folding
 *  both into one signature would leave a defaulted argument deciding
 *  which of the two the caller meant. */
constexpr Color withAlpha(Color c, float a) { return {c.r, c.g, c.b, a}; }

/** @p c scaled by @p k in every channel, at alpha @p a — or at its own
 *  alpha, which is what a negative @p a asks for. The shading verb a
 *  highlight and a shadow are both written with: one colour, brighter or
 *  darker, at a chosen opacity, and a tone ramp read off one sampled
 *  base when the opacity is not part of the question.
 *
 *  It does NOT clamp. A channel above 1 is a legal float colour and
 *  means something under a wide-gamut or an OCIO view; a renderer clamps
 *  when the colour lands in an eight-bit surface. A caller who needs the
 *  clamped value is asking for a different operation. */
constexpr Color scale(Color c, float k, float a = -1.0f) {
  return {c.r * k, c.g * k, c.b * k, a < 0.0f ? c.a : a};
}

/** THE LADDER UPWARD: @p k added to each colour channel, CLAMPED at 1,
 *  alpha kept — the highlight a lit edge is drawn with.
 *
 *  Clamping is what makes it a different operation from `scale`, not an
 *  inconsistency with it. A scale keeps the hue of what it scales and
 *  has no ceiling to hit; an offset walks every channel toward white and
 *  saturates there, and a caller lightening a nearly-white base wants
 *  the saturated answer rather than a channel above 1 that the next
 *  blend reads as glow. */
constexpr Color lighten(Color c, float k) {
  return {std::min(1.0f, c.r + k), std::min(1.0f, c.g + k),
          std::min(1.0f, c.b + k), c.a};
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
    // Two equal channels stand for one quantity of light, so the mix of
    // them IS that channel and no transfer function can change it. The
    // check is here rather than at the call site because the whole cost
    // of this walk is the curve either side of the mix, and a grey
    // ladder, a single-hue ramp and an alpha-only fade all hand it
    // channels that are already equal.
    if (x == y) return x;
    const float low = srgbToLinear(x);
    return linearToSrgb(low + (srgbToLinear(y) - low) * t);
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
    return t > 216.0f / 24389.0f ? std::cbrt(t)
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

/** AN ORDERED TABLE OF COLOURS READ BY INDEX — the fixed palette, which
 *  is a different thing from a ramp and is not a ramp with more stops.
 *
 *  A ramp says what lies BETWEEN its stops; a palette says there is
 *  nothing between its entries. An indexed picture's colour IS entry n,
 *  and blending entry n with entry n+1 makes a colour the palette does
 *  not contain — which is the one thing a fixed palette exists to
 *  prevent, and what a linear-filtered lookup silently does at every
 *  boundary. So every read here is EXACT: `at()` takes the index, and
 *  `nearest()` takes a unit position and answers the entry it falls in,
 *  never a blend of two.
 *
 *  Out of range CLAMPS rather than wrapping. An index past the end is a
 *  mistake somewhere upstream, and answering the last entry keeps the
 *  mistake visible as a flat band instead of hiding it as a plausible
 *  colour from the other end of the table. */
struct Palette {
  std::vector<Color> entries;

  bool operator==(const Palette&) const = default;
  bool empty() const { return entries.empty(); }
  size_t size() const { return entries.size(); }

  /** Entry @p index exactly, clamped into the table. Transparent black
   *  for an empty palette, which is the only colour a table with no
   *  entries can honestly answer. */
  Color at(int index) const {
    if (entries.empty()) return {0, 0, 0, 0};
    const int last = (int)entries.size() - 1;
    return entries[(size_t)std::clamp(index, 0, last)];
  }

  /** The entry a unit position falls IN — the table divided into equal
   *  bands, `t` at 1 landing on the last one. The reading a normalised
   *  parameter (a height, a heat, a depth) is quantised through. */
  Color nearest(float t) const {
    if (entries.empty()) return {0, 0, 0, 0};
    return at((int)std::floor(t * (float)entries.size()));
  }
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
/** WHERE A POSITION FALLS in a stop list: the two stops it lies between
 *  and how far across them it is. One position on or past an end answers
 *  that end twice at fraction 0, so every reader mixes without a special
 *  case for the ends. Undefined for an empty list — ask that first. */
struct RampBracket {
  size_t low = 0;
  size_t high = 0;
  float fraction = 0.0f;
};

/** The bracket @p t falls in, over stops read in the order given. Every
 *  reading of a ramp goes through this one search — the CPU sample below,
 *  and the ramp value's own read in whatever space it names — so the two
 *  cannot disagree about which stops a position lies between. */
inline RampBracket rampBracket(std::span<const RampStop> stops, float t) {
  const size_t last = stops.size() - 1;
  if (t <= stops.front().pos) return {0, 0, 0.0f};
  if (t >= stops.back().pos) return {last, last, 0.0f};
  for (size_t i = 1; i < stops.size(); ++i) {
    if (t > stops[i].pos) continue;
    const float span = stops[i].pos - stops[i - 1].pos;
    // Two stops at one position are a HARD EDGE, which is what a ramp
    // says a band boundary with: the upper one wins, and dividing by the
    // zero between them would not have said anything.
    return {i - 1, i, span > 0.0f ? (t - stops[i - 1].pos) / span : 1.0f};
  }
  return {last, last, 0.0f};
}

inline Color sampleRamp(std::span<const RampStop> stops, float t) {
  if (stops.empty()) return {0, 0, 0, 0};
  const RampBracket b = rampBracket(stops, t);
  const Color& lo = stops[b.low].color;
  const Color& hi = stops[b.high].color;
  const float f = b.fraction;
  return {lo.r + (hi.r - lo.r) * f, lo.g + (hi.g - lo.g) * f,
          lo.b + (hi.b - lo.b) * f, lo.a + (hi.a - lo.a) * f};
}

}  // namespace sigil::material
