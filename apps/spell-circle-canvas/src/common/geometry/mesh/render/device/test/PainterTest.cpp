/** @file
 * The mesh painter on the device: the runtime as a value, the style's
 * own answers read the same way on either executor, and the one thing
 * the two are the same BYTES about.
 *
 * A PANEL IS THAT ONE THING. Both executors concat the same transform
 * and hand the canvas to the caller, so the pixels are the same pixels
 * and the test says exactly that. Everything a rasteriser decides —
 * where an edge falls, how it is antialiased — is a picture two
 * rasterisers do not agree about bit for bit, and how far apart they
 * stand is judged against a committed baseline by the plate ledger's
 * device tier rather than here.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilgeometry/mesh/render/device/Painter.h>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "OnDevice.h"

using namespace sigil;
namespace gm = sigil::geometry::mesh;
namespace {

constexpr SkISize kExtent{160, 120};
constexpr SkSize kViewport{(float)kExtent.width(), (float)kExtent.height()};

/** LOOKING DOWN ON A BODY from above and in front, which is where the
 *  shading has something to say across it. */
gm::camera::Camera raisedEye() {
  gm::camera::Camera camera;
  camera.eye = {0, 90, 240};
  camera.target = {0, 0, 0};
  return camera;
}

SkBitmap plate() {
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(kExtent.width(), kExtent.height()));
  bitmap.eraseColor(SK_ColorBLACK);
  return bitmap;
}

bool identical(const SkBitmap& a, const SkBitmap& b) {
  if (a.width() != b.width() || a.height() != b.height()) return false;
  for (int y = 0; y < a.height(); ++y)
    if (std::memcmp(a.getAddr32(0, y), b.getAddr32(0, y),
                    (size_t)a.width() * 4) != 0)
      return false;
  return true;
}

/** A body with enough curvature for the shading to say something, and
 *  a uv lane for a texture to land on. */
gm::Mesh body() { return gm::superellipsoid({50, 50, 50}, 2.0f, 28, 20); }

geometry::mesh::render::MeshStyle litStyle() {
  geometry::mesh::render::MeshStyle style;
  style.baseColor = {0.8f, 0.6f, 0.3f, 1.0f};
  style.lights = {
      geometry::mesh::render::Light{
          {-0.4f, -0.8f, -0.4f}, SkColors::kWhite, 1.0f},
      geometry::mesh::render::Light{
          {0.6f, -0.2f, 0.5f}, SkColor4f{0.4f, 0.6f, 1.0f, 1.0f}, 0.6f}};
  return style;
}

SkBitmap drawnWith(const geometry::mesh::render::Runtime& runtime,
                   geometry::mesh::render::MeshStyle style) {
  style.runtime = runtime;
  SkBitmap bitmap = plate();
  SkCanvas canvas(bitmap);
  geometry::mesh::render::drawMesh(canvas, body(), glm::mat4(1.0f), raisedEye(),
                                   kViewport, style);
  return bitmap;
}

}  // namespace

TEST(Painter, TheRuntimeIsAValue) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  const geometry::mesh::render::Runtime runtime =
      geometry::mesh::render::deviceRuntime(*on);
  EXPECT_TRUE((bool)runtime);
  EXPECT_EQ(runtime, geometry::mesh::render::Runtime(runtime))
      << "copies of one runtime are one value";
  EXPECT_NE(runtime, geometry::mesh::render::Runtime::cpu());
  // Two separate calls hold separate device state, which is what a
  // reconciler asking "did the runtime change" has to be told.
  EXPECT_NE(runtime, geometry::mesh::render::deviceRuntime(*on));
}

