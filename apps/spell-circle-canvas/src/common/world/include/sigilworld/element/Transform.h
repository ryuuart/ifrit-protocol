#pragma once

/** @file
 * @ingroup world-element
 * Where a node's content stands, as lanes that can move: nine placement
 * lanes about an origin of three more, one turn about an arbitrary axis,
 * and the matrix escape that replaces all of them. What the lanes resolve
 * to in a frame is Geometry's `geometry::mesh::Transform`, whose
 * `matrix()` places the node; the lanes are what is added here, since a
 * placement that moves is this library's and a placement that stands is
 * Geometry's.
 */

#include <sigilgeometry/mesh/Transform.h>
#include <sigilmotion/values/Animatable.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <optional>

namespace sigil::world {

/** A node's placement, lane by lane: three of translation, three of
 *  rotation in degrees, three of scale, three for the origin they turn
 *  about, and one axis turn the three axis lanes cannot spell. Every
 *  lane is an `motion::Animatable<float>`.
 *  @trap `matrix` is the escape: a node carrying one is placed by it and
 *  every lane above is ignored. */
struct Transform {
  motion::Animatable<float> translateX{0.0f};
  motion::Animatable<float> translateY{0.0f};
  motion::Animatable<float> translateZ{0.0f};
  motion::Animatable<float> rotateX{0.0f};
  motion::Animatable<float> rotateY{0.0f};
  motion::Animatable<float> rotateZ{0.0f};
  motion::Animatable<float> scaleX{1.0f};
  motion::Animatable<float> scaleY{1.0f};
  motion::Animatable<float> scaleZ{1.0f};
  motion::Animatable<float> originX{0.0f};
  motion::Animatable<float> originY{0.0f};
  motion::Animatable<float> originZ{0.0f};
  /** The direction `axisDegrees` turns about. A zero-length axis turns
   *  nothing. */
  glm::vec3 axis{0.0f, 1.0f, 0.0f};
  motion::Animatable<float> axisDegrees{0.0f};
  std::optional<glm::mat4> matrix;
};

}  // namespace sigil::world
