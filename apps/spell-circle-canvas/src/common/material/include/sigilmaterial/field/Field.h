#pragma once

/** @file
 * Shader fields — surfaces evaluated per pixel rather than baked as a
 * tile: the halftone ramp, Perlin noise, luminance grain, and the ripple
 * that resamples a layer through a sine displacement. Every parameter is
 * a uniform; each returns a Material.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>

#include <glm/vec2.hpp>
#include <memory>

namespace sigil::material::field {

/** The halftone ramp's ABI. */
struct HalftoneRampParams {
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

/** The halftone RAMP: dot radius swells from @p rMin at the top of the
 *  box to @p rMax at its bottom, in one pass. Reads the resolution.
 *  @p angleDeg rotates the dot grid; the ramp stays vertical.
 *  @p rampFrom / @p rampTo remap where the swell runs, as fractions of the
 *  height. To DRIFT the field, bind `uDriftX` / `uDriftY`: drift wraps
 *  seamlessly at a period of 2·spacing·√2 px along a 45° grid. Keep rMax
 *  below roughly 0.45·spacing or neighbouring dots fuse. */
Material halftoneRamp(float spacing, float rMin, float rMax, Color color,
                      float angleDeg = 0.0f, float rampFrom = 0.0f,
                      float rampTo = 1.0f);
const std::shared_ptr<const Recipe>& halftoneRampRecipe();

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
struct GrainParams {
  glm::vec2 uFreq;  ///< frequency with the anisotropy folded in
  float uSeed;
  float uContrast;
};

/** LUMINANCE noise — value-noise fBm collapsed to one channel, so a blend
 *  mode over a coloured surface reads as light rather than a hue shift:
 *  paper tooth, film grain, stone veining, worn metal. @p contrast scales
 *  the field about 0.5; @p stretch divides the x frequency and multiplies
 *  the y one, so > 1 runs the fibre lengthwise. Keep
 *  `frequency · stretch · 2^(octaves-1)` under roughly 0.4 or the y axis
 *  aliases. The shader returns its own opaque luminance, so it composites
 *  as that luminance over a transparent base rather than modulating it:
 *  multiply grain over an opaque ground. One recipe per octave count: the
 *  count is a constant in the body, never a uniform a loop breaks
 *  against. */
Material grain(float frequency, int octaves = 4, float seed = 1.0f,
               float contrast = 1.0f, float stretch = 1.0f);
const std::shared_ptr<const Recipe>& grainRecipe(int octaves);

/** The ripple's ABI. */
struct RippleParams {
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
const std::shared_ptr<const Recipe>& rippleRecipe();

}  // namespace sigil::material::field
