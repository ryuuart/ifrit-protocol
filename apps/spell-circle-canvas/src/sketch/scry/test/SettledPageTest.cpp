/** @file
 * The still of a page is the frame the settle accepted, on a page that
 * never stops repainting — and a page settles the same way on an engine
 * booted after another was shut down, because the renderer under both is
 * the process's. One engine at a time, so these are cases ctest runs in
 * processes of their own.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <sigilscry/engine/WebEngine.h>
#include <sigilscry/engine/WebView.h>
#include <sigilsketch/scry/SettledPage.h>
#include <sigilsketch/scry/SharedEngine.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

namespace {

using sigil::scry::WebEngine;
using sigil::scry::WebEngineConfig;
using sigil::scry::WebView;

/** A page that says once that it has settled and then never stops
 *  changing: the heading is rewritten by the script the settle rule
 *  reads, and the ground walks a new colour every tick after that. A
 *  still of it is a claim about WHICH repaint was photographed. */
constexpr const char* kRestless = R"(<html><body style="margin:0">
<h1 id="head" style="color:#fff;font:20px sans-serif">WAITING</h1>
<script>
document.getElementById('head').textContent = 'SETTLED';
var step = 0;
setInterval(function () {
  step = (step + 7) % 256;
  document.body.style.background = 'rgb(' + step + ',0,0)';
}, 16);
</script></body></html>)";

/** The top-left pixel of @p image, or transparent when it cannot be
 *  read. The ground fills the page, so this is the colour the script
 *  last set in whichever repaint the image came from. */
SkColor corner(const sk_sp<SkImage>& image) {
  if (!image) return SK_ColorTRANSPARENT;
  SkBitmap one;
  if (!one.tryAllocPixels(SkImageInfo::MakeN32Premul(1, 1))) return 0;
  if (!image->readPixels(nullptr, one.pixmap(), 0, 0)) return 0;
  return one.getColor(0, 0);
}

/** The one CPU-mode engine this process boots. */
std::shared_ptr<WebEngine> engine() {
  static std::shared_ptr<WebEngine> held = WebEngine::create(WebEngineConfig{});
  return held;
}

TEST(SketchSettledPage, APageSettlesOnAnEngineBootedAfterOneWasShutDown) {
  // A host may take its web work down and stand it up again: the engine
  // ends, the process's renderer does not. Loading a page on the FIRST
  // engine is what makes this about the defect — the page load starts
  // WebCore's resource-usage thread, and releasing the renderer it reads
  // through is what used to leave that thread on freed memory, killing
  // whatever ran next in the process.
  namespace shared = sigil::sketch::scry;
  ASSERT_TRUE(shared::configureSharedEngine({}));
  {
    const std::shared_ptr<WebEngine> first = shared::sharedEngine();
    if (!first) GTEST_SKIP() << "no web engine on this machine";
    const std::shared_ptr<WebView> page = first->createView(160, 120);
    ASSERT_NE(page, nullptr);
    const shared::Events events(*page);
    page->loadHTML(kRestless);
    ASSERT_TRUE(events.awaitLoad());
  }
  shared::shutdownSharedEngine();

  ASSERT_TRUE(shared::configureSharedEngine({}));
  const std::shared_ptr<WebEngine> again = shared::sharedEngine();
  ASSERT_NE(again, nullptr) << "the renderer was not handed back";
  const std::shared_ptr<WebView> view = again->createView(160, 120);
  ASSERT_NE(view, nullptr);

  const shared::Events events(*view);
  view->loadHTML(kRestless);
  ASSERT_TRUE(events.awaitLoad());
  ASSERT_TRUE(shared::awaitAnswer(
      *view, events, "document.getElementById('head').textContent", "SETTLED"));
  const WebView::Frame still = events.accepted();
  ASSERT_TRUE(still.image) << "the second engine published no frame";
  EXPECT_NE(corner(still.image), SK_ColorTRANSPARENT);
  // The path lets go here; the engine itself ends with the handles below,
  // which is what leaves the process free to boot the next one.
  shared::shutdownSharedEngine();
}

