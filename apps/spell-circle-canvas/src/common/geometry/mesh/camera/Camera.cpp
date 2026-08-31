/** @file
 * The camera's view and projection, and the two transform helpers that
 * place geometry in front of it.
 */

#include "sigilgeometry/mesh/camera/Camera.h"

#include <cmath>
#include <glm/gtc/type_ptr.hpp>

#include "sigilgeometry/mesh/Vec.h"

namespace sigil::geometry::mesh::camera {

namespace {

constexpr float kDegToRad = (float)M_PI / 180.0f;

/** The other direction of the seam: Skia's camera factories build the
 *  matrix, glm carries it. Both column-major — a straight pour. */
glm::mat4 toGlm(const SkM44& m) {
  float c[16];
  m.getColMajor(c);
  return glm::make_mat4(c);
}

}  // namespace

glm::mat4 Camera::view() const {
  return toGlm(SkM44::LookAt({eye.x, eye.y, eye.z},
                             {target.x, target.y, target.z},
                             {up.x, up.y, up.z}));
}

glm::mat4 Camera::projection(float aspect) const {
  SkM44 m = SkM44::Perspective(zNear, zFar, fovYDeg * kDegToRad);
  if (aspect > 0) m.setRC(0, 0, m.rc(0, 0) / aspect);
  return toGlm(m);
}

glm::mat4 Camera::viewProjection(SkSize viewport) const {
  const float w = viewport.width(), h = viewport.height();
  const float aspect = h > 0 ? w / h : 1;
  // NDC -> pixels, y flipped back to Skia's y-down.
  SkM44 vp = SkM44::Translate(w * 0.5f, h * 0.5f, 0);
  vp.preScale(w * 0.5f, -h * 0.5f, 1);
  SkM44 out = vp;
  out.preConcat(toSkM44(projection(aspect)));
  out.preConcat(toSkM44(view()));
  return toGlm(out);
}

glm::mat4 place(glm::vec3 position, float yawDeg, float pitchDeg, float rollDeg,
                float scale) {
  SkM44 m = SkM44::Translate(position.x, position.y, position.z);
  if (yawDeg != 0) m.preConcat(SkM44::Rotate({0, 1, 0}, yawDeg * kDegToRad));
  if (pitchDeg != 0)
    m.preConcat(SkM44::Rotate({1, 0, 0}, pitchDeg * kDegToRad));
  if (rollDeg != 0) m.preConcat(SkM44::Rotate({0, 0, 1}, rollDeg * kDegToRad));
  if (scale != 1) m.preScale(scale, scale, scale);
  return toGlm(m);
}

glm::mat4 faceCamera(glm::vec3 eye, glm::vec3 at, glm::vec3 up) {
  glm::vec3 x, y, z;
  basisFor(eye - at, up, &x, &y, &z);
  glm::mat4 m{1.0f};
  m[0] = glm::vec4(x, 0);
  m[1] = glm::vec4(y, 0);
  m[2] = glm::vec4(z, 0);
  m[3] = glm::vec4(at, 1);
  return m;
}

}  // namespace sigil::geometry::mesh::camera
