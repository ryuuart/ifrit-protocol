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
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilworld/diligent/Painter.h>

#include <cstring>
#include <string>
#include <vector>

#include "DeviceSeams.h"

using namespace sigil;
namespace gm = sigil::geometry::mesh;
namespace {

constexpr SkISize kExtent{160, 120};
constexpr SkSize kViewport{(float)kExtent.width(), (float)kExtent.height()};

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
  geometry::mesh::render::drawMesh(canvas, body(), glm::mat4(1.0f),
                                   world::diligent::raisedEye(), kViewport,
                                   style);
  return bitmap;
}

}  // namespace

TEST(Painter, TheRuntimeIsAValue) {
  const auto on = world::diligent::onPainterDevice();
  if (!on) GTEST_SKIP() << on.error;
  EXPECT_TRUE((bool)on.runtime);
  EXPECT_EQ(on.runtime, geometry::mesh::render::Runtime(on.runtime))
      << "copies of one runtime are one value";
  EXPECT_NE(on.runtime, geometry::mesh::render::Runtime::cpu());
  // Two separate calls hold separate device state, which is what a
  // reconciler asking "did the runtime change" has to be told.
  EXPECT_NE(on.runtime, world::diligent::painterRuntime(*on.device));
}

TEST(Painter, ASurfaceThatIsItsOwnLightIsBrighterThanALitOne) {
  const auto on = world::diligent::onPainterDevice();
  if (!on) GTEST_SKIP() << on.error;

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

  const SkBitmap own = drawnWith(on.runtime, unlit);
  const SkBitmap lit = drawnWith(on.runtime, shaded);
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
  const auto on = world::diligent::onPainterDevice();
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
  geometry::mesh::render::drawPanel(
      hostCanvas, model, world::diligent::raisedEye(), kViewport, content,
      geometry::mesh::render::Runtime::cpu());

  SkBitmap device = plate();
  SkCanvas deviceCanvas(device);
  geometry::mesh::render::drawPanel(deviceCanvas, model,
                                    world::diligent::raisedEye(), kViewport,
                                    content, on.runtime);

  // NOT a tolerance: a panel's content is Skia's to rasterise on either
  // executor, so the two must be the same bytes.
  EXPECT_TRUE(identical(host, device));
}
