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

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>

#include <Alembic/Abc/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>

#include <glm/gtc/matrix_transform.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace sigil::shape;

namespace {

SkPath rect(float x, float y, float w, float h) {
  return SkPath::Rect(SkRect::MakeXYWH(x, y, w, h));
}

} // namespace

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
  // Every flattened vertex sits on the circle within tolerance-ish.
  for (const SkPoint &p : contours[0].points) {
    const float r = std::sqrt(p.fX * p.fX + p.fY * p.fY);
    EXPECT_NEAR(r, 100.0f, 0.5f);
  }
  EXPECT_NEAR(contours[0].length(), 2.0f * (float)M_PI * 100.0f, 4.0f);
}

TEST(Geometry, ResampleUniformSpacing) {
  const std::vector<Sampled> sampled = resample(SkPath::Circle(0, 0, 50), 64);
  ASSERT_EQ(sampled.size(), 1u);
  ASSERT_EQ(sampled[0].points.size(), 64u);
  // Consecutive samples should be near-equidistant.
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
  ASSERT_EQ(steps.size(), 5u); // 2 keys + 3 intermediates
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
  blend::Key nearWhite{SkPath::Circle(100, 0, 10),
                       {0.95f, 0.95f, 0.95f, 1}};
  blend::Options options;
  options.spacing = blend::Spacing::SmoothColor;
  const size_t far = blend::make(white, black, options).size();
  const size_t near = blend::make(white, nearWhite, options).size();
  EXPECT_GT(far, 200u); // ~254 steps for full range
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
  const SkColor4f mid = blend::detail::lerpOklab(
      {0, 0, 0, 1}, {1, 1, 1, 1}, 0.5f);
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
  spine.lineTo({0, 400}); // vertical spine
  blend::Key from{SkPath::Circle(0, 0, 10), {1, 0, 0, 1}};
  blend::Key to{SkPath::Circle(0, 0, 10), {0, 1, 0, 1}}; // same spot
  blend::Options options;
  options.steps = 3;
  options.spine = spine.detach();
  const std::vector<blend::Step> steps = blend::make(from, to, options);
  ASSERT_EQ(steps.size(), 5u);
  // Steps should march down the vertical spine.
  float lastY = -1;
  for (const blend::Step &step : steps) {
    const float y = step.path.computeTightBounds().centerY();
    EXPECT_GT(y, lastY);
    lastY = y;
  }
  EXPECT_NEAR(steps.back().path.computeTightBounds().centerY(), 400.0f,
              2.0f);
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
  for (const glm::vec3 &n : m.normals)
    EXPECT_NEAR(glm::length(n), 1.0f, 1e-4f);
}

TEST(Mesh, ExtrudeAnnulusKeepsHole) {
  SkPathBuilder ring;
  ring.addCircle(0, 0, 80);
  ring.addCircle(0, 0, 40, SkPathDirection::kCCW);
  Mesh m = mesh::extrude(ring.detach(), {.depth = 10});
  ASSERT_GT(m.triangleCount(), 0u);
  // Cap area ~= pi*(80^2-40^2); sum front-cap triangle areas (z>0 caps
  // have all vertices at z=+5).
  double area = 0;
  for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
    const glm::vec3 &a = m.positions[m.indices[t]];
    const glm::vec3 &b = m.positions[m.indices[t + 1]];
    const glm::vec3 &c = m.positions[m.indices[t + 2]];
    if (a.z > 4.9f && b.z > 4.9f && c.z > 4.9f) {
      const glm::vec3 ab = b - a, ac = c - a;
      area += 0.5 * std::abs((double)ab.x * ac.y - (double)ab.y * ac.x);
    }
  }
  const double expected = M_PI * (80.0 * 80.0 - 40.0 * 40.0);
  EXPECT_NEAR(area / expected, 1.0, 0.03);
}

TEST(Mesh, GridUvAndIndicesCoherent) {
  Mesh m = mesh::grid(4, 3, [](float u, float v) -> glm::vec3 {
    return {u * 10, v * 10, 0};
  });
  EXPECT_EQ(m.vertexCount(), 12u);
  EXPECT_EQ(m.triangleCount(), 12u); // 3x2 cells * 2
  // Image-convention UVs: v param 0 (sheet bottom) samples image v=1.
  EXPECT_EQ(m.uvs.front().x, 0.0f);
  EXPECT_EQ(m.uvs.front().y, 1.0f);
  EXPECT_EQ(m.uvs.back().y, 0.0f);
  for (uint32_t i : m.indices)
    EXPECT_LT(i, m.vertexCount());
}

TEST(Mesh, TorusNormalsPointOutward) {
  Mesh m = mesh::torus(100, 30, 32, 16);
  // At u=0,v=0: phi=0 -> outer equator, normal ~ +x.
  const glm::vec3 n0 = m.normals.front();
  EXPECT_GT(std::abs(n0.x), 0.7f);
  for (const glm::vec3 &n : m.normals)
    EXPECT_NEAR(glm::length(n), 1.0f, 1e-3f);
}

TEST(Mesh, TransformMovesBoundsAndKeepsUnitNormals) {
  Mesh m = mesh::quad(10, 10);
  m.transform(glm::translate(glm::mat4(1.0f), {5, 0, 0}));
  glm::vec3 lo, hi;
  m.bounds(&lo, &hi);
  EXPECT_NEAR((lo.x + hi.x) * 0.5f, 5.0f, 1e-4f);
  for (const glm::vec3 &n : m.normals)
    EXPECT_NEAR(glm::length(n), 1.0f, 1e-4f);
}

TEST(Mesh, TransformRotatesNormalsForward) {
  // Rotation must carry normals WITH it: the inverse transpose applies
  // in row form. Dotting its columns instead applies the plain inverse
  // — normals rotate BACKWARDS ({0,-1,0} here).
  Mesh m = mesh::quad(10, 10);
  m.normals.assign(m.vertexCount(), {1, 0, 0});
  m.transform(
      glm::rotate(glm::mat4(1.0f), (float)M_PI * 0.5f, {0, 0, 1}));
  for (const glm::vec3 &n : m.normals) {
    EXPECT_NEAR(n.x, 0.0f, 1e-4f);
    EXPECT_NEAR(n.y, 1.0f, 1e-4f);
    EXPECT_NEAR(n.z, 0.0f, 1e-4f);
  }
  // Non-uniform scale keeps inverse-transpose semantics: a normal
  // along the scaled axis renormalizes back to itself.
  Mesh s = mesh::quad(10, 10);
  s.normals.assign(s.vertexCount(), {1, 0, 0});
  s.transform(glm::scale(glm::mat4(1.0f), {2, 1, 1}));
  for (const glm::vec3 &n : s.normals) {
    EXPECT_NEAR(n.x, 1.0f, 1e-4f);
    EXPECT_NEAR(n.y, 0.0f, 1e-4f);
    EXPECT_NEAR(n.z, 0.0f, 1e-4f);
  }
}

TEST(Mesh, AppendKeepsNormalAndUvLanesSizedToPositions) {
  // The everyday break: a stamp authored with positions + indices only
  // instances into a normal-less, uv-less Mesh (points::instance copies
  // only the lanes the stamp has), and merging THAT into a lit mesh used
  // to leave normals.size() != positions.size(). space::drawMesh reads
  // exactly that comparison as its hasNormals bit, so lighting died for
  // the WHOLE merge — torus included, not just the flat flakes.
  Mesh stamp; // a bare triangle: no normals, no uvs
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
  // The remaining hole in the same family: a lane authored SHORTER than
  // its own mesh's element count. A bare insert leaves the MERGED lane
  // undersized, and every consumer reads "lane sized to positions" (or
  // to triangleCount, for prims) as the presence bit for the whole
  // mesh — so one short lane on one side turns tinting, lighting or
  // texturing off for the merge. Short lanes come from hand-built and
  // imported meshes (a PLY whose extra property list ran out early).
  Mesh a;
  a.positions = {{-50, -50, 0}, {50, -50, 0}, {50, 50, 0}, {-50, 50, 0}};
  a.indices = {0, 1, 2, 0, 2, 3};
  a.colors.assign(4, glm::vec4{1, 0, 0, 1});
  a.normals.assign(4, glm::vec3{0, 0, 1});
  a.uvs.assign(4, glm::vec2{0.25f, 0.5f});

  Mesh b; // 4 vertices / 2 triangles, but every lane one entry short
  b.positions = {{0, 0, 10}, {10, 0, 10}, {10, 10, 10}, {0, 10, 10}};
  b.indices = {0, 1, 2, 0, 2, 3};
  b.colors = {{0, 1, 0, 1}, {0, 1, 0, 1}, {0, 1, 0, 1}};
  b.normals = {{1, 0, 0}, {1, 0, 0}, {1, 0, 0}};
  b.uvs = {{1, 1}, {1, 1}, {1, 1}};
  b.prims["heat"] = {{5, 0, 0, 0}}; // 1 entry for 2 triangles

  a.append(b);
  ASSERT_EQ(a.positions.size(), 8u);
  EXPECT_EQ(a.colors.size(), a.positions.size()) << "short colors lane";
  EXPECT_EQ(a.normals.size(), a.positions.size());
  EXPECT_EQ(a.uvs.size(), a.positions.size());
  ASSERT_EQ(a.colors.size(), 8u);
  // Ours survived, theirs landed at the right run, and the hole pads
  // WHITE — the untinted identity. Black would darken the merged half.
  EXPECT_EQ(a.colors[0], (glm::vec4{1, 0, 0, 1}));
  EXPECT_EQ(a.colors[4], (glm::vec4{0, 1, 0, 1}));
  EXPECT_EQ(a.colors[7], (glm::vec4{1, 1, 1, 1})) << "pad stays white";
  EXPECT_NEAR(a.normals[7].z, 1.0f, 1e-6f); // +Z, never a zero normal
  EXPECT_NEAR(a.uvs[7].x, 0.0f, 1e-6f);
  EXPECT_NEAR(a.uvs[7].y, 0.0f, 1e-6f);
  // Prim lanes count per TRIANGLE; the prims block's trailing resize is
  // what covers the same hole there, padding by lane name.
  ASSERT_EQ(a.triangleCount(), 4u);
  const std::vector<glm::vec4> *heat = a.primIf("heat");
  ASSERT_TRUE(heat);
  ASSERT_EQ(heat->size(), 4u) << "short incoming prim lane";
  EXPECT_FLOAT_EQ((*heat)[2].x, 5.0f); // theirs
  EXPECT_FLOAT_EQ((*heat)[3].x, 0.0f); // padded by name

  // The short lane on OUR side is the mirror case: pad-to-old-count
  // repairs it before theirs is taken.
  Mesh shortSide;
  shortSide.positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}};
  shortSide.indices = {0, 1, 2};
  shortSide.colors = {{0, 0, 1, 1}}; // 1 entry for 3 vertices
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

