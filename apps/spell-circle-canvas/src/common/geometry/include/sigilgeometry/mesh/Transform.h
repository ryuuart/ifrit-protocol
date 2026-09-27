#pragma once
/** @file
 * @ingroup geometry-mesh
 *
 * A 3D PLACEMENT AS A VALUE: translation, three axis turns in degrees,
 * scale, the origin they turn and scale about, and one turn about an
 * arbitrary axis — and the matrix they describe.
 */
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace sigil::geometry::mesh {

/** Where a body stands, part by part, in a y-up right-handed space. Angles
 *  are degrees. Left alone, it places nothing: the identity. */
struct Transform {
  glm::vec3 translate{0.0f, 0.0f, 0.0f};
  /** Turns about x, y and z, in degrees. */
  glm::vec3 rotateDegrees{0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f, 1.0f, 1.0f};
  /** The point the turns and the scale hold still. */
  glm::vec3 origin{0.0f, 0.0f, 0.0f};
  /** The direction `axisDegrees` turns about. A zero-length axis turns
   *  nothing. */
  glm::vec3 axis{0.0f, 1.0f, 0.0f};
  float axisDegrees = 0.0f;

  /** The matrix this describes: the origin brought to zero, then scale,
   *  then the z, y and x turns in that order, then the axis turn, then the
   *  origin put back, then the translation. Reading it right to left is
   *  reading the order the operations apply in. */
  [[nodiscard]] glm::mat4 matrix() const;

  bool operator==(const Transform&) const = default;
};

}  // namespace sigil::geometry::mesh
