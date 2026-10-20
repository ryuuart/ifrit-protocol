/** @file
 * The live host's stills and captures: the moment a sketch declared, the
 * clock a capture steps, a still at the moment asked for, the scale and
 * size a capture is written at, and the readbacks and timings refused.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Factories.h>
#include <sigildraw/Pen.h>
#include <sigilsketch/live/Host.h>

#include <cstring>
#include <filesystem>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "HostTestSupport.h"
#include "support/Fixtures.h"

namespace {

using namespace sigil::sketch;
using sigil::sketch::test::fonts;
using sigil::sketch::test::options;
using sigil::sketch::test::Watched;

TEST(SketchHost, ReportsTheMomentTheSketchDeclared) {
  // A capture with no moment named on the command line steps to this
  // number, so a still is the frame the author chose rather than the
  // one a default happened to reach.
  const Watched file("sigil_sketch_host_moment");
  Host host(options(file.path), fonts());
  EXPECT_EQ(host.captureSeconds(), sigil::sketch::test::Square::kMoment);
}

TEST(SketchHost, WritesACaptureAtTheScaleItIsAsked) {
  const Watched file("sigil_sketch_host_capture");
  Host host(options(file.path), fonts());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 90));
  host.frame(*surface->getCanvas(), 1.0 / 60.0);
  const std::filesystem::path out = file.dir.path / "capture.png";
  EXPECT_TRUE(host.capture(out, 2.0f));
  EXPECT_TRUE(std::filesystem::exists(out));
}

TEST(SketchHost, TheStillIsPixelsAndTheCaptureIsThePng) {
  // THE SPLIT THE THUMBNAIL STORE LIVES IN. The repaint and the readback
  // need the thread the frames are drawn on; the encode and the write do
  // not, so `still` hands back pixels and stops there.
  const Watched file("sigil_sketch_host_still");
  Host host(options(file.path), fonts());
  const auto entriesIn = [](const std::filesystem::path& dir) {
    return static_cast<size_t>(
        std::distance(std::filesystem::directory_iterator(dir),
                      std::filesystem::directory_iterator{}));
  };
  const size_t before = entriesIn(file.dir.path);

  const SkBitmap still = host.still(2.0f);
  ASSERT_FALSE(still.isNull());
  EXPECT_EQ(still.dimensions(), SkISize::Make(240, 180));
  EXPECT_EQ(still.getColor(5, 5), SK_ColorGREEN);
  // Past the square is the ground the sketch declared, opaque black, laid
  // by the host: the session paints only what the body described.
  EXPECT_EQ(still.getColor(230, 170), SK_ColorBLACK);
  // The pixels are safe to hand to another thread, which is what the
  // thumbnail store does with them.
  EXPECT_TRUE(still.isImmutable());
  // And the half that moved off the drawing thread did not happen here:
  // a still encodes nothing and writes nothing.
  EXPECT_EQ(entriesIn(file.dir.path), before);

  // The old contract is unchanged: capture writes the PNG.
  const std::filesystem::path out = file.dir.path / "still.png";
  EXPECT_TRUE(host.capture(out, 2.0f));
  EXPECT_TRUE(std::filesystem::exists(out));
  EXPECT_EQ(entriesIn(file.dir.path), before + 1);
}

TEST(SketchHost, StillRefusesFailedReadback) {
  const Watched file("sigil_sketch_still_readback");
  Host host(options(file.path), fonts());
  Host::CaptureBackend backend;
  backend.readback = [](SkSurface&, const SkPixmap&) { return false; };
  host.setCaptureBackend(std::move(backend));
  EXPECT_TRUE(host.still().isNull());
}

struct CaptureClock {
  static inline std::vector<double> updates;
  static inline bool resize = false;
  static inline bool paintDiscarded = true;
  static inline int paints = 0;
  void setup(Scene2DContext& ctx) {
    ctx.canvas(120, 90);
    ctx.captureAt(0);
    ctx.paintDiscardedFrames(paintDiscarded);
    ctx.composer.render(sigil::compose::custom("capture-clock",
                                               [](sigil::draw::Pen& pen) {
                                                 ++paints;
                                                 pen.canvas()->clear(
                                                     SK_ColorGREEN);
                                               })
                            .cache(sigil::compose::Cache::None)
                            .width(120)
                            .height(90));
  }
  void update(double elapsed, Scene2DContext& ctx) {
    updates.push_back(elapsed);
    if (resize) ctx.canvas(150.5f, 100.25f);
  }
};
Kind captureClockKind() { return kindOf<CaptureClock>(); }
const Entry kCaptureClock{"capture_clock", "capture_clock", "Test", "",
                          &captureClockKind};

TEST(SketchHost,
     OptedInCaptureStepsUpdateWithoutPaintingButObservedFramesDraw) {
  const Watched file("sigil_sketch_capture_without_paint");
  auto opts = options(file.path);
  opts.compiledIn = &kCaptureClock;
  CaptureClock::paintDiscarded = false;
  Host host(std::move(opts), fonts());
  CaptureClock::paintDiscarded = true;
  CaptureClock::updates.clear();
  CaptureClock::paints = 0;

  EXPECT_EQ(host.prepareCapture(), 0);
  ASSERT_EQ(CaptureClock::updates.size(), 1u);
  EXPECT_EQ(CaptureClock::updates.back(), 0);
  EXPECT_EQ(CaptureClock::paints, 0);
  EXPECT_EQ(host.prepareCapture(.025), .025);
  ASSERT_EQ(CaptureClock::updates.size(), 3u);
  EXPECT_DOUBLE_EQ(CaptureClock::updates.back(), .025);
  EXPECT_EQ(CaptureClock::paints, 0);
  EXPECT_FALSE(host.frameTimes().work().empty());
  EXPECT_EQ(host.drawTimes().mean(), sigil::motion::Duration::zero());

  const auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 90));
  ASSERT_TRUE(host.frame(*surface->getCanvas(), 1.0 / 60.0));
  EXPECT_EQ(CaptureClock::paints, 1);
  ASSERT_TRUE(host.frame(1.0 / 60.0));
  EXPECT_EQ(CaptureClock::paints, 1);
  EXPECT_EQ(host.session()->timing().drawMilliseconds, 0);

  EXPECT_FALSE(host.photograph().isNull());
  EXPECT_EQ(CaptureClock::paints, 2);
  EXPECT_NEAR(CaptureClock::updates.back(), .025 + 2.0 / 60.0, 1e-9);
  const size_t updates = CaptureClock::updates.size();
  EXPECT_FALSE(host.still().isNull());
  EXPECT_EQ(CaptureClock::paints, 3);
  EXPECT_EQ(CaptureClock::updates.size(), updates);
}

TEST(SketchHost, CaptureMomentIncludesZeroAndTheFractionalStep) {
  const Watched file("sigil_sketch_capture_clock");
  auto opts = options(file.path);
  opts.compiledIn = &kCaptureClock;
  Host host(std::move(opts), fonts());
  CaptureClock::updates.clear();
  EXPECT_EQ(host.prepareCapture(), 0);
  ASSERT_EQ(CaptureClock::updates.size(), 1u);
  EXPECT_EQ(CaptureClock::updates.back(), 0);

  ASSERT_TRUE(host.restartSession());
  CaptureClock::updates.clear();
  EXPECT_EQ(host.prepareCapture(0.025), 0.025);
  ASSERT_EQ(CaptureClock::updates.size(), 2u);
  EXPECT_DOUBLE_EQ(CaptureClock::updates.front(), 1.0 / 60.0);
  EXPECT_DOUBLE_EQ(CaptureClock::updates.back(), 0.025);
}

/** An edge a hundred canvas units a second to the right, and the time
 *  every update read: what a still shows and the time it says it shows,
 *  side by side. */