TEST(Space, CameraProjectsCenterToViewportCenter) {
  space::Camera camera;
  camera.eye = {0, 0, 100};
  camera.target = {0, 0, 0};
  const glm::mat4 vp = camera.viewProjection({800, 600});
  const glm::vec4 out = vp * glm::vec4{0, 0, 0, 1};
  EXPECT_NEAR(out.x / out.w, 400.0f, 1e-2f);
  EXPECT_NEAR(out.y / out.w, 300.0f, 1e-2f);
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
  // Center pixel should be lit (reddish, not black).
  const SkColor c = bm.getColor(100, 75);
  EXPECT_GT(SkColorGetR(c), 40u);
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
    sk_sp<SkSurface> s =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(80, 80));
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
  materials::drawChrome(*surface->getCanvas(),
                        SkPath::Circle(60, 60, 40), env, 8);
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
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
  // Interior: flat normal -> b channel ~255, rg ~128.
  const SkColor center = bm.getColor(50, 50);
  EXPECT_GT(SkColorGetB(center), 240u);
  EXPECT_NEAR(SkColorGetR(center), 128, 6);
  // Left rim: normal tilts -x -> r well below 128.
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
  // Cache: same bucket returns the same image.
  EXPECT_EQ(env.image(0.6f).get(), rough.get());
}

// --- Ops ------------------------------------------------------------------

TEST(Ops, PathfinderBooleans) {
  const SkPath a = rect(0, 0, 100, 100);
  const SkPath b = rect(50, 0, 100, 100);
  EXPECT_NEAR(ops::unite(a, b).computeTightBounds().width(), 150, 1e-3);
  EXPECT_NEAR(ops::subtract(a, b).computeTightBounds().width(), 50, 1e-3);
  EXPECT_NEAR(ops::intersect(a, b).computeTightBounds().width(), 50, 1e-3);
  const SkPath xr = ops::exclude(a, b);
  EXPECT_NEAR(xr.computeTightBounds().width(), 150, 1e-3);
  EXPECT_FALSE(xr.contains(75, 50)); // the overlap is punched out
  EXPECT_TRUE(ops::unite({a, b, rect(140, 0, 100, 100)})
                  .contains(200, 50));
}

TEST(Ops, OffsetGrowsAndShrinks) {
  const SkPath circle = SkPath::Circle(0, 0, 50);
  const SkRect grown = ops::offset(circle, 10).computeTightBounds();
  EXPECT_NEAR(grown.width(), 120, 1.5f);
  const SkRect shrunk = ops::offset(circle, -15).computeTightBounds();
  EXPECT_NEAR(shrunk.width(), 70, 1.5f);
}

TEST(Ops, DistortsKeepBoundsSane) {
  const SkPath base = SkPath::Circle(100, 100, 60);
  const SkRect roughened =
      ops::Roughen{6, 8, 42}.apply(base).computeTightBounds();
  EXPECT_LT(std::abs(roughened.centerX() - 100), 4);
  EXPECT_LT(roughened.width(), 120 + 2 * 6 + 2);
  const SkPath twirled = ops::Twirl{90}.apply(base);
  EXPECT_LT(
      std::abs(twirled.computeTightBounds().centerX() - 100), 4);
  const SkRect bloated =
      ops::PuckerBloat{0.8f}.apply(base).computeTightBounds();
  EXPECT_GT(bloated.width(), 118); // pushed toward the silhouette
  const SkPath chained = ops::chain(
      {ops::offsetBy(6), ops::Zigzag{4, 20}})(base);
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
  EXPECT_NEAR(start.x, 0, 1e-3);   // Catmull-Rom passes through
  EXPECT_NEAR(end.x, 150, 1e-3);
}

TEST(Curves, ArcLengthSamplingIsEven) {
  Spline3 spline; // deliberately uneven knots
  spline.type = Spline3::Type::Linear; // keep the curve straight so
                                       // spacing is the only variable
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
    knot.points.push_back({std::cos(a) * 100,
                           std::sin(a * 2) * 40,
                           std::sin(a) * 100});
  }
  const std::vector<Frame3> rail = curves::frames(knot, 64);
  ASSERT_EQ(rail.size(), 64u);
  for (size_t i = 0; i < rail.size(); ++i) {
    const Frame3 &f = rail[i];
    EXPECT_NEAR(glm::length(f.tangent), 1, 1e-3);
    EXPECT_NEAR(glm::length(f.normal), 1, 1e-3);
    EXPECT_NEAR(glm::dot(f.tangent, f.normal), 0, 1e-3);
    if (i > 0) // parallel transport: no sudden flips
      EXPECT_GT(glm::dot(f.normal, rail[i - 1].normal), 0.5f);
  }
}

TEST(Curves, TubeAndRibbonAreWellFormed) {
  Spline3 arc;
  arc.points = {{0, 0, 0}, {60, 60, 0}, {120, 0, 0}};
  const Mesh t = curves::tube(arc, {.radius = 8, .segments = 24, .sides = 8});
  EXPECT_GT(t.triangleCount(), 0u);
  EXPECT_EQ(t.normals.size(), t.vertexCount());
  for (const glm::vec3 &n : t.normals)
    EXPECT_NEAR(glm::length(n), 1, 1e-3);
  const Mesh r = curves::ribbon(arc, {.width = 20, .segments = 24});
  EXPECT_EQ(r.vertexCount(), 48u);
  EXPECT_EQ(r.triangleCount(), 46u);
}

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
  for (const glm::vec3 &p : circle.positions) {
    EXPECT_NEAR(glm::length(p), 50, 1e-2);  // on the radius
    EXPECT_NEAR(p.y, 0, 1e-3);          // in the plane ⊥ default axis
  }

  Cloud box = points::scatterBox({0, 0, 0}, {10, 10, 10}, 100, 3);
  EXPECT_EQ(box.size(), 100u);
  for (const glm::vec3 &p : box.positions) {
    EXPECT_GE(p.x, 0);
    EXPECT_LE(p.x, 10);
  }
}

TEST(Points, OnMeshLandsOnSurface) {
  const Mesh quad = mesh::quad(100, 100); // z = 0 plane
  Cloud cloud = points::onMesh(quad, 64, 5);
  ASSERT_EQ(cloud.size(), 64u);
  for (const glm::vec3 &p : cloud.positions) {
    EXPECT_NEAR(p.z, 0, 1e-4);
    EXPECT_LE(std::abs(p.x), 50.01f);
  }
  ASSERT_TRUE(cloud.vectorIf("normal"));
  EXPECT_NEAR((*cloud.vectorIf("normal"))[0].z, 1, 1e-3);
}

TEST(Points, InstanceStampsWithLanes) {
  Cloud cloud = points::ring({0, 0, 0}, 80, 6);
  std::vector<float> &size = cloud.scalar("size", 1);
  size[0] = 2;
  std::vector<glm::vec4> &tint = cloud.color("tint");
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
  // The first stamp is scaled 2x: its extent from its ring point is 10.
  glm::vec3 lo, hi;
  Mesh first;
  first.positions.assign(merged.positions.begin(),
                         merged.positions.begin() + 4);
  first.bounds(&lo, &hi);
  EXPECT_GT(glm::length(hi - lo), 14.0f); // 2x-scaled quad diagonal-ish
}

TEST(Points, AppendPadsLanesWithConventionalDefaults) {
  // A lane missing on one side pads with the lane NAME's convention:
  // "size" pads 1 (not invisible instances) and "Tex" the identity
  // uv window (not white).
  Cloud a;
  a.positions = {{0, 0, 0}, {1, 0, 0}};
  Cloud b;
  b.positions = {{2, 0, 0}, {3, 0, 0}};
  b.scalar("size", 2);
  b.color("Tex", {0.5f, 0.5f, 0.5f, 0.5f});
  a.append(b);
  ASSERT_EQ(a.size(), 4u);
  const std::vector<float> *size = a.scalarIf("size");
  ASSERT_TRUE(size);
  ASSERT_EQ(size->size(), 4u);
  EXPECT_FLOAT_EQ((*size)[0], 1.0f); // a's side: scale 1, visible
  EXPECT_FLOAT_EQ((*size)[1], 1.0f);
  EXPECT_FLOAT_EQ((*size)[2], 2.0f); // b's actual values
  EXPECT_FLOAT_EQ((*size)[3], 2.0f);
  const std::vector<glm::vec4> *tex = a.colorIf("Tex");
  ASSERT_TRUE(tex);
  ASSERT_EQ(tex->size(), 4u);
  for (size_t i = 0; i < 2; ++i) { // a's side: identity uv window
    EXPECT_FLOAT_EQ((*tex)[i].x, 0.0f);
    EXPECT_FLOAT_EQ((*tex)[i].y, 0.0f);
    EXPECT_FLOAT_EQ((*tex)[i].z, 1.0f);
    EXPECT_FLOAT_EQ((*tex)[i].w, 1.0f);
  }
  EXPECT_FLOAT_EQ((*tex)[2].x, 0.5f); // b's actual window
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
  points::drawBillboards(*surface->getCanvas(), cloud, camera,
                         {200, 150}, style);
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
  int lit = 0;
  for (int y = 0; y < 150; ++y)
    for (int x = 0; x < 200; ++x)
      if (SkColorGetG(bm.getColor(x, y)) > 30)
        ++lit;
  EXPECT_GT(lit, 200);
}

