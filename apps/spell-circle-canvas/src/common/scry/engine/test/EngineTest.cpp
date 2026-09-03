/** @file
 * scry_engine_test — the CPU-mode engine end to end: bring-up, HTML
 * rendered into published SkImage frames, drawing onto an SkCanvas,
 * script evaluation, image slots filled and painted, the warning for a
 * slot no page registered, and the frame callback. Ultralight allows
 * one Renderer per process, so every test shares one threaded engine.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>
#include <sigilscry/engine/WebEngine.h>
#include <sigilscry/engine/WebImage.h>
#include <sigilscry/engine/WebView.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Wait.h"

using namespace sigil::scry;
using namespace sigil::scry::test;

namespace {

std::mutex g_logMutex;
std::vector<std::string> g_logMessages;

WebEngine& sharedEngine() {
  static std::shared_ptr<WebEngine> engine = [] {
    WebEngineConfig config;
    config.logCallback = [](LogLevel level, const std::string& message) {
      std::lock_guard<std::mutex> lock(g_logMutex);
      g_logMessages.push_back(message);
      if (level != LogLevel::Info)
        std::fprintf(stderr, "[SigilScry] %s\n", message.c_str());
    };
    return WebEngine::create(config);
  }();
  EXPECT_NE(engine, nullptr);
  return *engine;
}

bool logContains(const std::string& needle) {
  std::lock_guard<std::mutex> lock(g_logMutex);
  for (const std::string& message : g_logMessages)
    if (message.find(needle) != std::string::npos) return true;
  return false;
}

/** A view showing @p slot at 64 x 64 on black, so the slot's own colour
 *  is what the centre of the page reads. */
std::shared_ptr<WebView> viewShowingSlot(const char* slot) {
  auto view = sharedEngine().createView(64, 64, {.transparent = false});
  if (!view) return view;
  view->loadHTML(std::string("<html><body style='margin:0;background:#000'>"
                             "<img src='") +
                 slot +
                 ".imgsrc' style='display:block;width:64px;height:64px'>"
                 "</body></html>");
  return view;
}

}  // namespace

TEST(WebViewTest, PublishesTheColourTheDocumentDeclares) {
  auto view = sharedEngine().createView(160, 120, {.transparent = false});
  ASSERT_NE(view, nullptr);

  view->loadHTML(
      "<html><body style='background:#ff0000;margin:0'>"
      "</body></html>");
  ASSERT_TRUE(waitForFrame(*view, 0));
  EXPECT_TRUE(waitForCentre(*view, SK_ColorRED));
}

TEST(WebViewTest, DrawsThePageIntoTheRectItIsGiven) {
  auto view = sharedEngine().createView(64, 64, {.transparent = false});
  ASSERT_NE(view, nullptr);

  view->loadHTML(
      "<html><body style='background:#0000ff;margin:0'>"
      "</body></html>");
  ASSERT_TRUE(waitForFrame(*view, 0));

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(128, 128));
  ASSERT_NE(surface, nullptr);
  SkBitmap composite;
  ASSERT_TRUE(composite.tryAllocPixels(SkImageInfo::MakeN32Premul(128, 128)));

  // The page lands inside the rect it was given and nowhere else, so the
  // canvas's own colour still stands outside it.
  ASSERT_TRUE(waitForColour("the page inside its rect", SK_ColorBLUE, [&] {
    surface->getCanvas()->clear(SK_ColorGREEN);
    view->draw(*surface->getCanvas(), SkRect::MakeXYWH(32, 32, 64, 64));
    if (!surface->readPixels(composite.pixmap(), 0, 0))
      return SK_ColorTRANSPARENT;
    return composite.getColor(64, 64);
  }));
  EXPECT_EQ(composite.getColor(8, 8), SK_ColorGREEN);
}