struct MovingEdge {
  static inline std::vector<double> updates;

  void setup(Scene2DContext& ctx) {
    ctx.canvas(64, 8);
    ctx.background({0, 0, 0, 1});
  }
  void update(double elapsed, Scene2DContext& ctx) {
    updates.push_back(elapsed);
    ctx.composer.render(sigil::compose::box()
                            .width(4)
                            .height(8)
                            .inset(0, 0, 0, float(elapsed * 100.0))
                            .fill(sigil::compose::Fill::color({1, 0, 0, 1})));
  }
};
Kind movingEdgeKind() { return kindOf<MovingEdge>(); }
const Entry kMovingEdge{"moving_edge", "moving_edge", "Test", "",
                        &movingEdgeKind};

TEST(SketchHost, AStillShowsTheMomentAskedForAndTakingItAgainMovesNothing) {
  const Watched file("sigil_sketch_capture_exact_time");
  auto opts = options(file.path);
  opts.compiledIn = &kMovingEdge;
  Host host(std::move(opts), fonts());
  MovingEdge::updates.clear();
  EXPECT_EQ(host.prepareCapture(0.25), 0.25);
  const SkBitmap first = host.photograph();
  ASSERT_FALSE(first.isNull()) << host.errorLog();
  // The still drew once more, at the moment itself.
  ASSERT_FALSE(MovingEdge::updates.empty());
  EXPECT_NEAR(MovingEdge::updates.back(), 0.25, 1e-9);
  // The edge stands at 25 canvas units: covered from there for four.
  EXPECT_EQ(first.getColor(24, 4), SK_ColorBLACK);
  EXPECT_EQ(first.getColor(26, 4), SK_ColorRED);
  EXPECT_EQ(first.getColor(29, 4), SK_ColorBLACK);

  const SkBitmap second = host.photograph();
  ASSERT_FALSE(second.isNull());
  EXPECT_NEAR(MovingEdge::updates.back(), 0.25, 1e-9);
  ASSERT_EQ(first.computeByteSize(), second.computeByteSize());
  EXPECT_EQ(std::memcmp(first.getPixels(), second.getPixels(),
                        first.computeByteSize()),
            0);
}

