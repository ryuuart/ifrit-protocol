#pragma once

/** @file
 * @ingroup material-field
 *
 * The colour CRT, as the three subjects a screen is made of: the beam
 * that draws a picture on flat coordinates, the light that picture
 * throws, and the glass that bends every reading of both. Each is a
 * recipe of its own, usable over any surface; `crt` is their
 * composition — one program, in which the glass reads the beam.
 */

#include <sigilmaterial/core/Material.h>

#include <glm/vec4.hpp>

namespace sigil::material::field {

/** A screen in local coordinates. Distances are local pixels; strengths
 * are nonnegative. Zero strengths preserve the content inside the bounds.
 * Time is explicit: re-describe to advance noise, flicker and sync.
 * The screen is opaque; transparent source pixels read as black glass. */
struct CrtParameters {
  glm::vec4 uBounds{0, 0, 1, 1};  ///< left, top, width, height
  float uCurvature = 0;
  float uRgbShift = 0;
  float uScanPitch = 3;
  float uRaster = 0;
  float uBloomRadius = 3;  ///< Gaussian sigma of the bloom, local pixels
  float uBloom = 0;
  float uNoise = 0;
  float uVignette = 0;
  float uBrightness = 1;
  float uJitter = 0;
  float uFlicker = 0;
  float uSync = 0;
  float uTime = 0;
};

/** What the tube draws, in the coordinates it draws it in: the picture
 * in the `content` slot read line by line with the guns converging
 * `uRgbShift` apart, rastered at `uScanPitch`, swept by `uJitter` and
 * `uSync`, and carrying the supply's `uFlicker` and the signal's
 * `uNoise` at `uBrightness`. No curvature and no light — those are the
 * glass's and the bloom's. */
struct CrtBeamParameters {
  glm::vec4 uBounds{0, 0, 1, 1};  ///< left, top, width, height
  float uRgbShift = 0;
  float uScanPitch = 3;
  float uRaster = 0;
  float uNoise = 0;
  float uBrightness = 1;
  float uJitter = 0;
  float uFlicker = 0;
  float uSync = 0;
  float uTime = 0;
};

/** The light a picture throws: the layer blurred at `uBloomRadius` and
 * kept inside the bounds. */
struct CrtBloomParameters {
  glm::vec4 uBounds{0, 0, 1, 1};  ///< left, top, width, height
  float uBloomRadius = 3;  ///< Gaussian sigma of the light, local pixels
};

/** The glass over a screen: the barrel at `uCurvature`, the light from
 * the `bloom` slot at `uBloom`, and the corner falloff at `uVignette`.
 * What is under the glass is read ONCE, at the bent coordinate. */
struct CrtGlassParameters {
  glm::vec4 uBounds{0, 0, 1, 1};  ///< left, top, width, height
  float uCurvature = 0;
  float uBloomRadius = 3;  ///< Gaussian sigma of the light, local pixels
  float uBloom = 0;
  float uVignette = 0;
};

/** THE WHOLE SCREEN: the beam's picture, read through the glass, lit by
 * the light it throws — one program, so the glass bends a coordinate
 * once and the beam is drawn at the coordinate it bent to. Bind the
 * picture to the `content` slot, or let a layer effect provide it.
 *
 * The light is a second slot, `bloom`, an EXECUTOR fills from the same
 * layer blurred at `uBloomRadius`, read once at the bent coordinate. The
 * blur is taken over the whole layer, so something bright outside the
 * bounds lights the glass near that edge, though the light itself lands
 * only inside them.
 *
 * The body spells that slot whatever `uBloom` is, so a fill — which has
 * no layer and no executor — must bind a source to `bloom` as well, at
 * every strength including none; unbound, the material is refused
 * rather than shaded with an empty child. Burn-in requires history and
 * is not part of this stateless recipe. */
Material crt(const CrtParameters& parameters);
const std::shared_ptr<const Recipe>& crtRecipe();
/** Maximum source displacement for an image-filter executor: the glass's
 *  and the beam's together. */
float crtSampleRadius(const CrtParameters& parameters);

/** THE SCANLINES ALONE, over the picture in the `content` slot: the
 *  screen's beam with no glass over it, for a flat surface. */
Material crtBeam(const CrtBeamParameters& parameters);
const std::shared_ptr<const Recipe>& crtBeamRecipe();
/** Maximum source displacement for an image-filter executor: how far
 *  the sweep and the guns carry a reading. */
float crtBeamSampleRadius(const CrtBeamParameters& parameters);

/** THE BLOOM SOURCE ALONE: the light a layer throws, as a layer of its
 *  own to add over the picture that threw it. Its one slot, `bloom`, is
 *  filled by an EXECUTOR from the layer blurred, so a fill must bind a
 *  source to it itself. */
Material crtBloom(const CrtBloomParameters& parameters);
const std::shared_ptr<const Recipe>& crtBloomRecipe();

/** THE BARREL ALONE, over the picture in the `content` slot and the
 *  light in the `bloom` slot: the glass with no beam under it, for any
 *  surface that wants a tube's curvature, light and falloff over it.
 *  The `bloom` slot is the executor's as it is for the whole screen,
 *  and a fill owes it a source whatever `uBloom` is. */
Material crtGlass(const CrtGlassParameters& parameters);
const std::shared_ptr<const Recipe>& crtGlassRecipe();
/** Maximum source displacement for an image-filter executor: how far
 *  the bend carries a reading. */
float crtGlassSampleRadius(const CrtGlassParameters& parameters);

}  // namespace sigil::material::field
