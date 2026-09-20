#pragma once

/** @file
 * @ingroup material-kit
 *
 * A GLOBE: a sphere seen ORTHOGRAPHICALLY, ruled with a graticule and
 * lit from one direction, generated per pixel over the disc inscribed
 * in the node it fills. There is no perspective, so the disc IS the
 * sphere, and only the near hemisphere is sampled. Every line is a
 * PLANE DISTANCE, so the crowding toward the limb and the poles falls
 * out of the arithmetic. The reading is one text, crossed into SkSL.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>

#include <glm/vec4.hpp>
#include <memory>

namespace sigil::material::kit {

/** THE GLOBE'S DIALS: the two hemispheres and how each fades toward its
 *  pole, the attitude, the graticule's three pitches and weights, and
 *  the one light. The hemisphere colours are the ones AT THE HORIZON,
 *  with a second pair at the poles; the attitude is in RADIANS, the
 *  graticule's pitches in DEGREES, and every width and feather in
 *  pixels.
 *  @trap The alpha falls to nothing across `edgeFeather`, so nothing
 *  outside the disc is painted. */
struct GlobeParameters {
  Color sky = {0.24f, 0.48f, 0.71f, 1};
  Color skyPole = {0.12f, 0.30f, 0.49f, 1};
  Color ground = {0.54f, 0.42f, 0.24f, 1};
  Color groundPole = {0.29f, 0.21f, 0.13f, 1};
  Color grid = {1, 1, 1, 1};
  glm::vec4 light = {-0.42f, 0.52f, 0.74f, 0.0f};
  float yaw = 0.0f;    ///< radians
  float pitch = 0.0f;  ///< radians
  float roll = 0.0f;   ///< radians
  float horizonBlend = 0.010f;
  float minorDeg = 10.0f;
  float meridianDeg = 90.0f;
  float parallelDeg = 30.0f;
  float lineWidth = 0.85f;  ///< px
  float minorWeight = 0.34f;
  float majorWeight = 0.62f;
  float horizonWeight = 0.95f;
  float ambient = 0.58f;
  float diffuse = 0.42f;
  float specular = 0.16f;
  float shininess = 14.0f;
  float edgeFeather = 2.2f;  ///< px
  float fill = 1.0f;         ///< the inscribed disc's radius the sphere takes
};

/** The globe recipe, defined once. */
const std::shared_ptr<const Recipe>& globeRecipe();

/** A globe at @p parameters. It reads the node's resolution and fills the
 *  largest disc that fits in it, so the box is what sizes and places the
 *  sphere; bind `yaw`, `pitch` and `roll` to drive an attitude. */
Material globe(const GlobeParameters& parameters = {});

}  // namespace sigil::material::kit