// --- Easel (the artist surface) -------------------------------------------

TEST(Easel, ShapeRecipeCooksNonDestructively) {
  const easel::Shape recipe =
      easel::shape(easel::dot(50)).bloat(0.4f).offset(10);
  const SkPath once = recipe.path();
  const SkPath twice = recipe.path(); // cooking twice = same answer
  EXPECT_EQ(once.countPoints(), twice.countPoints());
  EXPECT_GT(once.computeTightBounds().width(), 115); // grew by ~offset
  // A tweaked COPY leaves the original recipe untouched.
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
  EXPECT_EQ(steps.size(), 9u); // 2 keys + 7
  EXPECT_NEAR(steps.back().path.computeTightBounds().centerX(), 400, 2);
}

TEST(Easel, WireAndParticlesCook) {
  const easel::Wire arc =
      easel::wire({{-100, 0, 0}, {0, 80, 0}, {100, 0, 0}});
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
  const auto *begin = reinterpret_cast<const std::byte *>(text.data());
  return {begin, begin + text.size()};
}

std::string base64(const std::vector<std::byte> &bytes) {
  static const char *alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (size_t i = 0; i < bytes.size(); i += 3) {
    uint32_t chunk = (uint32_t)bytes[i] << 16;
    if (i + 1 < bytes.size())
      chunk |= (uint32_t)bytes[i + 1] << 8;
    if (i + 2 < bytes.size())
      chunk |= (uint32_t)bytes[i + 2];
    out.push_back(alphabet[(chunk >> 18) & 63]);
    out.push_back(alphabet[(chunk >> 12) & 63]);
    out.push_back(i + 1 < bytes.size() ? alphabet[(chunk >> 6) & 63] : '=');
    out.push_back(i + 2 < bytes.size() ? alphabet[chunk & 63] : '=');
  }
  return out;
}

template <typename T>
void appendRaw(std::vector<std::byte> &out, const T &value) {
  const auto *begin = reinterpret_cast<const std::byte *>(&value);
  out.insert(out.end(), begin, begin + sizeof(T));
}

/** One triangle at (0,0,0) (1,0,0) (0,1,0), uint16 indices 0 1 2. */
std::vector<std::byte> triangleBufferBytes() {
  std::vector<std::byte> bin;
  const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  for (float f : positions)
    appendRaw(bin, f);
  for (uint16_t i : {uint16_t(0), uint16_t(1), uint16_t(2)})
    appendRaw(bin, i);
  return bin;
}

/** The minimal scene: one node (translated +10 x) holding one red
 *  triangle. @p bufferUri empty = GLB (no uri member). */
std::string triangleGltfJson(const std::string &bufferUri) {
  std::string buffer = "{\"byteLength\": 42";
  if (!bufferUri.empty())
    buffer += ", \"uri\": \"" + bufferUri + "\"";
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
  while (json.size() % 4)
    json.push_back(' ');
  std::vector<std::byte> bin = triangleBufferBytes();
  while (bin.size() % 4)
    bin.push_back(std::byte{0});
  std::vector<std::byte> out;
  appendRaw(out, (uint32_t)0x46546C67); // "glTF"
  appendRaw(out, (uint32_t)2);
  appendRaw(out, (uint32_t)(12 + 8 + json.size() + 8 + bin.size()));
  appendRaw(out, (uint32_t)json.size());
  appendRaw(out, (uint32_t)0x4E4F534A); // "JSON"
  for (char c : json)
    out.push_back((std::byte)c);
  appendRaw(out, (uint32_t)bin.size());
  appendRaw(out, (uint32_t)0x004E4942); // "BIN"
  out.insert(out.end(), bin.begin(), bin.end());
  return out;
}

constexpr const char *kCubeObj = R"(mtllib cube.mtl
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

constexpr const char *kCubeMtl = R"(newmtl scarlet
Kd 1 0 0
)";

} // namespace

TEST(Import, ObjCubeWithMaterialThroughResolver) {
  std::vector<std::string> asked;
  const import::Resolver resolve =
      [&](std::string_view uri) -> std::optional<std::vector<std::byte>> {
    asked.emplace_back(uri);
    if (uri == "cube.mtl")
      return toBytes(kCubeMtl);
    return std::nullopt;
  };
  const std::string obj = kCubeObj;
  auto model = import::model(obj.data(), obj.size(), "cube.obj", resolve);
  ASSERT_TRUE(model.has_value());
  ASSERT_EQ(model->parts.size(), 1u);
  const Part &part = model->parts.front();
  EXPECT_EQ(part.name, "Cube");
  EXPECT_EQ(part.mesh.vertexCount(), 8u);   // deduplicated corners
  EXPECT_EQ(part.mesh.triangleCount(), 12u); // quads triangulated
  EXPECT_FLOAT_EQ(part.baseColor.r, 1);
  EXPECT_FLOAT_EQ(part.baseColor.g, 0);
  ASSERT_EQ(asked.size(), 1u);
  EXPECT_EQ(asked.front(), "cube.mtl");
  // No normals in the file: computed, unit length.
  ASSERT_EQ(part.mesh.normals.size(), 8u);
  EXPECT_NEAR(glm::length(part.mesh.normals.front()), 1, 1e-4);
  EXPECT_EQ(part.mesh.uvs.size(), 8u); // lane sized even without vt
}

TEST(Import, GltfEmbeddedBase64Buffer) {
  const std::string json = triangleGltfJson(
      "data:application/octet-stream;base64," +
      base64(triangleBufferBytes()));
  auto model = import::model(json.data(), json.size(), "tri.gltf");
  ASSERT_TRUE(model.has_value());
  ASSERT_EQ(model->parts.size(), 1u);
  const Part &part = model->parts.front();
  EXPECT_EQ(part.name, "tri");
  EXPECT_EQ(part.mesh.vertexCount(), 3u);
  EXPECT_EQ(part.mesh.triangleCount(), 1u);
  EXPECT_FLOAT_EQ(part.baseColor.r, 1);
  EXPECT_FLOAT_EQ(part.baseColor.b, 0);
  // The node's +10 x translation is baked into model space.
  glm::vec3 lo, hi;
  model->bounds(&lo, &hi);
  EXPECT_FLOAT_EQ(lo.x, 10);
  EXPECT_FLOAT_EQ(hi.x, 11);
}

TEST(Import, GltfExternalBufferThroughResolver) {
  const std::string json = triangleGltfJson("tri.bin");
  const import::Resolver resolve =
      [](std::string_view uri) -> std::optional<std::vector<std::byte>> {
    if (uri == "tri.bin")
      return triangleBufferBytes();
    return std::nullopt;
  };
  auto model = import::model(json.data(), json.size(), "tri.gltf",
                             resolve);
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->triangleCount(), 1u);
  // Without the resolver the external buffer is unreachable.
  EXPECT_FALSE(
      import::model(json.data(), json.size(), "tri.gltf").has_value());
}

TEST(Import, GlbBinaryContainerAndSniffing) {
  const std::vector<std::byte> glb = glbBytes();
  auto model = import::model(glb.data(), glb.size(), "tri.glb");
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->vertexCount(), 3u);
  EXPECT_EQ(model->triangleCount(), 1u);
  // No useful extension: the GLB magic identifies it anyway.
  auto sniffed = import::model(glb.data(), glb.size(), "download");
  ASSERT_TRUE(sniffed.has_value());
  EXPECT_EQ(sniffed->triangleCount(), 1u);
}

TEST(Import, StlBinaryAndAscii) {
  // Binary: two triangles, zero normals force recomputation.
  std::vector<std::byte> stl(80, std::byte{0});
  appendRaw(stl, (uint32_t)2);
  const float tri[2][12] = {
      {0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0},
      {0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0},
  };
  for (const float *f : {tri[0], tri[1]}) {
    for (int i = 0; i < 12; ++i)
      appendRaw(stl, f[i]);
    appendRaw(stl, (uint16_t)0);
  }
  auto model = import::model(stl.data(), stl.size(), "part.stl");
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->triangleCount(), 2u);
  EXPECT_EQ(model->vertexCount(), 6u); // flat shaded, no dedup
  EXPECT_NEAR(glm::length(model->parts.front().mesh.normals.front()), 1,
              1e-4);

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
  const std::vector<float> *energy = merged.scalarIf("energy");
  ASSERT_TRUE(energy);
  ASSERT_EQ(energy->size(), 8u);
  EXPECT_FLOAT_EQ((*energy)[0], 1.0f);
  EXPECT_FLOAT_EQ((*energy)[3], 4.0f);
  EXPECT_FLOAT_EQ((*energy)[4], 0.0f); // b's side pads scalar 0
  EXPECT_FLOAT_EQ((*energy)[7], 0.0f);
  const std::vector<glm::vec4> *heat = merged.colorIf("heat");
  ASSERT_TRUE(heat);
  ASSERT_EQ(heat->size(), 8u);
  EXPECT_FLOAT_EQ((*heat)[0].r, 1.0f); // a's side pads white
  EXPECT_FLOAT_EQ((*heat)[0].g, 1.0f);
  EXPECT_FLOAT_EQ((*heat)[4].r, 1.0f); // b's red from offset 4
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
  EXPECT_NEAR(hi.x - lo.x, 100, 1e-3); // largest extent = target
  EXPECT_NEAR(hi.y - lo.y, 50, 1e-3);  // aspect kept
  EXPECT_NEAR(lo.x + hi.x, 0, 1e-3);   // centered
  EXPECT_NEAR(lo.y + hi.y, 0, 1e-3);
}