TEST(Painter, ASurfaceThatIsItsOwnLightIsBrighterThanALitOne) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  const geometry::mesh::render::Runtime runtime =
      geometry::mesh::render::deviceRuntime(*on);

  geometry::mesh::render::MeshStyle unlit = litStyle();
  unlit.lit = false;
  // A sun aimed away leaves a lit body at its ambient; an unlit one
  // stands at its base colour whatever the emitters do. The device must
  // read the same field of the style the host does.
  geometry::mesh::render::MeshStyle shaded = litStyle();
  shaded.lights = {
      geometry::mesh::render::Light{{0, 0, 1}, SkColors::kWhite, 1.0f}};
  shaded.specular = 0;
  shaded.rim = 0;

  const SkBitmap own = drawnWith(runtime, unlit);
  const SkBitmap lit = drawnWith(runtime, shaded);
  const auto brightness = [](const SkBitmap& b) {
    double total = 0;
    for (int y = 0; y < b.height(); ++y)
      for (int x = 0; x < b.width(); ++x) {
        const SkColor4f c = b.getColor4f(x, y);
        total += c.fR + c.fG + c.fB;
      }
    return total;
  };
  EXPECT_GT(brightness(own), brightness(lit));
  // …and the host says the same, which is what makes it the style's
  // answer rather than this executor's.
  EXPECT_GT(
      brightness(drawnWith(geometry::mesh::render::Runtime::cpu(), unlit)),
      brightness(drawnWith(geometry::mesh::render::Runtime::cpu(), shaded)));
}

TEST(Painter, APanelIsTheSamePixelsOnBothExecutors) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  const geometry::mesh::render::Runtime runtime =
      geometry::mesh::render::deviceRuntime(*on);

  const auto content = [](SkCanvas& canvas) {
    SkPaint paint;
    paint.setColor(SK_ColorMAGENTA);
    paint.setAntiAlias(true);
    canvas.drawRect(SkRect::MakeXYWH(-40, -30, 80, 60), paint);
    paint.setColor(SK_ColorCYAN);
    canvas.drawCircle(0, 0, 18, paint);
  };
  const glm::mat4 model =
      gm::camera::place({0, 10, 0}, /*yawDeg=*/25.0f, /*pitchDeg=*/-12.0f);

  SkBitmap host = plate();
  SkCanvas hostCanvas(host);
  geometry::mesh::render::drawPanel(hostCanvas, model, raisedEye(), kViewport,
                                    content,
                                    geometry::mesh::render::Runtime::cpu());

  SkBitmap device = plate();
  SkCanvas deviceCanvas(device);
  geometry::mesh::render::drawPanel(deviceCanvas, model, raisedEye(), kViewport,
                                    content, runtime);

  // NOT a tolerance: a panel's content is Skia's to rasterise on either
  // executor, so the two must be the same bytes.
  EXPECT_TRUE(identical(host, device));
}

namespace {

/** One level of a panorama: a sky that is bright overhead and dim below,
 *  warm on one side and cool on the other, so a normal turned anywhere
 *  reads a different colour and a reflection has something to say. Every
 *  level is half the one before it, which is what a prefiltered chain
 *  is — one texture the device reads a fractional level of, and a list
 *  the host reads two of and mixes. */
sk_sp<SkImage> panoramaLevel(int width, int height, float scale) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(width, height, kRGBA_F32_SkColorType,
                                       kPremul_SkAlphaType));
  for (int y = 0; y < height; ++y) {
    const float v = ((float)y + 0.5f) / (float)height;
    for (int x = 0; x < width; ++x) {
      const float u = ((float)x + 0.5f) / (float)width;
      const float sky = (1.0f - v) * 2.0f * scale;
      float* texel = (float*)bitmap.getAddr(x, y);
      texel[0] = sky * (0.4f + 0.6f * u);
      texel[1] = sky * 0.7f;
      texel[2] = sky * (1.0f - 0.6f * u);
      texel[3] = 1.0f;
    }
  }
  bitmap.setImmutable();
  return bitmap.asImage();
}

/** A sky as both executors take it: a chain of halving levels and the
 *  cosine convolution beside it. */
geometry::mesh::render::Environment sky() {
  geometry::mesh::render::Environment out;
  out.levels = {panoramaLevel(64, 32, 1.0f), panoramaLevel(32, 16, 0.9f),
                panoramaLevel(16, 8, 0.8f), panoramaLevel(8, 4, 0.7f)};
  out.irradiance = panoramaLevel(16, 8, 0.6f);
  return out;
}

