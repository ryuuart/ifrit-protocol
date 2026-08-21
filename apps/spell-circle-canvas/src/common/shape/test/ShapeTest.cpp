#include <Alembic/Abc/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>
#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "sigilshape/Blend.h"
#include "sigilshape/Curves.h"
#include "sigilshape/Easel.h"
#include "sigilshape/Geometry.h"
#include "sigilshape/Import.h"
#include "sigilshape/Materials.h"
#include "sigilshape/Mesh.h"
#include "sigilshape/Ops.h"
#include "sigilshape/Points.h"
#include "sigilshape/Pop.h"
#include "sigilshape/Save.h"
#include "sigilshape/Space.h"

using namespace sigil::shape;

namespace {

SkPath rect(float x, float y, float w, float h) {
  return SkPath::Rect(SkRect::MakeXYWH(x, y, w, h));
}

}  // namespace

// --- Geometry -------------------------------------------------------------

TEST(Geometry, FlattenRectKeepsCornersAndClosure) {
  const std::vector<Polyline> contours = flatten(rect(0, 0, 100, 50));
  ASSERT_EQ(contours.size(), 1u);
  EXPECT_TRUE(contours[0].closed);
  EXPECT_EQ(contours[0].points.size(), 4u);
  EXPECT_NEAR(contours[0].length(), 300.0f, 1e-3f);
}

TEST(Geometry, FlattenCircleHitsTolerance) {
  const std::vector<Polyline> contours =
      flatten(SkPath::Circle(0, 0, 100), 0.1f);
  ASSERT_EQ(contours.size(), 1u);
  // Flattening emits points sampled ON the curve, so every vertex keeps the
  // radius; the tolerance argument bounds how far the straight chords
  // BETWEEN them may sag inside the arc. That sag is also why the polyline
  // perimeter comes in slightly under the true circumference.
  for (const SkPoint& p : contours[0].points) {
    const float r = std::sqrt(p.fX * p.fX + p.fY * p.fY);
    EXPECT_NEAR(r, 100.0f, 0.5f);
  }
  EXPECT_NEAR(contours[0].length(), 2.0f * (float)M_PI * 100.0f, 4.0f);
}

TEST(Geometry, ResampleUniformSpacing) {
  const std::vector<Sampled> sampled = resample(SkPath::Circle(0, 0, 50), 64);
  ASSERT_EQ(sampled.size(), 1u);
  ASSERT_EQ(sampled[0].points.size(), 64u);
  // resample() spaces its N points evenly along ARC LENGTH, not evenly in
  // the underlying segment parameter — the two differ on any curved path,
  // so a parameter-uniform sampler would show up here as an uneven spread.
  float minD = 1e9f, maxD = 0;
  for (size_t i = 0; i < 64; ++i) {
    const SkPoint a = sampled[0].points[i];
    const SkPoint b = sampled[0].points[(i + 1) % 64];
    const float d = SkPoint::Distance(a, b);
    minD = std::min(minD, d);
    maxD = std::max(maxD, d);
  }
  EXPECT_LT(maxD - minD, 0.6f);
}

// bestAlignment searches for the index offset (and direction) that pairs two
// sampled contours with the least total distance. Blending relies on it: two
// shapes sampled from different start points would otherwise twist through
// the interpolation. A cyclic shift is the case with an exact answer, so it
// is the one that can be checked to within float noise.
TEST(Geometry, AlignmentFindsRotation) {
  Sampled a;
  a.closed = true;
  Sampled b;
  b.closed = true;
  const int n = 16;
  for (int i = 0; i < n; ++i) {
    const float t = (float)i / (float)n * 2.0f * (float)M_PI;
    a.points.push_back({std::cos(t) * 10, std::sin(t) * 10});
  }
  // b is a rotated by 4 slots.
  for (int i = 0; i < n; ++i)
    b.points.push_back(a.points[(size_t)((i + 4) % n)]);
  const Alignment alignment = bestAlignment(a, b);
  const Sampled aligned = applyAlignment(b, alignment);
  for (int i = 0; i < n; ++i) {
    EXPECT_NEAR(aligned.points[(size_t)i].fX, a.points[(size_t)i].fX, 1e-4f);
    EXPECT_NEAR(aligned.points[(size_t)i].fY, a.points[(size_t)i].fY, 1e-4f);
  }
}

// --- Blend ----------------------------------------------------------------

TEST(Blend, EndpointsMatchKeysExactly) {
  blend::Key from{SkPath::Circle(100, 100, 50), {1, 0, 0, 1}};
  blend::Key to{SkPath::Circle(400, 100, 30), {0, 0, 1, 1}};
  blend::Options options;
  options.steps = 3;
  const std::vector<blend::Step> steps = blend::make(from, to, options);
  ASSERT_EQ(steps.size(), 5u);  // 2 keys + 3 intermediates
  EXPECT_EQ(steps.front().t, 0.0f);
  EXPECT_EQ(steps.back().t, 1.0f);
  // Endpoint colors are the key colors (OKLab is identity at t=0/1).
  EXPECT_NEAR(steps.front().fill.fR, 1.0f, 0.01f);
  EXPECT_NEAR(steps.back().fill.fB, 1.0f, 0.01f);
  // Midpoint centroid sits between the keys.
  const SkRect mid = steps[2].path.computeTightBounds();
  EXPECT_NEAR(mid.centerX(), 250.0f, 2.0f);
  EXPECT_NEAR(mid.centerY(), 100.0f, 2.0f);
}

TEST(Blend, SmoothColorScalesWithColorDistance) {
  blend::Key white{SkPath::Circle(0, 0, 10), {1, 1, 1, 1}};
  blend::Key black{SkPath::Circle(100, 0, 10), {0, 0, 0, 1}};
  blend::Key nearWhite{SkPath::Circle(100, 0, 10), {0.95f, 0.95f, 0.95f, 1}};
  blend::Options options;
  options.spacing = blend::Spacing::SmoothColor;
  const size_t far = blend::make(white, black, options).size();
  const size_t near = blend::make(white, nearWhite, options).size();
  // Spacing::SmoothColor picks the step count from the perceptual distance
  // between the two key colours, so that each step is a just-noticeable
  // change: black to white needs a step per 8-bit level, two nearly equal
  // greys need a handful.
  EXPECT_GT(far, 200u);
  EXPECT_LT(near, 40u);
}

TEST(Blend, DistanceSpacingCountsSpineLength) {
  blend::Key from{SkPath::Circle(0, 0, 10), {1, 0, 0, 1}};
  blend::Key to{SkPath::Circle(300, 0, 10), {0, 1, 0, 1}};
  blend::Options options;
  options.spacing = blend::Spacing::Distance;
  options.distance = 50;
  // 300px span / 50px = 6 slots -> 5 intermediates + 2 keys.
  const std::vector<blend::Step> steps = blend::make(from, to, options);
  EXPECT_EQ(steps.size(), 7u);
}

TEST(Blend, OklabMidGrayIsPerceptual) {
  const SkColor4f mid =
      blend::detail::lerpOklab({0, 0, 0, 1}, {1, 1, 1, 1}, 0.5f);
  // OKLab L is cube-root lightness: its black-white midpoint is linear
  // luminance 0.125 = sRGB ~0.389 — well below a naive sRGB lerp's 0.5
  // and far below a linear-light lerp's 0.735.
  EXPECT_NEAR(mid.fR, 0.389f, 0.03f);
  EXPECT_NEAR(mid.fR, mid.fG, 0.01f);
  EXPECT_NEAR(mid.fG, mid.fB, 0.01f);
}

TEST(Blend, SpinePlacesStepsAlongPath) {
  SkPathBuilder spine;
  spine.moveTo({0, 0});
  spine.lineTo({0, 400});  // vertical spine
  blend::Key from{SkPath::Circle(0, 0, 10), {1, 0, 0, 1}};
  blend::Key to{SkPath::Circle(0, 0, 10), {0, 1, 0, 1}};  // same spot
  blend::Options options;
  options.steps = 3;
  options.spine = spine.detach();
  const std::vector<blend::Step> steps = blend::make(from, to, options);
  ASSERT_EQ(steps.size(), 5u);
  // Steps should march down the vertical spine.
  float lastY = -1;
  for (const blend::Step& step : steps) {
    const float y = step.path.computeTightBounds().centerY();
    EXPECT_GT(y, lastY);
    lastY = y;
  }
  EXPECT_NEAR(steps.back().path.computeTightBounds().centerY(), 400.0f, 2.0f);
}

// --- Mesh -----------------------------------------------------------------

TEST(Mesh, ExtrudeRectMakesABox) {
  Mesh m = mesh::extrude(rect(0, 0, 100, 60), {.depth = 20});
  ASSERT_GT(m.vertexCount(), 0u);
  ASSERT_EQ(m.normals.size(), m.vertexCount());
  ASSERT_EQ(m.uvs.size(), m.vertexCount());
  glm::vec3 lo, hi;
  m.bounds(&lo, &hi);
  EXPECT_NEAR(hi.x - lo.x, 100.0f, 1e-3f);
  EXPECT_NEAR(hi.y - lo.y, 60.0f, 1e-3f);
  EXPECT_NEAR(hi.z - lo.z, 20.0f, 1e-3f);
  // 2 caps (2 tris each) + 4 walls (2 tris each) = 12 triangles.
  EXPECT_EQ(m.triangleCount(), 12u);
  // All normals unit length.
  for (const glm::vec3& n : m.normals) EXPECT_NEAR(glm::length(n), 1.0f, 1e-4f);
}

TEST(Mesh, ExtrudeAnnulusKeepsHole) {
  SkPathBuilder ring;
  ring.addCircle(0, 0, 80);
  ring.addCircle(0, 0, 40, SkPathDirection::kCCW);
  Mesh m = mesh::extrude(ring.detach(), {.depth = 10});
  ASSERT_GT(m.triangleCount(), 0u);
  // The hole must survive triangulation, which is checked by area rather
  // than by counting triangles: the tessellation is free to change, the
  // covered area is not. Extrusion centres the profile on z, so the whole
  // front cap sits at z = +depth/2 and can be picked out by that alone.
  // The 3% slack absorbs the circle being flattened to a polygon, which
  // always under-measures.
  double area = 0;
  for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
    const glm::vec3& a = m.positions[m.indices[t]];
    const glm::vec3& b = m.positions[m.indices[t + 1]];
    const glm::vec3& c = m.positions[m.indices[t + 2]];
    if (a.z > 4.9f && b.z > 4.9f && c.z > 4.9f) {
      const glm::vec3 ab = b - a, ac = c - a;
      area += 0.5 * std::abs((double)ab.x * ac.y - (double)ab.y * ac.x);
    }
  }
  const double expected = M_PI * (80.0 * 80.0 - 40.0 * 40.0);
  EXPECT_NEAR(area / expected, 1.0, 0.03);
}

TEST(Mesh, GridUvAndIndicesCoherent) {
  Mesh m = mesh::grid(
      4, 3, [](float u, float v) -> glm::vec3 { return {u * 10, v * 10, 0}; });
  EXPECT_EQ(m.vertexCount(), 12u);
  EXPECT_EQ(m.triangleCount(), 12u);  // 3x2 cells * 2
  // UV origin is TOP-left, matching how images are addressed, while the
  // generator's v parameter runs bottom-up across the sheet. The two are
  // therefore opposite: v = 0 in the generator lands at uv.y = 1. Get this
  // backwards and every texture on a generated surface is upside down.
  EXPECT_EQ(m.uvs.front().x, 0.0f);
  EXPECT_EQ(m.uvs.front().y, 1.0f);
  EXPECT_EQ(m.uvs.back().y, 0.0f);
  for (uint32_t i : m.indices) EXPECT_LT(i, m.vertexCount());
}

TEST(Mesh, TorusNormalsPointOutward) {
  Mesh m = mesh::torus(100, 30, 32, 16);
  // Parameterization convention: the first vertex is u = v = 0, which is on
  // the outer equator facing +x, so its normal points outward along +x. An
  // inward-facing generator would put the normal at -x and light the torus
  // inside out.
  const glm::vec3 n0 = m.normals.front();
  EXPECT_GT(std::abs(n0.x), 0.7f);
  for (const glm::vec3& n : m.normals) EXPECT_NEAR(glm::length(n), 1.0f, 1e-3f);
}

TEST(Mesh, TransformMovesBoundsAndKeepsUnitNormals) {
  Mesh m = mesh::quad(10, 10);
  m.transform(glm::translate(glm::mat4(1.0f), {5, 0, 0}));
  glm::vec3 lo, hi;
  m.bounds(&lo, &hi);
  EXPECT_NEAR((lo.x + hi.x) * 0.5f, 5.0f, 1e-4f);
  for (const glm::vec3& n : m.normals) EXPECT_NEAR(glm::length(n), 1.0f, 1e-4f);
}

TEST(Mesh, TransformRotatesNormalsForward) {
  // Normals transform by the inverse transpose of the model matrix, and it
  // must be applied in row form. Dotting the columns instead is the plain
  // inverse, which rotates normals BACKWARDS — a +x normal under a +90
  // degree turn about z would come out at {0,-1,0} instead of {0,1,0}.
  Mesh m = mesh::quad(10, 10);
  m.normals.assign(m.vertexCount(), {1, 0, 0});
  m.transform(glm::rotate(glm::mat4(1.0f), (float)M_PI * 0.5f, {0, 0, 1}));
  for (const glm::vec3& n : m.normals) {
    EXPECT_NEAR(n.x, 0.0f, 1e-4f);
    EXPECT_NEAR(n.y, 1.0f, 1e-4f);
    EXPECT_NEAR(n.z, 0.0f, 1e-4f);
  }
  // Non-uniform scale keeps inverse-transpose semantics: a normal
  // along the scaled axis renormalizes back to itself.
  Mesh s = mesh::quad(10, 10);
  s.normals.assign(s.vertexCount(), {1, 0, 0});
  s.transform(glm::scale(glm::mat4(1.0f), {2, 1, 1}));
  for (const glm::vec3& n : s.normals) {
    EXPECT_NEAR(n.x, 1.0f, 1e-4f);
    EXPECT_NEAR(n.y, 0.0f, 1e-4f);
    EXPECT_NEAR(n.z, 0.0f, 1e-4f);
  }
}

TEST(Mesh, AppendKeepsNormalAndUvLanesSizedToPositions) {
  // Invariant across append: an optional vertex lane ends up either absent
  // on both sides or sized to positions on the merged mesh. Consumers read
  // "lane size == positions size" as the presence bit for the whole mesh —
  // space::drawMesh takes exactly that comparison as its hasNormals test —
  // so a merge that left the lane short would turn lighting off for every
  // vertex, not only for the ones that arrived without normals.
  //
  // The mismatch is easy to author by accident: points::instance copies
  // only the lanes the stamp actually has, so a stamp of bare positions and
  // indices instances into a mesh with no normals and no uvs at all.
  Mesh stamp;  // a bare triangle: no normals, no uvs
  stamp.positions = {{-2, -2, 0}, {2, -2, 0}, {0, 2, 0}};
  stamp.indices = {0, 1, 2};
  const Cloud cloud = points::ring({0, 0, 0}, 50, 4);
  const Mesh flakes = points::instance(cloud, stamp);
  ASSERT_TRUE(flakes.normals.empty()) << "the real source of the defect";
  ASSERT_TRUE(flakes.uvs.empty());

  Mesh lit = mesh::torus(40, 8, 12, 8);
  ASSERT_EQ(lit.normals.size(), lit.positions.size());
  ASSERT_EQ(lit.uvs.size(), lit.positions.size());
  const size_t litVerts = lit.positions.size();
  const glm::vec3 keptNormal = lit.normals.front();
  const glm::vec2 keptUv = lit.uvs.front();

  lit.append(flakes);
  EXPECT_EQ(lit.normals.size(), lit.positions.size()) << "hasNormals";
  EXPECT_EQ(lit.uvs.size(), lit.positions.size());
  EXPECT_NEAR(glm::length(lit.normals.front() - keptNormal), 0.0f, 1e-6f);
  EXPECT_NEAR(glm::length(lit.uvs.front() - keptUv), 0.0f, 1e-6f);
  // The padded half carries a UNIT +Z normal, never a degenerate zero:
  // zero shades black and never recovers through Mesh::transform.
  for (size_t i = litVerts; i < lit.normals.size(); ++i) {
    EXPECT_NEAR(glm::length(lit.normals[i]), 1.0f, 1e-6f) << "vertex " << i;
    EXPECT_NEAR(lit.normals[i].z, 1.0f, 1e-6f);
    EXPECT_NEAR(lit.uvs[i].x, 0.0f, 1e-6f);
    EXPECT_NEAR(lit.uvs[i].y, 0.0f, 1e-6f);
  }

  // Same in the other order — the pad has to land at the FRONT.
  Mesh reversed = points::instance(cloud, stamp);
  const size_t flakeVerts = reversed.positions.size();
  reversed.append(mesh::torus(40, 8, 12, 8));
  ASSERT_EQ(reversed.normals.size(), reversed.positions.size());
  ASSERT_EQ(reversed.uvs.size(), reversed.positions.size());
  for (size_t i = 0; i < flakeVerts; ++i)
    EXPECT_NEAR(glm::length(reversed.normals[i]), 1.0f, 1e-6f);

  // Neither side authoring a lane still means NO lane: append pads an
  // existing lane, it does not conjure one onto a flat merge.
  Mesh bare = points::instance(cloud, stamp);
  bare.append(points::instance(cloud, stamp));
  EXPECT_TRUE(bare.normals.empty());
  EXPECT_TRUE(bare.uvs.empty());
}

TEST(Mesh, AppendRepairsShortIncomingLanes) {
  // A lane can also arrive SHORTER than its own mesh's element count —
  // hand-built meshes and imported files do this, e.g. a PLY whose extra
  // property list runs out early. Append repairs both sides, because a
  // plain insert would leave the MERGED lane undersized, and every consumer
  // reads "lane sized to positions" (or to triangleCount, for prim lanes)
  // as the presence bit for the whole mesh: one short lane on one side
  // would switch tinting, lighting or texturing off for the merged result.
  Mesh a;
  a.positions = {{-50, -50, 0}, {50, -50, 0}, {50, 50, 0}, {-50, 50, 0}};
  a.indices = {0, 1, 2, 0, 2, 3};
  a.colors.assign(4, glm::vec4{1, 0, 0, 1});
  a.normals.assign(4, glm::vec3{0, 0, 1});
  a.uvs.assign(4, glm::vec2{0.25f, 0.5f});

  Mesh b;  // 4 vertices / 2 triangles, but every lane one entry short
  b.positions = {{0, 0, 10}, {10, 0, 10}, {10, 10, 10}, {0, 10, 10}};
  b.indices = {0, 1, 2, 0, 2, 3};
  b.colors = {{0, 1, 0, 1}, {0, 1, 0, 1}, {0, 1, 0, 1}};
  b.normals = {{1, 0, 0}, {1, 0, 0}, {1, 0, 0}};
  b.uvs = {{1, 1}, {1, 1}, {1, 1}};
  b.prims["heat"] = {{5, 0, 0, 0}};  // 1 entry for 2 triangles

  a.append(b);
  ASSERT_EQ(a.positions.size(), 8u);
  EXPECT_EQ(a.colors.size(), a.positions.size()) << "short colors lane";
  EXPECT_EQ(a.normals.size(), a.positions.size());
  EXPECT_EQ(a.uvs.size(), a.positions.size());
  ASSERT_EQ(a.colors.size(), 8u);
  // Ours survived, theirs landed at the right run, and the hole pads
  // WHITE, which is the identity for a colour lane that multiplies into
  // the base colour. Padding black would darken the merged half instead.
  EXPECT_EQ(a.colors[0], (glm::vec4{1, 0, 0, 1}));
  EXPECT_EQ(a.colors[4], (glm::vec4{0, 1, 0, 1}));
  EXPECT_EQ(a.colors[7], (glm::vec4{1, 1, 1, 1})) << "pad stays white";
  EXPECT_NEAR(a.normals[7].z, 1.0f, 1e-6f);  // +Z, never a zero normal
  EXPECT_NEAR(a.uvs[7].x, 0.0f, 1e-6f);
  EXPECT_NEAR(a.uvs[7].y, 0.0f, 1e-6f);
  // Prim lanes are counted per TRIANGLE rather than per vertex, and are
  // padded the same way, by the convention their lane name implies.
  ASSERT_EQ(a.triangleCount(), 4u);
  const std::vector<glm::vec4>* heat = a.primIf("heat");
  ASSERT_TRUE(heat);
  ASSERT_EQ(heat->size(), 4u) << "short incoming prim lane";
  EXPECT_FLOAT_EQ((*heat)[2].x, 5.0f);  // theirs
  EXPECT_FLOAT_EQ((*heat)[3].x, 0.0f);  // padded by name

  // Mirror case: the short lane belongs to the receiving mesh. It is padded
  // up to the old element count first, so the incoming values still land at
  // the offset the merged mesh expects.
  Mesh shortSide;
  shortSide.positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}};
  shortSide.indices = {0, 1, 2};
  shortSide.colors = {{0, 0, 1, 1}};  // 1 entry for 3 vertices
  Mesh full;
  full.positions = {{5, 0, 0}, {6, 0, 0}, {6, 1, 0}};
  full.indices = {0, 1, 2};
  full.colors.assign(3, glm::vec4{1, 1, 0, 1});
  shortSide.append(full);
  ASSERT_EQ(shortSide.colors.size(), 6u);
  EXPECT_EQ(shortSide.colors[1], (glm::vec4{1, 1, 1, 1}));
  EXPECT_EQ(shortSide.colors[3], (glm::vec4{1, 1, 0, 1}));
}

// --- Space ----------------------------------------------------------------

// viewProjection() already folds the viewport mapping in, so its output
// divided by w is in PIXELS, not in normalized device coordinates: the world
// origin lands on the middle pixel of an 800x600 canvas rather than on 0,0.
TEST(Space, CameraProjectsCenterToViewportCenter) {
  space::Camera camera;
  camera.eye = {0, 0, 100};
  camera.target = {0, 0, 0};
  const glm::mat4 vp = camera.viewProjection({800, 600});
  const glm::vec4 out = vp * glm::vec4{0, 0, 0, 1};
  EXPECT_NEAR(out.x / out.w, 400.0f, 1e-2f);
  EXPECT_NEAR(out.y / out.w, 300.0f, 1e-2f);
}