// --- Pop ------------------------------------------------------------------

TEST(Import, GltfCustomAttributesBecomeLanes) {
  // Blender/Houdini's _NAME accessors survive import as named lanes
  // and pour into a Cloud beside the conventional ones.
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
  const import::Part &part = model->parts.front();
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
  // Width routing beyond scalars: VEC2 customs ride the color lane
  // zero-padded in z/w, VEC4 land verbatim — and both pour through
  // asCloud() like any lane.
  std::vector<std::byte> bin;
  const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  for (float f : positions)
    appendRaw(bin, f);
  const float uv2[6] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
  for (float f : uv2)
    appendRaw(bin, f);
  const float wgt[12] = {1, 0, 0, 0.5f, 0, 1, 0, 0.25f,
                         0, 0, 1, 0.125f};
  for (float f : wgt)
    appendRaw(bin, f);
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
  const Part &part = model->parts.front();
  const auto uv = part.colorLanes.find("UV2");
  ASSERT_NE(uv, part.colorLanes.end());
  ASSERT_EQ(uv->second.size(), 3u);
  EXPECT_FLOAT_EQ(uv->second[1].x, 0.3f);
  EXPECT_FLOAT_EQ(uv->second[1].y, 0.4f);
  EXPECT_FLOAT_EQ(uv->second[1].z, 0.0f); // zero-padded z/w
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
  // The attribute carrier: conventional names build the mesh, every
  // other property becomes a lane raw (ids stay ids; only colors
  // normalize), and a faceless file is an honest point cloud whose
  // lanes drive instancing directly.
  const char *ascii =
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
  const import::Part &part = model->parts.front();
  ASSERT_EQ(part.mesh.vertexCount(), 4u);
  EXPECT_TRUE(part.mesh.indices.empty()); // a point cloud, honestly
  EXPECT_FLOAT_EQ(part.mesh.positions[1].x, 10);
  ASSERT_EQ(part.mesh.colors.size(), 4u);
  EXPECT_NEAR(part.mesh.colors[0].r, 1, 1e-2f); // uchar normalized
  EXPECT_NEAR(part.mesh.colors[2].b, 1, 1e-2f);
  const auto intensity = part.scalarLanes.find("intensity");
  ASSERT_NE(intensity, part.scalarLanes.end());
  EXPECT_FLOAT_EQ(intensity->second[3], 3.5f);

  // The lane drives the library: intensity scales instanced stamps.
  const Cloud cloud = part.asCloud();
  points::InstanceOptions options;
  options.scaleLane = "intensity";
  const Mesh stamped =
      points::instance(cloud, mesh::quad(2, 2), options);
  EXPECT_EQ(stamped.vertexCount(), 4u * 4u);
  glm::vec3 lo, hi;
  stamped.bounds(&lo, &hi);
  EXPECT_GT(hi.x, 12.0f); // the 3.5x stamp reaches past its point

  // Binary little-endian speaks the same rows, faces included.
  std::vector<std::byte> bin;
  const auto push = [&](const void *p, size_t n) {
    const auto *b = static_cast<const std::byte *>(p);
    bin.insert(bin.end(), b, b + n);
  };
  const char *header =
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
  const float verts[] = {0, 0, 0, 7,  4, 0, 0, 8,  0, 4, 0, 9};
  push(verts, sizeof(verts));
  const uint8_t faceCount = 3;
  const int32_t face[] = {0, 1, 2};
  push(&faceCount, 1);
  push(face, sizeof(face));
  auto binModel = import::model(bin.data(), bin.size(), "tri.ply");
  ASSERT_TRUE(binModel.has_value());
  const import::Part &tri = binModel->parts.front();
  EXPECT_EQ(tri.mesh.triangleCount(), 1u);
  EXPECT_FLOAT_EQ(tri.scalarLanes.at("intensity")[2], 9.0f);
  ASSERT_EQ(tri.mesh.normals.size(), 3u);
  EXPECT_NEAR(tri.mesh.normals.front().z, 1.0f, 1e-4f);
}

TEST(Import, PlyRejectsHostileCountsAndIndices) {
  // (a) A face naming a vertex past the count is dropped whole — the
  // vertices still import, and computeNormals never indexes OOB.
  const char *badFace =
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
  const char *hugeCount =
      "ply\nformat ascii 1.0\n"
      "element vertex 4000000000\n"
      "property float x\nproperty float y\nproperty float z\n"
      "end_header\n"
      "0 0 0\n";
  EXPECT_FALSE(
      import::model(hugeCount, std::strlen(hugeCount), "huge.ply")
          .has_value());

  // (c) A binary list count promising more bytes than remain fails
  // the row read instead of walking off the buffer.
  std::vector<std::byte> truncated;
  const char *binHeader =
      "ply\nformat binary_little_endian 1.0\n"
      "element vertex 1\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 1\n"
      "property list uchar int vertex_indices\n"
      "end_header\n";
  const auto pushBytes = [&](const void *p, size_t n) {
    const auto *b = static_cast<const std::byte *>(p);
    truncated.insert(truncated.end(), b, b + n);
  };
  pushBytes(binHeader, std::strlen(binHeader));
  const float vertex[3] = {0, 0, 0};
  pushBytes(vertex, sizeof(vertex));
  const uint8_t promised = 200; // 800 bytes of indices; none follow
  pushBytes(&promised, 1);
  EXPECT_FALSE(import::model(truncated.data(), truncated.size(),
                             "trunc.ply")
                   .has_value());
}