/** How far apart two pictures stand, as the mean absolute difference per
 *  channel over every pixel — the measure a shading disagreement shows
 *  up in and a silhouette barely does. */
double meanChannelDistance(const SkBitmap& a, const SkBitmap& b) {
  double total = 0;
  for (int y = 0; y < a.height(); ++y)
    for (int x = 0; x < a.width(); ++x) {
      const SkColor4f p = a.getColor4f(x, y), q = b.getColor4f(x, y);
      total +=
          std::abs(p.fR - q.fR) + std::abs(p.fG - q.fG) + std::abs(p.fB - q.fB);
    }
  return total / (double)(a.width() * a.height() * 3);
}

double meanBrightness(const SkBitmap& b) {
  double total = 0;
  for (int y = 0; y < b.height(); ++y)
    for (int x = 0; x < b.width(); ++x) {
      const SkColor4f c = b.getColor4f(x, y);
      total += c.fR + c.fG + c.fB;
    }
  return total / (double)(b.width() * b.height() * 3);
}

}  // namespace

TEST(Painter, TheSkyAndTheMetalSplitReadTheSameOnBothExecutors) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  const geometry::mesh::render::Runtime device =
      geometry::mesh::render::deviceRuntime(*on);
  const geometry::mesh::render::Runtime host =
      geometry::mesh::render::Runtime::cpu();

  geometry::mesh::render::MeshStyle flat = litStyle();
  geometry::mesh::render::MeshStyle skied = litStyle();
  skied.environment = sky();

  // A SKY CHANGES THE PICTURE, and by about as much on either executor.
  // The environment is the ambient a surface receives and the radiance
  // it mirrors, so a body under one cannot look like a body under a flat
  // constant.
  const double hostLift = meanBrightness(drawnWith(host, skied)) -
                          meanBrightness(drawnWith(host, flat));
  const double deviceLift = meanBrightness(drawnWith(device, skied)) -
                            meanBrightness(drawnWith(device, flat));
  EXPECT_GT(hostLift, 0.01);
  EXPECT_GT(deviceLift, 0.01);
  EXPECT_NEAR(deviceLift, hostLift, 0.05)
      << "host " << hostLift << ", device " << deviceLift;

  // …and the two pictures themselves stand within a shading distance
  // rather than a silhouette's. Not bit identity: the host sorts and
  // antialiases where this depth-tests and does not, and it reads the
  // panorama's texels itself where this reads them through a sampler.
  EXPECT_LT(
      meanChannelDistance(drawnWith(host, skied), drawnWith(device, skied)),
      0.06);

  // THE METAL SPLIT: light stops reaching the diffuse and the highlight
  // takes the surface's own colour, so a metal under this sky is a
  // different picture from a dielectric — on both executors, in the same
  // direction.
  geometry::mesh::render::MeshStyle metal = skied;
  metal.metallic = 1.0f;
  metal.roughness = 0.15f;
  EXPECT_GT(meanChannelDistance(drawnWith(host, skied), drawnWith(host, metal)),
            0.01);
  EXPECT_GT(
      meanChannelDistance(drawnWith(device, skied), drawnWith(device, metal)),
      0.01);
  EXPECT_LT(
      meanChannelDistance(drawnWith(host, metal), drawnWith(device, metal)),
      0.06);

  // ROUGHNESS PICKS A LEVEL of the chain on both, so a mirror and a
  // rough metal are two pictures and the two executors agree which is
  // which.
  geometry::mesh::render::MeshStyle rough = metal;
  rough.roughness = 0.95f;
  EXPECT_GT(meanChannelDistance(drawnWith(host, metal), drawnWith(host, rough)),
            0.005);
  EXPECT_LT(
      meanChannelDistance(drawnWith(host, rough), drawnWith(device, rough)),
      0.06);
}