TEST(Space, FaceCameraPointsTheQuadNormalAtTheEye) {
  // faceCamera(eye, at) is the billboard transform: it anchors at @p at and
  // turns mesh::quad's +z face toward the eye, wherever the eye stands. The
  // eye list deliberately includes the cases that break a naive
  // cross-product basis — an eye almost on top of the anchor, and eyes
  // directly above and below it, where the view direction is parallel to
  // the world up vector and the side vector degenerates.
  const glm::vec3 at = {40, -25, 60};
  const glm::vec3 eyes[] = {
      {0, 200, 1150},                    // an ordinary camera, well in front
      {-820, 260, -320}, {40, -25, 61},  // almost on top of the panel
      {40, 900, 60},   // directly ABOVE: dir ≈ +up, degenerate side
      {40, -900, 60},  // directly below: dir ≈ -up
  };
  for (const glm::vec3& eye : eyes) {
    const glm::mat4 m = space::faceCamera(eye, at);
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

  // The two ways of orienting a quad must agree: a one-point cloud whose
  // "facing" lane holds the eye direction, stamped through points::quads(),
  // produces the same vertices as transforming a quad by faceCamera. If they
  // drift apart, a scene mixing billboards with instanced panels shows two
  // different orientations for the same direction.
  const glm::vec3 eye = eyes[0];
  Cloud one;
  one.positions = {at};
  one.vector("facing") = {glm::normalize(eye - at)};
  points::InstanceOptions options;
  options.orientLane = "facing";
  const Mesh stamped = points::quads(one, 170, 112, options);
  const Mesh quad = mesh::quad(170, 112);
  const glm::mat4 m = space::faceCamera(eye, at);
  ASSERT_EQ(stamped.positions.size(), quad.positions.size());
  for (size_t i = 0; i < quad.positions.size(); ++i) {
    const glm::vec3 viaMatrix = glm::vec3(m * glm::vec4(quad.positions[i], 1));
    EXPECT_NEAR(glm::length(viaMatrix - stamped.positions[i]), 0.0f, 1e-4f)
        << "vertex " << i;
  }
}

TEST(Space, DrawMeshCoversPixels) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 150));
  surface->getCanvas()->clear(SK_ColorBLACK);
  space::Camera camera;
  camera.eye = {0, 0, 300};
  space::MeshStyle style;
  style.baseColor = {1, 0, 0, 1};
  space::drawMesh(*surface->getCanvas(), sigil::shape::mesh::quad(100, 100),
                  glm::mat4(1.0f), camera, {200, 150}, style);
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
  // A smoke check that the whole painter pipeline reaches pixels: transform,
  // lighting and SkVertices batching all have to work for the middle of a
  // face-on quad to come out lit rather than the cleared black.
  const SkColor c = bm.getColor(100, 75);
  EXPECT_GT(SkColorGetR(c), 40u);
}

TEST(Space, NormalsModeEncodesDeviceSpaceYDown) {
  // The Normals G-buffer is DEVICE-space, +y down (the Materials.h
  // convention): rgb = (n.x, -n.y, n.z) * 0.5 + 0.5.
  space::Camera camera;
  camera.eye = {0, 0, 300};
  space::MeshStyle style;
  style.mode = space::MeshStyle::Mode::Normals;

  // Face-on quad: its +z normal encodes as (128, 128, 255).
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 150));
  surface->getCanvas()->clear(SK_ColorBLACK);
  space::drawMesh(*surface->getCanvas(), sigil::shape::mesh::quad(100, 100),
                  glm::mat4(1.0f), camera, {200, 150}, style);
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
  const SkColor faceOn = bm.getColor(100, 75);
  EXPECT_NEAR(SkColorGetR(faceOn), 128, 2);
  EXPECT_NEAR(SkColorGetG(faceOn), 128, 2);
  EXPECT_NEAR(SkColorGetB(faceOn), 255, 2);

  // Tilt the quad so its normal points toward world +y (screen-up).
  // Under +y down that encodes BELOW mid-grey green; a view-space
  // no-flip buffer would put it above.
  sk_sp<SkSurface> tilted =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 150));
  tilted->getCanvas()->clear(SK_ColorBLACK);
  const glm::mat4 model =
      glm::rotate(glm::mat4(1.0f), glm::radians(-45.0f), glm::vec3{1, 0, 0});
  space::drawMesh(*tilted->getCanvas(), sigil::shape::mesh::quad(100, 100),
                  model, camera, {200, 150}, style);
  SkBitmap tiltedBm;
  tiltedBm.allocPixels(tilted->imageInfo());
  ASSERT_TRUE(tilted->readPixels(tiltedBm.pixmap(), 0, 0));
  EXPECT_LT(SkColorGetG(tiltedBm.getColor(100, 75)), 128u);
}

// --- Materials ------------------------------------------------------------

TEST(Materials, EffectsCompileAndShade) {
  const materials::Environment env = materials::Environment::studio(128);
  ASSERT_TRUE(env.valid());
  const SkPath shape = SkPath::Circle(40, 40, 30);
  sk_sp<SkImage> normals =
      materials::bevelNormals(shape, SkIRect::MakeWH(80, 80), 6);
  ASSERT_TRUE(normals);
  EXPECT_TRUE(materials::gold(normals, env, {0, 0}));
  EXPECT_TRUE(materials::chrome(normals, env, {0, 0}));
  sk_sp<SkImage> backdrop;
  {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(80, 80));
    s->getCanvas()->clear(SK_ColorCYAN);
    backdrop = s->makeImageSnapshot();
  }
  EXPECT_TRUE(materials::glass(normals, env, backdrop, {0, 0}));
}

TEST(Materials, DrawChromeShadesInsideShapeOnly) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 120));
  surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  const materials::Environment env = materials::Environment::studio(128);
  materials::drawChrome(*surface->getCanvas(), SkPath::Circle(60, 60, 40), env,
                        8);
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
  // The shader is clipped to the path: a material fills its shape and
  // leaves the rest of the canvas at whatever was already there. Checked on
  // alpha so it holds whatever colour the environment happens to reflect.
  EXPECT_NE(bm.getColor(60, 60) & 0xff000000, 0u);  // inside: painted
  EXPECT_EQ(bm.getColor(5, 5) & 0xff000000, 0u);    // outside: untouched
}

TEST(Materials, BevelNormalsFlatInteriorTiltedRim) {
  const SkPath shape = SkPath::Circle(50, 50, 40);
  sk_sp<SkImage> img =
      materials::bevelNormals(shape, SkIRect::MakeWH(100, 100), 10);
  ASSERT_TRUE(img);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(100, 100));
  ASSERT_TRUE(img->readPixels(nullptr, bm.pixmap(), 0, 0));
  // Normal-map encoding: rgb = n * 0.5 + 0.5, so a flat normal pointing
  // straight out of the surface is (128, 128, 255) and the mid-grey 128 is
  // the zero of each axis. The interior of a bevel is flat.
  const SkColor center = bm.getColor(50, 50);
  EXPECT_GT(SkColorGetB(center), 240u);
  EXPECT_NEAR(SkColorGetR(center), 128, 6);
  // x runs to the right, so the LEFT rim tilts toward -x and its red
  // channel drops below the 128 zero point. A sign flip here would light
  // every bevelled shape from the wrong side.
  const SkColor rim = bm.getColor(13, 50);
  EXPECT_LT(SkColorGetR(rim), 110u);
}

TEST(Materials, EnvironmentRoughnessBlursAndCaches) {
  const materials::Environment env = materials::Environment::sunset(128);
  sk_sp<SkImage> sharp = env.image(0);
  sk_sp<SkImage> rough = env.image(0.6f);
  ASSERT_TRUE(sharp);
  ASSERT_TRUE(rough);
  EXPECT_NE(sharp.get(), rough.get());
  EXPECT_EQ(rough->width(), sharp->width());
  // Roughness is quantized into buckets and each bucket's blurred image is
  // built once and kept, so asking twice for the same roughness returns the
  // identical object rather than re-blurring the environment per draw.
  EXPECT_EQ(env.image(0.6f).get(), rough.get());
}

// --- Ops ------------------------------------------------------------------

TEST(Ops, PathfinderBooleans) {
  const SkPath a = rect(0, 0, 100, 100);
  const SkPath b = rect(50, 0, 100, 100);
  EXPECT_NEAR(ops::unite(a, b).computeTightBounds().width(), 150, 1e-3);
  EXPECT_NEAR(ops::subtract(a, b).computeTightBounds().width(), 50, 1e-3);
  EXPECT_NEAR(ops::intersect(a, b).computeTightBounds().width(), 50, 1e-3);
  // Exclude keeps the union's outline but removes the overlap, so bounds
  // alone cannot tell it from unite — the interior probe is what separates
  // them.
  const SkPath xr = ops::exclude(a, b);
  EXPECT_NEAR(xr.computeTightBounds().width(), 150, 1e-3);
  EXPECT_FALSE(xr.contains(75, 50));  // the overlap is punched out
  EXPECT_TRUE(ops::unite({a, b, rect(140, 0, 100, 100)}).contains(200, 50));
}

// Offset distance is a radius, not a diameter: a positive amount grows the
// outline by that much on every side, a negative one eats into it, so a
// circle of radius 50 offset by 10 spans 120 across and by -15 spans 70.
TEST(Ops, OffsetGrowsAndShrinks) {
  const SkPath circle = SkPath::Circle(0, 0, 50);
  const SkRect grown = ops::offset(circle, 10).computeTightBounds();
  EXPECT_NEAR(grown.width(), 120, 1.5f);
  const SkRect shrunk = ops::offset(circle, -15).computeTightBounds();
  EXPECT_NEAR(shrunk.width(), 70, 1.5f);
}

// Distorts are shape-preserving in the large: they displace the outline but
// must not translate the shape or run away in size. Each bound below is the
// distort's own amplitude budget — Roughen's amplitude of 6 can move a point
// at most 6 outward, so the width cannot exceed the diameter plus twice that.
TEST(Ops, DistortsKeepBoundsSane) {
  const SkPath base = SkPath::Circle(100, 100, 60);
  const SkRect roughened =
      ops::Roughen{6, 8, 42}.apply(base).computeTightBounds();
  EXPECT_LT(std::abs(roughened.centerX() - 100), 4);
  EXPECT_LT(roughened.width(), 120 + 2 * 6 + 2);
  const SkPath twirled = ops::Twirl{90}.apply(base);
  EXPECT_LT(std::abs(twirled.computeTightBounds().centerX() - 100), 4);
  const SkRect bloated =
      ops::PuckerBloat{0.8f}.apply(base).computeTightBounds();
  EXPECT_GT(bloated.width(), 118);  // bloat pushes outward, never inward
  const SkPath chained =
      ops::chain({ops::offsetBy(6), ops::Zigzag{4, 20}})(base);
  EXPECT_FALSE(chained.isEmpty());
}

// --- Curves ---------------------------------------------------------------

TEST(Curves, SplineInterpolatesEndpointsAndLength) {
  Spline3 line;
  line.type = Spline3::Type::Linear;
  line.points = {{0, 0, 0}, {100, 0, 0}};
  EXPECT_NEAR(line.length(), 100, 1e-2);
  const glm::vec3 mid = line.position(0.5f);
  EXPECT_NEAR(mid.x, 50, 1e-3);

  Spline3 spline;
  spline.points = {{0, 0, 0}, {50, 40, 0}, {100, 0, 0}, {150, -40, 0}};
  const glm::vec3 start = spline.position(0);
  const glm::vec3 end = spline.position(1);
  // The default spline type INTERPOLATES its points (Catmull-Rom) rather
  // than approximating them like a B-spline, so t=0 and t=1 land exactly on
  // the first and last authored point.
  EXPECT_NEAR(start.x, 0, 1e-3);
  EXPECT_NEAR(end.x, 150, 1e-3);
}

TEST(Curves, ArcLengthSamplingIsEven) {
  // Knots deliberately bunched at one end: sampling by curve PARAMETER
  // would put three of eleven beads inside the first 10 units, while
  // sampling by arc length spreads them evenly over the full 200. Linear
  // segments keep the path straight so spacing is the only variable.
  Spline3 spline;
  spline.type = Spline3::Type::Linear;
  spline.points = {{0, 0, 0}, {5, 0, 0}, {10, 0, 0}, {200, 0, 0}};
  const std::vector<glm::vec3> beads = spline.sampleArcLength(11);
  ASSERT_EQ(beads.size(), 11u);
  for (size_t i = 1; i < beads.size(); ++i)
    EXPECT_NEAR(beads[i].x - beads[i - 1].x, 20, 1.5f);
}

TEST(Curves, FramesStayOrthonormalAndContinuous) {
  Spline3 knot;
  knot.closed = true;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    knot.points.push_back(
        {std::cos(a) * 100, std::sin(a * 2) * 40, std::sin(a) * 100});
  }
  const std::vector<Frame3> rail = curves::frames(knot, 64);
  ASSERT_EQ(rail.size(), 64u);
  for (size_t i = 0; i < rail.size(); ++i) {
    const Frame3& f = rail[i];
    EXPECT_NEAR(glm::length(f.tangent), 1, 1e-3);
    EXPECT_NEAR(glm::length(f.normal), 1, 1e-3);
    EXPECT_NEAR(glm::dot(f.tangent, f.normal), 0, 1e-3);
    // Frames are carried along the curve by parallel transport, so each
    // normal is the previous one rotated as little as the tangent allows.
    // A frame built independently per sample (from a fixed up vector, say)
    // would flip near vertical tangents and twist any swept surface.
    if (i > 0) EXPECT_GT(glm::dot(f.normal, rail[i - 1].normal), 0.5f);
  }
}

TEST(Curves, TubeAndRibbonAreWellFormed) {
  Spline3 arc;
  arc.points = {{0, 0, 0}, {60, 60, 0}, {120, 0, 0}};
  const Mesh t = curves::tube(arc, {.radius = 8, .segments = 24, .sides = 8});
  EXPECT_GT(t.triangleCount(), 0u);
  EXPECT_EQ(t.normals.size(), t.vertexCount());
  for (const glm::vec3& n : t.normals) EXPECT_NEAR(glm::length(n), 1, 1e-3);
  // A ribbon is a strip: `segments` cross-sections of two vertices each,
  // and two triangles per gap between consecutive sections.
  const Mesh r = curves::ribbon(arc, {.width = 20, .segments = 24});
  EXPECT_EQ(r.vertexCount(), 48u);    // 24 * 2
  EXPECT_EQ(r.triangleCount(), 46u);  // (24 - 1) * 2
}

// curves::project() flattens a 3D spline to a 2D SkPath through the same
// camera the mesh painter uses, so the two agree on where a point lands: a
// segment centred on the world origin comes back centred on the viewport in
// pixels. Drawing a projected curve over a drawn mesh depends on this.
TEST(Curves, ProjectMatchesCameraProjection) {
  Spline3 line;
  line.type = Spline3::Type::Linear;
  line.points = {{-50, 0, 0}, {50, 0, 0}};
  space::Camera camera;
  camera.eye = {0, 0, 200};
  const SkPath path = curves::project(line, camera, {400, 300}, 16);
  const SkRect bounds = path.computeTightBounds();
  EXPECT_NEAR(bounds.centerY(), 150, 1e-2);
  EXPECT_NEAR(bounds.centerX(), 200, 1e-2);
}

// --- Points ---------------------------------------------------------------

TEST(Points, GeneratorsWriteLanes) {
  Spline3 line;
  line.type = Spline3::Type::Linear;
  line.points = {{0, 0, 0}, {90, 0, 0}};
  Cloud onCurve = points::onSpline(line, 10);
  EXPECT_EQ(onCurve.size(), 10u);
  ASSERT_TRUE(onCurve.scalarIf("t"));
  EXPECT_NEAR(onCurve.scalarIf("t")->back(), 1, 1e-4);
  ASSERT_TRUE(onCurve.vectorIf("normal"));

  Cloud lattice = points::grid({0, 0, 0}, {90, 0, 0}, {0, 60, 0}, 4, 3);
  EXPECT_EQ(lattice.size(), 12u);
  EXPECT_NEAR(lattice.vectorIf("normal")->front().z, 1, 1e-4);

  Cloud circle = points::ring({0, 0, 0}, 50, 8);
  EXPECT_EQ(circle.size(), 8u);
  // A ring is laid out in the plane perpendicular to its axis, and the
  // default axis is +y, so an unaxised ring lives in the xz plane at y = 0.
  for (const glm::vec3& p : circle.positions) {
    EXPECT_NEAR(glm::length(p), 50, 1e-2);
    EXPECT_NEAR(p.y, 0, 1e-3);
  }

  Cloud box = points::scatterBox({0, 0, 0}, {10, 10, 10}, 100, 3);
  EXPECT_EQ(box.size(), 100u);
  for (const glm::vec3& p : box.positions) {
    EXPECT_GE(p.x, 0);
    EXPECT_LE(p.x, 10);
  }
}

TEST(Points, OnMeshLandsOnSurface) {
  const Mesh quad = mesh::quad(100, 100);  // z = 0 plane
  Cloud cloud = points::onMesh(quad, 64, 5);
  ASSERT_EQ(cloud.size(), 64u);
  for (const glm::vec3& p : cloud.positions) {
    EXPECT_NEAR(p.z, 0, 1e-4);
    EXPECT_LE(std::abs(p.x), 50.01f);
  }
  ASSERT_TRUE(cloud.vectorIf("normal"));
  EXPECT_NEAR((*cloud.vectorIf("normal"))[0].z, 1, 1e-3);
}

TEST(Points, InstanceStampsWithLanes) {
  Cloud cloud = points::ring({0, 0, 0}, 80, 6);
  std::vector<float>& size = cloud.scalar("size", 1);
  size[0] = 2;
  std::vector<glm::vec4>& tint = cloud.color("tint");
  tint[0] = {1, 0, 0, 1};
  const Mesh stamp = mesh::quad(10, 10);
  points::InstanceOptions options;
  options.scaleLane = "size";
  options.tintLane = "tint";
  options.orientLane = "normal";
  const Mesh merged = points::instance(cloud, stamp, options);
  EXPECT_EQ(merged.vertexCount(), 6u * stamp.vertexCount());
  EXPECT_EQ(merged.triangleCount(), 6u * stamp.triangleCount());
  ASSERT_EQ(merged.colors.size(), merged.vertexCount());
  EXPECT_NEAR(merged.colors[0].r, 1, 1e-4);
  EXPECT_NEAR(merged.colors[0].g, 0, 1e-4);
  // The scale lane multiplies the stamp about its own point. An unscaled
  // 10x10 quad has a 14.1 diagonal, so the first stamp — the only one whose
  // "size" was set to 2 — has to measure more than that.
  glm::vec3 lo, hi;
  Mesh first;
  first.positions.assign(merged.positions.begin(),
                         merged.positions.begin() + 4);
  first.bounds(&lo, &hi);
  EXPECT_GT(glm::length(hi - lo), 14.0f);
}

TEST(Points, AppendPadsLanesWithConventionalDefaults) {
  // When a lane exists on only one side of an append, the other side is
  // padded by what the lane NAME means, not by a generic zero: "size" pads
  // with 1, because 0 would make those instances invisible, and "Tex" pads
  // with the identity uv window (0,0,1,1) rather than white.
  Cloud a;
  a.positions = {{0, 0, 0}, {1, 0, 0}};
  Cloud b;
  b.positions = {{2, 0, 0}, {3, 0, 0}};
  b.scalar("size", 2);
  b.color("Tex", {0.5f, 0.5f, 0.5f, 0.5f});
  a.append(b);
  ASSERT_EQ(a.size(), 4u);
  const std::vector<float>* size = a.scalarIf("size");
  ASSERT_TRUE(size);
  ASSERT_EQ(size->size(), 4u);
  EXPECT_FLOAT_EQ((*size)[0], 1.0f);  // a's side: scale 1, visible
  EXPECT_FLOAT_EQ((*size)[1], 1.0f);
  EXPECT_FLOAT_EQ((*size)[2], 2.0f);  // b's actual values
  EXPECT_FLOAT_EQ((*size)[3], 2.0f);
  const std::vector<glm::vec4>* tex = a.colorIf("Tex");
  ASSERT_TRUE(tex);
  ASSERT_EQ(tex->size(), 4u);
  for (size_t i = 0; i < 2; ++i) {  // a's side: identity uv window
    EXPECT_FLOAT_EQ((*tex)[i].x, 0.0f);
    EXPECT_FLOAT_EQ((*tex)[i].y, 0.0f);
    EXPECT_FLOAT_EQ((*tex)[i].z, 1.0f);
    EXPECT_FLOAT_EQ((*tex)[i].w, 1.0f);
  }
  EXPECT_FLOAT_EQ((*tex)[2].x, 0.5f);  // b's actual window
}

TEST(Points, BillboardsCoverPixels) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 150));
  surface->getCanvas()->clear(SK_ColorBLACK);
  Cloud cloud = points::ring({0, 0, 0}, 40, 12, {0, 0, 1});
  space::Camera camera;
  camera.eye = {0, 0, 200};
  points::BillboardStyle style;
  style.size = 24;
  style.tint = {0, 1, 0, 1};
  points::drawBillboards(*surface->getCanvas(), cloud, camera, {200, 150},
                         style);
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
  // Twelve 24-pixel billboards cover thousands of pixels; the threshold is
  // set low because the point is only that the cloud reached the canvas at
  // all — an empty or entirely off-screen draw is what it must catch.
  int lit = 0;
  for (int y = 0; y < 150; ++y)
    for (int x = 0; x < 200; ++x)
      if (SkColorGetG(bm.getColor(x, y)) > 30) ++lit;
  EXPECT_GT(lit, 200);
}

// --- Easel (the fluent authoring surface) ---------------------------------

