/** @file
 * The immediate-mode session: what a sketch declares, what a host reads
 * back, what stepping it does, and what the canvas keeps between frames.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilsketch/draw/Draw.h>

#include <memory>
#include <string>
#include <vector>

#include "support/Fixtures.h"
#include "support/Sessions.h"

namespace {

using namespace sigil::sketch;
using namespace sigil::draw;

using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;

/** A sketch that declares a canvas, draws a red square during setup, and
 *  every frame drops one more red dot to the right of the last — a trail
 *  no frame clears — while recording what the pen told it. */
struct Trail : DrawSketch {
  static inline int draws = 0;
  static inline double lastMillis = 0.0;
  static inline float lastMouseX = 0.0f;
  static inline bool lastPressed = false;
  static inline int presses = 0;
  static inline int releases = 0;
  static inline std::vector<float> randoms;

  void setup(DrawContext& ctx) override {
    ctx.canvas(200, 120);
    ctx.background(0);
    ctx.captureAt(2.0);
    ctx.pen.noStroke();
    ctx.pen.fill(255, 0, 0);
    ctx.pen.rect(0, 0, 10, 10);
  }
  void draw(DrawContext& ctx) override {
    Pen& pen = ctx.pen;
    ++draws;
    lastMillis = pen.millis();
    lastMouseX = pen.mouseX;
    lastPressed = pen.mouseIsPressed;
    randoms.push_back(pen.random());
    pen.circle(pen.frameCount * 20.0f, 60, 12);
  }
  void mousePressed(Pen&) override { ++presses; }
  void mouseReleased(Pen&) override { ++releases; }
};

/** What every case here opens: the session over that sketch, and a
 *  raster surface at exactly the canvas it declared. */
class DrawSession : public ::testing::Test {
 protected:
  DrawSession()
      : session(kindOf<Trail>()->open(fonts(), assets())),
        surface(SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 120))) {
    Trail::draws = 0;
    Trail::presses = 0;
    Trail::releases = 0;
    Trail::randoms.clear();
  }

  SkCanvas& canvas() { return *surface->getCanvas(); }

  SkColor pixel(int x, int y) {
    return sigil::sketch::test::plateOf(*surface).getColor(x, y);
  }

  std::unique_ptr<Session> session;
  sk_sp<SkSurface> surface;
};

TEST_F(DrawSession, StepsAtTheRateItIsGivenAndTheClockIsStepped) {
  // Setup draws through the pen and not through draw(), so the body's
  // own loop has not run at all until a frame arrives — and then it runs
  // once per frame, off a clock stepped by the host rather than read.
  EXPECT_EQ(Trail::draws, 0);
  for (int i = 0; i < 6; ++i) session->frame(canvas(), 1.0 / 60.0);
  EXPECT_EQ(Trail::draws, 6);
  EXPECT_NEAR(Trail::lastMillis, 100.0, 1e-6);
}

TEST_F(DrawSession, TheCanvasKeepsWhatEarlierFramesDrew) {
  session->frame(canvas(), 1.0 / 60.0);
  session->frame(canvas(), 1.0 / 60.0);
  session->frame(canvas(), 1.0 / 60.0);
  // The first frame's dot at x = 20 is still there after the third.
  EXPECT_EQ(pixel(20, 60), SK_ColorRED);
  EXPECT_EQ(pixel(60, 60), SK_ColorRED);
  EXPECT_EQ(pixel(100, 60), SK_ColorBLACK);
}

TEST_F(DrawSession, ChangingPresentationScaleKeepsWhatEarlierFramesDrew) {
  session->frame(canvas(), 1.0 / 60.0);

  sk_sp<SkSurface> zoomed =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 240));
  zoomed->getCanvas()->scale(2, 2);
  session->frame(*zoomed->getCanvas(), 1.0 / 60.0);

  SkBitmap zoomedPixels;
  zoomedPixels.allocPixels(zoomed->imageInfo());
  ASSERT_TRUE(zoomed->readPixels(zoomedPixels.pixmap(), 0, 0));
  EXPECT_EQ(zoomedPixels.getColor(40, 120), SK_ColorRED);

  canvas().clear(SK_ColorWHITE);
  session->frame(canvas(), 1.0 / 60.0);
  EXPECT_EQ(pixel(20, 60), SK_ColorRED);
}

