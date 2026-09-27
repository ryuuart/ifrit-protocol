#pragma once

/** @file
 * @ingroup material-field
 *
 * Shader fields — surfaces evaluated per pixel rather than baked as a
 * tile: the halftone ramp, Perlin noise, luminance grain, and the ripple
 * that resamples a layer through a sine displacement. Every parameter is a
 * uniform; each returns a Material.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/advanced/Recipe.h>

#include <glm/vec2.hpp>
#include <memory>

/** Surfaces evaluated PER PIXEL rather than baked as a tile: the
 *  halftone ramp, Perlin noise, luminance grain, and the ripple. Reach for a field when the surface has no
 *  repeat to exploit, or when a parameter moves every frame and a baked
 *  tile would have to be regenerated; reach for `pattern` when one tile
 *  can be baked once and repeated. */
namespace sigil::material::field {

/** The halftone ramp's ABI. */
struct HalftoneRampParameters {
  float uSpacing;
  float uRMin;
  float uRMax;
  float uAngle;   ///< radians
  float uDriftX;  ///< px — bind for an idle drift
  float uDriftY;
  float uRamp0;  ///< swell band, fractions of height
  float uRamp1;
  Color uColor;
};

/** The halftone RAMP: dot radius swells from @p minimumRadius at the top of the
 *  box to @p maximumRadius at its bottom, in one pass. Reads the resolution.
 *  @p angleDegrees rotates the dot grid; the ramp stays vertical.
 *  @p rampFrom / @p rampTo remap where the swell runs, as fractions of the
 *  height. To DRIFT the field, bind `uDriftX` / `uDriftY`: drift wraps
 *  seamlessly at a period of 2·spacing·√2 px along a 45° grid. Keep maximumRadius
 *  below roughly 0.45·spacing or neighbouring dots fuse. */
Material halftoneRamp(float spacing, float minimumRadius, float maximumRadius, Color color,
                      float angleDegrees = 0.0f, float rampFrom = 0.0f,
                      float rampTo = 1.0f);

/** Perlin fractal noise — Skia's own generator, bound into a recipe that
 *  passes it through, so it fills a slot and compares like any material.
 *  @p frequency is features per px (0.01–0.05 reads as clouds or paper at
 *  UI scale; ~0.9 as film grain); @p turbulence uses the abs-value
 *  variant (sharper, veiny). The three channels are INDEPENDENT fields,
 *  which is right for a displacement source and wrong for grain — see
 *  grain(). */
Material noise(float frequency, int octaves = 4, float seed = 1.0f,
               bool turbulence = false);

/** The grain's ABI. */
struct GrainParameters {
  glm::vec2 uFreq;  ///< frequency with the anisotropy folded in
  float uSeed;
  float uContrast;
};

/** LUMINANCE noise — value-noise fBm collapsed to one channel, so a
 *  blend mode over a coloured surface reads as light rather than a hue
 *  shift. @p contrast scales the field about 0.5; @p stretch divides the
 *  x frequency and multiplies the y one, so above 1 it runs the fibre
 *  lengthwise. One recipe per octave count, the count a constant in the
 *  body rather than a uniform a loop breaks against.
 *  @trap Keep `frequency · stretch · 2^(octaves-1)` under roughly 0.4 or
 *  the y axis aliases. The shader returns its own OPAQUE luminance, so
 *  multiply it over an opaque ground rather than modulating one. */
Material grain(float frequency, int octaves = 4, float seed = 1.0f,
               float contrast = 1.0f, float stretch = 1.0f);
/** grain()'s recipe for @p octaves, defined once per count — the octave
 *  count is a constant in the body, so each count is its own program. */
const std::shared_ptr<const Recipe>& grainRecipe(int octaves);

/** The ripple's ABI. */
struct RippleParameters {
  float uAmp;
  float uFreq;  ///< radians per px
  float uPhase;
  float uVertical;
};

/** The water/heat warp: the child `content` resampled through a sine
 *  displacement — y shifted by a sine of x, or with @p vertical, x by a
 *  sine of y. Water reads at an amplitude of a few percent of the height
 *  with only a couple of waves across it. The content slot is left for
 *  the caller: a renderer binds the layer it warps. */
Material ripple(float amplitudePx, float wavelengthPx, float phase = 0.0f,
                bool vertical = false);
/** ripple()'s recipe, defined once. Declares the `content` slot the warp
 *  resamples. */
const std::shared_ptr<const Recipe>& rippleRecipe();

}  // namespace sigil::material::field

namespace sigil::material {

/** How a noise base is shaped. */
struct NoiseOptions {
  int octaves = 4;
  float seed = 1;
  /** Folded, sharper and veinier: the absolute value of each octave. */
  bool turbulence = false;
  /** One luminance, as film grain, rather than three independent
   *  channels — the choice for a grain layer; a displacement source wants
   *  the channels. */
  bool grain = false;
  /** Grain only: the spread of the luminance around its middle. */
  float contrast = 1;
  /** Grain only: the anisotropy, horizontal over vertical. */
  float stretch = 1;
  bool operator==(const NoiseOptions&) const = default;
};

/** FRACTAL NOISE at @p frequency cycles per local pixel, as a material. */
Material noise(float frequency, NoiseOptions options = {});

}  // namespace sigil::material

