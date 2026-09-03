/** @file
 * The 2D session: what a sketch declares, what a host reads back, and
 * what stepping it does.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cstring>
#include <memory>
#include <string>

#include "Support.h"

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;

using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;

/** The pixels of an image, read back. */
SkBitmap pixelsOf(const sk_sp<SkImage>& image) {
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(image->width(), image->height()));
  EXPECT_TRUE(image->readPixels(nullptr, bitmap.pixmap(), 0, 0));
  return bitmap;
}

/** A sketch that asks for a texture scene while declaring itself, paints
 *  one red sheet into it, and then LETS GO of it — keeping the image it
 *  took and a weak look at the scene, so that what stands afterwards is
 *  what the session is holding and nothing the sketch is. */
struct Screening : Sketch {
  static inline sk_sp<SkImage> sheet;
  static inline std::weak_ptr<TextureScene> scene;
  void setup(SketchContext& ctx) override {
    ctx.canvas(200, 120);
    const std::shared_ptr<TextureScene> kept = ctx.textureScene({32, 32});
    kept->render(box().width(32).height(32).fill(Fill::color({1, 0, 0, 1})));
    sheet = kept->image();
    scene = kept;
    ctx.composer.render(box().width(10).height(10));
  }
};

/** A sketch that declares a canvas, draws one box, and counts the times
 *  its body was run. The count is the observable behind the difference
 *  between repainting and stepping. */
struct Declaring : Sketch {
  static inline int updates = 0;
  void setup(SketchContext& ctx) override {
    ctx.canvas(320, 200);
    ctx.background({0.1f, 0.2f, 0.3f, 1});
    ctx.captureAt(2.5);
    ctx.composer.render(
        box().width(100).height(50).fill(Fill::color({1, 0, 0, 1})));
  }
  void update(double, SketchContext&) override { ++updates; }
};

/** What every case here opens: the session over that sketch, and a
 *  raster surface at exactly the canvas it declared. */
class CanvasSession : public ::testing::Test {
 protected:
  CanvasSession()
      : session(kindOf<Declaring>()->open(fonts(), assets())),
        surface(SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320, 200))) {}

  SkCanvas& canvas() { return *surface->getCanvas(); }

  /** What the surface holds now. */
  SkBitmap pixels() {
    SkBitmap bitmap;
    bitmap.allocPixels(surface->imageInfo());
    EXPECT_TRUE(surface->readPixels(bitmap.pixmap(), 0, 0));
    return bitmap;
  }

  std::unique_ptr<Session> session;
  sk_sp<SkSurface> surface;
};

TEST_F(CanvasSession, ReadsTheDeclarationBackAfterSetup) {
  EXPECT_EQ(session->canvas().size, SkSize::Make(320, 200));
  EXPECT_EQ(session->canvas().captureSeconds, 2.5);
  EXPECT_FLOAT_EQ(session->canvas().background.fB, 0.3f);
}

TEST_F(CanvasSession, StepsAtTheRateItIsGiven) {
  // The first stepped frame must advance, not be swallowed seeding the
  // clock: a plate is a function of the number of steps taken, so a
  // frame lost at the start moves every one after it.
  for (int i = 0; i < 6; ++i) session->frame(canvas(), 1.0 / 60.0);
  EXPECT_NEAR(session->timing().totalMs,
              session->timing().updateMs + session->timing().drawMs, 1e-6);
}

TEST_F(CanvasSession, ReportsTheLanesTheRuntimeSpent) {
  session->frame(canvas(), 1.0 / 60.0);
  ASSERT_EQ(session->lanes().size(), 4u);
  EXPECT_STREQ(session->lanes()[0].name, "recon");
  EXPECT_STREQ(session->lanes()[3].name, "paint");
  EXPECT_FALSE(session->counters().empty());
}

TEST_F(CanvasSession, PaintsWhatTheSketchDescribed) {
  canvas().clear(SK_ColorBLACK);
  session->frame(canvas(), 1.0 / 60.0);
  EXPECT_EQ(pixels().getColor(10, 10), SK_ColorRED);
}

TEST_F(CanvasSession, RepaintDoesNotAdvanceButStillDoes) {
  session->frame(canvas(), 1.0 / 60.0);
  session->frame(canvas(), 1.0 / 60.0);
  const int stepped = Declaring::updates;

  // Two repaints from the same ground are the same picture: a repaint
  // draws the state the frames left and runs nothing.
  canvas().clear(SK_ColorBLACK);
  session->repaint(canvas());
  const SkBitmap first = pixels();
  canvas().clear(SK_ColorBLACK);
  session->repaint(canvas());
  const SkBitmap second = pixels();
  EXPECT_EQ(Declaring::updates, stepped);
  ASSERT_EQ(first.computeByteSize(), second.computeByteSize());
  EXPECT_EQ(0, std::memcmp(first.getPixels(), second.getPixels(),
                           first.computeByteSize()));

  // The still takes exactly one more step, which is what makes a plate
  // the declared moment plus one.
  session->still(canvas());
  EXPECT_EQ(Declaring::updates, stepped + 1);
}

TEST_F(CanvasSession, TakesTheStillLargerThanItsCanvas) {
  EXPECT_GT(session->oversample(), 1.0f);
}

TEST(CanvasDoors, PaintsWhatTheSketchHandedTheTextureScene) {
  const std::unique_ptr<Session> session =
      kindOf<Screening>()->open(fonts(), assets());
  ASSERT_TRUE(Screening::sheet);
  EXPECT_EQ(Screening::sheet->width(), 32);
  EXPECT_EQ(Screening::sheet->height(), 32);
  EXPECT_EQ(pixelsOf(Screening::sheet).getColor(16, 16), SK_ColorRED);
}

TEST(CanvasDoors, KeepsTheTextureScenesItHandsOutUntilTheBodyDeclaresAgain) {
  std::unique_ptr<Session> session =
      kindOf<Screening>()->open(fonts(), assets());
  // The sketch dropped its own reference at the end of setup; the scene
  // stands because the SESSION holds it — which is what a picture on a
  // device needs, since a scene destroys the texture its image names
  // when it goes.
  EXPECT_FALSE(Screening::scene.expired());
  EXPECT_NE(session->counters().find("scenes 1"), std::string::npos);

  // Declaring again lets last time's scenes go and keeps this time's.
  const std::weak_ptr<TextureScene> first = Screening::scene;
  session->redeclare();
  EXPECT_TRUE(first.expired());
  EXPECT_FALSE(Screening::scene.expired());
  EXPECT_NE(session->counters().find("scenes 1"), std::string::npos);

  // …and the session going lets go of the last of them.
  session.reset();
  EXPECT_TRUE(Screening::scene.expired());
}

}  // namespace