TEST_F(DrawSession, SetupsDrawingLandsOnTheFirstFrame) {
  session->frame(canvas(), 1.0 / 60.0);
  EXPECT_EQ(pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(pixel(150, 100), SK_ColorBLACK);  // the declared ground
}

TEST_F(DrawSession, AStillIsTheKeptCanvasAndAdvancesNothing) {
  // This runtime's plate IS the surface earlier frames drew onto, so a
  // still presents it rather than describing anything again — every dot
  // the trail has laid is in it and the body does not run.
  session->frame(canvas(), 1.0 / 60.0);
  session->frame(canvas(), 1.0 / 60.0);
  const int stepped = Trail::draws;
  canvas().clear(SK_ColorWHITE);
  session->still(canvas());
  EXPECT_EQ(pixel(20, 60), SK_ColorRED);
  EXPECT_EQ(pixel(40, 60), SK_ColorRED);
  EXPECT_EQ(Trail::draws, stepped);
}

TEST_F(DrawSession, ThePointerReachesThePenAndItsEdgesAreEvents) {
  session->pointer(30, 40, true);
  session->frame(canvas(), 1.0 / 60.0);
  EXPECT_FLOAT_EQ(Trail::lastMouseX, 30.0f);
  EXPECT_TRUE(Trail::lastPressed);
  EXPECT_EQ(Trail::presses, 1);
  session->pointer(31, 40, false);
  session->frame(canvas(), 1.0 / 60.0);
  EXPECT_FALSE(Trail::lastPressed);
  EXPECT_EQ(Trail::releases, 1);
  session->frame(canvas(), 1.0 / 60.0);
  EXPECT_EQ(Trail::presses, 1);
}

TEST_F(DrawSession, RandomIsSeededPerSession) {
  for (int i = 0; i < 4; ++i) session->frame(canvas(), 1.0 / 60.0);
  const std::vector<float> first = Trail::randoms;
  Trail::randoms.clear();
  std::unique_ptr<Session> again = kindOf<Trail>()->open(fonts(), assets());
  for (int i = 0; i < 4; ++i) again->frame(canvas(), 1.0 / 60.0);
  EXPECT_EQ(first, Trail::randoms);
}

/** A sketch that stops its own loop on the first draw. */
struct Once : DrawSketch {
  static inline int draws = 0;
  void setup(DrawContext& ctx) override { ctx.canvas(50, 50); }
  void draw(DrawContext& ctx) override {
    Pen& pen = ctx.pen;
    ++draws;
    pen.noLoop();
  }
};

TEST(DrawSessionLoop, NoLoopStopsTheDrawLoopAndRedrawRunsItOnce) {
  Once::draws = 0;
  std::unique_ptr<Session> session = kindOf<Once>()->open(fonts(), assets());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(50, 50));
  for (int i = 0; i < 5; ++i) session->frame(*surface->getCanvas(), 1.0 / 60.0);
  EXPECT_EQ(Once::draws, 1);
}

/** A sketch asking for half the rate it is stepped at. */
struct Slow : DrawSketch {
  static inline int draws = 0;
  void setup(DrawContext& ctx) override {
    ctx.canvas(50, 50);
    ctx.pen.frameRate(30);
  }
  void draw(DrawContext&) override { ++draws; }
};

TEST(DrawSessionLoop, ARequestedFrameRateSkipsDraws) {
  // The clock is stepped by the host rather than read off a wall, so a
  // second of frames at sixty asking for thirty is an exact count and
  // not a rate that came out about right.
  Slow::draws = 0;
  std::unique_ptr<Session> session = kindOf<Slow>()->open(fonts(), assets());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(50, 50));
  for (int i = 0; i < 60; ++i)
    session->frame(*surface->getCanvas(), 1.0 / 60.0);
  EXPECT_EQ(Slow::draws, 30);
}

/** A sketch whose simulation runs at a fixed rate whatever the host
 *  draws at, and which draws the interpolant the ticker publishes. */
