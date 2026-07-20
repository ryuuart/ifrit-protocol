// SigilCompose × SigilScry integration (stress item 19): a live WebView
// frame as a compose leaf, and a Composer drawn into a page-facing
// canvas. Runs only when the Ultralight SDK is present (this target is
// gated on TARGET SigilScry).

#include <sigilcompose/Compose.h>
#include <sigilcompose/Web.h>

#include <sigilscry/WebEngine.h>
#include <sigilscry/WebView.h>

#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace sigil::compose;

namespace {

sigil::weave::FontContext &fonts() {
  static auto *context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

sigil::scry::WebEngine &sharedEngine() {
  static std::shared_ptr<sigil::scry::WebEngine> engine =
      sigil::scry::WebEngine::create({});
  EXPECT_NE(engine, nullptr);
  return *engine;
}

bool waitForFrame(sigil::scry::WebView &view, uint64_t sinceVersion) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (std::chrono::steady_clock::now() < deadline) {
    if (view.frameVersion() > sinceVersion)
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}

} // namespace

TEST(ComposeWeb, WebLeafDrawsPublishedFrame) {
  auto view = sharedEngine().createView(100, 100, {.transparent = false});
  ASSERT_NE(view, nullptr);
  view->loadHTML(
      "<html><body style='background:#00ff00;margin:0'></body></html>");
  ASSERT_TRUE(waitForFrame(*view, 0));

  sigil::motion::Ticker ticker;
  Composer composer(ticker, fonts());
  composer.setSize({200, 200});
  composer.render(box().padding(50).fill(Fill::color({1, 0, 0, 1}))
                      .child(web(view).grow(1)));

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 200));
  // The page may publish intermediate blank frames before styling
  // lands; redraw until the leaf shows green (bounded by the deadline).
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(10);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  SkColor center = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    surface->getCanvas()->clear(SK_ColorBLACK);
    composer.draw(*surface->getCanvas());
    surface->readPixels(bm.pixmap(), 100, 100);
    center = bm.getColor(0, 0);
    if (center == SK_ColorGREEN)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  EXPECT_EQ(center, SK_ColorGREEN); // web frame inside the box
  surface->readPixels(bm.pixmap(), 10, 10);
  EXPECT_EQ(bm.getColor(0, 0), SK_ColorRED); // compose chrome around it
}

TEST(ComposeWeb, ComposerDrawsIntoPageFacingCanvas) {
  // The reverse direction: a Composer is a guest in any canvas — here a
  // raster surface standing in for a WebImage::paint() callback canvas.
  sigil::motion::Ticker ticker;
  Composer composer(ticker, fonts());
  composer.setSize({64, 64});
  composer.render(box().fill(Fill::color({0, 0, 1, 1}))
                      .child(box().width(20).height(20)
                                 .fill(Fill::color({1, 1, 0, 1}))));
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 64));
  composer.draw(*surface->getCanvas());
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  surface->readPixels(bm.pixmap(), 40, 40);
  EXPECT_EQ(bm.getColor(0, 0), SK_ColorBLUE);
  surface->readPixels(bm.pixmap(), 10, 10);
  EXPECT_EQ(bm.getColor(0, 0), SK_ColorYELLOW);
}
