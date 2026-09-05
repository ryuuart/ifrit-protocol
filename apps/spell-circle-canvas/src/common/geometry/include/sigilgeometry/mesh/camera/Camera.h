#pragma once

/** @file
 * The camera and the transforms that place things in front of it: a
 * right-handed, y-up view with a vertical field of view, the model
 * matrix helper `place()`, and the billboard transform `faceCamera()`.
 *
 * Camera vectors and the matrices they produce speak glm; `toSkM44()`
 * is the seam where a matrix crosses into Skia. The view and projection
 * are built with Skia's own matrix factories so a point projected here
 * lands exactly where Skia's canvas concat would put it.
 */

#include <include/core/SkM44.h>
#include <include/core/SkSize.h>

#include <glm/glm.hpp>

namespace sigil::geometry::mesh::camera {

/** The glm -> Skia seam: both are column-major, so the conversion is a
 *  straight pour. */
inline SkM44 toSkM44(const glm::mat4& m) { return SkM44::ColMajor(&m[0][0]); }

/** Right-handed, y-up camera. Field of view is vertical. */
struct Camera {
  glm::vec3 eye = {0, 0, 480};
  glm::vec3 target = {0, 0, 0};
  glm::vec3 up = {0, 1, 0};
  float fovYDeg = 40;
  float zNear = 4;
  float zFar = 4096;

  glm::mat4 view() const;
  glm::mat4 projection(float aspect) const;
  /** view -> NDC -> viewport pixels (y flipped back to Skia's y-down). */
  glm::mat4 viewProjection(SkSize viewport) const;
  /** view -> CLIP SPACE, which is what a device wants: the projection
   *  and the view composed without the viewport step `viewProjection`
   *  ends with, and depth put where a device reads it — the projection
   *  runs z from one at the near plane to minus one at the far one, and
   *  a device wants zero to one the other way about. The x and y already
   *  agree, both counting y upward, so nothing turns them over. The
   *  aspect is @p extent's; a device's target is whole pixels. */
  glm::mat4 clipProjection(SkISize extent) const;
};

/** A VIEWPOINT AS A POINTER STATES IT: yaw and pitch in degrees about
 *  the point being looked at, and how far out from it the eye stands. It
 *  is the whole of what a drag can say about where to stand, and it says
 *  nothing about the lens — turning one of these back into a camera
 *  keeps that camera's aim, its up axis and its field of view, so
 *  something driving a viewpoint takes hold of the one it was given
 *  rather than inventing one. Yaw runs about the up axis from the
 *  direction the eye looks along; pitch is positive above the target. */
struct Orbit {
  float yawDeg = 0;
  float pitchDeg = 0;
  float distance = 0;
};

/** THE ORBIT @p camera ALREADY STANDS AT: yaw and pitch in degrees about
 *  its target, and the distance from it. All zero when the eye stands on
 *  the target, where no direction is named. */
[[nodiscard]] Orbit orbitOf(const Camera& camera);

/** @p pivot moved onto @p orbit — the same target, the same up axis and
 *  the same lens, with the eye put where the yaw, the pitch and the
 *  distance say.
 *
 *  It is the exact inverse of `orbitOf`, which is what lets a control
 *  take hold of a camera rather than replace it: reading a camera's
 *  orbit and moving it by nothing gives that camera back. */
[[nodiscard]] Camera cameraAt(const Camera& pivot, Orbit orbit);

/** Model-matrix helpers (row-major reading order: applied right to
 *  left, translate * rotate * scale). */
glm::mat4 place(glm::vec3 position, float yawDeg = 0, float pitchDeg = 0,
                float rollDeg = 0, float scale = 1);

/** The BILLBOARD transform: content placed at @p at with its +z face —
 *  mesh::quad()'s facing convention — pointed at @p eye. Camera math
 *  every runtime wants: a billboard is the same panel re-described each
 *  frame with a fresh faceCamera(), which a reconciler sees as a
 *  transform-only change (setTransform, never a re-upload). The basis
 *  is basisFor — the SAME construction points::instance() and
 *  world's instanced path stamp with, so a faceCamera'd quad and a
 *  facing-lane instance orient identically; Dir≈±up falls back the same
 *  way, and eye==at degenerates to facing +z. */
glm::mat4 faceCamera(glm::vec3 eye, glm::vec3 at, glm::vec3 up = {0, 1, 0});

}  // namespace sigil::geometry::mesh::camera