struct Stepped : DrawSketch {
  static inline int steps = 0;
  static inline int setups = 0;
  static inline float lastAlpha = -1.0f;
  choreograph::Output<float> alpha{0.0f};

  void setup(DrawContext& ctx) override {
    ++setups;
    ctx.canvas(50, 50);
    ctx.oversample(3);
    // 32 Hz stepped at 1/64 s: both are powers of two, so the step
    // count and the leftover fraction are exact rather than nearly so.
    ctx.ticker.addFixed(
        32.0,
        [] {
          ++steps;
          return true;
        },
        8, &alpha);
  }
  void draw(DrawContext&) override { lastAlpha = alpha; }
};

class DrawSessionFixedStep : public ::testing::Test {
 protected:
  DrawSessionFixedStep()
      : session(kindOf<Stepped>()->open(fonts(), assets())),
        surface(SkSurfaces::Raster(SkImageInfo::MakeN32Premul(50, 50))) {
    Stepped::steps = 0;
    Stepped::setups = 0;
    Stepped::lastAlpha = -1.0f;
  }
  std::unique_ptr<Session> session;
  sk_sp<SkSurface> surface;
};

TEST_F(DrawSessionFixedStep, TheSimulationRunsAtItsOwnRateWhateverTheDrawRate) {
  for (int i = 0; i < 64; ++i)
    session->frame(*surface->getCanvas(), 1.0 / 64.0);
  EXPECT_EQ(Stepped::steps, 32);  // one second at 32 Hz, drawn at 64
  Stepped::steps = 0;
  std::unique_ptr<Session> slower = kindOf<Stepped>()->open(fonts(), assets());
  for (int i = 0; i < 16; ++i)
    slower->frame(*surface->getCanvas(), 1.0 / 16.0);
  EXPECT_EQ(Stepped::steps, 32);  // the same second, drawn at 16
}

TEST_F(DrawSessionFixedStep, TheRenderInterpolantIsPublished) {
  // Half a step in: the leftover fraction is a half, and one whole step
  // in it is back to none.
  session->frame(*surface->getCanvas(), 1.0 / 64.0);
  EXPECT_EQ(Stepped::steps, 0);
  EXPECT_NEAR(Stepped::lastAlpha, 0.5f, 1e-4f);
  session->frame(*surface->getCanvas(), 1.0 / 64.0);
  EXPECT_EQ(Stepped::steps, 1);
  EXPECT_NEAR(Stepped::lastAlpha, 0.0f, 1e-4f);
}

TEST_F(DrawSessionFixedStep, AFreshSetupStepsTheSimulationOnce) {
  session->redeclare();
  EXPECT_EQ(Stepped::setups, 1);  // the fixture zeroed the first
  for (int i = 0; i < 64; ++i)
    session->frame(*surface->getCanvas(), 1.0 / 64.0);
  EXPECT_EQ(Stepped::steps, 32);
}

TEST_F(DrawSessionFixedStep, TheDeclaredOversampleFormsTheKeptSurface) {
  EXPECT_EQ(session->canvas().oversample, 3);
  session->frame(*surface->getCanvas(), 1.0 / 60.0);
  // Three device pixels per canvas pixel on a canvas that offers one,
  // and the blit back is the declared size, so the plate is 150 across
  // where the host scales by three.
  EXPECT_NE(session->counters().find("150x150"), std::string::npos);
}

/** The immediate-mode session's answers to what every session
 *  promises. */
struct DrawTraits {
  static Kind kind() { return kindOf<Trail>(); }
  static SkSize canvas() { return SkSize::Make(200, 120); }
  static double captureSeconds() { return 2.0; }
  static const char* runtime() { return "draw"; }
  static std::vector<const char*> lanes() { return {"draw", "blit"}; }
  /** The plate is the surface the frames drew onto, so there is nothing
   *  to form again larger. */
  static float oversample() { return 1.0f; }
  static void reset() { Trail::draws = 0; }
  static int bodyRuns() { return Trail::draws; }
};

}  // namespace

INSTANTIATE_TYPED_TEST_SUITE_P(TheDrawSession, SessionContract, DrawTraits);
