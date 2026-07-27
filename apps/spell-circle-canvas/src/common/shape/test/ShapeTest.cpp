#include "sigilshape/Blend.h"
#include "sigilshape/Curves.h"
#include "sigilshape/Geometry.h"
#include "sigilshape/Materials.h"
#include "sigilshape/Mesh.h"
#include "sigilshape/Ops.h"
#include "sigilshape/Points.h"
#include "sigilshape/Space.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>

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
  SkV3 lo, hi;
  m.bounds(&lo, &hi);
  EXPECT_NEAR(hi.x - lo.x, 100.0f, 1e-3f);
  EXPECT_NEAR(hi.y - lo.y, 60.0f, 1e-3f);
  EXPECT_NEAR(hi.z - lo.z, 20.0f, 1e-3f);
  // 2 caps (2 tris each) + 4 walls (2 tris each) = 12 triangles.
  EXPECT_EQ(m.triangleCount(), 12u);
  // All normals unit length.
  for (const SkV3 &n : m.normals)
    EXPECT_NEAR(n.length(), 1.0f, 1e-4f);
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
    const SkV3 &a = m.positions[m.indices[t]];
    const SkV3 &b = m.positions[m.indices[t + 1]];
    const SkV3 &c = m.positions[m.indices[t + 2]];
    if (a.z > 4.9f && b.z > 4.9f && c.z > 4.9f) {
      const SkV3 ab = b - a, ac = c - a;
      area += 0.5 * std::abs((double)ab.x * ac.y - (double)ab.y * ac.x);
    }
  }
  const double expected = M_PI * (80.0 * 80.0 - 40.0 * 40.0);
  EXPECT_NEAR(area / expected, 1.0, 0.03);
}

TEST(Mesh, GridUvAndIndicesCoherent) {
  Mesh m = mesh::grid(4, 3, [](float u, float v) -> SkV3 {
    return {u * 10, v * 10, 0};
  });
  EXPECT_EQ(m.vertexCount(), 12u);
  EXPECT_EQ(m.triangleCount(), 12u); // 3x2 cells * 2
  // Image-convention UVs: v param 0 (sheet bottom) samples image v=1.
  EXPECT_EQ(m.uvs.front().fX, 0.0f);
  EXPECT_EQ(m.uvs.front().fY, 1.0f);
  EXPECT_EQ(m.uvs.back().fY, 0.0f);
  for (uint32_t i : m.indices)
    EXPECT_LT(i, m.vertexCount());
}

TEST(Mesh, TorusNormalsPointOutward) {
  Mesh m = mesh::torus(100, 30, 32, 16);
  // At u=0,v=0: phi=0 -> outer equator, normal ~ +x.
  const SkV3 n0 = m.normals.front();
  EXPECT_GT(std::abs(n0.x), 0.7f);
  for (const SkV3 &n : m.normals)
    EXPECT_NEAR(n.length(), 1.0f, 1e-3f);
}

TEST(Mesh, TransformMovesBoundsAndKeepsUnitNormals) {
  Mesh m = mesh::quad(10, 10);
  m.transform(SkM44::Translate(5, 0, 0));
  SkV3 lo, hi;
  m.bounds(&lo, &hi);
  EXPECT_NEAR((lo.x + hi.x) * 0.5f, 5.0f, 1e-4f);
  for (const SkV3 &n : m.normals)
    EXPECT_NEAR(n.length(), 1.0f, 1e-4f);
}

// --- Space ----------------------------------------------------------------

TEST(Space, CameraProjectsCenterToViewportCenter) {
  space::Camera camera;
  camera.eye = {0, 0, 100};
  camera.target = {0, 0, 0};
  const SkM44 vp = camera.viewProjection({800, 600});
  const SkV4 out = vp * SkV4{0, 0, 0, 1};
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
                  SkM44(), camera, {200, 150}, style);
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
  const SkV3 mid = line.position(0.5f);
  EXPECT_NEAR(mid.x, 50, 1e-3);

  Spline3 spline;
  spline.points = {{0, 0, 0}, {50, 40, 0}, {100, 0, 0}, {150, -40, 0}};
  const SkV3 start = spline.position(0);
  const SkV3 end = spline.position(1);
  EXPECT_NEAR(start.x, 0, 1e-3);   // Catmull-Rom passes through
  EXPECT_NEAR(end.x, 150, 1e-3);
}

TEST(Curves, ArcLengthSamplingIsEven) {
  Spline3 spline; // deliberately uneven knots
  spline.type = Spline3::Type::Linear; // keep the curve straight so
                                       // spacing is the only variable
  spline.points = {{0, 0, 0}, {5, 0, 0}, {10, 0, 0}, {200, 0, 0}};
  const std::vector<SkV3> beads = spline.sampleArcLength(11);
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
    EXPECT_NEAR(f.tangent.length(), 1, 1e-3);
    EXPECT_NEAR(f.normal.length(), 1, 1e-3);
    EXPECT_NEAR(f.tangent.dot(f.normal), 0, 1e-3);
    if (i > 0) // parallel transport: no sudden flips
      EXPECT_GT(f.normal.dot(rail[i - 1].normal), 0.5f);
  }
}

TEST(Curves, TubeAndRibbonAreWellFormed) {
  Spline3 arc;
  arc.points = {{0, 0, 0}, {60, 60, 0}, {120, 0, 0}};
  const Mesh t = curves::tube(arc, {.radius = 8, .segments = 24, .sides = 8});
  EXPECT_GT(t.triangleCount(), 0u);
  EXPECT_EQ(t.normals.size(), t.vertexCount());
  for (const SkV3 &n : t.normals)
    EXPECT_NEAR(n.length(), 1, 1e-3);
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
  for (const SkV3 &p : circle.positions) {
    EXPECT_NEAR(p.length(), 50, 1e-2);  // on the radius
    EXPECT_NEAR(p.y, 0, 1e-3);          // in the plane ⊥ default axis
  }

  Cloud box = points::scatterBox({0, 0, 0}, {10, 10, 10}, 100, 3);
  EXPECT_EQ(box.size(), 100u);
  for (const SkV3 &p : box.positions) {
    EXPECT_GE(p.x, 0);
    EXPECT_LE(p.x, 10);
  }
}

TEST(Points, OnMeshLandsOnSurface) {
  const Mesh quad = mesh::quad(100, 100); // z = 0 plane
  Cloud cloud = points::onMesh(quad, 64, 5);
  ASSERT_EQ(cloud.size(), 64u);
  for (const SkV3 &p : cloud.positions) {
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
  std::vector<SkColor4f> &tint = cloud.color("tint");
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
  EXPECT_NEAR(merged.colors[0].fR, 1, 1e-4);
  EXPECT_NEAR(merged.colors[0].fG, 0, 1e-4);
  // The first stamp is scaled 2x: its extent from its ring point is 10.
  SkV3 lo, hi;
  Mesh first;
  first.positions.assign(merged.positions.begin(),
                         merged.positions.begin() + 4);
  first.bounds(&lo, &hi);
  EXPECT_GT((hi - lo).length(), 14.0f); // 2x-scaled quad diagonal-ish
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
