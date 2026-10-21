/** @file
 * Drawing with no frame open on the process's device: a stack's layers
 * and its composite stand on the process's own painter, a pen on that
 * device puts the composite down as it stands and draws the same pixels
 * as the drawing made by that pen itself, a stack drawn on another
 * thread is put down on the frame's thread the same way, a pen on raster
 * is handed a copy read back, and a Compose tree baked into a layer is
 * painted on the device too.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Hostless.h>
#include <sigildraw/Pen.h>
#include <sigildraw/Stack.h>
#include <sigilsketch/core/Painting.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/Readback.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <thread>

namespace {

namespace draw = sigil::draw;
namespace sketch = sigil::sketch;

constexpr int kSide = 100;

/** The process's host-less device installed for one case, and raster
 *  again after it. */
class SketchPainting : public ::testing::Test {
 protected:
  void SetUp() override {
    painter = sketch::ownPainter();
    if (!painter.graphite) GTEST_SKIP() << "no device of the process's own";
    draw::useHostlessDevice(&sketch::hostlessPainting());
  }
  void TearDown() override { draw::useHostlessDevice(nullptr); }

  /** A surface on the painter's device, cleared. */
  sk_sp<SkSurface> deviceSurface() const {
    sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
        painter.graphite->recorder(),
        SkImageInfo::MakeN32Premul(kSide, kSide));
    if (surface) surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    return surface;
  }

  /** Every pixel of @p surface, read back from the device. */
  SkBitmap pixelsOf(SkSurface& surface) const {
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(kSide, kSide));
    EXPECT_TRUE(
        sigil::skia::readbackPixels(*painter.graphite, surface, bitmap.pixmap()));
    return bitmap;
  }

  sketch::Painter painter;
};

/** Edges at every angle, more than one colour and a translucent fill, so
 *  a picture drawn or blended differently cannot match by accident. */
void drawSubject(draw::Pen& pen) {
  pen.noStroke();
  pen.fill(240, 90, 40);
  pen.circle(40, 45, 50);
  pen.stroke(20, 60, 200);
  pen.strokeWeight(5);
  pen.fill(40, 200, 120, 160);
  pen.triangle(55, 15, 90, 80, 30, 85);
  pen.line(5, 95, 95, 5);
}

int differingPixels(const SkBitmap& first, const SkBitmap& second) {
  int differing = 0;
  for (int y = 0; y < first.height(); ++y)
    for (int x = 0; x < first.width(); ++x)
      if (first.getColor(x, y) != second.getColor(x, y)) ++differing;
  return differing;
}

draw::Frame frame() {
  draw::Frame canvas;
  canvas.width = kSide;
  canvas.height = kSide;
  return canvas;
}

TEST_F(SketchPainting, AStackDrawnWithNoFrameIsPutDownOnTheDeviceAsItStands) {
  draw::Stack stack{kSide, kSide};
  sk_sp<SkImage> composite;
  {
    draw::HostlessPen host(1.0f, kSide, kSide, nullptr);
    draw::Layer& subject = stack.layer("subject");
    drawSubject(subject.begin(host.pen()));
    subject.end();
    composite = stack.image(host.pen());
  }
  ASSERT_TRUE(composite);
  EXPECT_TRUE(stack.layer("subject").image()->isTextureBacked());
  EXPECT_TRUE(composite->isTextureBacked());

  // A pen on the same device is handed the composite itself: nothing is
  // read back.
  sk_sp<SkSurface> stacked = deviceSurface();
  ASSERT_TRUE(stacked);
  draw::Pen onDevice;
  onDevice.begin(*stacked->getCanvas(), frame());
  const sk_sp<SkImage> handed =
      draw::HostlessPen::drawableOn(onDevice, composite);
  EXPECT_EQ(handed.get(), composite.get());
  onDevice.image(handed, 0, 0, kSide, kSide);
  onDevice.end();

  // The same drawing made by the device pen itself, every pixel.
  sk_sp<SkSurface> direct = deviceSurface();
  ASSERT_TRUE(direct);
  draw::Pen drawing;
  drawing.begin(*direct->getCanvas(), frame());
  drawSubject(drawing);
  drawing.end();
  const SkBitmap expected = pixelsOf(*direct);
  EXPECT_EQ(differingPixels(pixelsOf(*stacked), expected), 0);

  // A pen on raster is handed a copy read back, with the same pixels.
  sk_sp<SkSurface> paper =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSide, kSide));
  draw::Pen onRaster;
  onRaster.begin(*paper->getCanvas(), frame());
  const sk_sp<SkImage> copied =
      draw::HostlessPen::drawableOn(onRaster, composite);
  onRaster.end();
  ASSERT_TRUE(copied);
  EXPECT_FALSE(copied->isTextureBacked());
  SkBitmap read;
  read.allocPixels(SkImageInfo::MakeN32Premul(kSide, kSide));
  ASSERT_TRUE(copied->readPixels(read.pixmap(), 0, 0));
  EXPECT_EQ(differingPixels(read, expected), 0);
}