TEST(SketchHost, ReloadStartsElapsedTimeAtZero) {
  const Watched file("sigil_sketch_host_reload_clock");
  auto opts = options(file.path);
  opts.compiledIn = &kCaptureClock;
  Host host(std::move(opts), fonts());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 90));
  CaptureClock::updates.clear();
  for (int frame = 0; frame < 6; ++frame)
    ASSERT_TRUE(host.frame(*surface->getCanvas(), 1.0 / 60.0));
  const double ran = CaptureClock::updates.back();
  EXPECT_DOUBLE_EQ(ran, 6.0 / 60.0);

  // A RELOAD OPENS A FRESH SESSION, WHICH OWNS A FRESH CLOCK, so the
  // first frame after it is one step from zero rather than one step
  // from where the session before it stood — which is what makes the
  // entrance being edited play again on every save.
  ASSERT_TRUE(host.restartSession());
  CaptureClock::updates.clear();
  ASSERT_TRUE(host.frame(*surface->getCanvas(), 1.0 / 60.0));
  ASSERT_EQ(CaptureClock::updates.size(), 1u);
  EXPECT_DOUBLE_EQ(CaptureClock::updates.front(), 1.0 / 60.0);
  EXPECT_LT(CaptureClock::updates.front(), ran);
}

TEST(SketchHost, InvalidCaptureTimingDoesNotRunTheSketch) {
  const Watched file("sigil_sketch_capture_invalid_timing");
  auto opts = options(file.path);
  opts.compiledIn = &kCaptureClock;
  Host host(std::move(opts), fonts());
  CaptureClock::updates.clear();
  const double infinity = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  for (double at : {-1.0, infinity, nan, 1e20})
    EXPECT_THROW(host.prepareCapture(at), std::invalid_argument);
  for (double fps : {0.0, -1.0, infinity, nan, 1e-320})
    EXPECT_THROW(host.prepareCapture(0, fps), std::invalid_argument);
  EXPECT_TRUE(CaptureClock::updates.empty());
}

TEST(SketchHost, SlowCaptureRateDoesNotDiscardSceneTime) {
  const Watched file("sigil_sketch_capture_slow_rate");
  auto opts = options(file.path);
  opts.compiledIn = &kCaptureClock;
  Host host(std::move(opts), fonts());
  CaptureClock::updates.clear();
  EXPECT_EQ(host.prepareCapture(1.125, 1), 1.125);
  ASSERT_FALSE(CaptureClock::updates.empty());
  EXPECT_DOUBLE_EQ(CaptureClock::updates.back(), 1.125);
  EXPECT_EQ(host.prepareCapture(1, 1), 1);
  EXPECT_DOUBLE_EQ(CaptureClock::updates.back(), 2.125);
}

TEST(SketchHost, CaptureUsesResizedCanvasAndRoundsPixelExtentUp) {
  const Watched file("sigil_sketch_capture_resized");
  auto opts = options(file.path);
  opts.compiledIn = &kCaptureClock;
  Host host(std::move(opts), fonts());
  CaptureClock::resize = true;
  EXPECT_NO_THROW(host.prepareCapture(0));
  CaptureClock::resize = false;
  EXPECT_EQ(host.canvasSize(), SkSize::Make(150.5f, 100.25f));
  SkISize captured{};
  Host::CaptureBackend backend;
  backend.makeSurface = [&](const SkImageInfo& info) {
    captured = info.dimensions();
    return SkSurfaces::Raster(info);
  };
  host.setCaptureBackend(std::move(backend));
  EXPECT_TRUE(host.capture(file.dir.path / "resized.png", 0.5f));
  EXPECT_EQ(captured, SkISize::Make(76, 51));
  for (float scale :
       {0.0f, -1.0f, 1000.0f, std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()}) {
    const auto output = file.dir.path / "invalid.png";
    EXPECT_FALSE(host.capture(output, scale));
    EXPECT_FALSE(std::filesystem::exists(output));
  }
}

TEST(SketchHost, CaptureRefusesFailedReadback) {
  const Watched file("sigil_sketch_capture_readback");
  Host host(options(file.path), fonts());
  Host::CaptureBackend backend;
  backend.readback = [](SkSurface&, const SkPixmap&) { return false; };
  host.setCaptureBackend(std::move(backend));
  const auto output = file.dir.path / "failed.png";
  EXPECT_FALSE(host.capture(output));
  EXPECT_FALSE(std::filesystem::exists(output));
}

}  // namespace
