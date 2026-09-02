/** @file
 * The 2D session: what a sketch declares, what a host reads back, and
 * what stepping it does.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cstring>
#include <memory>

#include "Support.h"

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;

using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;

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

}  // namespace
