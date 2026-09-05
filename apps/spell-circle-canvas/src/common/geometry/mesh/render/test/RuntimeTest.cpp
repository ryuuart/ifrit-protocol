/** @file
 * The runtime seam: the built-in value is one value however it is
 * reached, a style carries it by default, and a substituted executor
 * receives the draw instead. Plus the shading terms this tier
 * evaluates, each against the closed form a device shader's own
 * spelling of it is held to.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <glm/glm.hpp>
#include <string>
#include <utility>
#include <vector>

#include "sigilgeometry/mesh/camera/Camera.h"
#include "sigilgeometry/mesh/render/Painter.h"

using namespace sigil::geometry::mesh;

namespace {

/** An executor that records rather than draws. Two of them compare by
 *  the label they carry, which is what makes a Runtime holding one a
 *  comparable value. */
struct Recorder : render::Executor {
  explicit Recorder(std::string name) : label(std::move(name)) {}

  std::string label;
  mutable int meshes = 0;
  mutable int panels = 0;

  bool operator==(const Recorder& o) const { return label == o.label; }

  void drawMesh(SkCanvas&, const Mesh&, const glm::mat4&, const camera::Camera&,
                SkSize, const render::MeshStyle&) const override {
    ++meshes;
  }
  void drawPanel(SkCanvas&, const glm::mat4&, const camera::Camera&, SkSize,
                 const std::function<void(SkCanvas&)>&) const override {
    ++panels;
  }
};

}  // namespace

TEST(Runtime, BuiltInIsOneValue) {
  EXPECT_TRUE((bool)render::Runtime::cpu());
  EXPECT_EQ(render::Runtime::cpu(), render::Runtime::cpu());
  EXPECT_EQ(render::MeshStyle{}.runtime, render::Runtime::cpu());
}

TEST(Runtime, ComparesByModelValue) {
  const render::Runtime a{Recorder{"a"}};
  const render::Runtime b{Recorder{"a"}};
  const render::Runtime c{Recorder{"c"}};
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_NE(a, render::Runtime::cpu());
  EXPECT_NE(render::Runtime(), a);
}

// The style is the whole of the switch: the same call, the same
// geometry, a different executor.
TEST(Runtime, StyleRoutesTheDrawToItsExecutor) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 48));
  surface->getCanvas()->clear(SK_ColorBLACK);

  render::MeshStyle style;
  style.runtime = Recorder{"counting"};
  render::drawMesh(*surface->getCanvas(), quad(20, 20), glm::mat4(1.0f), {},
                   {64, 48}, style);

  const auto* recorder = static_cast<const Recorder*>(style.runtime.get());
  ASSERT_NE(recorder, nullptr);
  EXPECT_EQ(recorder->meshes, 1);

  // Nothing reached the canvas: the substituted executor drew nowhere.
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
  EXPECT_EQ(bm.getColor(32, 24), SK_ColorBLACK);
}

TEST(Runtime, PanelRoutesToTheRuntimeItIsGiven) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 48));
  const render::Runtime runtime{Recorder{"panels"}};
  int drawn = 0;
  render::drawPanel(
      *surface->getCanvas(), glm::mat4(1.0f), {}, {64, 48},
      [&](SkCanvas&) { ++drawn; }, runtime);
  EXPECT_EQ(drawn, 0);
  EXPECT_EQ(static_cast<const Recorder*>(runtime.get())->panels, 1);
}

// ---- the shading terms this tier evaluates ---------------------------
// The same closed forms a device shader's terms are pinned to, so the
// two transcriptions of one arithmetic cannot part company silently.