TEST(Import, LoneTStaysAScalarLane) {
  // "t" is a texture coordinate only when paired with "s". Alone —
  // the scalar every readPoints/cook/asCloud cloud carries — it must
  // stay a lane instead of vanishing into uv.y.
  const char *ascii =
      "ply\nformat ascii 1.0\n"
      "element vertex 3\n"
      "property float x\nproperty float y\nproperty float z\n"
      "property float t\n"
      "end_header\n"
      "0 0 0 0.25\n1 0 0 0.5\n0 1 0 0.75\n";
  auto model = import::model(ascii, std::strlen(ascii), "lone_t.ply");
  ASSERT_TRUE(model.has_value());
  const Part &part = model->parts.front();
  const auto t = part.scalarLanes.find("t");
  ASSERT_NE(t, part.scalarLanes.end());
  ASSERT_EQ(t->second.size(), 3u);
  EXPECT_FLOAT_EQ(t->second[1], 0.5f);
  // uvs sized by finishPart, untouched by the lone "t".
  for (const glm::vec2 &uv : part.mesh.uvs)
    EXPECT_FLOAT_EQ(uv.y, 0.0f);
  const Cloud cloud = part.asCloud();
  ASSERT_TRUE(cloud.scalarIf("t"));
  EXPECT_FLOAT_EQ((*cloud.scalarIf("t"))[2], 0.75f);

  // The readPoints posture: a cloud whose ONLY extra lane is scalar
  // "t" round-trips through save::ply with its values intact.
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
  // The fold-back leg's edges: an incomplete _x/_y pair fabricates no
  // vector and keeps its raw scalars; a complete _r/_g/_b without _a
  // folds with alpha 1.
  const char *ascii =
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
  const Part &part = model->parts.front();
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
  // The PRIMITIVE class's closed loop: prim lanes write as PLY face
  // properties and read back into Mesh::prims under the SAME names,
  // through both spellings. A custom name rides along with the
  // conventional "Color" — nothing here is special-cased by name.
  Mesh quad = mesh::quad(10, 6); // 4 vertices, 2 triangles
  ASSERT_EQ(quad.triangleCount(), 2u);
  quad.prim("Color") = {{1, 0, 0, 1}, {0, 0.5f, 1, 0.25f}};
  quad.prim("Charge") = {{0.5f, -2, 7, 1.0f / 3.0f},
                         {1e-5f, 3, 0, 1}};

  for (const bool binary : {false, true}) {
    const std::string bytes = save::ply(quad, {.binary = binary});
    ASSERT_FALSE(bytes.empty());
    auto model = import::model(bytes.data(), bytes.size(), "prim.ply");
    ASSERT_TRUE(model.has_value());
    const import::Part &part = model->parts.front();
    const Mesh &back = part.mesh;
    ASSERT_EQ(back.triangleCount(), 2u);

    const std::vector<glm::vec4> *color = back.primIf("Color");
    ASSERT_NE(color, nullptr) << "binary=" << binary;
    ASSERT_EQ(color->size(), 2u);
    const std::vector<glm::vec4> *charge = back.primIf("Charge");
    ASSERT_NE(charge, nullptr) << "binary=" << binary;
    ASSERT_EQ(charge->size(), 2u);
    // ascii pays %g's six significant digits; binary is bit-exact.
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

    // Cardinality stays unmistakable: the per-face lanes are NOT point
    // lanes, so neither the Part's lanes nor asCloud() carry them.
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
  // The trap: the reader fan-triangulates an n-gon into n-2 triangles,
  // so ONE source face's attribute must be REPLICATED across all of
  // them. A quad (2) plus a pentagon (3) gives 5 triangles from 2 face
  // rows — a triangles-only file would prove nothing about this.
  const char *ascii =
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
  const Mesh &mesh = model->parts.front().mesh;
  ASSERT_EQ(mesh.triangleCount(), 5u); // 2 from the quad, 3 from the pent
  const std::vector<glm::vec4> *color = mesh.primIf("Color");
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
  // A lone per-face scalar widens into .x — the "Id" convention.
  const std::vector<glm::vec4> *density = mesh.primIf("density");
  ASSERT_NE(density, nullptr);
  ASSERT_EQ(density->size(), 5u);
  EXPECT_FLOAT_EQ((*density)[1].x, 2.5f);
  EXPECT_FLOAT_EQ((*density)[2].x, -1.5f);
  EXPECT_FLOAT_EQ((*density)[4].x, -1.5f);
  EXPECT_FLOAT_EQ((*density)[0].y, 0.0f);

  // binary_little_endian fans identically: uchar count, int32 indices,
  // then the face's raw floats.
  std::vector<std::byte> bin;
  const auto push = [&](const void *p, size_t n) {
    const auto *b = static_cast<const std::byte *>(p);
    bin.insert(bin.end(), b, b + n);
  };
  const char *header =
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
  const float verts[] = {0, 0, 0, 1, 0, 0, 1, 1, 0,
                         0, 1, 0, 2, 1, 0, 2, 0, 0};
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
  const Mesh &binMesh = binModel->parts.front().mesh;
  ASSERT_EQ(binMesh.triangleCount(), 5u);
  ASSERT_NE(binMesh.primIf("Color"), nullptr);
  ASSERT_EQ(binMesh.primIf("Color")->size(), 5u);
  EXPECT_FLOAT_EQ((*binMesh.primIf("Color"))[1].x, 1.0f);
  EXPECT_FLOAT_EQ((*binMesh.primIf("Color"))[2].y, 0.25f);
  ASSERT_NE(binMesh.primIf("density"), nullptr);
  EXPECT_FLOAT_EQ((*binMesh.primIf("density"))[4].x, -1.5f);
}

TEST(Import, PlyFaceLanesTakeConventionalColorAndAnyDeclaredOrder) {
  // (a) MeshLab spells per-face color red/green/blue/alpha; it lands in
  // the same "Color" lane save::ply writes, integers normalized.
  // (b) The face properties may be declared BEFORE the index list —
  // the row is buffered, so the triangle count a lane replicates
  // across does not depend on where the list sits.
  const char *ascii =
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
  const Mesh &mesh = model->parts.front().mesh;
  ASSERT_EQ(mesh.triangleCount(), 3u); // 1 triangle + a fanned quad
  const std::vector<glm::vec4> *color = mesh.primIf("Color");
  ASSERT_NE(color, nullptr);
  ASSERT_EQ(color->size(), 3u);
  EXPECT_NEAR((*color)[0].x, 1.0f, 1e-3f); // uchar normalized
  EXPECT_NEAR((*color)[0].y, 0.0f, 1e-3f);
  EXPECT_FLOAT_EQ((*color)[0].w, 1.0f); // no alpha channel -> 1
  EXPECT_NEAR((*color)[1].z, 1.0f, 1e-3f);
  EXPECT_NEAR((*color)[2].y, 1.0f, 1e-3f);
  const std::vector<glm::vec4> *heat = mesh.primIf("heat");
  ASSERT_NE(heat, nullptr);
  ASSERT_EQ(heat->size(), 3u);
  EXPECT_FLOAT_EQ((*heat)[0].x, 9.0f); // raw: ids stay ids
  EXPECT_FLOAT_EQ((*heat)[1].x, 4.0f);
  EXPECT_FLOAT_EQ((*heat)[2].x, 4.0f);
}

TEST(Import, PlyFaceLanesSurviveHostileFaceHeaders) {
  // (a) A face naming a vertex past the count is dropped whole, and
  // its per-face values go with it: the lane stays sized to
  // triangleCount() rather than carrying a phantom entry.
  const char *dropped =
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
  const Mesh &mesh = model->parts.front().mesh;
  ASSERT_EQ(mesh.triangleCount(), 1u);
  ASSERT_NE(mesh.primIf("Color"), nullptr);
  ASSERT_EQ(mesh.primIf("Color")->size(), 1u);
  // The SURVIVING face's value, not the dropped one's.
  EXPECT_FLOAT_EQ((*mesh.primIf("Color"))[0].x, 0.5f);

  // (b) A face count no data could back is rejected before anything is
  // sized from it — the prim path never resizes on a declared count.
  const char *hugeFaces =
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
      import::model(hugeFaces, std::strlen(hugeFaces), "huge.ply")
          .has_value());

  // (c) A header promising more face rows than the body delivers fails
  // the read instead of publishing a short lane.
  const char *shortBody =
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
  EXPECT_FALSE(
      import::model(shortBody, std::strlen(shortBody), "short.ply")
          .has_value());

  // (d) A duplicate face property claims its lane once: the second
  // declaration is read and discarded rather than appending a second
  // time and desyncing the lane off triangleCount().
  const char *duplicate =
      "ply\nformat ascii 1.0\n"
      "element vertex 3\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 1\n"
      "property list uchar int vertex_indices\n"
      "property float heat\nproperty float heat\n"
      "end_header\n"
      "0 0 0\n1 0 0\n0 1 0\n"
      "3 0 1 2 6 7\n";
  auto dupModel =
      import::model(duplicate, std::strlen(duplicate), "dup.ply");
  ASSERT_TRUE(dupModel.has_value());
  const Mesh &dupMesh = dupModel->parts.front().mesh;
  ASSERT_EQ(dupMesh.triangleCount(), 1u);
  ASSERT_NE(dupMesh.primIf("heat"), nullptr);
  ASSERT_EQ(dupMesh.primIf("heat")->size(), 1u);
  EXPECT_FLOAT_EQ((*dupMesh.primIf("heat"))[0].x, 6.0f);
}

namespace {

/** An Ogawa archive written fully in memory: a translated xform
 *  holding a static clockwise triangle with a kVertexScope arb param,
 *  an animated top-level point cloud (2 samples at 24 fps) with ids on
 *  the first sample, and "uvquad"/"uvweld" — one topology written twice,
 *  with FACEVARYING and with VERTEX-scope uvs, the two sides of the
 *  (point, uv, normal) dedup. See AlembicFacevaryingUvsFlipAndDedup. */
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

    AbcGeom::OPolyMesh meshObj(root, "tri"); // static, under the xform
    const std::vector<Imath::V3f> pos = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    const std::vector<int32_t> indices = {0, 2, 1}; // Alembic: clockwise
    const std::vector<int32_t> counts = {3};
    meshObj.getSchema().set(AbcGeom::OPolyMeshSchema::Sample(
        Abc::P3fArraySample(pos), Abc::Int32ArraySample(indices),
        Abc::Int32ArraySample(counts)));
    AbcGeom::OFloatGeomParam energy(meshObj.getSchema().getArbGeomParams(),
                                    "energy", false, AbcGeom::kVertexScope,
                                    1);
    const std::vector<float> values = {0.25f, 0.5f, 0.75f};
    energy.set(AbcGeom::OFloatGeomParam::Sample(
        Abc::FloatArraySample(values), AbcGeom::kVertexScope));

    // Two triangles sharing the 0-2 diagonal of a unit square, written
    // TWICE with the same topology and different uv scopes — the two
    // sides of the dedup. 4 points, 6 corners either way.
    const std::vector<Imath::V3f> qpos = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    const std::vector<int32_t> qidx = {0, 2, 1, 0, 3, 2}; // clockwise
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

    AbcGeom::OPoints pointsObj(archive.getTop(), "cloud", ts); // animated
    const std::vector<uint64_t> ids = {0, 1};
    const std::vector<Imath::V3f> frame0 = {{0, 0, 0}, {1, 0, 0}};
    pointsObj.getSchema().set(AbcGeom::OPointsSchema::Sample(
        Abc::P3fArraySample(frame0), Abc::UInt64ArraySample(ids)));
    const std::vector<Imath::V3f> frame1 = {{0, 1, 0}, {1, 1, 0}};
    pointsObj.getSchema().set(
        AbcGeom::OPointsSchema::Sample(Abc::P3fArraySample(frame1)));
  } // OArchive dtor finalizes the Ogawa bytes — .str() only after
  return std::move(out).str();
}

} // namespace

TEST(Import, AlembicMeshPointsAndLanes) {
  const std::string bytes = alembicArchiveBytes();

  // No useful extension on the hint: the Ogawa magic routes it.
  auto model = import::model(bytes.data(), bytes.size(), "download");
  ASSERT_TRUE(model.has_value());
  ASSERT_EQ(model->parts.size(), 4u); // tri, uvquad, uvweld, cloud
  const auto find = [&](std::string_view name) -> const Part * {
    for (const Part &part : model->parts)
      if (part.name == name)
        return &part;
    return nullptr;
  };
  const Part *tri = find("tri");
  const Part *cloud = find("cloud");
  ASSERT_NE(tri, nullptr);
  ASSERT_NE(cloud, nullptr);

  EXPECT_EQ(tri->mesh.triangleCount(), 1u);
  glm::vec3 lo, hi;
  tri->mesh.bounds(&lo, &hi);
  EXPECT_FLOAT_EQ(lo.x, 10.0f); // the root xform baked into positions
  EXPECT_FLOAT_EQ(hi.x, 11.0f);
  // Alembic winds clockwise; the importer reverses to CCW, so the
  // derived normal faces +z instead of -z.
  ASSERT_EQ(tri->mesh.normals.size(), 3u);
  EXPECT_GT(tri->mesh.normals[0].z, 0.0f);
  // kVertexScope arbGeomParam -> per-point scalar lane, each value
  // riding the dedup to its vertex (locally 0.25 + 0.25x + 0.5y).
  const auto energy = tri->scalarLanes.find("energy");
  ASSERT_NE(energy, tri->scalarLanes.end());
  ASSERT_EQ(energy->second.size(), 3u);
  for (size_t i = 0; i < 3; ++i) {
    const glm::vec3 local = tri->mesh.positions[i] - glm::vec3{10, 0, 0};
    EXPECT_NEAR(energy->second[i],
                0.25f + 0.25f * local.x + 0.5f * local.y, 1e-6f);
  }
  EXPECT_EQ(tri->asCloud().scalars.count("energy"), 1u);

  // The point cloud: faceless part, ids as a lane, frame 0 at time 0.
  EXPECT_TRUE(cloud->mesh.indices.empty());
  ASSERT_EQ(cloud->mesh.positions.size(), 2u);
  EXPECT_FLOAT_EQ(cloud->mesh.positions[0].y, 0.0f);
  const auto id = cloud->scalarLanes.find("id");
  ASSERT_NE(id, cloud->scalarLanes.end());
  ASSERT_EQ(id->second.size(), 2u);
  EXPECT_FLOAT_EQ(id->second[0], 0.0f);
  EXPECT_FLOAT_EQ(id->second[1], 1.0f);

  // Malformed bytes stay quiet: garbage and a truncated archive.
  const char garbage[] = "not an alembic archive at all";
  EXPECT_FALSE(import::alembic(garbage, sizeof(garbage)).has_value());
  EXPECT_FALSE(
      import::alembic(bytes.data(), bytes.size() / 2).has_value());
}

