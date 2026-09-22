---
kind: type
library: SigilMaterial
name: Color
qualified: sigil::material::Color
group: Colour
status: stable
---

# Color

THE ONE COLOUR VALUE: four straight — not premultiplied — sRGB
components, one float each, exactly four floats in memory, so a parameter
struct holding one mirrors to bytes as a plain float4 uniform. Everything
in this tree that means "a colour" means this, and every other four-float
colour in reach converts into it implicitly.

Beside it sit four RECORDS THAT ARE NOT COLOURS TO PAINT WITH: `Oklab`,
`Oklch`, `Lab` and `LinearRgb`. They are the spaces a colour is REASONED
about in — interpolated, measured, lifted, fitted — and each has a round
trip to and from this type. Nothing paints from one directly.

## Anatomy

`Color::r`, `Color::g`, `Color::b` and `Color::a`, in that order,
straight sRGB, alpha defaulting to 1. A channel above 1 survives: there
is no clamp on the way in, because a value outside the display's range is
still a number a shader may want.

The implicit constructor takes anything matching the `FourFloatColor`
concept — a value with `fR`, `fG`, `fB` and `fA` — which is how a Skia
colour becomes one without this library's leaf including a renderer's
header. It is implicit because the alternative is a conversion spelled by
hand at every call, and a hand-spelled conversion is where a channel
order or an alpha convention drifts silently. This is the one place the
mapping is written.

The four reasoning records:

| Record | What it holds | What it answers |
| --- | --- | --- |
| `Oklab` | `Oklab::L`, `Oklab::a`, `Oklab::b`, `Oklab::alpha` | the space to INTERPOLATE in — even steps, no dark band where two saturated stops cross |
| `Oklch` | `Oklch::L`, `Oklch::chroma`, `Oklch::hueDegrees`, `Oklch::alpha` | the same space in polar form, where a harmony, a tone ladder and a hue-preserving lift are stated |
| `Lab` | `Lab::L`, `Lab::a`, `Lab::b`, `Lab::alpha` | the space to MEASURE in — CIELAB under D65, which is what a published difference is quoted in |
| `LinearRgb` | `LinearRgb::r`, `LinearRgb::g`, `LinearRgb::b` | the light a colour stands for, before the transfer function and before any clamp |

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `Color(r, g, b)` / `Color(r, g, b, a)` | C++ | the components directly |
| `rgb(0x1f2933)` | C++ | a packed sRGB integer — the spelling a palette is authored in, constexpr, so palette constants stay constexpr |
| `hsv(hue, saturation, value)` | C++ | the wheel a palette is WALKED on; the hue wraps, s and v clamp |
| an `SkColor4f`, implicitly | C++ | any `FourFloatColor`, field for field |
| `skia::toColor(colour)` | C++ | the same crossing, spelled, from the Skia side |
| `fromOklab(lab)`, `fromOklch(lch)`, `fromLab(lab)` | C++ | back from a reasoning space, clamped component-wise |
| `fitToSrgb(lch)` | C++ | the nearest colour the display can show at that hue and lightness — the chroma reduced, nothing else touched |
| `withAlpha(c, a)`, `scale(c, k)`, `lighten(c, k)` | C++ | a colour derived from a colour |
| `lerpOklab(a, b, t)`, `mixLinear(a, b, t)`, `mixToward(c, target, t, a)` | C++ | the three mixes |
| `"#1f2933"` | Python | a CSS colour string, implicitly |
| `(0.12, 0.16, 0.20)` / `(0.12, 0.16, 0.20, 0.5)` | Python | a 3- or 4-tuple of unit floats, implicitly |
| `[0.12, 0.16, 0.20]` | Python | a list, implicitly |
| `material.rgb(0x1f2933)`, `material.hsv(...)` | Python | the two authoring spellings |
| `material.Color(0.12, 0.16, 0.20)` | Python | direct |

In Python the whole of that column is the union `ColorLike`, and every
parameter that takes a colour takes every row of it.

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| every `Color`-typed field of a recipe's parameter struct | field | SigilMaterial — it mirrors to bytes as a float4 |
| `RampStop::color` | field | SigilMaterial |
| `Palette::entries` | field | SigilMaterial |
| `harmony`, `rotateHue` | function | SigilMaterial |
| `deltaE`, `luminance`, `toOklab`, `toOklch`, `toLab` | function | SigilMaterial |
| `closestEntry` | function | SigilMaterial |
| `skia::toSkColor` | function | SigilMaterial — the crossing back to a Skia colour |
| `skia::Stop::color`, `skia::Paint::solid`, `skia::Paint::uniform`, `skia::Effect::glow` | field, function | SigilMaterial — the paint model states its colours in this one |

Outside this library a colour is what a fill, an ink, a shadow and a
light are stated in; those slots belong to the libraries that own them
and each takes this value.

## Also returned by

| What | Kind | Library |
| --- | --- | --- |
| `rgb`, `hsv` | function | SigilMaterial |
| `fromOklab`, `fromOklch`, `fromLab`, `fitToSrgb` | function | SigilMaterial |
| `lerpOklab`, `mixLinear`, `mixToward`, `withAlpha`, `scale`, `lighten` | function | SigilMaterial |
| `sampleRamp`, `Ramp::at` | function | SigilMaterial — and a ramp is callable, so the same reading answers wherever an interpolator is taken |
| `Palette::at`, `Palette::nearest` | member | SigilMaterial |
| `skia::toColor` | function | SigilMaterial |

## Description

The four records are separate types rather than tagged colours because
they answer different questions and mixing them silently is the bug the
separation exists to prevent. A ramp walked in `Oklab` is even; the same
ramp walked in sRGB is not. A difference quoted in `Lab` is comparable
with a published one; the same difference read in OKLab is not.

Out of gamut there are two answers and the difference is which fact
survives. `fromOklab` and `fromOklch` cut the channels to the range,
which moves the hue and the lightness both, because three channels are
cut by three different amounts. `fitToSrgb` reduces the CHROMA until the
colour is displayable and touches nothing else, so a set built by turning
one hue comes back at the hues and the weights it was asked for.
`inSrgbGamut` and `linearOf` are the two readings underneath, kept
separate because the numbers BEFORE the clamp are the ones that say
whether a colour is a colour at all.

`hsv` is not a perceptual space and must not be used as one. Its `value`
is the largest channel and nothing else, so a ramp built by moving it
bends in lightness. Interpolate in OKLab; reach for HSV when the
SEPARATION of hues is the point.

## See also

- `color/Color.h` — the header: `Color`, `FourFloatColor`, `rgb`, `hsv`,
  `Oklab`, `Oklch`, `Lab`, `LinearRgb`, `toOklab`, `fromOklab`,
  `lerpOklab`, `toOklch`, `fromOklch`, `fitToSrgb`, `inSrgbGamut`,
  `linearOf`, `toLab`, `fromLab`, `deltaE`, `srgbToLinear`,
  `linearToSrgb`, `withAlpha`, `scale`, `lighten`, `mixToward`,
  `mixLinear`, `luminance`, `RampStop`, `Palette`, `RampBracket`,
  `rampBracket`, `sampleRamp`
- [Ramp](value:sigil::material::Ramp) — the colours between two stops,
  as one value
- [Palette](value:sigil::material::Palette) — the ordered table read by
  index
- [Paint](value:sigil::material::skia::Paint) — what a colour is
  painted WITH
- The colour chapter on the [SigilCompose](doxygen:SigilCompose) site —
  the lattice whole, and where a colour sits in it
