#pragma once

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

/** SkSL screen treatment. Bind the picture to the content slot, or let
 * a layer effect provide it. The bloom is a second slot an EXECUTOR
 * fills from the same layer blurred at `uBloomRadius`, read once at the
 * warped coordinate. The blur is taken over the whole layer, so
 * something bright outside the bounds lights the glass near that edge,
 * though the light itself lands only inside them.
 *
 * The body spells that slot whatever `uBloom` is, so a fill — which has
 * no layer and no executor — must bind a source to `bloom` as well, at
 * every strength including none; unbound, the material is refused
 * rather than shaded with an empty child. Burn-in requires history and
 * is not part of this stateless recipe. */
Material crt(const CrtParameters& parameters);
const std::shared_ptr<const Recipe>& crtRecipe();
/** Maximum source displacement for an image-filter executor. */
float crtSampleRadius(const CrtParameters& parameters);

}  // namespace sigil::material::field