TEST(Import, AlembicFacevaryingUvsFlipAndDedup) {
  // The uv path had no Alembic coverage at all. Two claims, one fixture:
  // (1) Alembic's uv origin is BOTTOM-left and Mesh's is top-left, so v
  // flips on the way in; (2) corners dedup OBJ-style on (point, uv,
  // normal) — agreeing corners merge, a uv disagreement splits a point
  // into two vertices.
  const std::string bytes = alembicArchiveBytes();
  auto model = import::alembic(bytes.data(), bytes.size());
  ASSERT_TRUE(model.has_value());
  const auto part = [&](std::string_view name) -> const Part * {
    for (const Part &p : model->parts)
      if (p.name == name)
        return &p;
    return nullptr;
  };
  // uvs indexed by POSITION — insertion order is a detail of the walk.
  const auto uvsAt = [](const Mesh &m, glm::vec3 p) {
    std::vector<glm::vec2> found;
    for (size_t i = 0; i < m.positions.size(); ++i)
      if (glm::length(m.positions[i] - p) < 1e-6f)
        found.push_back(m.uvs[i]);
    std::sort(found.begin(), found.end(),
              [](glm::vec2 a, glm::vec2 b) { return a.x < b.x; });
    return found;
  };

  // --- FACEVARYING: the dedup keys on the corner INDEX, so every corner
  // is its own vertex. 6 corners -> 6 vertices, even where two of them
  // carry an identical uv. That is the "sources" simplification the
  // importAbcMesh header declares, pinned rather than assumed.
  const Part *quad = part("uvquad");
  ASSERT_NE(quad, nullptr);
  const Mesh &fv = quad->mesh;
  ASSERT_EQ(fv.positions.size(), 6u);
  ASSERT_EQ(fv.uvs.size(), 6u);
  EXPECT_EQ(fv.triangleCount(), 2u);
  // The v FLIP: file uv (0,0) at the origin corner arrives as (0,1) — a
  // v of 0 becoming 1 is something no non-flipping reader produces.
  const std::vector<glm::vec2> atOrigin = uvsAt(fv, {0, 0, 0});
  ASSERT_EQ(atOrigin.size(), 2u); // corners 0 and 3, NOT welded
  for (const glm::vec2 &uv : atOrigin) {
    EXPECT_FLOAT_EQ(uv.x, 0.0f);
    EXPECT_FLOAT_EQ(uv.y, 1.0f);
  }
  EXPECT_FLOAT_EQ(uvsAt(fv, {0, 1, 0}).at(0).y, 0.0f); // file v=1 -> 0
  // The shared point at (1,1,0) carried two different uvs; both survive,
  // both flipped.
  const std::vector<glm::vec2> shared = uvsAt(fv, {1, 1, 0});
  ASSERT_EQ(shared.size(), 2u);
  EXPECT_FLOAT_EQ(shared[0].x, 0.25f);
  EXPECT_FLOAT_EQ(shared[0].y, 0.25f); // file (0.25, 0.75)
  EXPECT_FLOAT_EQ(shared[1].x, 1.0f);
  EXPECT_FLOAT_EQ(shared[1].y, 0.0f); // file (1, 1)

  // --- VERTEX scope: the uv source IS the point, so the six corners
  // weld back to four vertices. Same topology, same flip.
  const Part *weld = part("uvweld");
  ASSERT_NE(weld, nullptr);
  const Mesh &vw = weld->mesh;
  ASSERT_EQ(vw.positions.size(), 4u); // the merge the facevarying case
  ASSERT_EQ(vw.uvs.size(), 4u);       // cannot make
  EXPECT_EQ(vw.triangleCount(), 2u);
  const std::vector<glm::vec2> weldOrigin = uvsAt(vw, {0, 0, 0});
  ASSERT_EQ(weldOrigin.size(), 1u);
  EXPECT_FLOAT_EQ(weldOrigin[0].x, 0.0f);
  EXPECT_FLOAT_EQ(weldOrigin[0].y, 1.0f); // file (0,0) -> (0,1)
  const std::vector<glm::vec2> weldShared = uvsAt(vw, {1, 1, 0});
  ASSERT_EQ(weldShared.size(), 1u);
  EXPECT_FLOAT_EQ(weldShared[0].x, 1.0f);
  EXPECT_FLOAT_EQ(weldShared[0].y, 0.0f); // file (1,1) -> (1,0)
}

TEST(Import, AlembicTimeSampleSelection) {
  const std::string bytes = alembicArchiveBytes();
  const auto cloudY = [&](double time) -> float {
    auto model =
        import::alembic(bytes.data(), bytes.size(), {.time = time});
    if (!model)
      return -1.0f;
    for (const Part &part : model->parts)
      if (part.name == "cloud")
        return part.mesh.positions.at(0).y;
    return -1.0f;
  };
  EXPECT_FLOAT_EQ(cloudY(0), 0.0f);        // frame 0
  EXPECT_FLOAT_EQ(cloudY(1.0 / 24), 1.0f); // frame 1
  EXPECT_FLOAT_EQ(cloudY(0.6 / 24), 1.0f); // NEAREST sample, not floor
}

TEST(Save, PlyRoundTripsCloudLanes) {
  // The return leg: a Cloud with every lane kind writes to PLY and
  // reads back reconstituted — vectors fold from _x/_y/_z, colors
  // from _r/_g/_b/_a, tint survives its uchar quantization, and the
  // conventional names stay conventional.
  Cloud cloud;
  cloud.positions = {{0, 0, 0}, {4, 0, 0}, {0, 4, 0}, {4, 4, 2}};
  cloud.scalar("energy") = {0.5f, 1.5f, 2.5f, 3.5f};
  cloud.vector("dir") = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 0, 0}};
  cloud.vector("normal") =
      {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  cloud.color("glow") = {{0.25f, 0.5f, 0.75f, 1.0f},
                         {1, 0, 0, 0.5f},
                         {0, 1, 0, 0.25f},
                         {0, 0, 1, 0.125f}};
  cloud.color("tint") = {{1, 0, 0, 1},
                         {0, 1, 0, 1},
                         {0, 0, 1, 1},
                         {1, 1, 1, 0.5f}};

  const std::string bytes = save::ply(cloud);
  auto model = import::model(bytes.data(), bytes.size(), "trip.ply");
  ASSERT_TRUE(model.has_value());
  const Cloud back = model->parts.front().asCloud();
  ASSERT_EQ(back.size(), 4u);
  EXPECT_NEAR(back.positions[3].z, 2, 1e-4f);
  ASSERT_TRUE(back.scalarIf("energy"));
  EXPECT_FLOAT_EQ((*back.scalarIf("energy"))[2], 2.5f);
  ASSERT_TRUE(back.vectorIf("dir")); // folded back from dir_x/_y/_z
  EXPECT_FLOAT_EQ((*back.vectorIf("dir"))[1].y, 1);
  ASSERT_TRUE(back.vectorIf("normal")); // via nx/ny/nz
  EXPECT_FLOAT_EQ((*back.vectorIf("normal"))[0].z, 1);
  ASSERT_TRUE(back.colorIf("glow")); // folded from glow_r/_g/_b/_a
  EXPECT_NEAR((*back.colorIf("glow"))[0].y, 0.5f, 1e-4f);
  EXPECT_NEAR((*back.colorIf("glow"))[3].w, 0.125f, 1e-4f);
  ASSERT_TRUE(back.colorIf("tint")); // uchar red/green/blue/alpha
  EXPECT_NEAR((*back.colorIf("tint"))[1].y, 1, 1.5f / 255.0f);
  EXPECT_NEAR((*back.colorIf("tint"))[3].w, 0.5f, 1.5f / 255.0f);
}

