/** @file
 * sketch_settled_test — the still of a page is the frame the settle
 * accepted, on a page that never stops repainting. Ultralight allows one
 * Renderer per process, so this is a binary of its own rather than a
 * case beside the shared-engine test.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <sigilscry/engine/WebEngine.h>
#include <sigilscry/engine/WebView.h>
#include <sigilsketch/scry/SettledPage.h>

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

TEST(SketchSettledPage, TheStillIsTheFrameTheSettleAcceptedAndNotALaterOne) {
  const std::shared_ptr<WebEngine> web = engine();
  if (!web) GTEST_SKIP() << "no web engine on this machine";
  std::shared_ptr<WebView> view = web->createView(160, 120);
  ASSERT_NE(view, nullptr);

  const sigil::sketch::scry::Events events(*view);
  view->loadHTML(kRestless);
  ASSERT_TRUE(events.awaitLoad());
  ASSERT_TRUE(sigil::sketch::scry::awaitAnswer(
      *view, events, "document.getElementById('head').textContent",
      "SETTLED"));

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
