/** @file
 * The camera and the transforms that place geometry in front of it: the
 * view-projection carries through to viewport pixels, place() composes
 * translate * rotate * scale, and faceCamera() turns a +z face toward the
 * eye from wherever the eye stands.
 */

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "sigilgeometry/mesh/camera/Camera.h"

using namespace sigil::geometry::mesh;

namespace {

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

TEST(Camera, FaceCameraPointsTheQuadNormalAtTheEyeFromAnywhere) {
  // faceCamera(eye, at) is the billboard transform: it anchors at `at` and
  // turns a +z face toward the eye, wherever the eye stands. The eye list
  // deliberately includes the cases that break a naive cross-product basis
  // — an eye almost on top of the anchor, and eyes directly above and
  // below it, where the view direction is parallel to the world up vector
  // and the side vector degenerates.
  const glm::vec3 at = {40, -25, 60};
  const glm::vec3 eyes[] = {
      {0, 200, 1150},                    // an ordinary camera, well in front
      {-820, 260, -320}, {40, -25, 61},  // almost on top of the panel
      {40, 900, 60},   // directly ABOVE: dir ≈ +up, degenerate side
      {40, -900, 60},  // directly below: dir ≈ -up
  };
  for (const glm::vec3& eye : eyes) {
    const glm::mat4 m = camera::faceCamera(eye, at);
    // Translation is the anchor.
    EXPECT_NEAR(m[3][0], at.x, 1e-5f);
    EXPECT_NEAR(m[3][1], at.y, 1e-5f);
    EXPECT_NEAR(m[3][2], at.z, 1e-5f);
    // The quad's +z normal lands on the unit eye direction.
    const glm::vec3 n = glm::mat3(m) * glm::vec3{0, 0, 1};
    const glm::vec3 want = glm::normalize(eye - at);
    EXPECT_NEAR(glm::dot(n, want), 1.0f, 1e-5f)
        << "eye " << eye.x << "," << eye.y << "," << eye.z;
    // And the basis stays orthonormal (no shear, no scale).
    const glm::vec3 bx = glm::mat3(m) * glm::vec3{1, 0, 0};
    const glm::vec3 by = glm::mat3(m) * glm::vec3{0, 1, 0};
    EXPECT_NEAR(glm::length(bx), 1.0f, 1e-5f);
    EXPECT_NEAR(glm::length(by), 1.0f, 1e-5f);
    EXPECT_NEAR(glm::dot(bx, by), 0.0f, 1e-5f);
    EXPECT_NEAR(glm::dot(bx, n), 0.0f, 1e-5f);
  }

  // Control: an untransformed quad's normal does NOT already point at an
  // off-axis eye, so the assertion above is capable of failing.
  const glm::vec3 offAxis = glm::normalize(eyes[1] - at);
  EXPECT_LT(glm::dot(glm::vec3{0, 0, 1}, offAxis), 0.99f);
}

TEST(Camera, AnEyeOnTopOfItsAnchorStillFacesSomewhere) {
  // eye == at is degenerate; the basis falls back to facing +z rather
  // than producing a zero column.
  const glm::mat4 same = camera::faceCamera({5, 5, 5}, {5, 5, 5});
  EXPECT_NEAR(glm::length(glm::vec3(same[2])), 1.0f, 1e-5f);
}

}  // namespace