TEST(Save, PlyRoundTripsMeshWithFaces) {
  Mesh quad = mesh::quad(10, 6);
  quad.colors.assign(quad.vertexCount(), {0.2f, 0.9f, 0.4f, 1});
  const std::string bytes = save::ply(quad);
  auto model = import::model(bytes.data(), bytes.size(), "quad.ply");
  ASSERT_TRUE(model.has_value());
  const Mesh &back = model->parts.front().mesh;
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
  // binary_little_endian rows are raw floats, so the round trip is
  // BIT-exact — EXPECT_FLOAT_EQ throughout, no %g truncation; only
  // tint pays its uchar quantization toll. The awkward values
  // (thirds, 1e-5) are the ones ascii would have rounded.
  Cloud cloud;
  cloud.positions = {{0.1f, 2.3f, -4.5f},
                     {6.7f, -8.9f, 10.11f},
                     {1.0f / 3.0f, 2.0f / 7.0f, 1e-5f}};
  cloud.scalar("energy") = {0.5f, 1.0f / 3.0f, 2.5f};
  cloud.vector("dir") = {{1, 0, 0}, {0.1f, 0.2f, 0.3f}, {0, 0, 1}};
  cloud.vector("normal") = {{0, 0, 1}, {0, 1, 0}, {1, 0, 0}};
  cloud.color("glow") = {{0.25f, 0.5f, 0.75f, 1.0f},
                         {1.0f / 3.0f, 0, 0, 0.5f},
                         {0, 1, 0, 0.125f}};
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
  ASSERT_TRUE(back.vectorIf("dir")); // folded back from dir_x/_y/_z
  EXPECT_FLOAT_EQ((*back.vectorIf("dir"))[1].z, 0.3f);
  ASSERT_TRUE(back.vectorIf("normal")); // via nx/ny/nz
  EXPECT_FLOAT_EQ((*back.vectorIf("normal"))[1].y, 1.0f);
  ASSERT_TRUE(back.colorIf("glow")); // folded from glow_r/_g/_b/_a
  EXPECT_FLOAT_EQ((*back.colorIf("glow"))[1].x, 1.0f / 3.0f);
  EXPECT_FLOAT_EQ((*back.colorIf("glow"))[2].w, 0.125f);
  ASSERT_TRUE(back.colorIf("tint")); // uchar red/green/blue/alpha
  EXPECT_NEAR((*back.colorIf("tint"))[2].w, 0.5f, 1.5f / 255.0f);

  // Faces ride binary too: list counts as single raw uchars, indices
  // as raw int32 — the same rows the ascii writer spells in text.
  Mesh quad = mesh::quad(10, 6);
  quad.colors.assign(quad.vertexCount(), {0.2f, 0.9f, 0.4f, 1});
  const std::string meshBytes = save::ply(quad, {.binary = true});
  auto meshModel =
      import::model(meshBytes.data(), meshBytes.size(), "quad.ply");
  ASSERT_TRUE(meshModel.has_value());
  const Mesh &tri = meshModel->parts.front().mesh;
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
  // Wrong-length lanes are skipped by header AND rows (the lockstep
  // predicate): the export still parses and the correct lane survives.
  Cloud cloud;
  cloud.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  cloud.scalar("energy") = {1, 2, 3};
  cloud.scalars["stub"] = {7};                    // wrong length
  cloud.vectors["off"] = {{1, 2, 3}, {4, 5, 6}}; // wrong length
  const std::string bytes = save::ply(cloud);
  auto model = import::model(bytes.data(), bytes.size(), "skip.ply");
  ASSERT_TRUE(model.has_value());
  const Part &part = model->parts.front();
  ASSERT_EQ(part.mesh.vertexCount(), 3u);
  ASSERT_EQ(part.scalarLanes.count("energy"), 1u);
  EXPECT_FLOAT_EQ(part.scalarLanes.at("energy")[2], 3.0f);
  EXPECT_EQ(part.scalarLanes.count("stub"), 0u);
  EXPECT_EQ(part.vectorLanes.count("off"), 0u);
  EXPECT_EQ(part.scalarLanes.count("off_x"), 0u);

  // Zero vertices decline loudly: "" from the string overloads (our
  // own importer refuses an empty PLY), false from the file overload.
  EXPECT_TRUE(save::ply(Cloud{}).empty());
  EXPECT_TRUE(save::ply(Mesh{}).empty());
  const std::filesystem::path file =
      std::filesystem::temp_directory_path() /
      "sigilshape_empty_decline.ply";
  EXPECT_FALSE(save::ply(file, Cloud{}));
  EXPECT_FALSE(save::ply(file, Mesh{}));
}

TEST(Pop, CookMeshFormsAModelFromAChain) {
  // The whole point of the split: a pop chain DESCRIBES a model, and
  // the CPU cook forms it as one Mesh — the same currency both
  // space::drawMesh (Skia) and World::addSurface (Diligent) draw.
  pop::SplineScatter scatter;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    scatter.loop.push_back(
        {200.0f * std::cos(a), 0, 200.0f * std::sin(a)});
  }
  scatter.count = 500;
  scatter.head = 1;
  scatter.span = 1;
  scatter.radius = 12;
  pop::Chain chain = {scatter,
                      pop::Vary{pop::Lane::Scale, 1.0f, 0.4f, 3},
                      pop::Ramp{pop::Lane::Color,
                                {1, 0, 0, 1},
                                {0, 0, 1, 1}}};
  const Mesh stamp = mesh::quad(6, 6);
  const Mesh model = popops::cookMesh(chain, stamp);
  EXPECT_EQ(model.vertexCount(), 500u * stamp.vertexCount());
  EXPECT_EQ(model.triangleCount(), 500u * stamp.triangleCount());
  ASSERT_EQ(model.colors.size(), model.vertexCount()); // tint baked
  // Determinism: the same description forms the same model.
  const Mesh again = popops::cookMesh(chain, stamp);
  ASSERT_EQ(again.positions.size(), model.positions.size());
  EXPECT_EQ(again.positions[123].x, model.positions[123].x);
  // The chain is a VALUE: editing it re-describes a different model.
  chain.push_back(pop::Math{pop::Lane::P, {1, 1, 1, 1}, {0, 500, 0, 0}});
  const Mesh lifted = popops::cookMesh(chain, stamp);
  EXPECT_GT(lifted.positions[123].y, model.positions[123].y + 400.0f);
}

TEST(Pop, SweptSinksBendWithTheChain) {
  // The chain's cooked points ARE the path: a noised circle scatter
  // becomes a wobbly tube; the same description ribbons; a Math
  // translate re-describes the whole model elsewhere.
  pop::SplineScatter scatter;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    scatter.loop.push_back(
        {300.0f * std::cos(a), 0, 300.0f * std::sin(a)});
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
  EXPECT_NEAR(hi.x - lo.x, 624, 130); // ~circle + noise + radius

  const Mesh ribbon =
      popops::cookRibbon(chain, 60, {.closed = true, .segments = 160});
  EXPECT_GT(ribbon.triangleCount(), 200u);

  chain.push_back(
      pop::Math{pop::Lane::P, {1, 1, 1, 1}, {0, 900, 0, 0}});
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
  // One breath: entry verb, intent verbs, a sink — every default loud.
  const Mesh wobble =
      pop::on(loop).count(64).noise(30).tube(10, 8, true);
  EXPECT_GT(wobble.triangleCount(), 500u);

  // The builder IS the chain: nothing hides, edits stay open.
  pop::Chain c = pop::on(loop)
                     .count(10)
                     .spread(5)
                     .vary(0.4f)
                     .fade({1, 0, 0, 1}, {0, 0, 1, 1});
  EXPECT_EQ(c.size(), 3u); // scatter + vary + ramp
  std::get<pop::SplineScatter>(c.front()).count = 20;
  EXPECT_EQ(popops::cook(c).size(), 20u);
}


TEST(Pop, SmoothHealsNoiseKinks) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({250.0f * std::cos(a), 0, 250.0f * std::sin(a)});
  }
  const auto jaggedness = [](const Cloud &cloud) {
    double sum = 0;
    for (size_t i = 1; i + 1 < cloud.size(); ++i)
      sum += glm::length(cloud.positions[i - 1] -
                         cloud.positions[i] * 2.0f +
                         cloud.positions[i + 1]);
    return sum;
  };
  const double rough = jaggedness(
      popops::cook(pop::on(loop).count(80).noise(30).chain()));
  const double healed = jaggedness(popops::cook(
      pop::on(loop).count(80).noise(30).smooth(0.6f, 3).chain()));
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
  EXPECT_NEAR(hi.x - lo.x, 2 * (220 + 24), 30); // ring + star arms
  EXPECT_GT(hi.y - lo.y, 20.0f); // the profile stands off the plane
}


TEST(Pop, ChainsComposeIntoEachOther) {
  // Pops feed pops: chain A's cooked result IS chain B's path.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({220.0f * std::cos(a), 0, 220.0f * std::sin(a)});
  }
  const pop::Chain spine =
      pop::on(loop).count(48).noise(26).smooth(0.5f);
  const Cloud beads = pop::on(spine).count(300).spread(6).cloud();
  EXPECT_EQ(beads.size(), 300u);
  // The beads inherit A's off-plane noise — they ride the composed
  // path, not the raw circle.
  float yMin = 1e9f, yMax = -1e9f;
  for (const glm::vec3 &p : beads.positions) {
    yMin = std::min(yMin, p.y);
    yMax = std::max(yMax, p.y);
  }
  EXPECT_GT(yMax - yMin, 12.0f);
  // And any sink still applies to the composition.
  EXPECT_GT(pop::on(spine).count(80).tube(6, 8, true).triangleCount(),
            500u);
}


TEST(Pop, ChainsSeedFromFormedModels) {
  // The recursive loop: form a model, scatter ON its surface, form
  // again — every stage one description.
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
  EXPECT_LE(dHi.x, mHi.x + 1); // dust lives on the cable
  EXPECT_GT(pop::on(cable, 300).stamps(mesh::quad(4, 4)).triangleCount(),
            500u);
}

TEST(Curves, BannerHangsGravityUpright) {
  Spline3 loop;
  loop.closed = true;
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i / 12.0f * 2.0f * (float)M_PI;
    loop.points.push_back(
        {300.0f * std::cos(a), 40.0f * std::sin(2 * a),
         300.0f * std::sin(a)});
  }
  const Mesh band =
      curves::banner(loop, {.width = 50, .sections = 120});
  ASSERT_EQ(band.vertexCount(), 240u);
  // Every cross-section's u=0 vertex sits ABOVE its u=1 partner: the
  // width hangs vertical, never rolled.
  for (size_t i = 0; i + 1 < band.positions.size(); i += 2)
    EXPECT_GT(band.positions[i].y, band.positions[i + 1].y);
}

TEST(Pop, ImportedModelsJoinTheSystem) {
  // The loaders' output is the pop system's input: an imported model
  // (the in-memory OBJ cube) both SEEDS a chain and serves as a
  // STAMP — no special casing, Mesh is Mesh.
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
  EXPECT_LE(hi.x, 1.01f); // points live on the unit cube

  // ...and use the imported model AS the stamp along a chain.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({80.0f * std::cos(a), 0, 80.0f * std::sin(a)});
  }
  const Mesh cubes =
      pop::on(loop).count(24).vary(0.4f).stamps(cube);
  EXPECT_EQ(cubes.triangleCount(), 24u * cube.triangleCount());
}

