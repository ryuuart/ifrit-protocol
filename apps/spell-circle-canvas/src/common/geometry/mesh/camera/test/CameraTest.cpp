/** @file
 * The camera and its transforms: the view-projection carries through to
 * viewport pixels, place() composes translate * rotate * scale, and
 * faceCamera() points a +z face at the eye.
 */

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "sigilgeometry/mesh/camera/Camera.h"

using namespace sigil::geometry::mesh;

// viewProjection() already folds the viewport mapping in, so its output
// divided by w is in PIXELS, not in normalized device coordinates: the world
// origin lands on the middle pixel of an 800x600 canvas rather than on 0,0.
TEST(Camera, ProjectsCenterToViewportCenter) {
  camera::Camera cam;
  cam.eye = {0, 0, 100};
  cam.target = {0, 0, 0};
  const glm::mat4 vp = cam.viewProjection({800, 600});
  const glm::vec4 out = vp * glm::vec4{0, 0, 0, 1};
  EXPECT_NEAR(out.x / out.w, 400.0f, 1e-2f);
  EXPECT_NEAR(out.y / out.w, 300.0f, 1e-2f);
}

// Scale first, then rotate, then translate: a yaw of 90 degrees about +Y
// sends the model's +x axis to world -z, and the translation is applied
// after it rather than being rotated with it.
TEST(Camera, PlaceComposesTranslateRotateScale) {
  const glm::mat4 m = camera::place({10, 0, 0}, 90.0f, 0, 0, 2.0f);
  const glm::vec4 x = m * glm::vec4{1, 0, 0, 1};
  EXPECT_NEAR(x.x, 10.0f, 1e-3f);
  EXPECT_NEAR(x.y, 0.0f, 1e-3f);
  EXPECT_NEAR(x.z, -2.0f, 1e-3f);
  const glm::vec4 origin = m * glm::vec4{0, 0, 0, 1};
  EXPECT_NEAR(origin.x, 10.0f, 1e-3f);
  EXPECT_NEAR(origin.z, 0.0f, 1e-3f);
}

// The billboard basis points the +z face at the eye, so a quad's own
// normal comes out along the eye-to-target direction reversed.
TEST(Camera, FaceCameraPointsPlusZAtTheEye) {
  const glm::mat4 m = camera::faceCamera({0, 0, 100}, {0, 0, 0});
  const glm::vec3 z = glm::vec3(m[2]);
  EXPECT_NEAR(z.x, 0.0f, 1e-5f);
  EXPECT_NEAR(z.y, 0.0f, 1e-5f);
  EXPECT_NEAR(z.z, 1.0f, 1e-5f);
  EXPECT_NEAR(m[3].x, 0.0f, 1e-5f);

  // eye == at is degenerate; the basis falls back to facing +z rather
  // than producing a zero column.
  const glm::mat4 same = camera::faceCamera({5, 5, 5}, {5, 5, 5});
  EXPECT_NEAR(glm::length(glm::vec3(same[2])), 1.0f, 1e-5f);
}
