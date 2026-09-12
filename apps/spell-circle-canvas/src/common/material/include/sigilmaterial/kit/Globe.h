#pragma once

/** @file
 * A GLOBE: a sphere seen orthographically, ruled with a graticule and
 * lit from one direction, generated per pixel over the disc inscribed in
 * the node it fills.
 *
 * The projection is the one a globe on a page has and an attitude
 * indicator has: no perspective, so the disc IS the sphere and the
 * foreshortening toward the limb is the projection's own. The near
 * hemisphere is the only one sampled — the far side of a solid sphere is
 * not visible — and the whole reading is an inverse projection: the pixel
 * to a point on the sphere, that point carried back into the sphere's own
 * frame by undoing heading, pitch and roll, and latitude and longitude
 * read off the result.
 *
 * EVERY LINE IS A PLANE DISTANCE. A meridian is the plane through the
 * poles at its longitude and a parallel the plane at its own sine, so a
 * rule's width is measured in the sphere's space rather than in the
 * angle, and the crowding toward the limb and toward the poles comes out
 * of the arithmetic instead of being drawn.
 *
 * ONE TEXT, BOTH LANGUAGES. The whole reading is written in Slang and
 * crossed into SkSL, and each target's body is that reading plus the one
 * line that spells the return in its own types, so the ball a device
 * shades and the ball a raster surface paints cannot part company.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>

#include <glm/vec4.hpp>
#include <memory>

namespace sigil::material::kit {

/** THE GLOBE'S DIALS: the two hemispheres and how each fades toward its
 *  pole, the attitude, the graticule's three pitches and weights, and the
 *  one light.
 *
 *  `sky` and `ground` are the colours AT THE HORIZON and `skyPole` and
 *  `groundPole` the colours at the poles, so a navball's blue-over-brown
 *  and a cartographer's plain globe are the same four fields.
 *  `horizonBlend` is the half-width, in sphere radii, of the crossfade
 *  between them, which keeps the equator from stepping.
 *
 *  `yaw`, `pitch` and `roll` are RADIANS and turn the sphere under a
 *  fixed eye: yaw spins it about its own poles, pitch tips it toward the
 *  viewer, roll turns the picture. `minorDeg` is the fine graticule's
 *  pitch in degrees, ruled both ways; `meridianDeg` and `parallelDeg` are
 *  the pitches of the heavier rules, which need not agree. `lineWidth` is
 *  the fine rule's width in pixels and scales every rule with it;
 *  `minorWeight`, `majorWeight` and `horizonWeight` are how opaquely each
 *  is laid in `grid`'s colour, and the horizon is drawn last so it reads
 *  over both.
 *
 *  `ambient` and `diffuse` are the light a point keeps at the limb and
 *  the light it gains facing the eye — the falloff that makes the disc
 *  read as a ball. `light` is the direction the specular comes from (its
 *  w is unread), `shininess` its exponent and `specular` its strength.
 *  `edgeFeather` is the limb's antialiasing, in pixels: the alpha the
 *  globe returns falls to nothing across it, so the disc has no jagged
 *  rim and nothing outside it is painted. `fill` is how much of the
 *  inscribed disc the sphere takes, for a globe that has to sit inside a
 *  bezel drawn in the same box. */
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