TEST(Pop, NamedAttributesFlowAndExport) {
  // TD's superpower: any NAME is an attribute. Create one, noise it,
  // scale it into Scale via Math-on-custom... and read it all back.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({200.0f * std::cos(a), 0, 200.0f * std::sin(a)});
  }
  const pop::Chain chain =
      pop::on(loop)
          .count(64)
          .set("energy", {0.5f, 0, 0, 0})
          .op(pop::Jitter{"energy", 0.25f, 5})
          .op(pop::Math{"energy", {2, 1, 1, 1}, {0, 0, 0, 0}});
  const Cloud cooked = popops::cook(chain);
  const std::vector<glm::vec4> *energy = cooked.colorIf("energy");
  ASSERT_TRUE(energy) << "customs must export under their own name";
  float lo = 1e9f, hi = -1e9f;
  for (const glm::vec4 &e : *energy) {
    lo = std::min(lo, e.r);
    hi = std::max(hi, e.r);
  }
  EXPECT_GT(hi, lo + 0.1f); // jittered, not constant
  EXPECT_GT(lo, 0.4f);      // 2 * (0.5 - 0.25) bounds
  EXPECT_LT(hi, 1.6f);
}

TEST(Pop, SharedPcgHashKeepsBothConsumersBitStable) {
  // ONE hash now (detail/Hash.h) feeds both the parity-locked pop
  // scatter and Points' PRNG. These goldens were captured from the two
  // separate copies that preceded it, so they fail if the shared
  // definition drifts a single bit -- which would silently desync the
  // CPU executor from the Slang kernels.
  //
  // Pop side: Jitter with amplitude 0.5 on a zeroed lane IS
  // hash1(i*3 + seed) - 0.5, undiluted.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.push_back({100.0f * std::cos(a), 0, 100.0f * std::sin(a)});
  }
  const Cloud cooked = popops::cook(pop::on(loop)
                                        .count(6)
                                        .set("h", {0, 0, 0, 0})
                                        .op(pop::Jitter{"h", 0.5f, 0}));
  const std::vector<glm::vec4> *h = cooked.colorIf("h");
  ASSERT_TRUE(h);
  ASSERT_EQ(h->size(), 6u);
  const float popGolden[6] = {0.231199384f,  -0.441547632f,
                              -0.184789419f, 0.0919363499f,
                              0.274898827f,  0.0884094238f};
  for (size_t i = 0; i < 6; ++i)
    EXPECT_NEAR((*h)[i].x, popGolden[i], 1e-7f) << "pop hash lane " << i;

  // Points side: a unit-box scatter is the raw rand01 stream, in order.
  const Cloud box =
      points::scatterBox({0, 0, 0}, {1, 1, 1}, 4, /*seed=*/7);
  ASSERT_EQ(box.positions.size(), 4u);
  const float pointsGolden[12] = {
      0.985658824f, 0.420034766f,  0.98710376f,  0.480089128f,
      0.151413783f, 0.589045703f,  0.263890147f, 0.0428663865f,
      0.913271964f, 0.0273712128f, 0.317990124f, 0.787527025f};
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
  // Every stamped point's uvs land inside ONE half-size atlas cell.
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
  EXPECT_GT(cellsSeen[0] + cellsSeen[1] + cellsSeen[2] + cellsSeen[3],
            39); // all classified
  int distinct = 0;
  for (int c : cellsSeen)
    distinct += c > 0 ? 1 : 0;
  EXPECT_GE(distinct, 3) << "the hash should spread across cells";
}

// --- Primitive attribute lanes --------------------------------------------

namespace {

/** Two triangles splitting a 100x100 square along the (-,-)->(+,+)
 *  diagonal, wound CCW in the y-up world so both survive backface
 *  culling. Triangle 0 is the lower-right half, triangle 1 the upper
 *  left — the flat-tint test samples one pixel inside each. */
Mesh splitQuad() {
  Mesh m;
  m.positions = {{-50, -50, 0}, {50, -50, 0}, {50, 50, 0}, {-50, 50, 0}};
  m.indices = {0, 1, 2, 0, 2, 3};
  return m;
}

} // namespace

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

  // Appending a lane-less mesh pads OUR lanes by name convention.
  Mesh b = splitQuad();
  a.append(b);
  ASSERT_EQ(a.triangleCount(), 4u);
  ASSERT_EQ(a.primIf("Color")->size(), 4u);
  EXPECT_EQ((*a.primIf("Color"))[2], (glm::vec4{1, 1, 1, 1}));
  EXPECT_EQ((*a.primIf("heat"))[3], (glm::vec4{0, 0, 0, 0}));

  // ...and the other direction: THEIR lane pads over our old triangles.
  Mesh c = splitQuad();
  c.prim("heat", {0, 0, 0, 0})[0] = {5, 0, 0, 0};
  a.append(c);
  ASSERT_EQ(a.triangleCount(), 6u);
  const std::vector<glm::vec4> *heat = a.primIf("heat");
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
  // Vertices unshared: three per triangle, indices renumbered.
  ASSERT_EQ(baked.vertexCount(), 6u);
  ASSERT_EQ(baked.triangleCount(), 2u);
  ASSERT_EQ(baked.colors.size(), 6u);
  for (size_t k = 0; k < 3; ++k) {
    EXPECT_FLOAT_EQ(baked.colors[k].r, 1.0f);
    EXPECT_FLOAT_EQ(baked.colors[k].b, 0.0f);
    EXPECT_FLOAT_EQ(baked.colors[3 + k].b, 1.0f);
    EXPECT_FLOAT_EQ(baked.colors[3 + k].r, 0.0f);
    // The pre-existing vertex colour multiplies through.
    EXPECT_FLOAT_EQ(baked.colors[k].a, 0.5f);
  }
  EXPECT_EQ(baked.positions[3], m.positions[0]) << "triangle order kept";
  EXPECT_TRUE(baked.primIf("Color")) << "lanes survive the unweld";
  // A missing lane is a no-op, not a corruption.
  EXPECT_EQ(mesh::bakePrimColor(m, "absent").vertexCount(), 4u);
}

TEST(Space, PrimColorLaneTintsTrianglesFlat) {
  Mesh m = splitQuad();
  m.prim("Color")[0] = {1, 0, 0, 1}; // lower-right half
  m.prim("Color")[1] = {0, 0, 1, 1}; // upper-left half

  space::Camera camera;
  camera.eye = {0, 0, 300};
  space::MeshStyle style;
  style.baseColor = {1, 1, 1, 1};

  const auto render = [&](const space::MeshStyle &s) {
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

  // Control: no lane named, both halves shade identically.
  const SkBitmap plain = render(style);
  EXPECT_EQ(plain.getColor(120, 95), plain.getColor(79, 54));

  style.primColorLane = "Color";
  const SkBitmap tinted = render(style);
  const SkColor lowerRight = tinted.getColor(120, 95);
  const SkColor upperLeft = tinted.getColor(79, 54);
  EXPECT_GT(SkColorGetR(lowerRight), SkColorGetB(lowerRight) + 40u);
  EXPECT_GT(SkColorGetB(upperLeft), SkColorGetR(upperLeft) + 40u);

  // The Normals G-buffer is a BUFFER: the tint must not touch it.
  style.mode = space::MeshStyle::Mode::Normals;
  space::MeshStyle bare = style;
  bare.primColorLane.clear();
  EXPECT_EQ(render(style).getColor(120, 95), render(bare).getColor(120, 95));
}

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

  // The point sink is untouched: a Cloud has no primitive class.
  const Cloud cooked = popops::cook(chain);
  ASSERT_EQ(cooked.size(), (size_t)kPoints);

  const Mesh model = popops::cookMesh(chain, stamp);
  const size_t perStamp = stamp.triangleCount();
  ASSERT_EQ(model.triangleCount(), (size_t)kPoints * perStamp);

  const std::vector<glm::vec4> *color = model.primIf("Color");
  const std::vector<glm::vec4> *id = model.primIf("Id");
  const std::vector<glm::vec4> *size = model.primIf("size");
  ASSERT_TRUE(color && id && size);
  ASSERT_EQ(color->size(), model.triangleCount());

  const std::vector<glm::vec4> *tint = cooked.colorIf("tint");
  const std::vector<float> *sizes = cooked.scalarIf("size");
  ASSERT_TRUE(tint && sizes);
  for (size_t p = 0; p < (size_t)kPoints; ++p)
    for (size_t k = 0; k < perStamp; ++k) {
      const size_t tri = p * perStamp + k;
      // Every triangle of a stamp carries its owning POINT's values...
      EXPECT_EQ((*color)[tri], (*tint)[p]);
      EXPECT_FLOAT_EQ((*size)[tri].x, (*sizes)[p]);
      // ...and "Id" is the owning point itself: one piece, one value.
      EXPECT_FLOAT_EQ((*id)[tri].x, (float)p);
    }
  // The ramp really varied, so this is not a constant-lane tautology.
  EXPECT_GT((*color)[model.triangleCount() - 1].b, (*color)[0].b + 0.5f);

  // Class boundary: the swept sinks resample, so they promote nothing.
  EXPECT_TRUE(popops::cookTube(chain, 4).prims.empty());
}

TEST(Save, PlyWritesPrimLanesAsFaceProperties) {
  Mesh m = splitQuad();
  m.prim("Color")[0] = {1, 0, 0, 1};
  m.prim("Color")[1] = {0, 0.25f, 0, 1};
  const std::string text = save::ply(m);
  ASSERT_FALSE(text.empty());
  // Declared on the FACE element, after the index list.
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
  // Geometry still round-trips through our own importer, which skips
  // face properties it does not know (prim lanes are export-only).
  auto back = import::model(text.data(), text.size(), "prims.ply");
  ASSERT_TRUE(back.has_value());
  ASSERT_EQ(back->parts.size(), 1u);
  EXPECT_EQ(back->parts.front().mesh.triangleCount(), 2u);
}