// An easel::Shape is a recipe, not a path: it stores the operations and cooks
// them on demand. Two properties follow, and both are what let a caller hold
// one and hand copies around — cooking is pure, and copies are independent.
TEST(Easel, ShapeRecipeCooksNonDestructively) {
  const easel::Shape recipe =
      easel::shape(easel::dot(50)).bloat(0.4f).offset(10);
  const SkPath once = recipe.path();
  const SkPath twice = recipe.path();
  EXPECT_EQ(once.countPoints(), twice.countPoints());
  EXPECT_GT(once.computeTightBounds().width(), 115);  // grew by ~offset
  // Extending a copy must not reach back into the original recipe.
  easel::Shape copy = recipe;
  copy.twirl(90);
  EXPECT_GT(copy.path().countPoints(), 0);
  EXPECT_EQ(recipe.path().countPoints(), once.countPoints());
}

TEST(Easel, BlendReadsLikeIllustrator) {
  const std::vector<blend::Step> steps =
      easel::blend(easel::star(5, 60), easel::dot(50))
          .colors({1, 0, 0, 1}, {0, 0, 1, 1})
          .steps(7)
          .between({0, 0}, {400, 0})
          .cook();
  // steps(n) counts INTERMEDIATES, as it does in a drawing program's blend
  // dialog: the two keys are always present on top of it.
  EXPECT_EQ(steps.size(), 9u);  // 2 keys + 7
  EXPECT_NEAR(steps.back().path.computeTightBounds().centerX(), 400, 2);
}

TEST(Easel, WireAndParticlesCook) {
  const easel::Wire arc = easel::wire({{-100, 0, 0}, {0, 80, 0}, {100, 0, 0}});
  EXPECT_GT(arc.tube(8).triangleCount(), 0u);
  EXPECT_EQ(arc.beads(12).size(), 12u);

  const Cloud sparks = easel::particles()
                           .on(arc)
                           .count(50)
                           .drift(10)
                           .ramp({1, 0, 0, 1}, {0, 0, 1, 1})
                           .cook();
  EXPECT_EQ(sparks.size(), 50u);
  ASSERT_TRUE(sparks.colorIf("tint"));
  EXPECT_NEAR(sparks.colorIf("tint")->front().r, 1, 1e-3);
  EXPECT_NEAR(sparks.colorIf("tint")->back().b, 1, 1e-3);
}

// --- Import ---------------------------------------------------------------

namespace {

using import::Model;
using import::Part;

std::vector<std::byte> toBytes(std::string_view text) {
  const auto* begin = reinterpret_cast<const std::byte*>(text.data());
  return {begin, begin + text.size()};
}

std::string base64(const std::vector<std::byte>& bytes) {
  static const char* alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (size_t i = 0; i < bytes.size(); i += 3) {
    uint32_t chunk = (uint32_t)bytes[i] << 16;
    if (i + 1 < bytes.size()) chunk |= (uint32_t)bytes[i + 1] << 8;
    if (i + 2 < bytes.size()) chunk |= (uint32_t)bytes[i + 2];
    out.push_back(alphabet[(chunk >> 18) & 63]);
    out.push_back(alphabet[(chunk >> 12) & 63]);
    out.push_back(i + 1 < bytes.size() ? alphabet[(chunk >> 6) & 63] : '=');
    out.push_back(i + 2 < bytes.size() ? alphabet[chunk & 63] : '=');
  }
  return out;
}

template <typename T>
void appendRaw(std::vector<std::byte>& out, const T& value) {
  const auto* begin = reinterpret_cast<const std::byte*>(&value);
  out.insert(out.end(), begin, begin + sizeof(T));
}

/** One triangle at (0,0,0) (1,0,0) (0,1,0), uint16 indices 0 1 2. */
std::vector<std::byte> triangleBufferBytes() {
  std::vector<std::byte> bin;
  const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  for (float f : positions) appendRaw(bin, f);
  for (uint16_t i : {uint16_t(0), uint16_t(1), uint16_t(2)}) appendRaw(bin, i);
  return bin;
}

/** The minimal glTF scene: one node, translated +10 along x, holding one red
 *  triangle. The node transform is deliberately non-identity so tests can
 *  tell whether it was applied. @p bufferUri empty writes no uri member,
 *  which is how a GLB's embedded buffer is spelled. */
std::string triangleGltfJson(const std::string& bufferUri) {
  std::string buffer = "{\"byteLength\": 42";
  if (!bufferUri.empty()) buffer += ", \"uri\": \"" + bufferUri + "\"";
  buffer += "}";
  return R"({
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [{"mesh": 0, "name": "tri", "translation": [10, 0, 0]}],
  "meshes": [{"primitives": [
    {"attributes": {"POSITION": 0}, "indices": 1, "material": 0}]}],
  "materials": [{"pbrMetallicRoughness":
    {"baseColorFactor": [1, 0, 0, 1]}}],
  "buffers": [)" +
         buffer + R"(],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 36},
    {"buffer": 0, "byteOffset": 36, "byteLength": 6}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
     "min": [0, 0, 0], "max": [1, 1, 0]},
    {"bufferView": 1, "componentType": 5123, "count": 3,
     "type": "SCALAR"}]
})";
}

std::vector<std::byte> glbBytes() {
  std::string json = triangleGltfJson("");
  while (json.size() % 4) json.push_back(' ');
  std::vector<std::byte> bin = triangleBufferBytes();
  while (bin.size() % 4) bin.push_back(std::byte{0});
  std::vector<std::byte> out;
  appendRaw(out, (uint32_t)0x46546C67);  // "glTF"
  appendRaw(out, (uint32_t)2);
  appendRaw(out, (uint32_t)(12 + 8 + json.size() + 8 + bin.size()));
  appendRaw(out, (uint32_t)json.size());
  appendRaw(out, (uint32_t)0x4E4F534A);  // "JSON"
  for (char c : json) out.push_back((std::byte)c);
  appendRaw(out, (uint32_t)bin.size());
  appendRaw(out, (uint32_t)0x004E4942);  // "BIN"
  out.insert(out.end(), bin.begin(), bin.end());
  return out;
}

constexpr const char* kCubeObj = R"(mtllib cube.mtl
o Cube
v -1 -1 -1
v 1 -1 -1
v 1 1 -1
v -1 1 -1
v -1 -1 1
v 1 -1 1
v 1 1 1
v -1 1 1
usemtl scarlet
f 1 2 3 4
f 5 8 7 6
f 1 5 6 2
f 2 6 7 3
f 3 7 8 4
f 4 8 5 1
)";

constexpr const char* kCubeMtl = R"(newmtl scarlet
Kd 1 0 0
)";

}  // namespace

TEST(Import, ObjCubeWithMaterialThroughResolver) {
  std::vector<std::string> asked;
  const import::Resolver resolve =
      [&](std::string_view uri) -> std::optional<std::vector<std::byte>> {
    asked.emplace_back(uri);
    if (uri == "cube.mtl") return toBytes(kCubeMtl);
    return std::nullopt;
  };
  const std::string obj = kCubeObj;
  auto model = import::model(obj.data(), obj.size(), "cube.obj", resolve);
  ASSERT_TRUE(model.has_value());
  ASSERT_EQ(model->parts.size(), 1u);
  const Part& part = model->parts.front();
  EXPECT_EQ(part.name, "Cube");
  // Corners shared by several faces collapse to one vertex where their
  // (position, uv, normal) agree, so a cube stays 8 vertices instead of 24,
  // and OBJ's n-gon faces are fanned into triangles on the way in.
  EXPECT_EQ(part.mesh.vertexCount(), 8u);
  EXPECT_EQ(part.mesh.triangleCount(), 12u);
  EXPECT_FLOAT_EQ(part.baseColor.r, 1);
  EXPECT_FLOAT_EQ(part.baseColor.g, 0);
  // External references reach the caller through the resolver and nowhere
  // else — the importer never touches the filesystem itself. The uri is
  // passed through exactly as the file spelled it.
  ASSERT_EQ(asked.size(), 1u);
  EXPECT_EQ(asked.front(), "cube.mtl");
  // The file declares no normals, so they are computed; and the uv lane is
  // sized to the vertices even though the file has no vt records, because
  // consumers read lane size as the presence bit.
  ASSERT_EQ(part.mesh.normals.size(), 8u);
  EXPECT_NEAR(glm::length(part.mesh.normals.front()), 1, 1e-4);
  EXPECT_EQ(part.mesh.uvs.size(), 8u);
}

TEST(Import, GltfEmbeddedBase64Buffer) {
  const std::string json = triangleGltfJson(
      "data:application/octet-stream;base64," + base64(triangleBufferBytes()));
  auto model = import::model(json.data(), json.size(), "tri.gltf");
  ASSERT_TRUE(model.has_value());
  ASSERT_EQ(model->parts.size(), 1u);
  const Part& part = model->parts.front();
  EXPECT_EQ(part.name, "tri");
  EXPECT_EQ(part.mesh.vertexCount(), 3u);
  EXPECT_EQ(part.mesh.triangleCount(), 1u);
  EXPECT_FLOAT_EQ(part.baseColor.r, 1);
  EXPECT_FLOAT_EQ(part.baseColor.b, 0);
  // Node transforms are BAKED into the positions rather than kept beside
  // them: the imported mesh is already in model space, so the unit triangle
  // under a +10 x node spans x = 10..11.
  glm::vec3 lo, hi;
  model->bounds(&lo, &hi);
  EXPECT_FLOAT_EQ(lo.x, 10);
  EXPECT_FLOAT_EQ(hi.x, 11);
}

TEST(Import, GltfExternalBufferThroughResolver) {
  const std::string json = triangleGltfJson("tri.bin");
  const import::Resolver resolve =
      [](std::string_view uri) -> std::optional<std::vector<std::byte>> {
    if (uri == "tri.bin") return triangleBufferBytes();
    return std::nullopt;
  };
  auto model = import::model(json.data(), json.size(), "tri.gltf", resolve);
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->triangleCount(), 1u);
  // Without the resolver the external buffer is unreachable.
  EXPECT_FALSE(import::model(json.data(), json.size(), "tri.gltf").has_value());
}

TEST(Import, GlbBinaryContainerAndSniffing) {
  const std::vector<std::byte> glb = glbBytes();
  auto model = import::model(glb.data(), glb.size(), "tri.glb");
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->vertexCount(), 3u);
  EXPECT_EQ(model->triangleCount(), 1u);
  // Format detection does not depend on the filename: with an extension
  // that says nothing, the leading "glTF" magic identifies the container.
  auto sniffed = import::model(glb.data(), glb.size(), "download");
  ASSERT_TRUE(sniffed.has_value());
  EXPECT_EQ(sniffed->triangleCount(), 1u);
}

TEST(Import, StlBinaryAndAscii) {
  // STL stores a normal per facet, and zeroes are the file saying it has
  // none — the importer must then derive them from the winding rather than
  // publish a zero-length normal, which would shade black.
  std::vector<std::byte> stl(80, std::byte{0});
  appendRaw(stl, (uint32_t)2);
  const float tri[2][12] = {
      {0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0},
      {0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0},
  };
  for (const float* f : {tri[0], tri[1]}) {
    for (int i = 0; i < 12; ++i) appendRaw(stl, f[i]);
    appendRaw(stl, (uint16_t)0);
  }
  auto model = import::model(stl.data(), stl.size(), "part.stl");
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->triangleCount(), 2u);
  // STL has no shared-vertex concept, so nothing is welded: every facet
  // keeps its own three vertices and the mesh stays flat shaded.
  EXPECT_EQ(model->vertexCount(), 6u);
  EXPECT_NEAR(glm::length(model->parts.front().mesh.normals.front()), 1, 1e-4);

  const std::string ascii = R"(solid tetra piece
facet normal 0 0 1
outer loop
vertex 0 0 0
vertex 1 0 0
vertex 0 1 0
endloop
endfacet
endsolid tetra piece
)";
  auto text = import::model(ascii.data(), ascii.size(), "part.stl");
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(text->triangleCount(), 1u);
  EXPECT_EQ(text->parts.front().name, "tetra piece");
  EXPECT_FLOAT_EQ(text->parts.front().mesh.normals.front().z, 1);
}

// merged() flattens a multi-part model into one Mesh, and a part's material
// base colour is the only thing that would be lost by that — so it is baked
// into the vertex colour lane, per part, as the merge happens.
TEST(Import, MergedBakesBaseColorsIntoLane) {
  Model model;
  Part a;
  a.mesh = mesh::quad(2, 2);
  a.baseColor = {1, 0, 0, 1};
  Part b;
  b.mesh = mesh::quad(2, 2);
  b.baseColor = {0, 1, 0, 1};
  model.parts = {a, b};
  const Mesh merged = model.merged();
  EXPECT_EQ(merged.vertexCount(), 8u);
  ASSERT_EQ(merged.colors.size(), 8u);
  EXPECT_FLOAT_EQ(merged.colors.front().r, 1);
  EXPECT_FLOAT_EQ(merged.colors.back().g, 1);
}

TEST(Import, MergedCloudConcatenatesLanesAcrossParts) {
  // Disjoint lanes across parts: each side's values land at its own
  // offset, and the other side pads with the lane's default.
  Model model;
  Part a;
  a.mesh = mesh::quad(2, 2);
  a.scalarLanes["energy"] = {1, 2, 3, 4};
  Part b;
  b.mesh = mesh::quad(2, 2);
  b.colorLanes["heat"].assign(4, {1, 0, 0, 1});
  model.parts = {a, b};
  const Cloud merged = model.mergedCloud();
  ASSERT_EQ(merged.size(), 8u);
  const std::vector<float>* energy = merged.scalarIf("energy");
  ASSERT_TRUE(energy);
  ASSERT_EQ(energy->size(), 8u);
  EXPECT_FLOAT_EQ((*energy)[0], 1.0f);
  EXPECT_FLOAT_EQ((*energy)[3], 4.0f);
  EXPECT_FLOAT_EQ((*energy)[4], 0.0f);  // b's side pads scalar 0
  EXPECT_FLOAT_EQ((*energy)[7], 0.0f);
  const std::vector<glm::vec4>* heat = merged.colorIf("heat");
  ASSERT_TRUE(heat);
  ASSERT_EQ(heat->size(), 8u);
  EXPECT_FLOAT_EQ((*heat)[0].r, 1.0f);  // a's side pads white
  EXPECT_FLOAT_EQ((*heat)[0].g, 1.0f);
  EXPECT_FLOAT_EQ((*heat)[4].r, 1.0f);  // b's red from offset 4
  EXPECT_FLOAT_EQ((*heat)[4].g, 0.0f);
}

TEST(Import, FitTransformCentersAndScales) {
  Model model;
  Part part;
  part.mesh = mesh::quad(4, 2);
  part.mesh.transform(glm::translate(glm::mat4(1.0f), {100, 50, 0}));
  model.parts = {part};
  Mesh fitted = model.parts.front().mesh;
  fitted.transform(model.fitTransform(100));
  glm::vec3 lo, hi;
  fitted.bounds(&lo, &hi);
  // fitTransform(n) is uniform: it scales so the LARGEST extent becomes n
  // and recentres on the origin, leaving the aspect ratio alone. A
  // per-axis fit would have stretched the 4x2 quad to 100x100.
  EXPECT_NEAR(hi.x - lo.x, 100, 1e-3);
  EXPECT_NEAR(hi.y - lo.y, 50, 1e-3);
  EXPECT_NEAR(lo.x + hi.x, 0, 1e-3);  // centered
  EXPECT_NEAR(lo.y + hi.y, 0, 1e-3);
}

// --- Pop ------------------------------------------------------------------

TEST(Import, GltfCustomAttributesBecomeLanes) {
  // glTF spells custom vertex attributes with a leading underscore. They
  // survive import as lanes named without it — "_ENERGY" becomes "ENERGY" —
  // and asCloud() carries them alongside the conventional attributes, so an
  // attribute authored in a DCC tool can drive this library directly.
  const std::string json = R"({
  "asset": {"version": "2.0"},
  "scenes": [{"nodes": [0]}], "scene": 0,
  "nodes": [{"mesh": 0, "name": "pts"}],
  "meshes": [{"primitives": [
    {"attributes": {"POSITION": 0, "_ENERGY": 1}}]}],
  "buffers": [{"byteLength": 48, "uri":
"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAACAPgAAAD8AAEA/"}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 36},
    {"buffer": 0, "byteOffset": 36, "byteLength": 12}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
     "min": [0, 0, 0], "max": [1, 1, 0]},
    {"bufferView": 1, "componentType": 5126, "count": 3,
     "type": "SCALAR"}]
})";
  auto model = import::model(json.data(), json.size(), "pts.gltf");
  ASSERT_TRUE(model.has_value());
  const import::Part& part = model->parts.front();
  ASSERT_EQ(part.mesh.vertexCount(), 3u);
  const auto energy = part.scalarLanes.find("ENERGY");
  ASSERT_NE(energy, part.scalarLanes.end());
  ASSERT_EQ(energy->second.size(), 3u);
  EXPECT_FLOAT_EQ(energy->second[0], 0.25f);
  EXPECT_FLOAT_EQ(energy->second[2], 0.75f);

  const Cloud cloud = part.asCloud();
  EXPECT_EQ(cloud.size(), 3u);
  ASSERT_TRUE(cloud.scalarIf("ENERGY"));
  EXPECT_FLOAT_EQ((*cloud.scalarIf("ENERGY"))[1], 0.5f);
}

TEST(Import, GltfVec2AndVec4CustomAttributesLandAsColorLanes) {
  // Custom attributes are routed by WIDTH: a scalar accessor becomes a
  // scalar lane, and anything wider becomes a four-component colour lane —
  // VEC2 zero-padded in z and w, VEC4 verbatim. There is no vec2 lane kind,
  // so reading .z of a VEC2 custom always gives 0, not garbage.
  std::vector<std::byte> bin;
  const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  for (float f : positions) appendRaw(bin, f);
  const float uv2[6] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
  for (float f : uv2) appendRaw(bin, f);
  const float wgt[12] = {1, 0, 0, 0.5f, 0, 1, 0, 0.25f, 0, 0, 1, 0.125f};
  for (float f : wgt) appendRaw(bin, f);
  const std::string json = R"({
  "asset": {"version": "2.0"},
  "scenes": [{"nodes": [0]}], "scene": 0,
  "nodes": [{"mesh": 0, "name": "pts"}],
  "meshes": [{"primitives": [
    {"attributes": {"POSITION": 0, "_UV2": 1, "_WGT": 2}}]}],
  "buffers": [{"byteLength": 108, "uri":
"data:application/octet-stream;base64,)" +
                           base64(bin) + R"("}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 36},
    {"buffer": 0, "byteOffset": 36, "byteLength": 24},
    {"buffer": 0, "byteOffset": 60, "byteLength": 48}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
     "min": [0, 0, 0], "max": [1, 1, 0]},
    {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC2"},
    {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC4"}]
})";
  auto model = import::model(json.data(), json.size(), "pts.gltf");
  ASSERT_TRUE(model.has_value());
  const Part& part = model->parts.front();
  const auto uv = part.colorLanes.find("UV2");
  ASSERT_NE(uv, part.colorLanes.end());
  ASSERT_EQ(uv->second.size(), 3u);
  EXPECT_FLOAT_EQ(uv->second[1].x, 0.3f);
  EXPECT_FLOAT_EQ(uv->second[1].y, 0.4f);
  EXPECT_FLOAT_EQ(uv->second[1].z, 0.0f);  // zero-padded z/w
  EXPECT_FLOAT_EQ(uv->second[1].w, 0.0f);
  const auto wgtLane = part.colorLanes.find("WGT");
  ASSERT_NE(wgtLane, part.colorLanes.end());
  ASSERT_EQ(wgtLane->second.size(), 3u);
  EXPECT_FLOAT_EQ(wgtLane->second[0].w, 0.5f);
  EXPECT_FLOAT_EQ(wgtLane->second[2].z, 1.0f);
  EXPECT_FLOAT_EQ(wgtLane->second[2].w, 0.125f);

  const Cloud cloud = part.asCloud();
  ASSERT_TRUE(cloud.colorIf("UV2"));
  ASSERT_TRUE(cloud.colorIf("WGT"));
  EXPECT_FLOAT_EQ((*cloud.colorIf("UV2"))[2].y, 0.6f);
  EXPECT_FLOAT_EQ((*cloud.colorIf("WGT"))[1].y, 1.0f);
}

TEST(Import, PlyAttributesFlowFromAsciiAndBinary) {
  // PLY property routing: the conventional names (x/y/z, nx/ny/nz, s/t,
  // red/green/blue) build the mesh, and every other property becomes a lane
  // carrying its RAW value — no rescaling, so an integer id stays that
  // number. Only the colour properties are normalized, uchar 0..255 to
  // 0..1. A file with no face element imports as a point cloud with no
  // indices rather than being rejected.
  const char* ascii =
      "ply\n"
      "format ascii 1.0\n"
      "comment a houdini-ish scatter\n"
      "element vertex 4\n"
      "property float x\n"
      "property float y\n"
      "property float z\n"
      "property float intensity\n"
      "property uchar red\n"
      "property uchar green\n"
      "property uchar blue\n"
      "end_header\n"
      "0 0 0 0.5 255 0 0\n"
      "10 0 0 1.5 0 255 0\n"
      "0 10 0 2.5 0 0 255\n"
      "10 10 0 3.5 255 255 255\n";
  auto model = import::model(ascii, std::strlen(ascii), "scatter.ply");
  ASSERT_TRUE(model.has_value());
  const import::Part& part = model->parts.front();
  ASSERT_EQ(part.mesh.vertexCount(), 4u);
  EXPECT_TRUE(part.mesh.indices.empty());  // no faces declared, none invented
  EXPECT_FLOAT_EQ(part.mesh.positions[1].x, 10);
  ASSERT_EQ(part.mesh.colors.size(), 4u);
  EXPECT_NEAR(part.mesh.colors[0].r, 1, 1e-2f);  // uchar normalized
  EXPECT_NEAR(part.mesh.colors[2].b, 1, 1e-2f);
  const auto intensity = part.scalarLanes.find("intensity");
  ASSERT_NE(intensity, part.scalarLanes.end());
  EXPECT_FLOAT_EQ(intensity->second[3], 3.5f);

  // An imported lane is an ordinary lane: naming it as the scale lane makes
  // it drive instancing with no conversion step in between.
  const Cloud cloud = part.asCloud();
  points::InstanceOptions options;
  options.scaleLane = "intensity";
  const Mesh stamped = points::instance(cloud, mesh::quad(2, 2), options);
  EXPECT_EQ(stamped.vertexCount(), 4u * 4u);
  glm::vec3 lo, hi;
  stamped.bounds(&lo, &hi);
  // The last point's intensity is 3.5, so its 2x2 stamp reaches 3.5 beyond
  // the point at x = 10 — past anything the unscaled stamps could reach.
  EXPECT_GT(hi.x, 12.0f);

  // The binary_little_endian body encodes the same rows: properties in
  // declaration order, and a face list as a uchar count then int32 indices.
  std::vector<std::byte> bin;
  const auto push = [&](const void* p, size_t n) {
    const auto* b = static_cast<const std::byte*>(p);
    bin.insert(bin.end(), b, b + n);
  };
  const char* header =
      "ply\n"
      "format binary_little_endian 1.0\n"
      "element vertex 3\n"
      "property float x\n"
      "property float y\n"
      "property float z\n"
      "property float intensity\n"
      "element face 1\n"
      "property list uchar int vertex_indices\n"
      "end_header\n";
  push(header, std::strlen(header));
  const float verts[] = {0, 0, 0, 7, 4, 0, 0, 8, 0, 4, 0, 9};
  push(verts, sizeof(verts));
  const uint8_t faceCount = 3;
  const int32_t face[] = {0, 1, 2};
  push(&faceCount, 1);
  push(face, sizeof(face));
  auto binModel = import::model(bin.data(), bin.size(), "tri.ply");
  ASSERT_TRUE(binModel.has_value());
  const import::Part& tri = binModel->parts.front();
  EXPECT_EQ(tri.mesh.triangleCount(), 1u);
  EXPECT_FLOAT_EQ(tri.scalarLanes.at("intensity")[2], 9.0f);
  ASSERT_EQ(tri.mesh.normals.size(), 3u);
  EXPECT_NEAR(tri.mesh.normals.front().z, 1.0f, 1e-4f);
}