TEST_F(SketchPainting, AStackDrawnOnAnotherThreadIsPutDownAsItStands) {
  // The frame's recorder is the context's own, on this thread; the stack
  // is drawn on a thread of its own, through a recorder of that thread's.
  draw::Stack stack{kSide, kSide};
  sk_sp<SkImage> composite;
  std::thread drawing([&] {
    draw::HostlessPen host(1.0f, kSide, kSide, nullptr);
    drawSubject(stack.layer("subject").begin(host.pen()));
    stack.layer("subject").end();
    composite = stack.image(host.pen());
  });
  drawing.join();
  ASSERT_TRUE(composite);
  ASSERT_TRUE(composite->isTextureBacked());

  sk_sp<SkSurface> stacked = deviceSurface();
  ASSERT_TRUE(stacked);
  draw::Pen onDevice;
  onDevice.begin(*stacked->getCanvas(), frame());
  EXPECT_TRUE(draw::HostlessPen::drawnAsItStands(onDevice, *composite));
  onDevice.image(composite, 0, 0, kSide, kSide);
  onDevice.end();

  sk_sp<SkSurface> direct = deviceSurface();
  ASSERT_TRUE(direct);
  draw::Pen pen;
  pen.begin(*direct->getCanvas(), frame());
  drawSubject(pen);
  pen.end();
  EXPECT_EQ(differingPixels(pixelsOf(*stacked), pixelsOf(*direct)), 0);
}

TEST_F(SketchPainting, WithNoDeviceInstalledAHostlessPenDrawsOnRaster) {
  draw::useHostlessDevice(nullptr);
  draw::Stack stack{kSide, kSide};
  draw::HostlessPen host(1.0f, kSide, kSide, nullptr);
  drawSubject(stack.layer("subject").begin(host.pen()));
  stack.layer("subject").end();
  const sk_sp<SkImage> composite = stack.image(host.pen());
  ASSERT_TRUE(composite);
  EXPECT_FALSE(composite->isTextureBacked());
  EXPECT_EQ(draw::HostlessPen::rasterImage(composite).get(), composite.get());
}

TEST_F(SketchPainting, AComposeTreeBakedIntoALayerIsPaintedOnTheDevice) {
  sigil::weave::FontContext fonts(sigil::weave::ports::systemFontManager());
  draw::Stack stack{60, 40};
  draw::Layer& baked = stack.layer("baked");
  {
    draw::HostlessPen host(1.0f, 60, 40, &fonts);
    baked.bake(host.pen(),
               sigil::compose::box().width(30).height(20).fill(
                   sigil::compose::Fill::color({0.2f, 0.4f, 1.0f, 1.0f})),
               {60, 40});
  }
  ASSERT_TRUE(baked.image());
  EXPECT_TRUE(baked.image()->isTextureBacked());
  const sk_sp<SkImage> read = draw::HostlessPen::rasterImage(baked.image());
  ASSERT_TRUE(read);
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(60, 40));
  ASSERT_TRUE(read->readPixels(pixels.pixmap(), 0, 0));
  EXPECT_EQ(pixels.getColor(10, 10), SkColorSetARGB(255, 0x33, 0x66, 0xFF));
  EXPECT_EQ(SkColorGetA(pixels.getColor(50, 30)), 0u);
}

}  // namespace