TEST(Shading, TheTermsMeetTheirClosedForms) {
  using namespace sigil::geometry::mesh::render;
  EXPECT_NEAR(atan2P(1.0f, 1.0f), std::atan2(1.0f, 1.0f), 2e-3f);
  EXPECT_NEAR(atan2P(-0.3f, 0.7f), std::atan2(-0.3f, 0.7f), 2e-3f);
  EXPECT_NEAR(acosP(0.5f), std::acos(0.5f), 2e-3f);
  EXPECT_NEAR(acosP(-0.9f), std::acos(-0.9f), 2e-3f);

  // u = 0.5 looks along -z; v = 0 is the zenith.
  const glm::vec2 forward = equirectUv({0, 0, -1});
  EXPECT_NEAR(forward.x, 0.5f, 2e-3f);
  EXPECT_NEAR(forward.y, 0.5f, 2e-3f);
  EXPECT_NEAR(equirectUv({0, 1, 0}).y, 0.0f, 2e-3f);

  // A metal's reflectance is its base colour; a dielectric's is four
  // per cent whatever colour it is.
  EXPECT_NEAR(specularColor({0.9f, 0.6f, 0.3f}, 1.0f).y, 0.6f, 1e-5f);
  EXPECT_NEAR(specularColor({0.9f, 0.6f, 0.3f}, 0.0f).y, 0.04f, 1e-5f);
  // Fresnel head on is the reflectance itself.
  EXPECT_NEAR(fresnelRough(glm::vec3(0.04f), 1.0f, 0.0f).x, 0.04f, 1e-5f);
  EXPECT_NEAR(fresnelRough(glm::vec3(0.04f), 0.0f, 0.0f).x, 1.0f, 1e-5f);

  // The split sum at its one exact point: a mirror head on returns the
  // radiance it was handed.
  EXPECT_NEAR(environmentSpecular(glm::vec3(1.0f), glm::vec3(1.0f), 0.0f, 1.0f).x,
              1.0f, 3e-3f);
  // A rough surface takes less of it than a smooth one.
  EXPECT_LT(environmentSpecular(glm::vec3(1.0f), glm::vec3(0.04f), 1.0f, 1.0f).x,
            environmentSpecular(glm::vec3(1.0f), glm::vec3(0.04f), 0.0f, 1.0f).x);

  // Beer-Lambert over half a unit of a medium that takes one per unit.
  EXPECT_NEAR(attenuate(glm::vec3(1.0f), glm::vec3(1.0f), 0.5f).x,
              std::exp(-0.5f), 1e-5f);
  // Refraction through no change of index is the ray it was given.
  EXPECT_NEAR(refraction({0, 0, -1}, {0, 0, 1}, 1.0f).z, -1.0f, 1e-5f);
  // Past total internal reflection there is none.
  EXPECT_NEAR(glm::length(refraction(glm::normalize(glm::vec3(1, 0, -0.05f)),
                                     {0, 0, 1}, 1.6f)),
              0.0f, 1e-5f);
}