TEST(WebViewTest, TheScrollDeltaIsWhatTheContentMovesBy) {
  auto view = sharedEngine().createView(64, 64, {.transparent = false});
  ASSERT_NE(view, nullptr);

  // Two bands, each the height of the view: red standing where the view
  // is, blue waiting below it. Which band the centre reads says which
  // way the page went.
  view->loadHTML(
      "<html><body style='margin:0'>"
      "<div style='height:64px;background:#ff0000'></div>"
      "<div style='height:64px;background:#0000ff'></div>"
      "</body></html>");
  ASSERT_TRUE(waitForFrame(*view, 0));
  ASSERT_TRUE(waitForCentre(*view, SK_ColorRED));

  // NEGATIVE WALKS DOWN THE PAGE. The delta is what the CONTENT moves
  // by, the way a wheel event states it, so reaching the band below
  // means moving the content up.
  view->scroll(0, -64);
  EXPECT_TRUE(waitForCentre(*view, SK_ColorBLUE));

  // …and back, which is the same statement from the other side.
  view->scroll(0, 64);
  EXPECT_TRUE(waitForCentre(*view, SK_ColorRED));
}

TEST(WebViewTest, AnswersTheValueAScriptEvaluatesTo) {
  auto view = sharedEngine().createView(32, 32);
  ASSERT_NE(view, nullptr);

  std::mutex mutex;
  std::condition_variable cv;
  std::string result;
  bool done = false;

  view->evaluateScript("6 * 7", [&](std::string value) {
    std::lock_guard<std::mutex> lock(mutex);
    result = std::move(value);
    done = true;
    cv.notify_all();
  });

  std::unique_lock<std::mutex> lock(mutex);
  ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(10), [&] { return done; }))
      << "the wait expired: the script never answered";
  EXPECT_EQ(result, "42");
}

// The reverse compositing direction: Skia-rendered pixels displayed
// inside the page (WebImage), then the page composited back onto an
// SkCanvas — a full Skia -> Ultralight -> Skia round trip.
TEST(WebViewTest, CompositesSkiaContentIntoPage) {
  auto image = sharedEngine().createImage("cpu_swatch", 32, 32);
  ASSERT_NE(image, nullptr);

  SkBitmap swatch;
  ASSERT_TRUE(swatch.tryAllocPixels(SkImageInfo::MakeN32Premul(32, 32)));
  SkCanvas swatchCanvas(swatch);
  swatchCanvas.clear(SK_ColorMAGENTA);
  image->update(swatch.pixmap());

  auto view = viewShowingSlot("cpu_swatch");
  ASSERT_NE(view, nullptr);
  ASSERT_TRUE(waitForFrame(*view, 0));
  EXPECT_TRUE(waitForCentre(*view, SK_ColorMAGENTA));
}

// Same round trip through the one-call paint() API, which wraps the
// backing store, flushes, and invalidates in a single step.
TEST(WebViewTest, PaintsSlotWithCallback) {
  auto image = sharedEngine().createImage("cpu_paint_swatch", 32, 32);
  ASSERT_NE(image, nullptr);
  ASSERT_TRUE(
      image->paint([](SkCanvas& canvas) { canvas.clear(SK_ColorYELLOW); }));

  auto view = viewShowingSlot("cpu_paint_swatch");
  ASSERT_NE(view, nullptr);
  ASSERT_TRUE(waitForFrame(*view, 0));
  EXPECT_TRUE(waitForCentre(*view, SK_ColorYELLOW));
}

// Referencing a slot no WebImage is registered under must be loud, not a
// silent broken image.
TEST(WebViewTest, WarnsOnUnregisteredSlot) {
  auto view = sharedEngine().createView(32, 32);
  ASSERT_NE(view, nullptr);
  view->loadHTML(
      "<html><body><img src='definitely_missing.imgsrc'>"
      "</body></html>");

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!logContains("definitely_missing") &&
         std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_TRUE(logContains("definitely_missing"))
      << "the wait expired: no message ever named the slot";
}

TEST(WebViewTest, CallsBackWithEachFrameItPublishes) {
  auto view = sharedEngine().createView(48, 48, {.transparent = false});
  ASSERT_NE(view, nullptr);

  std::mutex mutex;
  std::condition_variable cv;
  uint64_t callbackVersion = 0;

  view->setFrameCallback([&](const WebView::Frame& frame) {
    std::lock_guard<std::mutex> lock(mutex);
    callbackVersion = frame.version;
    cv.notify_all();
  });
  view->loadHTML("<html><body style='background:#123456'></body></html>");

  std::unique_lock<std::mutex> lock(mutex);
  EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(10), [&] {
    return callbackVersion > 0;
  })) << "the wait expired: no frame was ever handed to the callback";
}
