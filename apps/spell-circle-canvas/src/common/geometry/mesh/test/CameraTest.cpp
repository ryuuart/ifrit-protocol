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


// The orbit and the camera it stands for are exact inverses, which is
// what lets a control take hold of a camera rather than replace it:
// reading a camera's orbit and moving it by nothing gives that camera
// back.
TEST(Camera, ReadsACameraBackAsTheOrbitThatMakesIt) {
  camera::Camera lens;
  lens.eye = {180, 120, -260};
  lens.target = {10, 30, 5};
  lens.fovYDeg = 32;
  const camera::Orbit orbit = camera::orbitOf(lens);
  EXPECT_NEAR(orbit.distance, glm::length(lens.eye - lens.target), 1e-3f);
  const camera::Camera back = camera::cameraAt(lens, orbit);
  EXPECT_NEAR(back.eye.x, lens.eye.x, 1e-3f);
  EXPECT_NEAR(back.eye.y, lens.eye.y, 1e-3f);
  EXPECT_NEAR(back.eye.z, lens.eye.z, 1e-3f);
  // Everything but where the eye stands is the pivot's own.
  EXPECT_EQ(back.target, lens.target);
  EXPECT_EQ(back.up, lens.up);
  EXPECT_FLOAT_EQ(back.fovYDeg, lens.fovYDeg);
}

// An eye standing on its target names no direction, so there is no orbit
// to report and the whole of it reads zero.
TEST(Camera, ReportsNoOrbitWhereTheEyeStandsOnItsTarget) {
  camera::Camera lens;
  lens.eye = lens.target = {4, 5, 6};
  const camera::Orbit orbit = camera::orbitOf(lens);
  EXPECT_FLOAT_EQ(orbit.distance, 0.0f);
  EXPECT_FLOAT_EQ(orbit.yawDeg, 0.0f);
  EXPECT_FLOAT_EQ(orbit.pitchDeg, 0.0f);
}

// CLIP SPACE IS NOT PIXELS. viewProjection() ends with the viewport step
// and lands on the middle pixel; clipProjection() stops before it, so the
// same point lands on the origin — and the x and y of the two agree once
// the viewport step is undone, because nothing between them turns an axis
// over.
TEST(Camera, ProjectsToClipSpaceWithoutTheViewportStep) {
  camera::Camera cam;
  cam.eye = {0, 0, 100};
  cam.target = {0, 0, 0};
  const glm::mat4 clip = cam.clipProjection({800, 600});
  const glm::vec4 centre = clip * glm::vec4{0, 0, 0, 1};
  EXPECT_NEAR(centre.x / centre.w, 0.0f, 1e-4f);
  EXPECT_NEAR(centre.y / centre.w, 0.0f, 1e-4f);

  const glm::vec4 point{30, 20, 0, 1};
  const glm::vec4 inClip = clip * point;
  const glm::vec4 inPixels = cam.viewProjection({800, 600}) * point;
  EXPECT_NEAR(inClip.x / inClip.w * 400.0f + 400.0f,
              inPixels.x / inPixels.w, 1e-2f);
  EXPECT_NEAR(inClip.y / inClip.w * -300.0f + 300.0f,
              inPixels.y / inPixels.w, 1e-2f);
}

// DEPTH IS THE DEVICE'S WAY ROUND: the projection runs z from one at the
// near plane to minus one at the far one, and a device reads zero to one
// the other way about, so a nearer point must come back with a SMALLER
// depth and both must stand inside the range a device samples.
TEST(Camera, ProjectsDepthTheDeviceWayRound) {
  camera::Camera cam;
  cam.eye = {0, 0, 100};
  cam.target = {0, 0, 0};
  cam.zNear = 4;
  cam.zFar = 1000;
  const glm::mat4 clip = cam.clipProjection({800, 600});
  const glm::vec4 near = clip * glm::vec4{0, 0, 50, 1};
  const glm::vec4 far = clip * glm::vec4{0, 0, -400, 1};
  const float nearDepth = near.z / near.w;
  const float farDepth = far.z / far.w;
  EXPECT_LT(nearDepth, farDepth);
  EXPECT_GE(nearDepth, 0.0f);
  EXPECT_LE(farDepth, 1.0f);
}

}  // namespace

// A quad this wide and this tall, standing square to the view at that
// distance, fills the frame exactly — which is the whole of how a head-up
// overlay maps a texture pixel onto a plate pixel. It is the frustum the
// projection opens, not the one the angle alone describes.
TEST(Camera, TheFrustumExtentIsWhatFillsTheFrameAtThatDistance) {
  camera::Camera cam;
  cam.eye = {0, 0, 0};
  cam.target = {0, 0, -1};
  cam.fovYDeg = 90.0f;  // half-angle 45 degrees: the height is twice the run
  const SkSize at100 = cam.extentAt(100.0f, 2.0f);
  EXPECT_NEAR(at100.height(), 202.0f, 1e-2f);
  EXPECT_NEAR(at100.width(), 404.0f, 1e-2f);  // the aspect is the width's

  // The claim it makes is the one worth checking: a point half that
  // height above the axis lands exactly on the top edge of the viewport,
  // and half that width across lands exactly on the right edge.
  const SkSize viewport{800, 400};
  const std::optional<SkPoint> top =
      cam.project({0, at100.height() * 0.5f, -100.0f}, viewport);
  ASSERT_TRUE(top.has_value());
  EXPECT_NEAR(top->fY, 0.0f, 1e-2f);
  const std::optional<SkPoint> right =
      cam.project({at100.width() * 0.5f, 0, -100.0f}, viewport);
  ASSERT_TRUE(right.has_value());
  EXPECT_NEAR(right->fX, 800.0f, 1e-2f);

  // Twice as far is very nearly twice as wide, and the shortfall is the
  // projection's, not the field of view's.
  const SkSize at200 = cam.extentAt(200.0f, 2.0f);
  EXPECT_NEAR(at200.height(), 402.0f, 1e-2f);
}

// Projection is the vertex's own path to the canvas, so a mark placed by
// it sits on the geometry rather than near it — and a point that is not in
// front of the eye has no place on the canvas at all.
TEST(Camera, ProjectFollowsTheVertexAndDeclinesWhatIsBehindTheEye) {
  camera::Camera cam;
  cam.eye = {0, 0, 100};
  cam.target = {0, 0, 0};
  const SkSize viewport{800, 600};

  const std::optional<SkPoint> center = cam.project({0, 0, 0}, viewport);
  ASSERT_TRUE(center.has_value());
  EXPECT_NEAR(center->fX, 400.0f, 1e-2f);
  EXPECT_NEAR(center->fY, 300.0f, 1e-2f);

  // The same answer the matrix gives, because it is the same matrix.
  const glm::vec4 clip =
      cam.viewProjection(viewport) * glm::vec4{40, 25, -60, 1};
  const std::optional<SkPoint> p = cam.project({40, 25, -60}, viewport);
  ASSERT_TRUE(p.has_value());
  EXPECT_NEAR(p->fX, clip.x / clip.w, 1e-3f);
  EXPECT_NEAR(p->fY, clip.y / clip.w, 1e-3f);
  // y counts down the canvas: something above the target is nearer the top.
  EXPECT_LT(p->fY, center->fY);

  // The eye plane is the boundary: on it and behind it there is no answer,
  // even where the divide would still hand one back.
  EXPECT_FALSE(cam.project({0, 0, 100}, viewport).has_value());
  EXPECT_FALSE(cam.project({0, 0, 100.5f}, viewport).has_value());
  EXPECT_FALSE(cam.project({0, 0, 400}, viewport).has_value());
}