TEST(SketchSettledPage,
     AStillPageSettlesWithNoFurtherRepaintAndIsPaintedWhole) {
  // The two doors a page that goes QUIET needs, and neither is what a
  // page that keeps moving wants. A document with nothing animating
  // stops handing frames over the moment it is there, so a settle that
  // waited for the next repaint before it looked would wait for
  // something that is not coming. And the still has to be a painting of
  // the WHOLE page: the engine paints what a change damaged and copies
  // the rest, so a page that was driven otherwise carries the seams of
  // however that driving was broken into steps.
  const std::shared_ptr<WebEngine> web = WebEngine::create(WebEngineConfig{});
  if (!web) GTEST_SKIP() << "no web engine on this machine";
  const std::shared_ptr<WebView> view = web->createView(120, 90);
  ASSERT_NE(view, nullptr);

  const sigil::sketch::scry::Events events(*view);
  view->loadHTML(
      "<html><body style='margin:0;background:#0000ff'></body></html>");
  ASSERT_TRUE(events.awaitLoad());
  ASSERT_TRUE(sigil::sketch::scry::awaitQuiet(
      *view, events, "String(document.readyState)", "complete"))
      << "the settle waited for a repaint this page was never going to make";
  const WebView::Frame arrived = events.accepted();
  ASSERT_TRUE(arrived.image);

  ASSERT_TRUE(sigil::sketch::scry::repaintWhole(*view, events))
      << "nothing was painted when the whole page was asked for";
  const WebView::Frame whole = events.accepted();
  ASSERT_TRUE(whole.image);
  EXPECT_GT(whole.version, arrived.version)
      << "the still is the frame the page arrived on, not a fresh painting";
  EXPECT_EQ(corner(whole.image), SK_ColorBLUE);
}

TEST(SketchSettledPage, TheQuietRuleStopsOnTheSamePassHoweverSlowlyEventsCome) {
  namespace shared = sigil::sketch::scry;
  // THE RULE A SETTLE ENDS ON, FED THE SAME ENGINE EVENTS TWICE: once as
  // fast as this thread can deliver them, and once with every event held
  // back so the whole run takes orders of magnitude longer. A rule made
  // of elapsed time would call the page still somewhere in the second
  // run's waiting; this one reads what the engine DID, so both stop on
  // the same pass.
  const auto firstQuietPass = [](std::chrono::microseconds between) {
    uint64_t repaints = 0;
    uint64_t passesSinceRepaint = 0;
    const int painting = 5;  // passes with a repaint in each
    const int bound = painting + 4 * static_cast<int>(shared::kQuietPasses);
    for (int pass = 1; pass <= bound; ++pass) {
      if (pass <= painting) {
        ++repaints;
        passesSinceRepaint = 0;
      } else {
        ++passesSinceRepaint;
      }
      std::this_thread::sleep_for(between);
      if (shared::goneQuiet(repaints, passesSinceRepaint)) return pass;
    }
    return 0;
  };

  const int hurried = firstQuietPass(std::chrono::microseconds(0));
  const int held = firstQuietPass(std::chrono::microseconds(500));
  EXPECT_EQ(hurried, held)
      << "the same events in the same order ended the settle on different "
         "passes, so something other than the events decided it";
  EXPECT_EQ(hurried, 5 + static_cast<int>(shared::kQuietPasses))
      << "the window is counted from the last repaint";
  // …and a page that has published nothing at all is one that has not
  // finished rather than one at rest, however many passes have gone by.
  EXPECT_FALSE(shared::goneQuiet(0, 4 * shared::kQuietPasses));
}

TEST(SketchSettledPage, TheStillIsTheFrameTheSettleAcceptedAndNotALaterOne) {
  const std::shared_ptr<WebEngine> web = engine();
  if (!web) GTEST_SKIP() << "no web engine on this machine";
  std::shared_ptr<WebView> view = web->createView(160, 120);
  ASSERT_NE(view, nullptr);

  const sigil::sketch::scry::Events events(*view);
  view->loadHTML(kRestless);
  ASSERT_TRUE(events.awaitLoad());
  ASSERT_TRUE(sigil::sketch::scry::awaitAnswer(
      *view, events, "document.getElementById('head').textContent", "SETTLED"));

  const WebView::Frame still = events.accepted();
  ASSERT_TRUE(still.image) << "a CPU engine hands the frame over as an image";
  const SkColor photographed = corner(still.image);

  // The page goes on repainting. Every frame after the settle is a later
  // document, and none of them may become the still.
  bool viewMovedOn = false;
  for (int tick = 0; tick < 24; ++tick) {
    ASSERT_TRUE(events.awaitRepaint(events.repaints()));
    if (corner(view->frame().image) != photographed) viewMovedOn = true;
  }

  EXPECT_EQ(events.accepted().version, still.version);
  EXPECT_EQ(events.accepted().image.get(), still.image.get());
  EXPECT_EQ(corner(still.image), photographed);
  EXPECT_GT(view->frameVersion(), still.version);
  EXPECT_TRUE(viewMovedOn)
      << "the page never repainted differently, so nothing was proved";
}

}  // namespace