TEST(Import, PlyRejectsHostileCountsAndIndices) {
  // (a) A face naming a vertex past the count is dropped whole — the
  // vertices still import, and computeNormals never indexes OOB.
  const char* badFace =
      "ply\nformat ascii 1.0\n"
      "element vertex 3\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 1\n"
      "property list uchar int vertex_indices\n"
      "end_header\n"
      "0 0 0\n1 0 0\n0 1 0\n"
      "3 0 1 9\n";
  auto dropped = import::model(badFace, std::strlen(badFace), "bad.ply");
  ASSERT_TRUE(dropped.has_value());
  EXPECT_EQ(dropped->parts.front().mesh.vertexCount(), 3u);
  EXPECT_EQ(dropped->parts.front().mesh.triangleCount(), 0u);

  // (b) A vertex count no data could back is rejected before any
  // resize acts on it.
  const char* hugeCount =
      "ply\nformat ascii 1.0\n"
      "element vertex 4000000000\n"
      "property float x\nproperty float y\nproperty float z\n"
      "end_header\n"
      "0 0 0\n";
  EXPECT_FALSE(
      import::model(hugeCount, std::strlen(hugeCount), "huge.ply").has_value());

  // (c) A binary list count promising more bytes than remain fails
  // the row read instead of walking off the buffer.
  std::vector<std::byte> truncated;
  const char* binHeader =
      "ply\nformat binary_little_endian 1.0\n"
      "element vertex 1\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 1\n"
      "property list uchar int vertex_indices\n"
      "end_header\n";
  const auto pushBytes = [&](const void* p, size_t n) {
    const auto* b = static_cast<const std::byte*>(p);
    truncated.insert(truncated.end(), b, b + n);
  };
  pushBytes(binHeader, std::strlen(binHeader));
  const float vertex[3] = {0, 0, 0};
  pushBytes(vertex, sizeof(vertex));
  const uint8_t promised = 200;  // 800 bytes of indices; none follow
  pushBytes(&promised, 1);
  EXPECT_FALSE(import::model(truncated.data(), truncated.size(), "trunc.ply")
                   .has_value());
}

TEST(Import, LoneTStaysAScalarLane) {
  // In PLY, "t" is a texture coordinate only when it is paired with "s".
  // On its own it is an ordinary scalar property — and "t" is the name this
  // library's own point clouds use for the parameter along a curve, so
  // folding a lone "t" into uv.y would silently eat that lane on every file
  // this library writes.
  const char* ascii =
      "ply\nformat ascii 1.0\n"
      "element vertex 3\n"
      "property float x\nproperty float y\nproperty float z\n"
      "property float t\n"
      "end_header\n"
      "0 0 0 0.25\n1 0 0 0.5\n0 1 0 0.75\n";
  auto model = import::model(ascii, std::strlen(ascii), "lone_t.ply");
  ASSERT_TRUE(model.has_value());
  const Part& part = model->parts.front();
  const auto t = part.scalarLanes.find("t");
  ASSERT_NE(t, part.scalarLanes.end());
  ASSERT_EQ(t->second.size(), 3u);
  EXPECT_FLOAT_EQ(t->second[1], 0.5f);
  // The uv lane is still sized to the vertices, and still all zeroes: the
  // lone "t" went nowhere near it.
  for (const glm::vec2& uv : part.mesh.uvs) EXPECT_FLOAT_EQ(uv.y, 0.0f);
  const Cloud cloud = part.asCloud();
  ASSERT_TRUE(cloud.scalarIf("t"));
  EXPECT_FLOAT_EQ((*cloud.scalarIf("t"))[2], 0.75f);

  // And the loop closes: a cloud whose only extra lane is the scalar "t"
  // writes to PLY and reads back with its values intact.
  Cloud dump;
  dump.positions = {{0, 0, 0}, {2, 0, 0}, {0, 2, 0}};
  dump.scalar("t") = {0.1f, 0.6f, 0.9f};
  const std::string bytes = save::ply(dump);
  auto trip = import::model(bytes.data(), bytes.size(), "trip.ply");
  ASSERT_TRUE(trip.has_value());
  const Cloud back = trip->parts.front().asCloud();
  ASSERT_EQ(back.size(), 3u);
  ASSERT_TRUE(back.scalarIf("t"));
  EXPECT_FLOAT_EQ((*back.scalarIf("t"))[0], 0.1f);
  EXPECT_FLOAT_EQ((*back.scalarIf("t"))[1], 0.6f);
  EXPECT_FLOAT_EQ((*back.scalarIf("t"))[2], 0.9f);
}

TEST(Import, PlyPartialSuffixTriplesStayScalarAndRgbFoldsAlphaOne) {
  // Properties named with an _x/_y/_z or _r/_g/_b/_a suffix are folded back
  // into a single vector or colour lane. The fold is all-or-nothing: an
  // incomplete triple stays as its raw scalar properties rather than
  // becoming a vector with an invented component, while an _r/_g/_b with no
  // _a folds with alpha 1, which is opaque.
  const char* ascii =
      "ply\nformat ascii 1.0\n"
      "element vertex 2\n"
      "property float x\nproperty float y\nproperty float z\n"
      "property float foo_x\nproperty float foo_y\n"
      "property float warm_r\nproperty float warm_g\n"
      "property float warm_b\n"
      "end_header\n"
      "0 0 0 1 2 0.25 0.5 0.75\n"
      "1 0 0 3 4 1 0 0.5\n";
  auto model = import::model(ascii, std::strlen(ascii), "fold.ply");
  ASSERT_TRUE(model.has_value());
  const Part& part = model->parts.front();
  EXPECT_EQ(part.vectorLanes.count("foo"), 0u);
  ASSERT_EQ(part.scalarLanes.count("foo_x"), 1u);
  ASSERT_EQ(part.scalarLanes.count("foo_y"), 1u);
  EXPECT_FLOAT_EQ(part.scalarLanes.at("foo_x")[1], 3.0f);
  EXPECT_FLOAT_EQ(part.scalarLanes.at("foo_y")[1], 4.0f);
  const auto warm = part.colorLanes.find("warm");
  ASSERT_NE(warm, part.colorLanes.end());
  ASSERT_EQ(warm->second.size(), 2u);
  EXPECT_FLOAT_EQ(warm->second[0].x, 0.25f);
  EXPECT_FLOAT_EQ(warm->second[0].y, 0.5f);
  EXPECT_FLOAT_EQ(warm->second[0].z, 0.75f);
  EXPECT_FLOAT_EQ(warm->second[0].w, 1.0f);
  EXPECT_EQ(part.scalarLanes.count("warm_r"), 0u);
  EXPECT_EQ(part.scalarLanes.count("warm_g"), 0u);
  EXPECT_EQ(part.scalarLanes.count("warm_b"), 0u);
}

TEST(Save, PlyRoundTripsPrimitiveLanes) {
  // Per-triangle (prim) lanes survive a PLY round trip in both encodings:
  // they are written as face properties and read back into Mesh::prims
  // under the same names. Names are not special-cased — an arbitrary
  // "Charge" travels exactly like the conventional "Color".
  Mesh quad = mesh::quad(10, 6);  // 4 vertices, 2 triangles
  ASSERT_EQ(quad.triangleCount(), 2u);
  quad.prim("Color") = {{1, 0, 0, 1}, {0, 0.5f, 1, 0.25f}};
  quad.prim("Charge") = {{0.5f, -2, 7, 1.0f / 3.0f}, {1e-5f, 3, 0, 1}};

  for (const bool binary : {false, true}) {
    const std::string bytes = save::ply(quad, {.binary = binary});
    ASSERT_FALSE(bytes.empty());
    auto model = import::model(bytes.data(), bytes.size(), "prim.ply");
    ASSERT_TRUE(model.has_value());
    const import::Part& part = model->parts.front();
    const Mesh& back = part.mesh;
    ASSERT_EQ(back.triangleCount(), 2u);

    const std::vector<glm::vec4>* color = back.primIf("Color");
    ASSERT_NE(color, nullptr) << "binary=" << binary;
    ASSERT_EQ(color->size(), 2u);
    const std::vector<glm::vec4>* charge = back.primIf("Charge");
    ASSERT_NE(charge, nullptr) << "binary=" << binary;
    ASSERT_EQ(charge->size(), 2u);
    // The ascii writer prints with %g, which keeps six significant digits,
    // so ascii values come back close but not identical; binary rows are
    // raw floats and come back bit-exact.
    const float tol = binary ? 0.0f : 1e-6f;
    for (size_t t = 0; t < 2; ++t)
      for (int c = 0; c < 4; ++c) {
        EXPECT_NEAR((*color)[t][c], quad.prims.at("Color")[t][c], tol);
        EXPECT_NEAR((*charge)[t][c], quad.prims.at("Charge")[t][c],
                    std::abs(quad.prims.at("Charge")[t][c]) * tol + tol);
      }
    if (binary) {
      EXPECT_FLOAT_EQ((*charge)[0].w, 1.0f / 3.0f);
      EXPECT_FLOAT_EQ((*charge)[1].x, 1e-5f);
    }

    // Cardinality is preserved on the way back: a per-face lane has one
    // value per triangle, a point lane one per vertex, and the two never
    // mix. So the face lanes appear in Mesh::prims and nowhere else —
    // neither in the Part's point lanes nor in the Cloud it pours into.
    EXPECT_EQ(part.scalarLanes.count("Color_r"), 0u);
    EXPECT_EQ(part.colorLanes.count("Color"), 0u);
    EXPECT_EQ(part.colorLanes.count("Charge"), 0u);
    const Cloud cloud = part.asCloud();
    EXPECT_EQ(cloud.colorIf("Charge"), nullptr);

    // And merged() carries them out through Mesh::append.
    EXPECT_EQ(model->merged().prims.count("Charge"), 1u);
  }
}

TEST(Import, PlyFacePropertiesReplicateAcrossFanTriangles) {
  // The reader fan-triangulates an n-gon into n-2 triangles, so ONE source
  // face's per-face attribute has to be REPLICATED across all of them, or
  // the lane ends up sized to the face count instead of triangleCount() and
  // every value after the first n-gon is read against the wrong triangle. A
  // quad and a pentagon give 5 triangles from 2 face rows; a triangles-only
  // fixture could not tell the two sizings apart.
  const char* ascii =
      "ply\nformat ascii 1.0\n"
      "element vertex 6\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 2\n"
      "property list uchar int vertex_indices\n"
      "property float Color_r\nproperty float Color_g\n"
      "property float Color_b\nproperty float Color_a\n"
      "property float density\n"
      "end_header\n"
      "0 0 0\n1 0 0\n1 1 0\n0 1 0\n2 1 0\n2 0 0\n"
      "4 0 1 2 3 1 0 0 1 2.5\n"
      "5 1 2 3 4 5 0 0.25 0.5 0.75 -1.5\n";
  auto model = import::model(ascii, std::strlen(ascii), "fan.ply");
  ASSERT_TRUE(model.has_value());
  const Mesh& mesh = model->parts.front().mesh;
  ASSERT_EQ(mesh.triangleCount(), 5u);  // 2 from the quad, 3 from the pent
  const std::vector<glm::vec4>* color = mesh.primIf("Color");
  ASSERT_NE(color, nullptr);
  ASSERT_EQ(color->size(), 5u);
  for (size_t t = 0; t < 2; ++t) {
    EXPECT_FLOAT_EQ((*color)[t].x, 1.0f);
    EXPECT_FLOAT_EQ((*color)[t].w, 1.0f);
  }
  for (size_t t = 2; t < 5; ++t) {
    EXPECT_FLOAT_EQ((*color)[t].x, 0.0f);
    EXPECT_FLOAT_EQ((*color)[t].y, 0.25f);
    EXPECT_FLOAT_EQ((*color)[t].z, 0.5f);
    EXPECT_FLOAT_EQ((*color)[t].w, 0.75f);
  }
  // Prim lanes are always four-component, so a single per-face scalar
  // property widens into .x with the other three left at zero.
  const std::vector<glm::vec4>* density = mesh.primIf("density");
  ASSERT_NE(density, nullptr);
  ASSERT_EQ(density->size(), 5u);
  EXPECT_FLOAT_EQ((*density)[1].x, 2.5f);
  EXPECT_FLOAT_EQ((*density)[2].x, -1.5f);
  EXPECT_FLOAT_EQ((*density)[4].x, -1.5f);
  EXPECT_FLOAT_EQ((*density)[0].y, 0.0f);

  // The binary encoding fans identically. A binary face row is a uchar
  // list count, then that many int32 indices, then the face's remaining
  // properties as raw floats.
  std::vector<std::byte> bin;
  const auto push = [&](const void* p, size_t n) {
    const auto* b = static_cast<const std::byte*>(p);
    bin.insert(bin.end(), b, b + n);
  };
  const char* header =
      "ply\nformat binary_little_endian 1.0\n"
      "element vertex 6\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 2\n"
      "property list uchar int vertex_indices\n"
      "property float Color_r\nproperty float Color_g\n"
      "property float Color_b\nproperty float Color_a\n"
      "property float density\n"
      "end_header\n";
  push(header, std::strlen(header));
  const float verts[] = {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 2, 1, 0, 2, 0, 0};
  push(verts, sizeof(verts));
  const uint8_t quadCount = 4;
  const int32_t quadIdx[] = {0, 1, 2, 3};
  const float quadAttrs[] = {1, 0, 0, 1, 2.5f};
  push(&quadCount, 1);
  push(quadIdx, sizeof(quadIdx));
  push(quadAttrs, sizeof(quadAttrs));
  const uint8_t pentCount = 5;
  const int32_t pentIdx[] = {1, 2, 3, 4, 5};
  const float pentAttrs[] = {0, 0.25f, 0.5f, 0.75f, -1.5f};
  push(&pentCount, 1);
  push(pentIdx, sizeof(pentIdx));
  push(pentAttrs, sizeof(pentAttrs));
  auto binModel = import::model(bin.data(), bin.size(), "fan.ply");
  ASSERT_TRUE(binModel.has_value());
  const Mesh& binMesh = binModel->parts.front().mesh;
  ASSERT_EQ(binMesh.triangleCount(), 5u);
  ASSERT_NE(binMesh.primIf("Color"), nullptr);
  ASSERT_EQ(binMesh.primIf("Color")->size(), 5u);
  EXPECT_FLOAT_EQ((*binMesh.primIf("Color"))[1].x, 1.0f);
  EXPECT_FLOAT_EQ((*binMesh.primIf("Color"))[2].y, 0.25f);
  ASSERT_NE(binMesh.primIf("density"), nullptr);
  EXPECT_FLOAT_EQ((*binMesh.primIf("density"))[4].x, -1.5f);
}

TEST(Import, PlyFaceLanesTakeConventionalColorAndAnyDeclaredOrder) {
  // (a) Other tools spell per-face colour as red/green/blue/alpha rather
  // than Color_r/_g/_b/_a; both land in the same "Color" lane, with integer
  // channels normalized to 0..1 and a missing alpha filled with 1.
  // (b) Face properties may be declared BEFORE the index list. The whole
  // row is read before it is interpreted, so the triangle count a lane
  // replicates across does not depend on where the list sits.
  const char* ascii =
      "ply\nformat ascii 1.0\n"
      "element vertex 4\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 2\n"
      "property uchar red\nproperty uchar green\n"
      "property uchar blue\n"
      "property float heat\n"
      "property list uchar int vertex_indices\n"
      "end_header\n"
      "0 0 0\n1 0 0\n1 1 0\n0 1 0\n"
      "255 0 0 9 3 0 1 2\n"
      "0 255 255 4 4 0 1 2 3\n";
  auto model = import::model(ascii, std::strlen(ascii), "meshlab.ply");
  ASSERT_TRUE(model.has_value());
  const Mesh& mesh = model->parts.front().mesh;
  ASSERT_EQ(mesh.triangleCount(), 3u);  // 1 triangle + a fanned quad
  const std::vector<glm::vec4>* color = mesh.primIf("Color");
  ASSERT_NE(color, nullptr);
  ASSERT_EQ(color->size(), 3u);
  EXPECT_NEAR((*color)[0].x, 1.0f, 1e-3f);  // uchar normalized
  EXPECT_NEAR((*color)[0].y, 0.0f, 1e-3f);
  EXPECT_FLOAT_EQ((*color)[0].w, 1.0f);  // no alpha channel -> 1
  EXPECT_NEAR((*color)[1].z, 1.0f, 1e-3f);
  EXPECT_NEAR((*color)[2].y, 1.0f, 1e-3f);
  const std::vector<glm::vec4>* heat = mesh.primIf("heat");
  ASSERT_NE(heat, nullptr);
  ASSERT_EQ(heat->size(), 3u);
  EXPECT_FLOAT_EQ((*heat)[0].x, 9.0f);  // raw: only colour names normalize
  EXPECT_FLOAT_EQ((*heat)[1].x, 4.0f);
  EXPECT_FLOAT_EQ((*heat)[2].x, 4.0f);
}

TEST(Import, PlyFaceLanesSurviveHostileFaceHeaders) {
  // (a) A face naming a vertex past the count is dropped whole, and
  // its per-face values go with it: the lane stays sized to
  // triangleCount() rather than carrying a phantom entry.
  const char* dropped =
      "ply\nformat ascii 1.0\n"
      "element vertex 3\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 2\n"
      "property list uchar int vertex_indices\n"
      "property float Color_r\nproperty float Color_g\n"
      "property float Color_b\nproperty float Color_a\n"
      "end_header\n"
      "0 0 0\n1 0 0\n0 1 0\n"
      "3 0 1 9 1 1 1 1\n"
      "3 0 1 2 0.5 0.25 0.125 1\n";
  auto model = import::model(dropped, std::strlen(dropped), "drop.ply");
  ASSERT_TRUE(model.has_value());
  const Mesh& mesh = model->parts.front().mesh;
  ASSERT_EQ(mesh.triangleCount(), 1u);
  ASSERT_NE(mesh.primIf("Color"), nullptr);
  ASSERT_EQ(mesh.primIf("Color")->size(), 1u);
  // The SURVIVING face's value, not the dropped one's.
  EXPECT_FLOAT_EQ((*mesh.primIf("Color"))[0].x, 0.5f);

  // (b) A face count no data could back is rejected before anything is
  // sized from it — the prim path never resizes on a declared count.
  const char* hugeFaces =
      "ply\nformat ascii 1.0\n"
      "element vertex 3\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 4000000000\n"
      "property list uchar int vertex_indices\n"
      "property float Color_r\nproperty float Color_g\n"
      "property float Color_b\nproperty float Color_a\n"
      "end_header\n"
      "0 0 0\n1 0 0\n0 1 0\n"
      "3 0 1 2 1 1 1 1\n";
  EXPECT_FALSE(
      import::model(hugeFaces, std::strlen(hugeFaces), "huge.ply").has_value());

  // (c) A header promising more face rows than the body delivers fails
  // the read instead of publishing a short lane.
  const char* shortBody =
      "ply\nformat ascii 1.0\n"
      "element vertex 3\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 3\n"
      "property list uchar int vertex_indices\n"
      "property float Color_r\nproperty float Color_g\n"
      "property float Color_b\nproperty float Color_a\n"
      "end_header\n"
      "0 0 0\n1 0 0\n0 1 0\n"
      "3 0 1 2 1 1 1 1\n";
  EXPECT_FALSE(import::model(shortBody, std::strlen(shortBody), "short.ply")
                   .has_value());

  // (d) A duplicate face property claims its lane once: the second
  // declaration is read and discarded rather than appending a second
  // time and desyncing the lane off triangleCount().
  const char* duplicate =
      "ply\nformat ascii 1.0\n"
      "element vertex 3\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 1\n"
      "property list uchar int vertex_indices\n"
      "property float heat\nproperty float heat\n"
      "end_header\n"
      "0 0 0\n1 0 0\n0 1 0\n"
      "3 0 1 2 6 7\n";
  auto dupModel = import::model(duplicate, std::strlen(duplicate), "dup.ply");
  ASSERT_TRUE(dupModel.has_value());
  const Mesh& dupMesh = dupModel->parts.front().mesh;
  ASSERT_EQ(dupMesh.triangleCount(), 1u);
  ASSERT_NE(dupMesh.primIf("heat"), nullptr);
  ASSERT_EQ(dupMesh.primIf("heat")->size(), 1u);
  EXPECT_FLOAT_EQ((*dupMesh.primIf("heat"))[0].x, 6.0f);
}

