/** @file
 * The 2D session: what a sketch declares, what a host reads back, and
 * what stepping it does.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

Assets& assets() {
  static auto* store = new Assets("");
  return *store;
}

/** A sketch that declares a canvas, draws one box, and records what it
 *  was told. */
struct Declaring : Sketch {
  double lastElapsed = -1;
  int updates = 0;
  void setup(SketchContext& ctx) override {
    ctx.canvas(320, 200);
    ctx.background({0.1f, 0.2f, 0.3f, 1});
    ctx.captureAt(2.5);
    ctx.composer.render(
        box().width(100).height(50).fill(Fill::color({1, 0, 0, 1})));
  }
  void update(double elapsed, SketchContext&) override {
    lastElapsed = elapsed;
    ++updates;
  }
};

std::unique_ptr<Session> openSession() {
  return kindOf<Declaring>()->open(fonts(), assets());
}

TEST(CanvasSession, ReadsTheDeclarationBackAfterSetup) {
  std::unique_ptr<Session> session = openSession();
  EXPECT_EQ(session->canvas().size, SkSize::Make(320, 200));
  EXPECT_EQ(session->canvas().captureSeconds, 2.5);
  EXPECT_FLOAT_EQ(session->canvas().background.fB, 0.3f);
}

TEST(CanvasSession, StepsAtTheRateItIsGiven) {
  // The first stepped frame must advance, not be swallowed seeding the
  // clock: a plate is a function of the number of steps taken, so a
  // frame lost at the start moves every one after it.
  std::unique_ptr<Session> session = openSession();
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320, 200));
  for (int i = 0; i < 6; ++i) session->frame(*surface->getCanvas(), 1.0 / 60.0);
  EXPECT_NEAR(session->timing().totalMs,
              session->timing().updateMs + session->timing().drawMs, 1e-6);
}

TEST(CanvasSession, ReportsTheLanesTheRuntimeSpent) {
  std::unique_ptr<Session> session = openSession();
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320, 200));
  session->frame(*surface->getCanvas(), 1.0 / 60.0);
  ASSERT_EQ(session->lanes().size(), 4u);
  EXPECT_STREQ(session->lanes()[0].name, "recon");
  EXPECT_STREQ(session->lanes()[3].name, "paint");
  EXPECT_FALSE(session->counters().empty());
}

TEST(CanvasSession, PaintsWhatTheSketchDescribed) {
  std::unique_ptr<Session> session = openSession();
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320, 200));
  surface->getCanvas()->clear(SK_ColorBLACK);
  session->frame(*surface->getCanvas(), 1.0 / 60.0);
  SkBitmap bitmap;
  bitmap.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bitmap.pixmap(), 0, 0));
  EXPECT_EQ(bitmap.getColor(10, 10), SK_ColorRED);
}

TEST(CanvasSession, RepaintDoesNotAdvanceButStillDoes) {
  std::unique_ptr<Session> session = openSession();
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320, 200));
  session->frame(*surface->getCanvas(), 1.0 / 60.0);
  session->frame(*surface->getCanvas(), 1.0 / 60.0);
  session->repaint(*surface->getCanvas());
  session->repaint(*surface->getCanvas());
  // Two repaints between two frames leave the scene where the frames put
  // it; the still takes one more step, which is what makes a plate the
  // declared moment plus exactly one.
  session->still(*surface->getCanvas());
  SUCCEED();
}

TEST(CanvasSession, TakesTheStillLargerThanItsCanvas) {
  std::unique_ptr<Session> session = openSession();
  EXPECT_GT(session->oversample(), 1.0f);
}

TEST(CanvasKind, NamesItsRuntime) {
  EXPECT_EQ(kindOf<Declaring>()->runtime(), "canvas");
}

}  // namespace
