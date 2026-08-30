#pragma once

/** @file
 * Text paint presets: six animated fields a glyph run is painted with —
 * rippling water, a moving mesh gradient, twinkling sparkle, a star-nest
 * raymarch, drifting clouds, a kaleidoscope tunnel — over one ABI of the
 * run's origin and extent, the clock and a slow motion vector; and the
 * two chrome-type ramps, in unit space so the horizon crosses the
 * capitals at half cap height at any size.
 */

#include <include/core/SkRect.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/kit/LayerStyles.h>

#include <glm/vec2.hpp>
#include <memory>
#include <vector>

namespace sigil::material::kit {

/** The ABI every text paint shares: where the run sits, how large it is,
 *  the clock, and a slow two-axis drift derived from it. */
struct TextPaintParams {
  glm::vec2 origin;
  glm::vec2 extent;
  float time;
  glm::vec2 motion;
};

/** The params for a run at @p bounds at @p timeSeconds: the extent is at
 *  least one pixel each way, and the motion is the sine and cosine of two
 *  slow rates. */
TextPaintParams textPaintParams(const SkRect& bounds, float timeSeconds);

/** Animated, rippling blue water with fine caustic highlights. */
Material water(const SkRect& bounds, float timeSeconds);
/** A four-corner mesh-style gradient with softly moving control regions. */
Material meshGradient(const SkRect& bounds, float timeSeconds);
/** A transparent field of independently sized, tinted, twinkling points —
 *  for a Screen or Plus overlay. */
Material sparkle(const SkRect& bounds, float timeSeconds);
/** A volumetric star-nest raymarch: a deep, drifting field of glowing
 *  fractal dust. Heavier than the others (a nested loop). */
Material starNest(const SkRect& bounds, float timeSeconds);
/** Drifting painterly sky and clouds from layered ridged/fbm noise. */
Material clouds(const SkRect& bounds, float timeSeconds);
/** An endless raymarched kaleidoscope tunnel, falling away. */
Material tunnel(const SkRect& bounds, float timeSeconds);

const std::shared_ptr<const Recipe>& waterRecipe();
const std::shared_ptr<const Recipe>& meshGradientRecipe();
const std::shared_ptr<const Recipe>& sparkleRecipe();
const std::shared_ptr<const Recipe>& starNestRecipe();
const std::shared_ptr<const Recipe>& cloudsRecipe();
const std::shared_ptr<const Recipe>& tunnelRecipe();

/** The sunset-chrome ramp in unit space, top to bottom: sky to a hard
 *  horizon to warm ground. */
std::vector<RampStop> sunsetChromeText();
/** The silver-chrome ramp in unit space. */
std::vector<RampStop> silverChromeText();

}  // namespace sigil::material::kit