namespace {

/** An Ogawa archive built entirely in memory, so the Alembic tests need no
 *  fixture files. It contains:
 *   - "root", an xform translated +10 along x, holding "tri": a static
 *     clockwise triangle carrying a per-vertex-scope arbitrary geometry
 *     parameter;
 *   - "uvquad" and "uvweld": the SAME topology written twice, once with
 *     facevarying uvs and once with vertex-scope uvs, which are the two
 *     sides of the corner dedup;
 *   - "cloud": a top-level point cloud with two time samples at 24 fps, and
 *     ids written on the first sample only. */
std::string alembicArchiveBytes() {
  namespace Abc = Alembic::Abc;
  namespace AbcGeom = Alembic::AbcGeom;
  std::ostringstream out(std::ios::binary);
  {
    Abc::OArchive archive(
        Alembic::AbcCoreOgawa::WriteArchive()(&out, Abc::MetaData()));
    const uint32_t ts = archive.addTimeSampling(
        Alembic::AbcCoreAbstract::TimeSampling(1.0 / 24.0, 0.0));

    AbcGeom::OXform root(archive.getTop(), "root");
    AbcGeom::XformSample xs;
    xs.setTranslation(Abc::V3d(10, 0, 0));
    root.getSchema().set(xs);

    AbcGeom::OPolyMesh meshObj(root, "tri");  // static, under the xform
    const std::vector<Imath::V3f> pos = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    const std::vector<int32_t> indices = {0, 2, 1};  // Alembic: clockwise
    const std::vector<int32_t> counts = {3};
    meshObj.getSchema().set(AbcGeom::OPolyMeshSchema::Sample(
        Abc::P3fArraySample(pos), Abc::Int32ArraySample(indices),
        Abc::Int32ArraySample(counts)));
    AbcGeom::OFloatGeomParam energy(meshObj.getSchema().getArbGeomParams(),
                                    "energy", false, AbcGeom::kVertexScope, 1);
    const std::vector<float> values = {0.25f, 0.5f, 0.75f};
    energy.set(AbcGeom::OFloatGeomParam::Sample(Abc::FloatArraySample(values),
                                                AbcGeom::kVertexScope));

    // Two triangles sharing the 0-2 diagonal of a unit square: 4 points and
    // 6 corners, written twice with identical topology and different uv
    // scopes so the dedup can be observed from both sides.
    const std::vector<Imath::V3f> qpos = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    const std::vector<int32_t> qidx = {0, 2, 1, 0, 3, 2};  // clockwise
    const std::vector<int32_t> qcounts = {3, 3};
    // FACEVARYING: one uv per corner. Corners 1 and 5 are the same
    // point with different uvs; corners 0 and 3 are the same point with
    // the SAME uv — and still do not weld, because the key is the
    // corner INDEX, not the value.
    AbcGeom::OPolyMesh uvObj(archive.getTop(), "uvquad");
    const std::vector<Imath::V2f> quv = {{0, 0}, {1, 1}, {1, 0},
                                         {0, 0}, {0, 1}, {0.25f, 0.75f}};
    uvObj.getSchema().set(AbcGeom::OPolyMeshSchema::Sample(
        Abc::P3fArraySample(qpos), Abc::Int32ArraySample(qidx),
        Abc::Int32ArraySample(qcounts),
        AbcGeom::OV2fGeomParam::Sample(Abc::V2fArraySample(quv),
                                       AbcGeom::kFacevaryingScope)));
    // VERTEX scope: one uv per point, so every corner of a point keys
    // the same and the six corners weld back down to four vertices.
    AbcGeom::OPolyMesh weldObj(archive.getTop(), "uvweld");
    const std::vector<Imath::V2f> wuv = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    weldObj.getSchema().set(AbcGeom::OPolyMeshSchema::Sample(
        Abc::P3fArraySample(qpos), Abc::Int32ArraySample(qidx),
        Abc::Int32ArraySample(qcounts),
        AbcGeom::OV2fGeomParam::Sample(Abc::V2fArraySample(wuv),
                                       AbcGeom::kVertexScope)));

    AbcGeom::OPoints pointsObj(archive.getTop(), "cloud", ts);  // animated
    const std::vector<uint64_t> ids = {0, 1};
    const std::vector<Imath::V3f> frame0 = {{0, 0, 0}, {1, 0, 0}};
    pointsObj.getSchema().set(AbcGeom::OPointsSchema::Sample(
        Abc::P3fArraySample(frame0), Abc::UInt64ArraySample(ids)));
    const std::vector<Imath::V3f> frame1 = {{0, 1, 0}, {1, 1, 0}};
    pointsObj.getSchema().set(
        AbcGeom::OPointsSchema::Sample(Abc::P3fArraySample(frame1)));
  }  // The OArchive destructor is what finalizes the Ogawa stream, so the
     // scope must close before .str() is read — an earlier read returns a
     // truncated, unreadable archive.
  return std::move(out).str();
}

}  // namespace

TEST(Import, AlembicMeshPointsAndLanes) {
  const std::string bytes = alembicArchiveBytes();

  // As with GLB, the filename hint carries nothing useful here and the
  // leading Ogawa magic is what routes the bytes to the Alembic reader.
  auto model = import::model(bytes.data(), bytes.size(), "download");
  ASSERT_TRUE(model.has_value());
  ASSERT_EQ(model->parts.size(), 4u);  // tri, uvquad, uvweld, cloud
  const auto find = [&](std::string_view name) -> const Part* {
    for (const Part& part : model->parts)
      if (part.name == name) return &part;
    return nullptr;
  };
  const Part* tri = find("tri");
  const Part* cloud = find("cloud");
  ASSERT_NE(tri, nullptr);
  ASSERT_NE(cloud, nullptr);

  EXPECT_EQ(tri->mesh.triangleCount(), 1u);
  glm::vec3 lo, hi;
  tri->mesh.bounds(&lo, &hi);
  // Parent xforms are baked into the positions, as glTF node transforms are.
  EXPECT_FLOAT_EQ(lo.x, 10.0f);
  EXPECT_FLOAT_EQ(hi.x, 11.0f);
  // WINDING: Alembic faces are wound clockwise, this library's meshes
  // counter-clockwise, so the importer reverses each face. The fixture's
  // triangle therefore ends up with its derived normal along +z; leaving
  // the file's order alone would point it at -z and cull the whole model.
  ASSERT_EQ(tri->mesh.normals.size(), 3u);
  EXPECT_GT(tri->mesh.normals[0].z, 0.0f);
  // A vertex-scope arbitrary geometry parameter becomes a per-point scalar
  // lane, and each value has to follow its point through the corner dedup
  // and the winding reversal — both of which renumber vertices. Checked by
  // recomputing the value from the position it ended up on (the fixture's
  // values are 0.25 + 0.25x + 0.5y in local space) rather than by index,
  // since matching by index would pass even if the lane were shuffled.
  const auto energy = tri->scalarLanes.find("energy");
  ASSERT_NE(energy, tri->scalarLanes.end());
  ASSERT_EQ(energy->second.size(), 3u);
  for (size_t i = 0; i < 3; ++i) {
    const glm::vec3 local = tri->mesh.positions[i] - glm::vec3{10, 0, 0};
    EXPECT_NEAR(energy->second[i], 0.25f + 0.25f * local.x + 0.5f * local.y,
                1e-6f);
  }
  EXPECT_EQ(tri->asCloud().scalars.count("energy"), 1u);

  // An OPoints object imports as a part with no indices, its ids carried as
  // an ordinary scalar lane. With no time requested, the first sample is
  // the one read.
  EXPECT_TRUE(cloud->mesh.indices.empty());
  ASSERT_EQ(cloud->mesh.positions.size(), 2u);
  EXPECT_FLOAT_EQ(cloud->mesh.positions[0].y, 0.0f);
  const auto id = cloud->scalarLanes.find("id");
  ASSERT_NE(id, cloud->scalarLanes.end());
  ASSERT_EQ(id->second.size(), 2u);
  EXPECT_FLOAT_EQ(id->second[0], 0.0f);
  EXPECT_FLOAT_EQ(id->second[1], 1.0f);

  // Malformed input returns an empty optional rather than throwing or
  // aborting: the Alembic library signals errors by exception, and those
  // must not escape into a caller that is merely opening an untrusted file.
  const char garbage[] = "not an alembic archive at all";
  EXPECT_FALSE(import::alembic(garbage, sizeof(garbage)).has_value());
  EXPECT_FALSE(import::alembic(bytes.data(), bytes.size() / 2).has_value());
}

TEST(Import, AlembicFacevaryingUvsFlipAndDedup) {
  // Two conventions, one fixture.
  //
  // (1) UV ORIGIN: Alembic's is BOTTOM-left, this library's Mesh uses
  // top-left, so v is flipped on import. Get it wrong and every textured
  // Alembic model is upside down.
  //
  // (2) DEDUP: corners merge OBJ-style on (position, uv, normal) — corners
  // that agree on all three become one vertex, and a disagreement in uv
  // splits one point into two vertices so each can keep its own uv.
  const std::string bytes = alembicArchiveBytes();
  auto model = import::alembic(bytes.data(), bytes.size());
  ASSERT_TRUE(model.has_value());
  const auto part = [&](std::string_view name) -> const Part* {
    for (const Part& p : model->parts)
      if (p.name == name) return &p;
    return nullptr;
  };
  // uvs indexed by POSITION — insertion order is a detail of the walk.
  const auto uvsAt = [](const Mesh& m, glm::vec3 p) {
    std::vector<glm::vec2> found;
    for (size_t i = 0; i < m.positions.size(); ++i)
      if (glm::length(m.positions[i] - p) < 1e-6f) found.push_back(m.uvs[i]);
    std::sort(found.begin(), found.end(),
              [](glm::vec2 a, glm::vec2 b) { return a.x < b.x; });
    return found;
  };

  // --- FACEVARYING: the uv source is the CORNER, and the dedup keys on the
  // corner index rather than on the uv value, so every corner becomes its
  // own vertex — 6 corners give 6 vertices even where two of them happen to
  // carry an identical uv. Nothing downstream depends on the merge, so the
  // reader does not pay for comparing values.
  const Part* quad = part("uvquad");
  ASSERT_NE(quad, nullptr);
  const Mesh& fv = quad->mesh;
  ASSERT_EQ(fv.positions.size(), 6u);
  ASSERT_EQ(fv.uvs.size(), 6u);
  EXPECT_EQ(fv.triangleCount(), 2u);
  // The flip in one value: the file's uv (0,0) on the origin corner arrives
  // as (0,1). No reader that passed v through unchanged could produce that.
  const std::vector<glm::vec2> atOrigin = uvsAt(fv, {0, 0, 0});
  ASSERT_EQ(atOrigin.size(), 2u);  // corners 0 and 3, NOT welded
  for (const glm::vec2& uv : atOrigin) {
    EXPECT_FLOAT_EQ(uv.x, 0.0f);
    EXPECT_FLOAT_EQ(uv.y, 1.0f);
  }
  EXPECT_FLOAT_EQ(uvsAt(fv, {0, 1, 0}).at(0).y, 0.0f);  // file v=1 -> 0
  // The shared point at (1,1,0) carried two different uvs; both survive,
  // both flipped.
  const std::vector<glm::vec2> shared = uvsAt(fv, {1, 1, 0});
  ASSERT_EQ(shared.size(), 2u);
  EXPECT_FLOAT_EQ(shared[0].x, 0.25f);
  EXPECT_FLOAT_EQ(shared[0].y, 0.25f);  // file (0.25, 0.75)
  EXPECT_FLOAT_EQ(shared[1].x, 1.0f);
  EXPECT_FLOAT_EQ(shared[1].y, 0.0f);  // file (1, 1)

  // --- VERTEX scope: the uv source IS the point, so every corner of a
  // point keys the same and the six corners weld back down to four
  // vertices. Identical topology to the facevarying case, identical flip —
  // only the scope differs.
  const Part* weld = part("uvweld");
  ASSERT_NE(weld, nullptr);
  const Mesh& vw = weld->mesh;
  ASSERT_EQ(vw.positions.size(), 4u);  // the merge the facevarying case
  ASSERT_EQ(vw.uvs.size(), 4u);        // cannot make
  EXPECT_EQ(vw.triangleCount(), 2u);
  const std::vector<glm::vec2> weldOrigin = uvsAt(vw, {0, 0, 0});
  ASSERT_EQ(weldOrigin.size(), 1u);
  EXPECT_FLOAT_EQ(weldOrigin[0].x, 0.0f);
  EXPECT_FLOAT_EQ(weldOrigin[0].y, 1.0f);  // file (0,0) -> (0,1)
  const std::vector<glm::vec2> weldShared = uvsAt(vw, {1, 1, 0});
  ASSERT_EQ(weldShared.size(), 1u);
  EXPECT_FLOAT_EQ(weldShared[0].x, 1.0f);
  EXPECT_FLOAT_EQ(weldShared[0].y, 0.0f);  // file (1,1) -> (1,0)
}

TEST(Import, AlembicTimeSampleSelection) {
  const std::string bytes = alembicArchiveBytes();
  const auto cloudY = [&](double time) -> float {
    auto model = import::alembic(bytes.data(), bytes.size(), {.time = time});
    if (!model) return -1.0f;
    for (const Part& part : model->parts)
      if (part.name == "cloud") return part.mesh.positions.at(0).y;
    return -1.0f;
  };
  // A requested time selects the NEAREST stored sample, not the one before
  // it: 0.6 of a frame in is closer to frame 1, so frame 1 is what is read.
  // Samples are never interpolated, so the values are always ones an
  // authoring tool actually wrote.
  EXPECT_FLOAT_EQ(cloudY(0), 0.0f);
  EXPECT_FLOAT_EQ(cloudY(1.0 / 24), 1.0f);
  EXPECT_FLOAT_EQ(cloudY(0.6 / 24), 1.0f);
}

TEST(Save, PlyRoundTripsCloudLanes) {
  // A Cloud carrying one of every lane kind writes to PLY and comes back
  // reconstituted: vectors fold from _x/_y/_z, colours from _r/_g/_b/_a,
  // and the conventional names keep their conventional spellings ("normal"
  // as nx/ny/nz, "tint" as red/green/blue/alpha). "tint" is the one lane
  // written as uchar channels, so it comes back quantized to 1/255 while
  // every other lane is float.
  Cloud cloud;
  cloud.positions = {{0, 0, 0}, {4, 0, 0}, {0, 4, 0}, {4, 4, 2}};
  cloud.scalar("energy") = {0.5f, 1.5f, 2.5f, 3.5f};
  cloud.vector("dir") = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 0, 0}};
  cloud.vector("normal") = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  cloud.color("glow") = {{0.25f, 0.5f, 0.75f, 1.0f},
                         {1, 0, 0, 0.5f},
                         {0, 1, 0, 0.25f},
                         {0, 0, 1, 0.125f}};
  cloud.color("tint") = {
      {1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 1, 0.5f}};

  const std::string bytes = save::ply(cloud);
  auto model = import::model(bytes.data(), bytes.size(), "trip.ply");
  ASSERT_TRUE(model.has_value());
  const Cloud back = model->parts.front().asCloud();
  ASSERT_EQ(back.size(), 4u);
  EXPECT_NEAR(back.positions[3].z, 2, 1e-4f);
  ASSERT_TRUE(back.scalarIf("energy"));
  EXPECT_FLOAT_EQ((*back.scalarIf("energy"))[2], 2.5f);
  ASSERT_TRUE(back.vectorIf("dir"));  // folded back from dir_x/_y/_z
  EXPECT_FLOAT_EQ((*back.vectorIf("dir"))[1].y, 1);
  ASSERT_TRUE(back.vectorIf("normal"));  // via nx/ny/nz
  EXPECT_FLOAT_EQ((*back.vectorIf("normal"))[0].z, 1);
  ASSERT_TRUE(back.colorIf("glow"));  // folded from glow_r/_g/_b/_a
  EXPECT_NEAR((*back.colorIf("glow"))[0].y, 0.5f, 1e-4f);
  EXPECT_NEAR((*back.colorIf("glow"))[3].w, 0.125f, 1e-4f);
  ASSERT_TRUE(back.colorIf("tint"));  // uchar red/green/blue/alpha
  EXPECT_NEAR((*back.colorIf("tint"))[1].y, 1, 1.5f / 255.0f);
  EXPECT_NEAR((*back.colorIf("tint"))[3].w, 0.5f, 1.5f / 255.0f);
}

TEST(Save, PlyRoundTripsMeshWithFaces) {
  Mesh quad = mesh::quad(10, 6);
  quad.colors.assign(quad.vertexCount(), {0.2f, 0.9f, 0.4f, 1});
  const std::string bytes = save::ply(quad);
  auto model = import::model(bytes.data(), bytes.size(), "quad.ply");
  ASSERT_TRUE(model.has_value());
  const Mesh& back = model->parts.front().mesh;
  ASSERT_EQ(back.vertexCount(), 4u);
  EXPECT_EQ(back.triangleCount(), 2u);
  glm::vec3 lo, hi;
  back.bounds(&lo, &hi);
  EXPECT_NEAR(hi.x - lo.x, 10, 1e-4f);
  EXPECT_NEAR(hi.y - lo.y, 6, 1e-4f);
  ASSERT_EQ(back.uvs.size(), 4u);
  ASSERT_EQ(back.colors.size(), 4u);
  EXPECT_NEAR(back.colors[0].y, 0.9f, 1.5f / 255.0f);
}

TEST(Save, BinaryPlyRoundTripsExactly) {
  // Binary rows are raw floats, so the round trip is BIT-exact and every
  // check can be an equality; only "tint", written as uchar channels, still
  // pays its quantization. The values are chosen to be ones the ascii
  // writer's six significant digits would have rounded — thirds, sevenths,
  // 1e-5 — so a silent fallback to the ascii path would fail here.
  Cloud cloud;
  cloud.positions = {{0.1f, 2.3f, -4.5f},
                     {6.7f, -8.9f, 10.11f},
                     {1.0f / 3.0f, 2.0f / 7.0f, 1e-5f}};
  cloud.scalar("energy") = {0.5f, 1.0f / 3.0f, 2.5f};
  cloud.vector("dir") = {{1, 0, 0}, {0.1f, 0.2f, 0.3f}, {0, 0, 1}};
  cloud.vector("normal") = {{0, 0, 1}, {0, 1, 0}, {1, 0, 0}};
  cloud.color("glow") = {
      {0.25f, 0.5f, 0.75f, 1.0f}, {1.0f / 3.0f, 0, 0, 0.5f}, {0, 1, 0, 0.125f}};
  cloud.color("tint") = {{1, 0, 0, 1}, {0, 1, 0, 1}, {1, 1, 1, 0.5f}};

  const std::string bytes = save::ply(cloud, {.binary = true});
  auto model = import::model(bytes.data(), bytes.size(), "bin.ply");
  ASSERT_TRUE(model.has_value());
  const Cloud back = model->parts.front().asCloud();
  ASSERT_EQ(back.size(), 3u);
  for (size_t i = 0; i < 3; ++i)
    for (int c = 0; c < 3; ++c)
      EXPECT_FLOAT_EQ(back.positions[i][c], cloud.positions[i][c]);
  ASSERT_TRUE(back.scalarIf("energy"));
  EXPECT_FLOAT_EQ((*back.scalarIf("energy"))[1], 1.0f / 3.0f);
  ASSERT_TRUE(back.vectorIf("dir"));  // folded back from dir_x/_y/_z
  EXPECT_FLOAT_EQ((*back.vectorIf("dir"))[1].z, 0.3f);
  ASSERT_TRUE(back.vectorIf("normal"));  // via nx/ny/nz
  EXPECT_FLOAT_EQ((*back.vectorIf("normal"))[1].y, 1.0f);
  ASSERT_TRUE(back.colorIf("glow"));  // folded from glow_r/_g/_b/_a
  EXPECT_FLOAT_EQ((*back.colorIf("glow"))[1].x, 1.0f / 3.0f);
  EXPECT_FLOAT_EQ((*back.colorIf("glow"))[2].w, 0.125f);
  ASSERT_TRUE(back.colorIf("tint"));  // uchar red/green/blue/alpha
  EXPECT_NEAR((*back.colorIf("tint"))[2].w, 0.5f, 1.5f / 255.0f);

  // Faces are written in the binary encoding too: a list count as one raw
  // uchar, then raw int32 indices — the same rows the ascii writer spells
  // out in text.
  Mesh quad = mesh::quad(10, 6);
  quad.colors.assign(quad.vertexCount(), {0.2f, 0.9f, 0.4f, 1});
  const std::string meshBytes = save::ply(quad, {.binary = true});
  auto meshModel =
      import::model(meshBytes.data(), meshBytes.size(), "quad.ply");
  ASSERT_TRUE(meshModel.has_value());
  const Mesh& tri = meshModel->parts.front().mesh;
  ASSERT_EQ(tri.vertexCount(), 4u);
  EXPECT_EQ(tri.triangleCount(), 2u);
  glm::vec3 lo, hi;
  tri.bounds(&lo, &hi);
  EXPECT_FLOAT_EQ(hi.x - lo.x, 10.0f);
  EXPECT_FLOAT_EQ(hi.y - lo.y, 6.0f);
  ASSERT_EQ(tri.colors.size(), 4u);
  EXPECT_NEAR(tri.colors[0].y, 0.9f, 1.5f / 255.0f);
}

