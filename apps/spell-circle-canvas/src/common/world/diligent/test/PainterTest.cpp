/** @file
 * The mesh painter on the device, held against the one on the host.
 *
 * TWO RASTERISERS ARE NOT ASKED TO AGREE BIT FOR BIT. The host paints
 * shaded vertices through a triangle sort with Skia's antialiasing; the
 * device rasterises the same shading through a depth buffer with none.
 * What is asserted is a per-channel distance — the mean, which says the
 * two are the same picture, and the 99th percentile, which says the
 * disagreement is confined to the edges where two rasterisers always
 * differ.
 *
 * A PANEL IS NOT ONE OF THOSE. Both executors concat the same transform
 * and hand the canvas to the caller, so the pixels are the same pixels
 * and the test says exactly that.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilworld/diligent/Device.h>
#include <sigilworld/diligent/Painter.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace sigil;
namespace gm = sigil::geometry::mesh;
namespace render = sigil::geometry::mesh::render;

namespace {

constexpr SkISize kExtent{160, 120};
constexpr SkSize kViewport{(float)kExtent.width(), (float)kExtent.height()};

/** A DEVICE AND THE PAINTER ON IT, or the reason there is neither. Every
 *  test that needs one SKIPS rather than fails without a Vulkan runtime,
 *  so a machine with no GPU stays green. */
struct OnDevice {
  std::unique_ptr<world::diligent::Device> device;
  render::Runtime painter;
  std::string error;
  explicit operator bool() const { return (bool)device; }
};

OnDevice onDevice() {
  OnDevice out;
  const world::diligent::DeviceConfig config;
  out.device = world::diligent::Device::create(config, &out.error);
  if (out.device) out.painter = world::diligent::painterRuntime(*out.device);
  return out;
}

gm::camera::Camera eye() {
  gm::camera::Camera camera;
  camera.eye = {0, 90, 240};
  camera.target = {0, 0, 0};
  return camera;
}

/** HOW FAR TWO PLATES STAND APART, per colour channel in 0..255. */
struct Distance {
  double mean = 0;
  int p99 = 0;
  int max = 0;
};

Distance distanceOf(const SkBitmap& a, const SkBitmap& b) {
  Distance out;
  if (a.width() != b.width() || a.height() != b.height()) {
    out.max = 255;
    return out;
  }
  std::vector<int> histogram(256, 0);
  double total = 0;
  size_t count = 0;
  for (int y = 0; y < a.height(); ++y)
    for (int x = 0; x < a.width(); ++x) {
      const SkColor4f left = a.getColor4f(x, y);
      const SkColor4f right = b.getColor4f(x, y);
      const float channels[4][2] = {{left.fR, right.fR},
                                    {left.fG, right.fG},
                                    {left.fB, right.fB},
                                    {left.fA, right.fA}};
      for (const auto& pair : channels) {
        const int diff = (int)std::lround(std::abs(pair[0] - pair[1]) * 255.0f);
        ++histogram[(size_t)std::clamp(diff, 0, 255)];
        total += diff;
        ++count;
        out.max = std::max(out.max, diff);
      }
    }
  out.mean = count ? total / (double)count : 0.0;
  const size_t cut = (size_t)((double)count * 0.99);
  size_t seen = 0;
  for (int value = 0; value < 256; ++value) {
    seen += (size_t)histogram[(size_t)value];
    if (seen >= cut) {
      out.p99 = value;
      break;
    }
  }
  return out;
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

render::MeshStyle litStyle() {
  render::MeshStyle style;
  style.baseColor = {0.8f, 0.6f, 0.3f, 1.0f};
  style.lights = {
      render::Light{{-0.4f, -0.8f, -0.4f}, SkColors::kWhite, 1.0f},
      render::Light{
          {0.6f, -0.2f, 0.5f}, SkColor4f{0.4f, 0.6f, 1.0f, 1.0f}, 0.6f}};
  return style;
}

SkBitmap drawnWith(const render::Runtime& runtime, render::MeshStyle style) {
  style.runtime = runtime;
  SkBitmap bitmap = plate();
  SkCanvas canvas(bitmap);
  render::drawMesh(canvas, body(), glm::mat4(1.0f), eye(), kViewport, style);
  return bitmap;
}

}  // namespace

TEST(Painter, TheRuntimeIsAValue) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;
  EXPECT_TRUE((bool)on.painter);
  EXPECT_EQ(on.painter, render::Runtime(on.painter))
      << "copies of one runtime are one value";
  EXPECT_NE(on.painter, render::Runtime::cpu());
  // Two separate calls hold separate device state, which is what a
  // reconciler asking "did the runtime change" has to be told.
  EXPECT_NE(on.painter, world::diligent::painterRuntime(*on.device));
}

TEST(Painter, ALitMeshLandsWhereTheHostPutsIt) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;

  const SkBitmap host = drawnWith(render::Runtime::cpu(), litStyle());
  const SkBitmap device = drawnWith(on.painter, litStyle());
  const Distance distance = distanceOf(host, device);
  // THE TOLERANCE, and what each half of it is for. The mean says the
  // two are the same picture; the 99th says the disagreement is
  // confined. The worst channel is an edge — the host antialiases and
  // the device does not — and is reported rather than judged.
  EXPECT_LT(distance.mean, 1.0) << "mean channel distance";
  EXPECT_LE(distance.p99, 4) << "99th percentile channel distance";
  std::cerr << "[ lit mesh ] mean " << distance.mean << " p99 " << distance.p99
            << " max " << distance.max << "\n";
}

TEST(Painter, TheNormalBufferAgreesToo) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;

  render::MeshStyle style = litStyle();
  style.mode = render::MeshStyle::Mode::Normals;
  const SkBitmap host = drawnWith(render::Runtime::cpu(), style);
  const SkBitmap device = drawnWith(on.painter, style);
  const Distance distance = distanceOf(host, device);
  EXPECT_LT(distance.mean, 1.0) << "mean channel distance";
  EXPECT_LE(distance.p99, 4) << "99th percentile channel distance";
  std::cerr << "[ normals ] mean " << distance.mean << " p99 " << distance.p99
            << " max " << distance.max << "\n";
}

TEST(Painter, ASurfaceThatIsItsOwnLightIsBrighterThanALitOne) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;

  render::MeshStyle unlit = litStyle();
  unlit.lit = false;
  // A sun aimed away leaves a lit body at its ambient; an unlit one
  // stands at its base colour whatever the emitters do. The device must
  // read the same field of the style the host does.
  render::MeshStyle shaded = litStyle();
  shaded.lights = {render::Light{{0, 0, 1}, SkColors::kWhite, 1.0f}};
  shaded.specular = 0;
  shaded.rim = 0;

  const SkBitmap own = drawnWith(on.painter, unlit);
  const SkBitmap lit = drawnWith(on.painter, shaded);
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
  EXPECT_GT(brightness(drawnWith(render::Runtime::cpu(), unlit)),
            brightness(drawnWith(render::Runtime::cpu(), shaded)));
}

TEST(Painter, APanelIsTheSamePixelsOnBothExecutors) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;

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
  render::drawPanel(hostCanvas, model, eye(), kViewport, content,
                    render::Runtime::cpu());

  SkBitmap device = plate();
  SkCanvas deviceCanvas(device);
  render::drawPanel(deviceCanvas, model, eye(), kViewport, content, on.painter);

  // NOT a tolerance: a panel's content is Skia's to rasterise on either
  // executor, so the two must be the same bytes.
  EXPECT_TRUE(identical(host, device));
}