TEST(Shading, AConstantPanoramaAnswersItsConstantFromEveryDirection) {
  using namespace sigil::geometry::mesh::render;
  // A sky of one radiance is the one panorama whose reading is known
  // without integrating it, on either tier.
  const int w = 32, h = 16;
  std::vector<float> px((size_t)w * h * 4);
  for (size_t i = 0; i < (size_t)w * h; ++i) {
    px[i * 4 + 0] = 0.25f;
    px[i * 4 + 1] = 0.5f;
    px[i * 4 + 2] = 0.75f;
    px[i * 4 + 3] = 1.0f;
  }
  const SkImageInfo info =
      SkImageInfo::Make(w, h, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkImage> flat = SkImages::RasterFromPixmapCopy(
      {info, px.data(), (size_t)w * 4 * sizeof(float)});
  ASSERT_TRUE(flat);

  Environment sky;
  sky.levels = {flat, flat};
  sky.irradiance = flat;
  EXPECT_TRUE(sky.valid());
  for (glm::vec3 direction : {glm::vec3{0, 1, 0}, glm::vec3{0, 0, -1},
                              glm::vec3{0.6f, -0.5f, 0.4f}}) {
    const glm::vec3 mirrored =
        environmentRadiance(sky, glm::normalize(direction), 0.3f);
    EXPECT_NEAR(mirrored.x, 0.25f, 1e-4f);
    EXPECT_NEAR(mirrored.z, 0.75f, 1e-4f);
    const glm::vec3 received =
        environmentIrradiance(sky, glm::normalize(direction));
    EXPECT_NEAR(received.y, 0.5f, 1e-4f);
  }

  // The dials scale the two sides apart, which is the whole reason they
  // are two dials.
  sky.specular = 2.0f;
  sky.diffuse = 0.5f;
  EXPECT_NEAR(environmentRadiance(sky, {0, 0, -1}, 0.0f).x, 0.5f, 1e-4f);
  EXPECT_NEAR(environmentIrradiance(sky, {0, 0, -1}).y, 0.25f, 1e-4f);

  // And an environment with nothing in it is no environment: a surface
  // keeps the flat ambient it always had.
  const Environment none;
  EXPECT_FALSE(none.valid());
  EXPECT_EQ(environmentRadiance(none, {0, 0, -1}, 0).x, 0.0f);
}

TEST(Shading, TheBackdropPutsTheZENITHAtTheTOP) {
  using namespace sigil::geometry::mesh::render;
  // The one sky whose reading is legible from a single pixel: two
  // hemispheres, two colours, a hard horizon. A tier that read the
  // panorama's second axis the wrong way round would put the ground
  // where the zenith is, and nothing about a smooth sky would show it.
  const int w = 64, h = 32;
  std::vector<float> px((size_t)w * h * 4);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      float* t = &px[((size_t)y * w + x) * 4];
      t[0] = y < h / 2 ? 0.9f : 0.1f;
      t[1] = 0.1f;
      t[2] = y < h / 2 ? 0.1f : 0.9f;
      t[3] = 1.0f;
    }
  const SkImageInfo info =
      SkImageInfo::Make(w, h, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkImage> panorama = SkImages::RasterFromPixmapCopy(
      {info, px.data(), (size_t)w * 4 * sizeof(float)});
  ASSERT_TRUE(panorama);

  Environment sky;
  sky.levels = {panorama};
  sky.irradiance = panorama;
  sky.backdrop = 1.0f;

  sigil::geometry::mesh::camera::Camera camera;
  camera.eye = {0, 0, 200};
  camera.target = {0, 0, 0};
  const SkSize viewport = SkSize::Make(120, 120);
  SkBitmap plate;
  plate.allocPixels(SkImageInfo::MakeN32Premul(120, 120));
  SkCanvas canvas(plate);
  canvas.clear(SK_ColorBLACK);
  drawBackdrop(canvas, sky, camera.projection(1.0f), camera.view(), viewport);

  const SkColor4f top = plate.getColor4f(60, 8);
  const SkColor4f bottom = plate.getColor4f(60, 112);
  EXPECT_GT(top.fR, top.fB) << "the zenith is red and it belongs at the top";
  EXPECT_GT(bottom.fB, bottom.fR)
      << "the nadir is blue and it belongs at the bottom";

  // The strength is also the switch: a set that carries a panorama and
  // shows none of it leaves the canvas as it was.
  sky.backdrop = 0.0f;
  canvas.clear(SK_ColorBLACK);
  drawBackdrop(canvas, sky, camera.projection(1.0f), camera.view(), viewport);
  EXPECT_EQ(plate.getColor4f(60, 8).fR, 0.0f);
}

namespace {

/** Two hemispheres, two colours, a hard horizon: red above, blue
 *  below. */
sk_sp<SkImage> horizonPanorama() {
  const int w = 64, h = 32;
  std::vector<float> px((size_t)w * h * 4);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      float* t = &px[((size_t)y * w + x) * 4];
      t[0] = y < h / 2 ? 0.9f : 0.1f;
      t[1] = 0.1f;
      t[2] = y < h / 2 ? 0.1f : 0.9f;
      t[3] = 1.0f;
    }
  const SkImageInfo info =
      SkImageInfo::Make(w, h, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  return SkImages::RasterFromPixmapCopy(
      {info, px.data(), (size_t)w * 4 * sizeof(float)});
}

/** The row the horizon lands on down the middle of a backdrop drawn for
 *  an eye at @p height looking level along -z, or -1 where no row turns
 *  from red to blue. */
int horizonRow(const sigil::geometry::mesh::render::Environment& sky,
               float height) {
  sigil::geometry::mesh::camera::Camera camera;
  camera.eye = {0, height, 0};
  camera.target = {0, height, -100};
  const int side = 120;
  const SkSize viewport = SkSize::Make((float)side, (float)side);
  SkBitmap plate;
  plate.allocPixels(SkImageInfo::MakeN32Premul(side, side));
  SkCanvas canvas(plate);
  canvas.clear(SK_ColorBLACK);
  sigil::geometry::mesh::render::drawBackdrop(
      canvas, sky, camera.projection(1.0f), camera.view(), viewport);
  for (int y = 0; y < side; ++y) {
    const SkColor4f c = plate.getColor4f(side / 2, y);
    if (c.fB > c.fR) return y;
  }
  return -1;
}

}  // namespace

TEST(Shading, AGroundProjectedBackdropMovesTheHorizonWithTheEye) {
  using namespace sigil::geometry::mesh::render;
  // At infinity a sky is the same picture from everywhere, which is the
  // one thing ground projection exists to change: an eye that rises
  // above the sphere's centre sees more ground and the horizon drops.
  Environment sky;
  sky.levels = {horizonPanorama()};
  sky.irradiance = sky.levels.front();
  sky.backdrop = 1.0f;

  const int atInfinityLow = horizonRow(sky, 0.0f);
  const int atInfinityHigh = horizonRow(sky, 20.0f);
  ASSERT_GE(atInfinityLow, 0);
  EXPECT_EQ(atInfinityLow, atInfinityHigh)
      << "a sky at infinity does not move with the eye";

  sky.groundRadius = 100.0f;
  const int projectedCentre = horizonRow(sky, 0.0f);
  const int projectedHigh = horizonRow(sky, 20.0f);
  EXPECT_EQ(projectedCentre, atInfinityLow)
      << "an eye at the centre reads the sphere by direction";
  EXPECT_GT(projectedHigh, projectedCentre + 4)
      << "an eye above the centre looks down on the horizon";

  // The remap itself, at its two ends: by direction at the centre and
  // wherever there is no sphere, and through the exit point otherwise
  // — a fifth of the radius up, a level ray leaves the sphere a fifth
  // of the way up it.
  const glm::vec3 level{0, 0, -1};
  EXPECT_EQ(backdropRay(sky, {0, 0, 0}, level), level);
  Environment flat = sky;
  flat.groundRadius = 0;
  EXPECT_EQ(backdropRay(flat, {0, 20, 0}, level), level);
  const glm::vec3 exit = backdropRay(sky, {0, 20, 0}, level);
  EXPECT_NEAR(exit.y, 0.2f, 1e-4f);
  EXPECT_NEAR(exit.z, -std::sqrt(1.0f - 0.2f * 0.2f), 1e-4f);
  EXPECT_NEAR(glm::length(exit), 1.0f, 1e-4f);
  // …and an eye outside the sphere has no inside to project onto.
  EXPECT_EQ(backdropRay(sky, {0, 140, 0}, level), level);
}

TEST(Shading, APanoramaReadAfterAnotherWasReleasedReadsItsOwnTexels) {
  using namespace sigil::geometry::mesh::render;
  // The kept read outlives the image it came from, so an entry keyed on
  // the sk_sp's address lets the allocator hand the next panorama that
  // address and answer it with the previous sky's texels. Same
  // dimensions, alternating radiance, each one released before the next
  // is made: the recycling an address key cannot see.
  const int w = 8, h = 4;
  const SkImageInfo info =
      SkImageInfo::Make(w, h, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  for (int round = 0; round < 64; ++round) {
    const float radiance = (round % 2) ? 0.875f : 0.125f;
    std::vector<float> px((size_t)w * h * 4, 1.0f);
    for (size_t i = 0; i < (size_t)w * h; ++i) {
      px[i * 4 + 0] = radiance;
      px[i * 4 + 1] = radiance;
      px[i * 4 + 2] = radiance;
    }
    const sk_sp<SkImage> sky = SkImages::RasterFromPixmapCopy(
        {info, px.data(), (size_t)w * 4 * sizeof(float)});
    ASSERT_TRUE(sky);
    EXPECT_NEAR(samplePanorama(sky, {0.5f, 0.5f}).x, radiance, 1e-4f)
        << "round " << round;
  }
}