TEST(Save, PlyHeaderAndRowsAgreeWhenLanesMismatchAndEmptyCloudDeclines) {
  // A lane whose length does not match the point count is skipped, and the
  // header and the row writer must agree on WHICH lanes those are — a
  // property declared in the header but not written on each row (or the
  // reverse) desyncs the file and makes every later value read wrong. The
  // export parsing at all is most of the assertion here.
  Cloud cloud;
  cloud.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  cloud.scalar("energy") = {1, 2, 3};
  cloud.scalars["stub"] = {7};                    // wrong length
  cloud.vectors["off"] = {{1, 2, 3}, {4, 5, 6}};  // wrong length
  const std::string bytes = save::ply(cloud);
  auto model = import::model(bytes.data(), bytes.size(), "skip.ply");
  ASSERT_TRUE(model.has_value());
  const Part& part = model->parts.front();
  ASSERT_EQ(part.mesh.vertexCount(), 3u);
  ASSERT_EQ(part.scalarLanes.count("energy"), 1u);
  EXPECT_FLOAT_EQ(part.scalarLanes.at("energy")[2], 3.0f);
  EXPECT_EQ(part.scalarLanes.count("stub"), 0u);
  EXPECT_EQ(part.vectorLanes.count("off"), 0u);
  EXPECT_EQ(part.scalarLanes.count("off_x"), 0u);

  // Nothing to write is refused rather than emitted: the string overloads
  // return empty and the file overloads return false, so no zero-element
  // PLY is ever produced — this library's own reader rejects one.
  EXPECT_TRUE(save::ply(Cloud{}).empty());
  EXPECT_TRUE(save::ply(Mesh{}).empty());
  const std::filesystem::path file =
      std::filesystem::temp_directory_path() / "sigilshape_empty_decline.ply";
  EXPECT_FALSE(save::ply(file, Cloud{}));
  EXPECT_FALSE(save::ply(file, Mesh{}));
}

TEST(Pop, CookMeshFormsAModelFromAChain) {
  // A pop chain is a DESCRIPTION; cooking is what forms it. The output is a
  // plain Mesh, the same type the Skia painter and the GPU surface path
  // both take, so a chain needs no adapter to be drawn either way.
  // Cooking is also pure: the same chain cooks to the same model, and
  // editing the chain value is the only way to get a different one.
  pop::SplineScatter scatter;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    scatter.loop.push_back({200.0f * std::cos(a), 0, 200.0f * std::sin(a)});
  }
  scatter.count = 500;
  scatter.head = 1;
  scatter.span = 1;
  scatter.radius = 12;
  pop::Chain chain = {scatter, pop::Vary{pop::Lane::Scale, 1.0f, 0.4f, 3},
                      pop::Ramp{pop::Lane::Color, {1, 0, 0, 1}, {0, 0, 1, 1}}};
  const Mesh stamp = mesh::quad(6, 6);
  const Mesh model = popops::cookMesh(chain, stamp);
  EXPECT_EQ(model.vertexCount(), 500u * stamp.vertexCount());
  EXPECT_EQ(model.triangleCount(), 500u * stamp.triangleCount());
  ASSERT_EQ(model.colors.size(), model.vertexCount());  // tint baked
  const Mesh again = popops::cookMesh(chain, stamp);
  ASSERT_EQ(again.positions.size(), model.positions.size());
  EXPECT_EQ(again.positions[123].x, model.positions[123].x);
  chain.push_back(pop::Math{pop::Lane::P, {1, 1, 1, 1}, {0, 500, 0, 0}});
  const Mesh lifted = popops::cookMesh(chain, stamp);
  EXPECT_GT(lifted.positions[123].y, model.positions[123].y + 400.0f);
}

TEST(Pop, SweptSinksBendWithTheChain) {
  // The chain's cooked POINTS are the path a sweep follows, so any operator
  // that moves points also bends every swept surface built from the chain.
  // The same description feeds a tube and a ribbon unchanged.
  pop::SplineScatter scatter;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    scatter.loop.push_back({300.0f * std::cos(a), 0, 300.0f * std::sin(a)});
  }
  scatter.count = 96;
  scatter.head = 1;
  scatter.span = 1;
  pop::Chain chain = {scatter, pop::Noise{pop::Lane::P, 40, 0.01f, 5}};

  const Mesh tube =
      popops::cookTube(chain, 12, 10, {.closed = true, .segments = 200});
  EXPECT_GT(tube.triangleCount(), 1000u);
  glm::vec3 lo, hi;
  tube.bounds(&lo, &hi);
  EXPECT_GT(hi.y - lo.y, 20.0f) << "noise must bend the sweep off-plane";
  // 600 across the scattered circle, plus the tube radius on each side;
  // the wide tolerance is the noise, which is free to push either way.
  EXPECT_NEAR(hi.x - lo.x, 624, 130);

  const Mesh ribbon =
      popops::cookRibbon(chain, 60, {.closed = true, .segments = 160});
  EXPECT_GT(ribbon.triangleCount(), 200u);

  chain.push_back(pop::Math{pop::Lane::P, {1, 1, 1, 1}, {0, 900, 0, 0}});
  const Mesh lifted = popops::cookTube(chain, 12, 10, {.closed = true});
  glm::vec3 lo2, hi2;
  lifted.bounds(&lo2, &hi2);
  EXPECT_GT(lo2.y, hi.y + 400.0f) << "value edit re-forms the model high";
}

TEST(Pop, ArtistSpellingReadsLikeTouchDesigner) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({250.0f * std::cos(a), 0, 250.0f * std::sin(a)});
  }
  // The builder spelling is one expression: an entry verb, the operators,
  // and a terminal verb that cooks. Every parameter has a default, so a
  // chain can be written without naming any of them.
  const Mesh wobble = pop::on(loop).count(64).noise(30).tube(10, 8, true);
  EXPECT_GT(wobble.triangleCount(), 500u);

  // The builder holds nothing the chain does not: it converts to a
  // pop::Chain whose operators are still ordinary values, so a chain built
  // fluently can still be taken apart and edited afterwards.
  pop::Chain c = pop::on(loop).count(10).spread(5).vary(0.4f).fade(
      {1, 0, 0, 1}, {0, 0, 1, 1});
  EXPECT_EQ(c.size(), 3u);  // scatter + vary + ramp
  std::get<pop::SplineScatter>(c.front()).count = 20;
  EXPECT_EQ(popops::cook(c).size(), 20u);
}

// Smooth must undo what noise did to the local shape of the path, measured
// as the summed discrete second difference along the points — the quantity
// that shows up as kinks in anything swept along them. Halving it is a loose
// bar deliberately: the check is that smoothing acts on neighbours at all,
// not that it reaches a particular amount.
TEST(Pop, SmoothHealsNoiseKinks) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({250.0f * std::cos(a), 0, 250.0f * std::sin(a)});
  }
  const auto jaggedness = [](const Cloud& cloud) {
    double sum = 0;
    for (size_t i = 1; i + 1 < cloud.size(); ++i)
      sum += glm::length(cloud.positions[i - 1] - cloud.positions[i] * 2.0f +
                         cloud.positions[i + 1]);
    return sum;
  };
  const double rough =
      jaggedness(popops::cook(pop::on(loop).count(80).noise(30).chain()));
  const double healed = jaggedness(
      popops::cook(pop::on(loop).count(80).noise(30).smooth(0.6f, 3).chain()));
  EXPECT_LT(healed, rough * 0.5) << rough << " -> " << healed;
}

TEST(Pop, SweepCarriesAnyProfileAlongTheChain) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({220.0f * std::cos(a), 0, 220.0f * std::sin(a)});
  }
  SkPathBuilder starProfile;
  for (int i = 0; i < 10; ++i) {
    const float r = i % 2 == 0 ? 24.0f : 10.0f;
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    const SkPoint p = {r * std::cos(a), r * std::sin(a)};
    if (i == 0)
      starProfile.moveTo(p);
    else
      starProfile.lineTo(p);
  }
  starProfile.close();
  const Mesh swept = pop::on(loop).count(60).smooth(0.4f).sweep(
      starProfile.detach(), true, 120);
  EXPECT_GT(swept.triangleCount(), 1500u);
  glm::vec3 lo, hi;
  swept.bounds(&lo, &hi);
  // The swept profile is carried in the frame's normal plane, so the star's
  // longest arm adds its radius on each side of the 220 scatter circle...
  EXPECT_NEAR(hi.x - lo.x, 2 * (220 + 24), 30);
  // ...and stands out of the loop's own plane rather than lying flat in it.
  EXPECT_GT(hi.y - lo.y, 20.0f);
}

TEST(Pop, ChainsComposeIntoEachOther) {
  // A chain is accepted anywhere a path is: chain A's cooked points become
  // chain B's path, so operators compose without a separate combinator.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({220.0f * std::cos(a), 0, 220.0f * std::sin(a)});
  }
  const pop::Chain spine = pop::on(loop).count(48).noise(26).smooth(0.5f);
  const Cloud beads = pop::on(spine).count(300).spread(6).cloud();
  EXPECT_EQ(beads.size(), 300u);
  // The beads inherit A's off-plane noise, which proves they were scattered
  // along the composed path and not along the flat circle it started from.
  float yMin = 1e9f, yMax = -1e9f;
  for (const glm::vec3& p : beads.positions) {
    yMin = std::min(yMin, p.y);
    yMax = std::max(yMax, p.y);
  }
  EXPECT_GT(yMax - yMin, 12.0f);
  // And any sink still applies to the composition.
  EXPECT_GT(pop::on(spine).count(80).tube(6, 8, true).triangleCount(), 500u);
}

TEST(Pop, ChainsSeedFromFormedModels) {
  // A formed Mesh is also a valid seed: scattering ON a cooked surface and
  // forming again closes the loop, so a model can be built in stages
  // without any stage being a special case.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({200.0f * std::cos(a), 0, 200.0f * std::sin(a)});
  }
  const Mesh cable = pop::on(loop).count(64).noise(20).tube(9, 8, true);
  const Cloud dust = pop::on(cable, 500).cloud();
  EXPECT_EQ(dust.size(), 500u);
  glm::vec3 mLo, mHi, dLo, dHi;
  cable.bounds(&mLo, &mHi);
  Mesh asPoints;
  asPoints.positions = dust.positions;
  asPoints.bounds(&dLo, &dHi);
  EXPECT_GE(dLo.x, mLo.x - 1);
  EXPECT_LE(dHi.x, mHi.x + 1);  // dust lives on the cable
  EXPECT_GT(pop::on(cable, 300).stamps(mesh::quad(4, 4)).triangleCount(), 500u);
}

TEST(Curves, BannerHangsGravityUpright) {
  Spline3 loop;
  loop.closed = true;
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i / 12.0f * 2.0f * (float)M_PI;
    loop.points.push_back(
        {300.0f * std::cos(a), 40.0f * std::sin(2 * a), 300.0f * std::sin(a)});
  }
  const Mesh band = curves::banner(loop, {.width = 50, .sections = 120});
  ASSERT_EQ(band.vertexCount(), 240u);
  // A banner hangs: its width is held vertical in world space rather than
  // rolling with the curve's frame, which is what keeps text on it upright
  // all the way round a closed loop. Vertices come in pairs per section, so
  // the first of every pair must sit ABOVE its partner in y everywhere.
  for (size_t i = 0; i + 1 < band.positions.size(); i += 2)
    EXPECT_GT(band.positions[i].y, band.positions[i + 1].y);
}

TEST(Pop, ImportedModelsJoinTheSystem) {
  // An imported model is an ordinary Mesh, so it can both SEED a chain (be
  // scattered on) and serve as a STAMP (be instanced along one). Nothing in
  // either path distinguishes a loaded mesh from a generated one.
  const std::string obj = kCubeObj;
  auto model = import::model(obj.data(), obj.size(), "cube.obj");
  ASSERT_TRUE(model.has_value());
  const Mesh cube = model->merged();

  // Scatter on the imported surface...
  const Cloud dust = pop::on(cube, 300).cloud();
  EXPECT_EQ(dust.size(), 300u);
  glm::vec3 lo, hi;
  Mesh asPoints;
  asPoints.positions = dust.positions;
  asPoints.bounds(&lo, &hi);
  EXPECT_GE(lo.x, -1.01f);
  EXPECT_LE(hi.x, 1.01f);  // points live on the unit cube

  // ...and use the imported model AS the stamp along a chain.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({80.0f * std::cos(a), 0, 80.0f * std::sin(a)});
  }
  const Mesh cubes = pop::on(loop).count(24).vary(0.4f).stamps(cube);
  EXPECT_EQ(cubes.triangleCount(), 24u * cube.triangleCount());
}

TEST(Pop, NamedAttributesFlowAndExport) {
  // Any NAME is an attribute: an operator that takes a lane takes a custom
  // one on equal terms with the built-in ones, so a lane can be created,
  // jittered and scaled by the same ops the standard lanes use, and it
  // exports under the name it was given.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({200.0f * std::cos(a), 0, 200.0f * std::sin(a)});
  }
  const pop::Chain chain =
      pop::on(loop)
          .count(64)
          .fill("energy", {0.5f, 0, 0, 0})
          .op(pop::Jitter{"energy", 0.25f, 5})
          .op(pop::Math{"energy", {2, 1, 1, 1}, {0, 0, 0, 0}});
  const Cloud cooked = popops::cook(chain);
  const std::vector<glm::vec4>* energy = cooked.colorIf("energy");
  ASSERT_TRUE(energy) << "customs must export under their own name";
  float lo = 1e9f, hi = -1e9f;
  for (const glm::vec4& e : *energy) {
    lo = std::min(lo, e.r);
    hi = std::max(hi, e.r);
  }
  EXPECT_GT(hi, lo + 0.1f);  // jittered, not constant
  // Jitter is bounded by its amplitude, so the lane stays in
  // 0.5 +/- 0.25 before the Math op doubles it into 0.5 .. 1.5.
  EXPECT_GT(lo, 0.4f);
  EXPECT_LT(hi, 1.6f);
}

TEST(Pop, RampByDrivesOneAttributeFromAnotherThroughATable) {
  // rampBy is a value curve, not a palette: one lane's component indexes a
  // table of stops over an EXPLICIT domain, and the result is written to
  // another lane. With three stops the interesting behaviour is between
  // them, so the checks sample the interpolation and the clamping, not just
  // the stop values.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({200.0f * std::cos(a), 0, 200.0f * std::sin(a)});
  }
  const glm::vec4 lowStop{0, 0, 1, 1};
  const glm::vec4 midStop{0, 1, 0, 1};
  const glm::vec4 highStop{1, 0, 0, 1};

  // Height is authored by hand so every sample point is known: P.y is
  // set outright, then read back through the table over [-100, 100].
  const auto colorAtHeight = [&](float y) {
    const pop::Chain chain =
        pop::on(loop)
            .count(4)
            .fill(pop::Lane::P, {0, y, 0, 0})
            .rampBy(pop::Lane::P, 1, {lowStop, midStop, highStop}, -100, 100);
    const Cloud cooked = popops::cook(chain);
    const std::vector<glm::vec4>* tint = cooked.colorIf("tint");
    EXPECT_TRUE(tint);
    return tint ? (*tint)[0] : glm::vec4{0, 0, 0, 0};
  };

  // The stops themselves.
  EXPECT_NEAR(colorAtHeight(-100).b, lowStop.b, 1e-5f);
  EXPECT_NEAR(colorAtHeight(0).g, midStop.g, 1e-5f);
  EXPECT_NEAR(colorAtHeight(100).r, highStop.r, 1e-5f);
  // BETWEEN two stops: a quarter of the way up the domain is halfway from
  // the low stop to the mid stop, with nothing of the far stop mixed in. A
  // nearest-stop lookup would return a stop colour here instead.
  const glm::vec4 quarter = colorAtHeight(-50);
  EXPECT_NEAR(quarter.b, 0.5f, 1e-5f);
  EXPECT_NEAR(quarter.g, 0.5f, 1e-5f);
  EXPECT_NEAR(quarter.r, 0.0f, 1e-5f);
  const glm::vec4 threeQuarters = colorAtHeight(50);
  EXPECT_NEAR(threeQuarters.g, 0.5f, 1e-5f);
  EXPECT_NEAR(threeQuarters.r, 0.5f, 1e-5f);
  // ...and the domain CLAMPS at both ends rather than extrapolating.
  EXPECT_NEAR(colorAtHeight(-1e4f).b, lowStop.b, 1e-5f);
  EXPECT_NEAR(colorAtHeight(1e4f).r, highStop.r, 1e-5f);
  EXPECT_NEAR(colorAtHeight(1e4f).g, 0.0f, 1e-5f);

  // Custom lanes work at BOTH ends: read a named lane, write a named lane,
  // and the destination is created if it does not exist yet. The stops are
  // ordinary values, not colours, so a lookup can drive any quantity.
  const pop::Chain custom =
      pop::on(loop)
          .count(16)
          .fill("energy", {0.25f, 0, 0, 0})
          .rampBy("energy", 0, {{10, 0, 0, 0}, {20, 0, 0, 0}}, 0, 1, "heat");
  const Cloud cooked = popops::cook(custom);
  const std::vector<glm::vec4>* heat = cooked.colorIf("heat");
  ASSERT_TRUE(heat) << "a lookup must create the lane it writes";
  EXPECT_NEAR((*heat)[0].r, 12.5f, 1e-4f);
}

TEST(Pop, OrderPutsTheWholePointInDrawOrder) {
  // Sorting is a PERMUTATION of whole points. Two things must hold: the key
  // really orders them, and every lane travels with its own point — a sort
  // that moved positions alone would shear a cloud's colours and sizes onto
  // the wrong points, which nothing downstream could detect.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    loop.push_back(
        {180.0f * std::cos(a), 40.0f * std::sin(3 * a), 180.0f * std::sin(a)});
  }
  const auto describe = [&](bool sorted, bool descending) {
    pop::Builder b = pop::on(loop);
    b.count(200).seed(3).spread(20).vary(0.4f).fade({0, 0, 0, 1}, {1, 1, 1, 1});
    if (sorted) b.order({0, 1, 0}, descending);
    return (pop::Chain)b;
  };

  const Cloud plain = popops::cook(describe(false, false));
  const Cloud rising = popops::cook(describe(true, false));
  ASSERT_EQ(plain.size(), 200u);
  ASSERT_EQ(rising.size(), 200u);

  // 1. The order is real, and it is not the order it started in.
  for (size_t i = 1; i < rising.size(); ++i)
    EXPECT_LE(rising.positions[i - 1].y, rising.positions[i].y);
  size_t moved = 0;
  for (size_t i = 0; i < plain.size(); ++i)
    if (plain.positions[i] != rising.positions[i]) ++moved;
  EXPECT_GT(moved, 100u) << "the sort must actually reorder";

  // 2. Coherence: each sorted point's t/size/tint are the ones its
  // ORIGINAL position carried. Found by matching position, so the
  // check knows nothing about the permutation itself.
  const std::vector<float>* plainT = plain.scalarIf("t");
  const std::vector<float>* risingT = rising.scalarIf("t");
  const std::vector<float>* plainSize = plain.scalarIf("size");
  const std::vector<float>* risingSize = rising.scalarIf("size");
  const std::vector<glm::vec4>* plainTint = plain.colorIf("tint");
  const std::vector<glm::vec4>* risingTint = rising.colorIf("tint");
  ASSERT_TRUE(plainT && risingT && plainSize && risingSize && plainTint &&
              risingTint);
  size_t matched = 0;
  for (size_t i = 0; i < rising.size(); ++i)
    for (size_t j = 0; j < plain.size(); ++j)
      if (rising.positions[i] == plain.positions[j]) {
        EXPECT_FLOAT_EQ((*risingT)[i], (*plainT)[j]);
        EXPECT_FLOAT_EQ((*risingSize)[i], (*plainSize)[j]);
        EXPECT_FLOAT_EQ((*risingTint)[i].r, (*plainTint)[j].r);
        ++matched;
        break;
      }
  EXPECT_EQ(matched, rising.size()) << "every point must survive";

  // 3. Descending is exactly the ascending permutation reversed. The keys
  // here are distinct, so there are no ties to make that ambiguous. This is
  // the painter-order spelling: farthest first.
  const Cloud falling = popops::cook(describe(true, true));
  ASSERT_EQ(falling.size(), rising.size());
  for (size_t i = 0; i < falling.size(); ++i)
    EXPECT_EQ(falling.positions[i], rising.positions[rising.size() - 1 - i]);

  // 4. Point order IS the swept path, so sorting the same points forms a
  // genuinely different tube. Sorting is therefore an authoring operation
  // with geometric consequences, not just a draw-order adjustment.
  const Mesh unsorted = popops::cookTube(describe(false, false), 4);
  const Mesh threaded = popops::cookTube(describe(true, false), 4);
  ASSERT_EQ(unsorted.positions.size(), threaded.positions.size());
  float drift = 0;
  for (size_t i = 0; i < unsorted.positions.size(); ++i)
    drift += glm::length(unsorted.positions[i] - threaded.positions[i]);
  EXPECT_GT(drift, 1000.0f);
}

TEST(Pop, SharedPcgHashKeepsBothConsumersBitStable) {
  // One hash definition feeds both the pop operators and the point
  // generators, and the GPU kernels reimplement the same function. These
  // goldens are the exact bit patterns it produces: a single-bit change to
  // the hash would desync the CPU results from the GPU ones with no other
  // symptom, since both would still look like plausible randomness.
  //
  // The values are readable straight out of the definition. Jitter with
  // amplitude 0.5 on a zeroed lane is hash1(i * 3 + seed) - 0.5 with
  // nothing else mixed in...
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({100.0f * std::cos(a), 0, 100.0f * std::sin(a)});
  }
  const Cloud cooked = popops::cook(pop::on(loop)
                                        .count(6)
                                        .fill("h", {0, 0, 0, 0})
                                        .op(pop::Jitter{"h", 0.5f, 0}));
  const std::vector<glm::vec4>* h = cooked.colorIf("h");
  ASSERT_TRUE(h);
  ASSERT_EQ(h->size(), 6u);
  const float popGolden[6] = {0.231199384f,  -0.441547632f, -0.184789419f,
                              0.0919363499f, 0.274898827f,  0.0884094238f};
  for (size_t i = 0; i < 6; ++i)
    EXPECT_NEAR((*h)[i].x, popGolden[i], 1e-7f) << "pop hash lane " << i;

  // ...and a scatter into the unit box is the raw 0..1 stream in order,
  // three draws per point.
  const Cloud box = points::scatterBox({0, 0, 0}, {1, 1, 1}, 4, /*seed=*/7);
  ASSERT_EQ(box.positions.size(), 4u);
  const float pointsGolden[12] = {0.985658824f,  0.420034766f,  0.98710376f,
                                  0.480089128f,  0.151413783f,  0.589045703f,
                                  0.263890147f,  0.0428663865f, 0.913271964f,
                                  0.0273712128f, 0.317990124f,  0.787527025f};
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_NEAR(box.positions[i].x, pointsGolden[i * 3], 1e-7f);
    EXPECT_NEAR(box.positions[i].y, pointsGolden[i * 3 + 1], 1e-7f);
    EXPECT_NEAR(box.positions[i].z, pointsGolden[i * 3 + 2], 1e-7f);
  }
}

