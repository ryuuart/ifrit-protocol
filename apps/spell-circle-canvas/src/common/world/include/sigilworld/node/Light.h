#pragma once

/** @file
 * @ingroup world-node
 * A LIGHT: one node type, made by three factories — a directional light,
 * a point light and a spot light. A light lights every body in the scene
 * wherever it stands in the tree; the bodies it reaches are narrowed by
 * its light link and never by nesting.
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
  /** How strongly, as a factor on the colour. */
  motion::Animatable<float> intensity = 1.0f;
  /** The distance at which it has fallen to nothing. */
  float range = 600;
};

/** How a spot light shines, how far it reaches and how wide it opens. */
struct SpotLightOptions {
  /** The colour it shines in, including values above one. */
  motion::Animatable<material::Color> color = material::Color{1, 1, 1, 1};
  /** How strongly, as a factor on the colour. */
  motion::Animatable<float> intensity = 1.0f;
  /** The distance at which it has fallen to nothing. */
  float range = 600;
  /** Degrees from its aim within which it shines at full strength. */
  float innerAngle = 0;
  /** Degrees from its aim beyond which it is dark. */
  float outerAngle = 45;
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

}  // namespace sigil::world
