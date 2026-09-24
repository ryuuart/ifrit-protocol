/** @file
 * A page under the loading clock: a session opened under
 * `PauseWhileLoading` has its page there before the open is answered —
 * its settle is driven through on the thread that opens it — and a still
 * taken after frames drawn at the wall's pace is, byte for byte, the
 * plate the sweep takes of the same scene, which is the deterministic
 * capture every other route to a picture of a page is held to.
 *
 * It boots the process's one engine, so ctest runs it in a process of
 * its own.
 */

#include <gtest/gtest.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/web/Web.h>
#include <sigilscry/engine/WebEngine.h>
#include <sigilscry/engine/WebView.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/plate/Sweep.h>
#include <sigilsketch/scry/SharedEngine.h>
#include <sigilsketch/scry/Settling.h>
#include <sigilsketch/testing/Comparison.h>
#include <sigilsketch/testing/InProcessHost.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "ScratchDir.h"
#include "support/Fixtures.h"

namespace {

namespace protocol = sigil::protocol;
namespace sketch = sigil::sketch;
using namespace std::chrono_literals;

constexpr int kWidth = 96;
constexpr int kHeight = 64;

/** A static document: it publishes one picture and then stops, so every
 *  frame after it has arrived draws that picture whenever it is asked. */
constexpr const char* kPage = R"(<!doctype html><meta charset="utf-8">
<style>html,body{margin:0;background:#2040ff}
div{margin:12px;width:40px;height:20px;background:#ff8000}</style><div></div>)";

/** Whether the last page this sketch opened had arrived as its setup
 *  returned. */
bool g_arrivedAtOpen = false;

/** A page as the leaf of a scene, settled the way the session's clock
 *  says: driven through for a repeatable run, advanced a frame at a time
 *  under the wall's. */
struct LoadingClockPage {
  std::shared_ptr<sigil::scry::WebView> view;
  /** After the view it settles, so it goes first. */
  std::unique_ptr<sketch::scry::Settling> page;

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.background({0, 0, 0, 1});
    g_arrivedAtOpen = false;
    const std::shared_ptr<sigil::scry::WebEngine> engine =
        sketch::scry::sharedEngine();
    if (engine) {
      view = engine->createView(kWidth, kHeight);
      page = sketch::scry::settle(*view, {.html = kPage}, ctx.deterministic);
      g_arrivedAtOpen = page->arrived();
    }
    describe(ctx);
  }
  void update(double, sketch::SketchContext& ctx) {
    if (page && page->advance()) describe(ctx);
  }
  void describe(sketch::SketchContext& ctx) {
    using namespace sigil::compose;
    ctx.composer.render(
        page && page->painted()
            ? web(view).width(kWidth).height(kHeight)
            : box().width(kWidth).height(kHeight).fill(
                  Fill::color({1, 0, 1, 1})));
  }
};

[[maybe_unused]] const bool kRegistered = sketch::add(
    "loading_clock_page", nullptr, "Test",
    "a static page the loading clock photographs",
    &sketch::kindOf<LoadingClockPage>);

TEST(SketchLoadingClock, APageOpenedUnderItIsTheSweepsDeterministicCapture) {
  ASSERT_TRUE(sketch::scry::configureSharedEngine({}));
  const sigil::test::ScratchDir scratch("sketch-loading-clock");
  {
    // Today's deterministic capture of the scene: the sweep's plate.
    sketch::SweepOptions sweep;
    sweep.outputDirectory = (scratch.path / "sweep").string();
    sweep.only = sketch::find("loading_clock_page");
    sweep.ledger = true;
    ASSERT_GE(sweep.only, 0);
    ASSERT_EQ(
        sketch::sweep(sweep, sketch::test::fonts(), sketch::test::assets()),
        0);
    const std::filesystem::path plate =
        scratch.path / "sweep" / "plate_loading_clock_page.png";
    ASSERT_TRUE(std::filesystem::exists(plate));
    ASSERT_TRUE(g_arrivedAtOpen);
    // The density the sweep chose for this canvas, read off its plate.
    const SkISize plateSize = sketch::testing::compare(plate, plate).actual;
    ASSERT_GT(plateSize.width(), 0);
    const double density = (double)plateSize.width() / kWidth;

    sketch::testing::InProcessHostOptions options;
    options.stateDirectory = scratch.path / "state";
    sketch::testing::InProcessHost host(std::move(options));
    ASSERT_TRUE(host.clock(protocol::clock::Policy_PauseWhileLoading));
    g_arrivedAtOpen = false;
    const auto opened = host.open("loading_clock_page");
    ASSERT_TRUE(opened) << opened.error().message;
    // The page was there before the open was answered, and no frame had
    // been drawn.
    EXPECT_TRUE(g_arrivedAtOpen);
    EXPECT_EQ(host.current().result().frame, 0u);

    const auto until = std::chrono::steady_clock::now() + 200ms;
    while (std::chrono::steady_clock::now() < until) {
      host.frame();
      std::this_thread::sleep_for(5ms);
    }
    EXPECT_GT(host.current().result().seconds, 0.0);
    const auto still = host.still(density, "loading.png");
    ASSERT_TRUE(still) << still.error().message;
    const sketch::testing::Comparison same =
        sketch::testing::compare(still.result().path, plate);
    EXPECT_TRUE(same.identical())
        << same.problem << " differing pixels "
        << same.pixels.differingPixels << ", worst " << same.pixels.worst;
  }
  sketch::scry::shutdownSharedEngine();
}

}  // namespace