TEST(Pop, AtlasTexHintsRemapStampUvs) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({150.0f * std::cos(a), 0, 150.0f * std::sin(a)});
  }
  const pop::Chain chain = pop::on(loop).count(40).atlas(2, 2);
  const Mesh stamped = popops::cookMesh(chain, mesh::quad(8, 8));
  // atlas(2, 2) divides the texture into a 2x2 grid and assigns each point
  // one cell, remapping its stamp's uvs into that cell. So every stamp's
  // uvs span exactly half the range in each axis and sit wholly inside one
  // cell — a stamp straddling a cell edge would sample two sprites at once.
  const size_t stampVerts = mesh::quad(8, 8).vertexCount();
  int cellsSeen[4] = {0, 0, 0, 0};
  for (size_t p = 0; p < 40; ++p) {
    float uMin = 2, uMax = -1, vMin = 2, vMax = -1;
    for (size_t v = 0; v < stampVerts; ++v) {
      const glm::vec2 uv = stamped.uvs[p * stampVerts + v];
      uMin = std::min(uMin, uv.x);
      uMax = std::max(uMax, uv.x);
      vMin = std::min(vMin, uv.y);
      vMax = std::max(vMax, uv.y);
    }
    EXPECT_NEAR(uMax - uMin, 0.5f, 1e-4f);
    EXPECT_NEAR(vMax - vMin, 0.5f, 1e-4f);
    cellsSeen[(uMin > 0.25f ? 1 : 0) + (vMin > 0.25f ? 2 : 0)]++;
  }
  // Every one of the 40 points fell into one of the four cells.
  EXPECT_GT(cellsSeen[0] + cellsSeen[1] + cellsSeen[2] + cellsSeen[3], 39);
  int distinct = 0;
  for (int c : cellsSeen) distinct += c > 0 ? 1 : 0;
  EXPECT_GE(distinct, 3) << "the hash should spread across cells";
}

// --- Primitive attribute lanes --------------------------------------------

namespace {

/** Two triangles splitting a 100x100 square along the (-,-) to (+,+)
 *  diagonal. Front faces are wound counter-clockwise in this y-up world, and
 *  both triangles here are, so neither is dropped by backface culling.
 *
 *  Triangle 0 covers the half below the diagonal and triangle 1 the half
 *  above it; in the rendered image, with y increasing DOWNWARD on the
 *  canvas, those are the lower-right and upper-left halves respectively.
 *  Tests that sample a pixel per triangle depend on that mapping. */
Mesh splitQuad() {
  Mesh m;
  m.positions = {{-50, -50, 0}, {50, -50, 0}, {50, 50, 0}, {-50, 50, 0}};
  m.indices = {0, 1, 2, 0, 2, 3};
  return m;
}

}  // namespace

TEST(Mesh, PrimLanesSizeToTrianglesAndAppendPadsByName) {
  Mesh a = splitQuad();
  EXPECT_EQ(a.triangleCount(), 2u);
  EXPECT_EQ(a.primIf("Color"), nullptr) << "absent until touched";
  a.prim("Color")[0] = {1, 0, 0, 1};
  ASSERT_TRUE(a.primIf("Color"));
  ASSERT_EQ(a.primIf("Color")->size(), 2u) << "one float4 per triangle";
  EXPECT_EQ((*a.primIf("Color"))[1], (glm::vec4{1, 1, 1, 1}))
      << "\"Color\" creates white";
  a.prim("heat", {0, 0, 0, 0})[1] = {9, 0, 0, 0};

  // Appending a mesh with no prim lanes still pads ours, by the same name
  // convention, so the lane stays sized to triangleCount().
  Mesh b = splitQuad();
  a.append(b);
  ASSERT_EQ(a.triangleCount(), 4u);
  ASSERT_EQ(a.primIf("Color")->size(), 4u);
  EXPECT_EQ((*a.primIf("Color"))[2], (glm::vec4{1, 1, 1, 1}));
  EXPECT_EQ((*a.primIf("heat"))[3], (glm::vec4{0, 0, 0, 0}));

  // ...and in the other direction, a lane only THEY have is padded back
  // over our existing triangles so their values still start at our old
  // triangle count.
  Mesh c = splitQuad();
  c.prim("heat", {0, 0, 0, 0})[0] = {5, 0, 0, 0};
  a.append(c);
  ASSERT_EQ(a.triangleCount(), 6u);
  const std::vector<glm::vec4>* heat = a.primIf("heat");
  ASSERT_EQ(heat->size(), 6u);
  EXPECT_FLOAT_EQ((*heat)[1].x, 9.0f);  // ours survived
  EXPECT_FLOAT_EQ((*heat)[2].x, 0.0f);  // padded
  EXPECT_FLOAT_EQ((*heat)[4].x, 5.0f);  // theirs landed at the right run
  EXPECT_EQ(a.primIf("Color")->size(), 6u);
}

TEST(Mesh, BakePrimColorUnweldsFlatColoursIntoVertices) {
  Mesh m = splitQuad();
  m.colors.assign(4, glm::vec4{1, 1, 1, 0.5f});
  m.prim("Color")[0] = {1, 0, 0, 1};
  m.prim("Color")[1] = {0, 0, 1, 1};
  const Mesh baked = mesh::bakePrimColor(m, "Color");
  // A flat per-face colour cannot be expressed on shared vertices, so the
  // bake unwelds: every triangle gets its own three vertices and the
  // indices are renumbered to match.
  ASSERT_EQ(baked.vertexCount(), 6u);
  ASSERT_EQ(baked.triangleCount(), 2u);
  ASSERT_EQ(baked.colors.size(), 6u);
  for (size_t k = 0; k < 3; ++k) {
    EXPECT_FLOAT_EQ(baked.colors[k].r, 1.0f);
    EXPECT_FLOAT_EQ(baked.colors[k].b, 0.0f);
    EXPECT_FLOAT_EQ(baked.colors[3 + k].b, 1.0f);
    EXPECT_FLOAT_EQ(baked.colors[3 + k].r, 0.0f);
    // The face colour MULTIPLIES into whatever vertex colour was already
    // there rather than replacing it, so the original alpha survives.
    EXPECT_FLOAT_EQ(baked.colors[k].a, 0.5f);
  }
  EXPECT_EQ(baked.positions[3], m.positions[0]) << "triangle order kept";
  EXPECT_TRUE(baked.primIf("Color")) << "lanes survive the unweld";
  // Naming a lane that does not exist leaves the mesh alone — still welded,
  // still valid — rather than unwelding it or clearing its colours.
  EXPECT_EQ(mesh::bakePrimColor(m, "absent").vertexCount(), 4u);
}

TEST(Space, PrimColorLaneTintsTrianglesFlat) {
  Mesh m = splitQuad();
  m.prim("Color")[0] = {1, 0, 0, 1};  // lower-right half
  m.prim("Color")[1] = {0, 0, 1, 1};  // upper-left half

  space::Camera camera;
  camera.eye = {0, 0, 300};
  space::MeshStyle style;
  style.baseColor = {1, 1, 1, 1};

  const auto render = [&](const space::MeshStyle& s) {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 150));
    surface->getCanvas()->clear(SK_ColorBLACK);
    space::drawMesh(*surface->getCanvas(), m, glm::mat4(1.0f), camera,
                    {200, 150}, s);
    SkBitmap bm;
    bm.allocPixels(surface->imageInfo());
    EXPECT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
    return bm;
  };

  // Control: with no lane named the two halves are the same colour, so the
  // difference measured below can only come from the lane.
  const SkBitmap plain = render(style);
  EXPECT_EQ(plain.getColor(120, 95), plain.getColor(79, 54));

  style.primColorLane = "Color";
  const SkBitmap tinted = render(style);
  const SkColor lowerRight = tinted.getColor(120, 95);
  const SkColor upperLeft = tinted.getColor(79, 54);
  EXPECT_GT(SkColorGetR(lowerRight), SkColorGetB(lowerRight) + 40u);
  EXPECT_GT(SkColorGetB(upperLeft), SkColorGetR(upperLeft) + 40u);

  // The Normals mode writes a G-buffer, whose pixels are data to be decoded
  // by a later shading pass, not a picture. A colour tint applied there
  // would silently corrupt the normals it encodes, so the lane must be
  // ignored outside lit rendering.
  style.mode = space::MeshStyle::Mode::Normals;
  space::MeshStyle bare = style;
  bare.primColorLane.clear();
  EXPECT_EQ(render(style).getColor(120, 95), render(bare).getColor(120, 95));
}

// promote() moves a POINT lane onto the PRIMITIVES a point becomes: every
// triangle of a point's stamp carries that point's value. It is only
// meaningful where the output geometry can be traced back to a point, which
// is instancing — the swept sinks resample the chain into new geometry that
// no longer corresponds to points one for one, so they promote nothing.
TEST(Pop, PromoteCarriesPointLanesOntoPrimitives) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({180.0f * std::cos(a), 0, 180.0f * std::sin(a)});
  }
  const int kPoints = 24;
  const Mesh stamp = mesh::quad(6, 6);
  const pop::Chain chain = pop::on(loop)
                               .count(kPoints)
                               .fade({1, 0, 0, 1}, {0, 0, 1, 1})
                               .vary(0.5f)
                               .promote(pop::Lane::Color)
                               .promote("Id", "Id")
                               .promote(pop::Lane::Scale, "size");

  // Cooking to a Cloud is unaffected: points have no primitives, so a
  // promote in the chain is simply inert there.
  const Cloud cooked = popops::cook(chain);
  ASSERT_EQ(cooked.size(), (size_t)kPoints);

  const Mesh model = popops::cookMesh(chain, stamp);
  const size_t perStamp = stamp.triangleCount();
  ASSERT_EQ(model.triangleCount(), (size_t)kPoints * perStamp);

  const std::vector<glm::vec4>* color = model.primIf("Color");
  const std::vector<glm::vec4>* id = model.primIf("Id");
  const std::vector<glm::vec4>* size = model.primIf("size");
  ASSERT_TRUE(color && id && size);
  ASSERT_EQ(color->size(), model.triangleCount());

  const std::vector<glm::vec4>* tint = cooked.colorIf("tint");
  const std::vector<float>* sizes = cooked.scalarIf("size");
  ASSERT_TRUE(tint && sizes);
  for (size_t p = 0; p < (size_t)kPoints; ++p)
    for (size_t k = 0; k < perStamp; ++k) {
      const size_t tri = p * perStamp + k;
      // Every triangle of a stamp carries its owning POINT's values...
      EXPECT_EQ((*color)[tri], (*tint)[p]);
      EXPECT_FLOAT_EQ((*size)[tri].x, (*sizes)[p]);
      // ..."Id" is the owning point's index, so every triangle of one stamp
      // reports the same id and different stamps report different ones —
      // this is what lets a shader address a whole instance.
      EXPECT_FLOAT_EQ((*id)[tri].x, (float)p);
    }
  // The source lane really did vary along the chain, so the equalities
  // above are not comparing a constant against itself.
  EXPECT_GT((*color)[model.triangleCount() - 1].b, (*color)[0].b + 0.5f);

  EXPECT_TRUE(popops::cookTube(chain, 4).prims.empty());
}

TEST(Save, PlyWritesPrimLanesAsFaceProperties) {
  Mesh m = splitQuad();
  m.prim("Color")[0] = {1, 0, 0, 1};
  m.prim("Color")[1] = {0, 0.25f, 0, 1};
  const std::string text = save::ply(m);
  ASSERT_FALSE(text.empty());
  // Prim lanes are per-triangle, so they are declared on the FACE element,
  // and after the vertex_indices list because that is the order the rows
  // are written in. A reader walks properties in declaration order, so
  // header order and row order have to agree.
  const size_t face = text.find("element face 2");
  const size_t list = text.find("property list uchar int vertex_indices");
  const size_t prop = text.find("property float Color_r");
  ASSERT_NE(face, std::string::npos);
  ASSERT_NE(prop, std::string::npos);
  EXPECT_LT(face, list);
  EXPECT_LT(list, prop);
  EXPECT_NE(text.find("property float Color_a"), std::string::npos);
  // ...and written on each face row, after the three indices.
  EXPECT_NE(text.find("3 0 1 2 1 0 0 1\n"), std::string::npos);
  EXPECT_NE(text.find("3 0 2 3 0 0.25 0 1\n"), std::string::npos);
  // Adding face properties must not disturb the geometry: the written file
  // still reads back through this library's own importer with its triangles
  // intact. (That the prim VALUES also survive the trip is checked
  // separately, in PlyRoundTripsPrimitiveLanes.)
  auto back = import::model(text.data(), text.size(), "prims.ply");
  ASSERT_TRUE(back.has_value());
  ASSERT_EQ(back->parts.size(), 1u);
  EXPECT_EQ(back->parts.front().mesh.triangleCount(), 2u);
}

namespace {

std::vector<glm::vec3> flatRing(int n, float radius) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < n; ++i) {
    const float a = (float)i / (float)n * 2.0f * (float)M_PI;
    loop.push_back({radius * std::cos(a), 0, radius * std::sin(a)});
  }
  return loop;
}

}  // namespace

TEST(Pop, GroupWritesASelectionAndMasksTheNextFilter) {
  // A ring in the xz plane; a sphere selector around +x picks the points
  // on that side and nowhere else; a masked Math then lifts ONLY those,
  // and an unmasked point stays on the floor. Feather grades the edge.
  const std::vector<glm::vec3> loop = flatRing(12, 200);
  const pop::Chain chain = pop::on(loop)
                               .count(400)
                               .select("east", {200, 0, 0}, 120)
                               .move({0, 50, 0})
                               .masked("east");
  const Cloud cooked = popops::cook(chain);
  const std::vector<glm::vec4>* east = cooked.colorIf("east");
  ASSERT_TRUE(east);
  int lifted = 0, grounded = 0;
  for (size_t i = 0; i < cooked.size(); ++i) {
    const float sel = (*east)[i].x;
    EXPECT_TRUE(sel == 0.0f || sel == 1.0f) << "hard edge selects 0/1";
    if (sel == 1.0f) {
      EXPECT_NEAR(cooked.positions[i].y, 50.0f, 1e-3f);
      EXPECT_GT(cooked.positions[i].x, 80.0f);
      ++lifted;
    } else {
      EXPECT_NEAR(cooked.positions[i].y, 0.0f, 1e-3f);
      ++grounded;
    }
  }
  EXPECT_GT(lifted, 40);
  EXPECT_GT(grounded, 200);

  // Feathered: values in between exist, and the blend is proportional.
  const pop::Chain soft = pop::on(loop)
                              .count(400)
                              .select("east", {200, 0, 0}, 160, 0.6f)
                              .move({0, 50, 0})
                              .masked("east");
  const Cloud softCooked = popops::cook(soft);
  const std::vector<glm::vec4>* softEast = softCooked.colorIf("east");
  ASSERT_TRUE(softEast);
  int partial = 0;
  for (size_t i = 0; i < softCooked.size(); ++i) {
    const float sel = (*softEast)[i].x;
    EXPECT_NEAR(softCooked.positions[i].y, 50.0f * sel, 1e-3f);
    if (sel > 0.05f && sel < 0.95f) ++partial;
  }
  EXPECT_GT(partial, 10) << "the feather band must grade";

  // Combine: a second box selector UNIONS the west side in.
  pop::Chain both = chain;
  both.insert(both.begin() + 2, pop::Select{"east",
                                            pop::Select::Shape::Box,
                                            {-200, 0, 0},
                                            {120, 400, 120},
                                            0,
                                            false,
                                            pop::Select::Combine::Union});
  const Cloud unioned = popops::cook(both);
  int liftedBoth = 0;
  for (const glm::vec3& p : unioned.positions) liftedBoth += p.y > 25.0f;
  EXPECT_GT(liftedBoth, lifted + 40);

  // A mask naming a lane nothing wrote selects nobody.
  const Cloud nobody =
      popops::cook(pop::on(loop).count(50).move({0, 50, 0}).masked("ghost"));
  for (const glm::vec3& p : nobody.positions) EXPECT_NEAR(p.y, 0.0f, 1e-4f);
}

TEST(Pop, TransformAndPeakMovePointsAlongTheirFrame) {
  const std::vector<glm::vec3> loop = flatRing(12, 100);
  // A pure translation on P is Math's move; a rotation is not, and Dir
  // follows through orient() renormalized.
  const glm::mat4 turn = space::place({0, 30, 0}, 90);
  const Cloud a = popops::cook(pop::on(loop).count(60).affine(turn));
  const Cloud b = popops::cook(pop::on(loop).count(60));
  ASSERT_EQ(a.size(), b.size());
  for (size_t i = 0; i < a.size(); ++i) {
    const glm::vec4 expected = turn * glm::vec4(b.positions[i], 1.0f);
    EXPECT_NEAR(a.positions[i].x, expected.x, 1e-3f);
    EXPECT_NEAR(a.positions[i].y, expected.y, 1e-3f);
    EXPECT_NEAR(a.positions[i].z, expected.z, 1e-3f);
  }
  const Cloud oriented =
      popops::cook(pop::on(loop).count(60).orient(space::place({}, 90)));
  const std::vector<glm::vec3>* dirA = oriented.vectorIf("dir");
  const std::vector<glm::vec3>* dirB = b.vectorIf("dir");
  ASSERT_TRUE(dirA && dirB);
  for (size_t i = 0; i < 60; ++i) {
    EXPECT_NEAR(glm::length((*dirA)[i]), 1.0f, 1e-4f);
    // Yaw by 90 about +Y: (x, y, z) -> (z, y, -x).
    EXPECT_NEAR((*dirA)[i].x, (*dirB)[i].z, 1e-3f);
    EXPECT_NEAR((*dirA)[i].z, -(*dirB)[i].x, 1e-3f);
  }

  // Peak: on a loop scatter Dir is the tangent, so peaking slides every
  // point along the ring by the same distance — the radius holds.
  const Cloud peaked = popops::cook(pop::on(loop).count(60).peak(25));
  for (size_t i = 0; i < 60; ++i) {
    const float moved = glm::length(peaked.positions[i] - b.positions[i]);
    EXPECT_NEAR(moved, 25.0f, 1e-3f);
  }
  // Peak along a custom zero lane moves nothing.
  const Cloud still = popops::cook(pop::on(loop).count(60).peak(25, "nowhere"));
  for (size_t i = 0; i < 60; ++i)
    EXPECT_NEAR(glm::length(still.positions[i] - b.positions[i]), 0.0f, 1e-4f);
}

TEST(Pop, DeformersTwistTaperAndBend) {
  // A vertical column: points along y from 0 to 200, all at x = 50.
  std::vector<glm::vec3> loop = {
      {50, 0, 0}, {50, 200, 0}, {50, 200, 1}, {50, 0, 1}};
  const auto column = [&] {
    return pop::on(loop).count(200).window(0.5f, 0.5f);
  };
  const Cloud base = popops::cook(column());

  // Twist 180 degrees over 0..200: a point at the top lands at x = -50.
  const Cloud twisted = popops::cook(column().twist(180, {0, 1, 0}, 0, 200));
  for (size_t i = 0; i < 200; ++i) {
    const glm::vec3& p0 = base.positions[i];
    const glm::vec3& p1 = twisted.positions[i];
    EXPECT_NEAR(p1.y, p0.y, 1e-3f);
    EXPECT_NEAR(glm::length(glm::vec2{p1.x, p1.z}),
                glm::length(glm::vec2{p0.x, p0.z}), 1e-3f)
        << "twist preserves the radius";
    const float u = std::clamp(p0.y / 200.0f, 0.0f, 1.0f);
    const float ang = (float)M_PI * u;
    // Rodrigues about +Y: x' = x cos + z sin.
    EXPECT_NEAR(p1.x, p0.x * std::cos(ang) + p0.z * std::sin(ang), 1e-2f);
  }

  // Taper to 0.2 at the top: the radius shrinks linearly.
  const Cloud tapered = popops::cook(column().taper(0.2f, {0, 1, 0}, 0, 200));
  for (size_t i = 0; i < 200; ++i) {
    const glm::vec3& p0 = base.positions[i];
    const glm::vec3& p1 = tapered.positions[i];
    const float u = std::clamp(p0.y / 200.0f, 0.0f, 1.0f);
    EXPECT_NEAR(p1.x, p0.x * (1.0f + (0.2f - 1.0f) * u), 1e-2f);
    EXPECT_NEAR(p1.y, p0.y, 1e-3f);
  }

  // Bend 90 degrees toward +x over 0..200: the band's centreline
  // becomes a quarter circle of radius 200 * 2 / pi; the top of the
  // column ends up pointing along +x, at height R and x = R + offset
  // adjustment. Arc length is preserved for the x = 0 fibre.
  const std::vector<glm::vec3> spine = {
      {0, 0, 0}, {0, 200, 0}, {0, 200, 1}, {0, 0, 1}};
  const Cloud bent = popops::cook(pop::on(spine)
                                      .count(200)
                                      .window(0.5f, 0.5f)
                                      .bend(90, {0, 1, 0}, {1, 0, 0}, 0, 200));
  const Cloud spineBase =
      popops::cook(pop::on(spine).count(200).window(0.5f, 0.5f));
  const float R = 200.0f / ((float)M_PI * 0.5f);
  for (size_t i = 0; i < 200; ++i) {
    const glm::vec3& p0 = spineBase.positions[i];
    const glm::vec3& p1 = bent.positions[i];
    // The spline overshoots its control points a little at both ends;
    // points outside the band ride the end tangents rigidly, so only
    // the band itself is on the arc.
    if (p0.y < 0.0f || p0.y > 200.0f) continue;
    const float theta = p0.y / R;
    EXPECT_NEAR(p1.y, R * std::sin(theta), 1e-2f);
    EXPECT_NEAR(p1.x, R - R * std::cos(theta), 1e-2f);
    // Distance from the arc centre (x = R, y = 0) is R everywhere.
    EXPECT_NEAR(std::hypot(p1.x - R, p1.y), R, 1e-2f);
  }
  // Amount 0 is the identity.
  const Cloud unbent = popops::cook(
      pop::on(spine).count(200).window(0.5f, 0.5f).bend(0, {0, 1, 0}));
  for (size_t i = 0; i < 200; ++i)
    EXPECT_NEAR(glm::length(unbent.positions[i] - spineBase.positions[i]), 0.0f,
                1e-4f);
}

