#pragma once

/** @file
 * Where a node's content stands, as lanes that can move: nine placement
 * lanes about an origin of three more, one turn about an arbitrary axis,
 * and the matrix escape that replaces all of them.
 */

#include <sigilmotion/values/Animatable.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <optional>

namespace sigil::world {

/** A node's placement, lane by lane.
 *
 *  Three lanes of translation, three of rotation in degrees about the x,
 *  y and z axes, three of scale, and three that put the origin those
 *  rotations and scales turn about. `axis` with `axisDegrees` adds one
 *  turn about a direction the three axis lanes cannot spell. Every lane
 *  is a `motion::Animatable<float>`, so each takes a constant, a
 *  transition or a live binding on its own.
 *
 *  `matrix` is the escape: a node carrying one is placed by it and every
 *  lane above is ignored, which is the door for a placement computed
 *  outside this vocabulary. */
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

/** What a Transform's lanes resolved to this frame — the numbers, with
 *  no animation left in them. */
struct TransformValues {
  glm::vec3 translate{0.0f, 0.0f, 0.0f};
  glm::vec3 rotateDegrees{0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f, 1.0f, 1.0f};
  glm::vec3 origin{0.0f, 0.0f, 0.0f};
  glm::vec3 axis{0.0f, 1.0f, 0.0f};
  float axisDegrees = 0.0f;

  bool operator==(const TransformValues&) const = default;
};

/** The local matrix @p values describe: the origin brought to zero, then
 *  scale, then the z, y and x turns in that order, then the axis turn,
 *  then the origin put back, then the translation. Reading it right to
 *  left is reading the order the operations apply in. */
glm::mat4 localMatrix(const TransformValues& values);

}  // namespace sigil::world
