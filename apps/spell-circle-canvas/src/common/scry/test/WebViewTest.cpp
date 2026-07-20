// Smoke tests for SigilScry: engine bring-up, HTML rendering into published
// SkImage frames, drawing onto an SkCanvas, and script evaluation.
//
// Ultralight allows one Renderer per process, so all tests share a single
// threaded engine.

#include <sigilscry/WebEngine.h>
#include <sigilscry/WebImage.h>
#include <sigilscry/WebView.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

using namespace sigil::scry;

namespace {

std::mutex g_logMutex;
std::vector<std::string> g_logMessages;

WebEngine &sharedEngine() {
  static std::shared_ptr<WebEngine> engine = [] {
    WebEngineConfig config;
    config.logCallback = [](LogLevel level, const std::string &message) {
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

bool logContains(const std::string &needle) {
  std::lock_guard<std::mutex> lock(g_logMutex);
  for (const std::string &message : g_logMessages)
    if (message.find(needle) != std::string::npos)
      return true;
  return false;
}

/** Polls until the view has published a frame newer than @p sinceVersion,
 *  or fails after @p timeout. */
bool waitForFrame(WebView &view, uint64_t sinceVersion,
                  std::chrono::milliseconds timeout =
                      std::chrono::milliseconds(10000)) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (view.frameVersion() > sinceVersion)
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}

} // namespace

TEST(WebViewTest, RendersSolidColorHtml) {
  auto view = sharedEngine().createView(160, 120, {.transparent = false});
  ASSERT_NE(view, nullptr);

  view->loadHTML("<html><body style='background:#ff0000;margin:0'>"
                 "</body></html>");
  ASSERT_TRUE(waitForFrame(*view, 0));

  // The page may publish an intermediate blank frame before the styled
  // document paints; wait until the pixels actually turn red.
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  SkColor center = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    WebView::Frame frame = view->frame();
    ASSERT_NE(frame.image, nullptr);
    SkBitmap readback;
    ASSERT_TRUE(readback.tryAllocPixels(
        SkImageInfo::MakeN32Premul(frame.image->width(),
                                   frame.image->height())));
    ASSERT_TRUE(frame.image->readPixels(nullptr, readback.pixmap(), 0, 0));
    center = readback.getColor(80, 60);
    if (center == SK_ColorRED)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  EXPECT_EQ(center, SK_ColorRED);
}

TEST(WebViewTest, DrawsOntoSkCanvas) {
  auto view = sharedEngine().createView(64, 64, {.transparent = false});
  ASSERT_NE(view, nullptr);

  view->loadHTML("<html><body style='background:#0000ff;margin:0'>"
                 "</body></html>");
  ASSERT_TRUE(waitForFrame(*view, 0));

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(128, 128));
  ASSERT_NE(surface, nullptr);

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  SkColor sampled = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    surface->getCanvas()->clear(SK_ColorGREEN);
    view->draw(*surface->getCanvas(), SkRect::MakeXYWH(32, 32, 64, 64));
    SkBitmap readback;
    ASSERT_TRUE(
        readback.tryAllocPixels(SkImageInfo::MakeN32Premul(128, 128)));
    ASSERT_TRUE(surface->readPixels(readback.pixmap(), 0, 0));
    sampled = readback.getColor(64, 64);
    if (sampled == SK_ColorBLUE &&
        readback.getColor(8, 8) == SK_ColorGREEN)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  EXPECT_EQ(sampled, SK_ColorBLUE);
}

TEST(WebViewTest, EvaluatesScript) {
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
  ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(10),
                          [&] { return done; }));
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

  auto view = sharedEngine().createView(64, 64, {.transparent = false});
  ASSERT_NE(view, nullptr);
  view->loadHTML("<html><body style='margin:0;background:#000'>"
                 "<img src='cpu_swatch.imgsrc' "
                 "style='display:block;width:64px;height:64px'>"
                 "</body></html>");
  ASSERT_TRUE(waitForFrame(*view, 0));

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  SkColor center = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    WebView::Frame frame = view->frame();
    ASSERT_NE(frame.image, nullptr);
    SkBitmap readback;
    ASSERT_TRUE(readback.tryAllocPixels(
        SkImageInfo::MakeN32Premul(frame.image->width(),
                                   frame.image->height())));
    ASSERT_TRUE(frame.image->readPixels(nullptr, readback.pixmap(), 0, 0));
    center = readback.getColor(32, 32);
    if (center == SK_ColorMAGENTA)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  EXPECT_EQ(center, SK_ColorMAGENTA);
}

// Same round trip through the one-call paint() API, which wraps the
// backing store, flushes, and invalidates in a single step.
TEST(WebViewTest, PaintsSlotWithCallback) {
  auto image = sharedEngine().createImage("cpu_paint_swatch", 32, 32);
  ASSERT_NE(image, nullptr);
  ASSERT_TRUE(image->paint(
      [](SkCanvas &canvas) { canvas.clear(SK_ColorYELLOW); }));

  auto view = sharedEngine().createView(64, 64, {.transparent = false});
  ASSERT_NE(view, nullptr);
  view->loadHTML("<html><body style='margin:0;background:#000'>"
                 "<img src='cpu_paint_swatch.imgsrc' "
                 "style='display:block;width:64px;height:64px'>"
                 "</body></html>");
  ASSERT_TRUE(waitForFrame(*view, 0));

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  SkColor center = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    WebView::Frame frame = view->frame();
    ASSERT_NE(frame.image, nullptr);
    SkBitmap readback;
    ASSERT_TRUE(readback.tryAllocPixels(
        SkImageInfo::MakeN32Premul(frame.image->width(),
                                   frame.image->height())));
    ASSERT_TRUE(frame.image->readPixels(nullptr, readback.pixmap(), 0, 0));
    center = readback.getColor(32, 32);
    if (center == SK_ColorYELLOW)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  EXPECT_EQ(center, SK_ColorYELLOW);
}

// Referencing a slot no WebImage is registered under must be loud, not a
// silent broken image.
TEST(WebViewTest, WarnsOnUnregisteredSlot) {
  auto view = sharedEngine().createView(32, 32);
  ASSERT_NE(view, nullptr);
  view->loadHTML("<html><body><img src='definitely_missing.imgsrc'>"
                 "</body></html>");

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!logContains("definitely_missing") &&
         std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_TRUE(logContains("definitely_missing"));
}

TEST(WebViewTest, FrameCallbackFires) {
  auto view = sharedEngine().createView(48, 48, {.transparent = false});
  ASSERT_NE(view, nullptr);

  std::mutex mutex;
  std::condition_variable cv;
  uint64_t callbackVersion = 0;

  view->setFrameCallback([&](const WebView::Frame &frame) {
    std::lock_guard<std::mutex> lock(mutex);
    callbackVersion = frame.version;
    cv.notify_all();
  });
  view->loadHTML("<html><body style='background:#123456'></body></html>");

  std::unique_lock<std::mutex> lock(mutex);
  EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(10),
                          [&] { return callbackVersion > 0; }));
}