TEST(Pop, MixBlendsCopiesAndFadesByALane) {
  const std::vector<glm::vec3> loop = flatRing(8, 100);
  const pop::Chain chain = pop::on(loop)
                               .count(40)
                               .fill("a", {1, 0, 0, 1})
                               .fill("b", {0, 0, 1, 1})
                               .mix("a", "b", "half", 0.5f)
                               .copy("a", "again")
                               .mixBy("a", "b", "byT", "T");
  const Cloud cooked = popops::cook(chain);
  const std::vector<glm::vec4>* half = cooked.colorIf("half");
  const std::vector<glm::vec4>* again = cooked.colorIf("again");
  const std::vector<glm::vec4>* byT = cooked.colorIf("byT");
  const std::vector<float>* t = cooked.scalarIf("t");
  ASSERT_TRUE(half && again && byT && t);
  for (size_t i = 0; i < 40; ++i) {
    EXPECT_NEAR((*half)[i].r, 0.5f, 1e-5f);
    EXPECT_NEAR((*half)[i].b, 0.5f, 1e-5f);
    EXPECT_NEAR((*again)[i].r, 1.0f, 1e-5f);
    EXPECT_NEAR((*byT)[i].b, (*t)[i], 1e-5f);
    EXPECT_NEAR((*byT)[i].r, 1.0f - (*t)[i], 1e-5f);
  }
}

TEST(Import, HoudiniGeoPolygonsUnweldWithVertexAndPrimitiveClasses) {
  // A quad and a triangle over five points, written the way Houdini
  // saves ASCII .geo: alternating key/value arrays, paged attribute
  // storage for P, a plain tuple list for a point N, a VERTEX uv (which
  // outranks any point uv), a primitive Cd, a point group and a
  // primitive group. Points 0-3 are the quad (y = 0 and y = 100),
  // point 4 sits above and forms a triangle with points 2 and 3.
  const char* geo = R"([
    "fileversion","20.5.278",
    "hasindex",false,
    "pointcount",5,
    "vertexcount",7,
    "primitivecount",2,
    "info",{"software":"Houdini 20.5.278"},
    "topology",["pointref",["indices",[0,1,2,3,3,2,4]]],
    "attributes",[
      "vertexattributes",[
        [
          ["scope","public","type","numeric","name","uv","options",{}],
          ["size",3,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[0]],
           "values",["size",3,"storage","fpreal32","tuples",
             [[0,0,0],[1,0,0],[1,1,0],[0,1,0],[0,1,0],[1,1,0],[0.5,1,0]]]]
        ]
      ],
      "pointattributes",[
        [
          ["scope","public","type","numeric","name","P","options",{"type":{"type":"string","value":"point"}}],
          ["size",3,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[0]],
           "values",["size",3,"storage","fpreal32","packing",[3],"pagesize",1024,
             "constantpageflags",[[false]],
             "rawpagedata",[0,0,0, 100,0,0, 100,100,0, 0,100,0, 50,180,0]]]
        ],
        [
          ["scope","public","type","numeric","name","N","options",{"type":{"type":"string","value":"normal"}}],
          ["size",3,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[0]],
           "values",["size",3,"storage","fpreal32","tuples",
             [[0,0,1],[0,0,1],[0,0,1],[0,0,1],[0,0,1]]]]
        ],
        [
          ["scope","public","type","numeric","name","pscale","options",{}],
          ["size",1,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[1]],
           "values",["size",1,"storage","fpreal32","arrays",[[1,2,3,4,5]]]]
        ]
      ],
      "primitiveattributes",[
        [
          ["scope","public","type","numeric","name","Cd","options",{"type":{"type":"string","value":"color"}}],
          ["size",3,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[1]],
           "values",["size",3,"storage","fpreal32","tuples",[[1,0,0],[0,0,1]]]]
        ]
      ],
      "globalattributes",[
        [
          ["scope","public","type","numeric","name","frame","options",{}],
          ["size",1,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[0]],
           "values",["size",1,"storage","fpreal32","arrays",[[12]]]]
        ]
      ]
    ],
    "primitives",[
      [["type","Polygon"],["vertex",[0,1,2,3],"closed",true]],
      [["type","Polygon"],["vertex",[4,5,6],"closed",true]]
    ],
    "pointgroups",[
      [["name","top"],["selection",["unordered",["boolRLE",[2,false,2,true,1,true]]]]]
    ],
    "primitivegroups",[
      [["name","front"],["selection",["unordered",["i8",[1,0]]]]]
    ]
  ])";
  const std::optional<import::Model> model =
      import::model(geo, std::strlen(geo), "scene.geo");
  ASSERT_TRUE(model);
  ASSERT_EQ(model->parts.size(), 1u);
  const import::Part& part = model->parts.front();
  const Mesh& mesh = part.mesh;
  // Unwelded: 4 + 3 vertices; the quad fans into two triangles.
  EXPECT_EQ(mesh.vertexCount(), 7u);
  EXPECT_EQ(mesh.triangleCount(), 3u);
  EXPECT_EQ(mesh.positions[2].x, 100.0f);
  EXPECT_EQ(mesh.positions[2].y, 100.0f);
  EXPECT_EQ(mesh.positions[6].y, 180.0f);  // vertex 6 -> point 4
  ASSERT_EQ(mesh.normals.size(), 7u);
  EXPECT_FLOAT_EQ(mesh.normals[0].z, 1.0f);
  ASSERT_EQ(mesh.uvs.size(), 7u);
  // Vertex uv, v flipped to the top-left convention.
  EXPECT_FLOAT_EQ(mesh.uvs[2].x, 1.0f);
  EXPECT_FLOAT_EQ(mesh.uvs[2].y, 0.0f);
  EXPECT_FLOAT_EQ(mesh.uvs[6].x, 0.5f);
  // Primitive Cd -> the "Color" prim lane, replicated over the fan.
  const std::vector<glm::vec4>* color = mesh.primIf("Color");
  ASSERT_TRUE(color);
  ASSERT_EQ(color->size(), 3u);
  EXPECT_FLOAT_EQ((*color)[0].r, 1.0f);
  EXPECT_FLOAT_EQ((*color)[1].r, 1.0f);
  EXPECT_FLOAT_EQ((*color)[2].b, 1.0f);
  // Primitive group -> a 0/1 prim lane.
  const std::vector<glm::vec4>* front = mesh.primIf("front");
  ASSERT_TRUE(front);
  EXPECT_FLOAT_EQ((*front)[0].x, 1.0f);
  EXPECT_FLOAT_EQ((*front)[1].x, 1.0f);
  EXPECT_FLOAT_EQ((*front)[2].x, 0.0f);
  // Point attributes ride to the Part through the owning point;
  // point groups are 0/1 scalar lanes.
  const auto pscale = part.scalarLanes.find("pscale");
  ASSERT_NE(pscale, part.scalarLanes.end());
  ASSERT_EQ(pscale->second.size(), 7u);
  EXPECT_FLOAT_EQ(pscale->second[6], 5.0f);
  const auto top = part.scalarLanes.find("top");
  ASSERT_NE(top, part.scalarLanes.end());
  EXPECT_FLOAT_EQ(top->second[0], 0.0f);
  EXPECT_FLOAT_EQ(top->second[2], 1.0f);
  EXPECT_FLOAT_EQ(top->second[6], 1.0f);
  // Sniffed without an extension too.
  EXPECT_TRUE(import::model(geo, std::strlen(geo), ""));
  // ...and it feeds the pop system through asCloud like any import.
  const Cloud cloud = part.asCloud();
  EXPECT_EQ(cloud.size(), 7u);
  EXPECT_TRUE(cloud.scalarIf("top"));
}

TEST(Import, HoudiniGeoPointsBecomeAHonestCloud) {
  // No primitives: a particle-style file. P in a paged layout whose
  // second page is CONSTANT (every point on it shares one tuple), an
  // int id, a float4 orient, a string name (kept out of the lanes), and
  // a Cd point colour that lands on the mesh colour lane.
  const char* geo = R"([
    "fileversion","20.5.278",
    "pointcount",6,
    "vertexcount",0,
    "primitivecount",0,
    "topology",["pointref",["indices",[]]],
    "attributes",[
      "pointattributes",[
        [
          ["scope","public","type","numeric","name","P","options",{}],
          ["size",3,"storage","fpreal32",
           "values",["size",3,"storage","fpreal32","packing",[3],"pagesize",4,
             "constantpageflags",[[false,true]],
             "rawpagedata",[0,0,0, 1,0,0, 2,0,0, 3,0,0,  9,9,9]]]
        ],
        [
          ["scope","public","type","numeric","name","id","options",{}],
          ["size",1,"storage","int32",
           "values",["size",1,"storage","int32","arrays",[[10,11,12,13,14,15]]]]
        ],
        [
          ["scope","public","type","numeric","name","orient","options",{}],
          ["size",4,"storage","fpreal32",
           "values",["size",4,"storage","fpreal32","tuples",
             [[0,0,0,1],[0,0,0,1],[0,0,0,1],[0,0,0,1],[0,0,0,1],[0,0,0,1]]]]
        ],
        [
          ["scope","public","type","numeric","name","Cd","options",{}],
          ["size",3,"storage","fpreal32",
           "values",["size",3,"storage","fpreal32","packing",[1,1,1],"pagesize",8,
             "constantpageflags",[[true],[true],[false]],
             "rawpagedata",[0.5, 0.25, 0,0.2,0.4,0.6,0.8,1.0]]]
        ],
        [
          ["scope","public","type","string","name","name","options",{}],
          ["size",1,"storage","int32","strings",["a","b"],
           "indices",["size",1,"storage","int32","arrays",[[0,1,0,1,0,1]]]]
        ]
      ]
    ],
    "primitives",[]
  ])";
  const std::optional<import::Model> model =
      import::model(geo, std::strlen(geo), "particles.geo");
  ASSERT_TRUE(model);
  const import::Part& part = model->parts.front();
  const Mesh& mesh = part.mesh;
  EXPECT_EQ(mesh.vertexCount(), 6u);
  EXPECT_EQ(mesh.triangleCount(), 0u);
  EXPECT_FLOAT_EQ(mesh.positions[3].x, 3.0f);
  // The constant page: points 4 and 5 both read the one tuple.
  EXPECT_FLOAT_EQ(mesh.positions[4].x, 9.0f);
  EXPECT_FLOAT_EQ(mesh.positions[5].z, 9.0f);
  // Split packing [1,1,1]: R and G constant pages, B a full page.
  ASSERT_EQ(mesh.colors.size(), 6u);
  EXPECT_FLOAT_EQ(mesh.colors[0].r, 0.5f);
  EXPECT_FLOAT_EQ(mesh.colors[5].g, 0.25f);
  EXPECT_FLOAT_EQ(mesh.colors[2].b, 0.4f);
  EXPECT_FLOAT_EQ(mesh.colors[5].b, 1.0f);
  const auto id = part.scalarLanes.find("id");
  ASSERT_NE(id, part.scalarLanes.end());
  EXPECT_FLOAT_EQ(id->second[5], 15.0f);
  const auto orient = part.colorLanes.find("orient");
  ASSERT_NE(orient, part.colorLanes.end());
  EXPECT_FLOAT_EQ(orient->second[0].w, 1.0f);
  EXPECT_EQ(part.scalarLanes.count("name"), 0u);
  const Cloud cloud = part.asCloud();
  EXPECT_EQ(cloud.size(), 6u);
  EXPECT_TRUE(cloud.colorIf("tint"));
  EXPECT_TRUE(cloud.scalarIf("id"));
}

TEST(Pop, PointSetSeedsAChainFromAnExistingCloudLanesAndAll) {
  // A cloud with the conventional lanes and a custom one (a Houdini
  // group, say) enters a chain as-is: positions become P, "size" Scale,
  // "tint" Color, "normal" Dir, and "top" a custom attribute — usable
  // straight away as a mask. Filters then run over it like any chain.
  Cloud given;
  for (int i = 0; i < 40; ++i) {
    given.positions.push_back({(float)i * 10, i % 2 ? 100.0f : 0.0f, 0});
  }
  std::vector<float>& size = given.scalar("size", 1);
  std::vector<glm::vec4>& tint = given.color("tint");
  std::vector<glm::vec3>& normal = given.vector("normal");
  std::vector<float>& top = given.scalar("top");
  for (int i = 0; i < 40; ++i) {
    size[(size_t)i] = 2.0f + (float)(i % 3);
    tint[(size_t)i] = {1, 0, 0, 1};
    normal[(size_t)i] = {0, 1, 0};
    top[(size_t)i] = i % 2 ? 1.0f : 0.0f;
  }
  const pop::Chain chain = pop::on(given)
                               .move({0, 0, 50})
                               .masked("top")
                               .peak(5)
                               .fade({0, 1, 0, 1}, {0, 1, 0, 1});
  const Cloud cooked = popops::cook(chain);
  ASSERT_EQ(cooked.size(), 40u);
  const std::vector<float>* outSize = cooked.scalarIf("size");
  const std::vector<glm::vec3>* dir = cooked.vectorIf("dir");
  const std::vector<glm::vec4>* outTint = cooked.colorIf("tint");
  const std::vector<glm::vec4>* outTop = cooked.colorIf("top");
  ASSERT_TRUE(outSize && dir && outTint && outTop);
  for (size_t i = 0; i < 40; ++i) {
    // The mask came in with the cloud: only odd points moved in z.
    EXPECT_NEAR(cooked.positions[i].z, (i % 2 ? 50.0f : 0.0f), 1e-4f) << i;
    // Peak rides Dir, seeded from "normal": +5 in y for everyone.
    EXPECT_NEAR(cooked.positions[i].y, (i % 2 ? 105.0f : 5.0f), 1e-4f) << i;
    EXPECT_FLOAT_EQ((*outSize)[i], 2.0f + (float)(i % 3));
    EXPECT_FLOAT_EQ((*outTint)[i].g, 1.0f);  // recoloured by the fade
    EXPECT_FLOAT_EQ((*outTop)[i].x, i % 2 ? 1.0f : 0.0f);
    EXPECT_FLOAT_EQ((*dir)[i].y, 1.0f);
  }
  // count() and window() are inert on a point set: the count is the
  // cloud's.
  EXPECT_EQ(popops::cook(pop::on(given).count(5).window(0.5f, 0.5f)).size(),
            40u);
  // The layout the GPU executor uploads is the same function.
  std::map<std::string, std::vector<glm::vec4>, std::less<>> lanes;
  popops::seedAttrs(given, lanes);
  EXPECT_EQ(lanes.count("P"), 1u);
  EXPECT_EQ(lanes.count("Scale"), 1u);
  EXPECT_EQ(lanes.count("top"), 1u);
  EXPECT_EQ(lanes.count("size"), 0u);
  const std::vector<std::string> customs = popops::seedCustomNames(given);
  ASSERT_EQ(customs.size(), 1u);
  EXPECT_EQ(customs[0], "top");
}

TEST(Import, GltfCarriesTheWholeMaterial) {
  // The fetched Khronos Avocado (skipped when the asset is absent):
  // base colour, a normal map and a packed metallicRoughness image,
  // with the occlusion slot naming the same bytes as the pack.
  const std::filesystem::path glb = "assets/models/Avocado.glb";
  std::filesystem::path found;
  for (const std::filesystem::path candidate :
       {glb, std::filesystem::path("build") / glb,
        std::filesystem::path("../build") / glb,
        std::filesystem::path("../../build") / glb})
    if (std::filesystem::exists(candidate)) found = candidate;
  if (found.empty()) GTEST_SKIP() << "Avocado.glb not fetched";
  const std::optional<import::Model> model = import::model(found);
  ASSERT_TRUE(model);
  ASSERT_FALSE(model->parts.empty());
  const import::Part& part = model->parts.front();
  EXPECT_FALSE(part.textureBytes.empty());
  ASSERT_TRUE(part.textures.count("normal"));
  ASSERT_TRUE(part.textures.count("orm"));
  EXPECT_FALSE(part.textures.at("normal").bytes.empty());
  EXPECT_FALSE(part.textures.at("orm").bytes.empty());
  EXPECT_FLOAT_EQ(part.metallic, 1.0f);
  EXPECT_FLOAT_EQ(part.roughness, 1.0f);
  if (part.textures.count("occlusion"))
    EXPECT_EQ(part.textures.at("occlusion").bytes,
              part.textures.at("orm").bytes)
        << "the Avocado packs occlusion into the same image";
}

TEST(Pop, FieldsAreAddressableByName) {
  // The dial door: any operator's numeric field by its own name, vector
  // components dotted, enums and bools as numbers; a name the operator
  // lacks is refused and leaves it untouched.
  pop::Op twist = pop::Deform{};
  EXPECT_TRUE(popops::setField(twist, "amount", 45.0f));
  EXPECT_TRUE(popops::setField(twist, "origin.x", 12.0f));
  EXPECT_TRUE(popops::setField(twist, "kind", (float)pop::Deform::Kind::Bend));
  EXPECT_FALSE(popops::setField(twist, "wibble", 1.0f));
  const auto& d = std::get<pop::Deform>(twist);
  EXPECT_FLOAT_EQ(d.amount, 45.0f);
  EXPECT_FLOAT_EQ(d.origin.x, 12.0f);
  EXPECT_EQ(d.kind, pop::Deform::Kind::Bend);
  EXPECT_FLOAT_EQ(*popops::getField(twist, "amount"), 45.0f);
  EXPECT_FLOAT_EQ(*popops::getField(twist, "kind"), 2.0f);
  EXPECT_FALSE(popops::getField(twist, "mask"));  // a string, not a dial

  pop::Op group = pop::Select{};
  EXPECT_TRUE(popops::setField(group, "center.y", 80.0f));
  EXPECT_TRUE(popops::setField(group, "invert", 1.0f));
  EXPECT_TRUE(
      popops::setField(group, "combine", (float)pop::Select::Combine::Union));
  const auto& g = std::get<pop::Select>(group);
  EXPECT_FLOAT_EQ(g.center.y, 80.0f);
  EXPECT_TRUE(g.invert);
  EXPECT_EQ(g.combine, pop::Select::Combine::Union);

  pop::Op ramp = pop::Ramp{};
  EXPECT_TRUE(popops::setField(ramp, "to.g", 0.25f));  // colour spelling
  EXPECT_FLOAT_EQ(std::get<pop::Ramp>(ramp).to.y, 0.25f);
  EXPECT_FLOAT_EQ(*popops::getField(ramp, "to.y"), 0.25f);

  pop::Op scatter = pop::SplineScatter{};
  EXPECT_TRUE(popops::setField(scatter, "count", 250.7f));  // int truncates
  EXPECT_EQ(std::get<pop::SplineScatter>(scatter).count, 250);
  EXPECT_TRUE(popops::setField(scatter, "seed", 9.0f));
  EXPECT_EQ(std::get<pop::SplineScatter>(scatter).seed, 9u);

  // Operators without dials say no to everything.
  pop::Op promote = pop::Promote{};
  EXPECT_FALSE(popops::setField(promote, "to", 1.0f));
  pop::Op given = pop::PointSet{};
  EXPECT_FALSE(popops::getField(given, "count"));
}

TEST(Import, MaterialSlotsRideThePrimitiveClass) {
  // A .geo with a string shop_materialpath per primitive lands as the
  // "Material" prim lane by string-table index; the fetched Avocado (one
  // material) names slot 0 and merged() keeps the lane.
  const char* geo = R"([
    "fileversion","20.5.278","pointcount",4,"vertexcount",6,"primitivecount",2,
    "topology",["pointref",["indices",[0,1,2,0,2,3]]],
    "attributes",["pointattributes",[
      [["scope","public","type","numeric","name","P","options",{}],
       ["size",3,"storage","fpreal32","values",["size",3,"storage","fpreal32",
        "tuples",[[0,0,0],[1,0,0],[1,1,0],[0,1,0]]]]]],
     "primitiveattributes",[
      [["scope","public","type","string","name","shop_materialpath","options",{}],
       ["size",1,"storage","int32","strings",["/mat/steel","/mat/glass"],
        "indices",["size",1,"storage","int32","arrays",[[1,0]]]]]]],
    "primitives",[[["type","Polygon"],["vertex",[0,1,2],"closed",true]],
                  [["type","Polygon"],["vertex",[3,4,5],"closed",true]]]
  ])";
  const std::optional<import::Model> model =
      import::model(geo, std::strlen(geo), "slots.geo");
  ASSERT_TRUE(model);
  const std::vector<glm::vec4>* lane =
      model->parts.front().mesh.primIf("Material");
  ASSERT_TRUE(lane);
  ASSERT_EQ(lane->size(), 2u);
  EXPECT_FLOAT_EQ((*lane)[0].x, 1.0f);  // "/mat/glass"
  EXPECT_FLOAT_EQ((*lane)[1].x, 0.0f);  // "/mat/steel"

  std::filesystem::path found;
  for (const std::filesystem::path candidate :
       {std::filesystem::path("assets/models/Avocado.glb"),
        std::filesystem::path("build/assets/models/Avocado.glb"),
        std::filesystem::path("../build/assets/models/Avocado.glb"),
        std::filesystem::path("../../build/assets/models/Avocado.glb")})
    if (std::filesystem::exists(candidate)) found = candidate;
  if (found.empty()) return;  // the .geo half already stands
  const std::optional<import::Model> avocado = import::model(found);
  ASSERT_TRUE(avocado);
  EXPECT_EQ(avocado->materialSlotCount(), 1);
  EXPECT_EQ(avocado->parts.front().materialIndex, 0);
  const Mesh merged = avocado->merged();
  const std::vector<glm::vec4>* slots = merged.primIf("Material");
  ASSERT_TRUE(slots);
  EXPECT_EQ(slots->size(), merged.triangleCount());
}
