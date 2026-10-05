#pragma once

/** @file
 * @ingroup world-node
 * A LIGHT: one node type, made by five factories — a directional light,
 * a point light, a spot light, an ambient light and a hemisphere light. A
 * light lights every body in the scene wherever it stands in the tree;
 * the bodies it reaches are narrowed by its light link and never by
 * nesting. What each delivers is `LightDescription`'s to say.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilworld/advanced/Light.h>
#include <sigilworld/advanced/Node.h>
#include <sigilworld/node/Collection.h>
#include <sigilworld/node/Verbs.h>

#include <glm/vec3.hpp>
#include <string>

namespace sigil::world {

/** ONE LIGHT, standing where its node is placed and aimed along a
 *  direction its node's rotation turns. */
class Light : public Node,
              public PlacementVerbs<Light>,
              public IdentityVerbs<Light> {
 public:
  Light() = default;

  /** THE BODIES THIS LIGHT REACHES — USD's light link. Unlinked, it
   *  lights every body in the scene. */
  Light& lightLink(Collection bodies);
  /** …the members of the one collection called @p name. */
  Light& lightLink(std::string name);
  /** THE BODIES THAT BLOCK THIS LIGHT — USD's shadow link. Stored and
   *  written out; nothing casts a shadow yet. */
  Light& shadowLink(Collection blockers);
  /** …the members of the one collection called @p name. */
  Light& shadowLink(std::string name);

  /** What this node describes. */
  [[nodiscard]] const LightDescription& description() const;
};

/** How a directional light shines. */
struct DirectionalLightOptions {
  /** The colour it shines in, including values above one. */
  motion::Animatable<material::Color> color = material::Color{1, 1, 1, 1};
  /** How strongly, as a factor on the colour. */
  motion::Animatable<float> intensity = 1.0f;
};

/** How a point light shines and how far it reaches. */
struct PointLightOptions {
  /** The colour it shines in, including values above one. */
  motion::Animatable<material::Color> color = material::Color{1, 1, 1, 1};
  /** How strongly: what it delivers at a distance of one unit, falling
   *  off with the square of the distance — `intensityAt` states it by
   *  what it delivers further off. */
  motion::Animatable<float> intensity = 1.0f;
  /** Past zero, the distance at which it has been brought smoothly to
   *  nothing; zero reaches everywhere. */
  float range = 0;
};

/** How a spot light shines, how far it reaches and how wide it opens. */
struct SpotLightOptions {
  /** The colour it shines in, including values above one. */
  motion::Animatable<material::Color> color = material::Color{1, 1, 1, 1};
  /** How strongly, as for a point light. */
  motion::Animatable<float> intensity = 1.0f;
  /** Past zero, the distance at which it has been brought smoothly to
   *  nothing; zero reaches everywhere. */
  float range = 0;
  /** Degrees from its aim within which it shines at full strength. */
  float innerAngle = 0;
  /** Degrees from its aim beyond which it is dark. */
  float outerAngle = 45;
};

/** How an ambient light shines: the same from every direction. */
struct AmbientLightOptions {
  motion::Animatable<material::Color> color = material::Color{1, 1, 1, 1};
  /** How strongly, as a factor on the colour: a white matte surface under
   *  an ambient light alone shows this much of its own colour. */
  motion::Animatable<float> intensity = 1.0f;
};

/** How a hemisphere light shines: one colour from above the horizon and
 *  another from below it — a sky and the ground it lights. */
struct HemisphereLightOptions {
  /** The colour from above. */
  motion::Animatable<material::Color> color = material::Color{1, 1, 1, 1};
  /** The colour from below. */
  motion::Animatable<material::Color> groundColor = material::Color{0, 0, 0, 1};
  /** How strongly, as a factor on both colours. */
  motion::Animatable<float> intensity = 1.0f;
};

/** A light the same everywhere, travelling along @p direction toward
 *  what it lights: the sun. */
Light directionalLight(glm::vec3 direction,
                       DirectionalLightOptions options = {});
/** A light shining every way from where its node stands. */
Light pointLight(PointLightOptions options = {});
/** A light shining along @p direction from where its node stands,
 *  within a cone. */
Light spotLight(glm::vec3 direction, SpotLightOptions options = {});
/** A light arriving equally from every direction: what fills the shadows
 *  of a set with no environment. Where its node stands does not matter. */
Light ambientLight(AmbientLightOptions options = {});
/** A light arriving from a sky above and a ground below, about the up
 *  axis of its node — +y, turned by the node's rotation. */
Light hemisphereLight(HemisphereLightOptions options = {});

/** THE INTENSITY a point or spot light needs to deliver @p strength at
 *  @p distance: the strength times the distance squared, which the
 *  inverse square brings back to @p strength there. A rig stated this way
 *  reads in the numbers it lights its subject with, at the distance the
 *  subject stands. */
[[nodiscard]] float intensityAt(float strength, float distance);
/** …for a strength that moves: the light follows it, scaled the same
 *  way. */
[[nodiscard]] motion::Animatable<float> intensityAt(
    motion::Animatable<float> strength, float distance);

}  // namespace sigil::world
