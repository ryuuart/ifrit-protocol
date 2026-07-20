// Headless SigilScry demo — drives the engine in unthreaded (lockstep)
// mode, renders an HTML/CSS layout, and composites it with direct SkCanvas
// drawing. Writes PNGs to the output dir (first argument, default
// ./scry_demo_out):
//
//   web_frame.png  the raw Ultralight surface, grabbed zero-copy via
//                  WebView::peekPixels()
//   composite.png  the same view drawn over a Skia-painted backdrop with
//                  extra SkCanvas decorations on top

#include <sigilscry/WebEngine.h>
#include <sigilscry/WebView.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRect.h>
#include <include/core/SkStream.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkGradient.h>
#include <include/encode/SkPngEncoder.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

using namespace sigil::scry;

namespace {

constexpr int kWidth = 900;
constexpr int kHeight = 620;

constexpr const char *kDemoHtml = R"html(
<html>
<head><style>
  html, body { background: transparent; margin: 0; font-family: -apple-system, 'Helvetica Neue', sans-serif; }
  .board {
    box-sizing: border-box; width: 100%; height: 100vh; padding: 28px;
    display: grid; grid-template-columns: repeat(3, 1fr);
    grid-template-rows: auto 1fr 1fr; gap: 18px;
  }
  h1 {
    grid-column: 1 / -1; margin: 0; font-size: 40px; font-weight: 800;
    color: #ffb46b; letter-spacing: -0.5px;
  }
  h1 em { color: #ff5e8a; font-style: normal; }
  .card {
    background: rgba(255, 255, 255, 0.92); border-radius: 16px;
    padding: 18px; box-shadow: 0 8px 24px rgba(0, 0, 0, 0.35);
    display: flex; flex-direction: column; justify-content: space-between;
  }
  .card h2 { margin: 0 0 8px; font-size: 20px; color: #1d1d1f; }
  .card p { margin: 0; font-size: 14px; line-height: 1.5; color: #48484c; }
  .tag {
    align-self: flex-start; margin-top: 12px; padding: 4px 10px;
    border-radius: 999px; font-size: 12px; font-weight: 600; color: #fff;
    background: linear-gradient(135deg, #e52e71, #ff8a00);
  }
  .wide { grid-column: span 2; }
  .accent { background: linear-gradient(160deg, #2b1055, #7597de); }
  .accent h2, .accent p { color: #f4f4f8; }
</style></head>
<body><div class="board">
  <h1>Ifrit Protocol &mdash; <em>Ultralight layout</em></h1>
  <div class="card wide"><div>
    <h2>WebKit layout on the scene canvas</h2>
    <p>This panel is HTML/CSS &mdash; grid, flexbox, gradient text, border
       radii, and box shadows &mdash; rendered by Ultralight and composited
       onto an SkCanvas as an ordinary SkImage.</p></div>
    <span class="tag">grid + flexbox</span></div>
  <div class="card accent"><div>
    <h2>Transparent</h2>
    <p>The page background is transparent, so the Skia-painted backdrop
       shows through between the cards.</p></div>
    <span class="tag">is_transparent</span></div>
  <div class="card"><div>
    <h2>Lockstep mode</h2>
    <p>This demo drives Renderer::Update and Render from its own loop
       (threaded=false) &mdash; no web thread.</p></div>
    <span class="tag">renderFrame()</span></div>
  <div class="card"><div>
    <h2>Zero copy</h2>
    <p>web_frame.png was grabbed straight from the live surface pixels via
       WebView::peekPixels().</p></div>
    <span class="tag">SkPixmap</span></div>
  <div class="card"><div>
    <h2>Threaded mode</h2>
    <p>The default engine owns a web thread and publishes immutable frames
       for any consumer thread to draw.</p></div>
    <span class="tag">SkImage</span></div>
</div></body></html>
)html";

bool writePng(const SkPixmap &pixmap, const std::filesystem::path &path) {
  SkFILEWStream stream(path.string().c_str());
  if (!stream.isValid())
    return false;
  return SkPngEncoder::Encode(&stream, pixmap, {});
}

} // namespace

int main(int argc, char **argv) {
  std::filesystem::path outDir =
      argc > 1 ? argv[1] : "scry_demo_out";
  std::filesystem::create_directories(outDir);

  WebEngineConfig config;
  config.threaded = false;
  auto engine = WebEngine::create(config);
  if (!engine) {
    std::fprintf(stderr, "engine bring-up failed\n");
    return 1;
  }

  auto view = engine->createView(kWidth, kHeight);
  bool loaded = false;
  view->setLoadCallback([&loaded] { loaded = true; });
  view->loadHTML(kDemoHtml);

  // Lockstep loop: pump events and repaint until the document has loaded
  // and its paint has settled (no dirty repaint for a few frames).
  int settledFrames = 0;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  while (std::chrono::steady_clock::now() < deadline) {
    engine->update();
    bool painted = engine->renderFrame();
    if (loaded && view->frameVersion() > 0)
      settledFrames = painted ? 0 : settledFrames + 1;
    if (settledFrames >= 5)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }
  if (settledFrames < 5) {
    std::fprintf(stderr, "page never finished painting\n");
    return 1;
  }

  // Zero-copy grab of the live surface (valid here: unthreaded engine,
  // between renderFrame() calls).
  SkPixmap livePixels;
  if (!view->peekPixels(&livePixels) ||
      !writePng(livePixels, outDir / "web_frame.png")) {
    std::fprintf(stderr, "failed to write web_frame.png\n");
    return 1;
  }

  // Composite: Skia backdrop, then the web frame, then Skia decorations.
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  SkCanvas *canvas = surface->getCanvas();

  SkPoint gradientSpan[2] = {{0, 0},
                             {static_cast<float>(kWidth),
                              static_cast<float>(kHeight)}};
  SkColor4f gradientColors[2] = {SkColor4f::FromColor(SkColorSetRGB(0x14, 0x0e, 0x26)),
                                 SkColor4f::FromColor(SkColorSetRGB(0x5b, 0x21, 0x38))};
  SkPaint backdrop;
  backdrop.setShader(SkShaders::LinearGradient(
      gradientSpan,
      SkGradient({gradientColors, SkTileMode::kClamp}, {})));
  canvas->drawPaint(backdrop);

  view->draw(*canvas, SkRect::MakeWH(kWidth, kHeight));

  SkPaint ring;
  ring.setAntiAlias(true);
  ring.setStyle(SkPaint::kStroke_Style);
  ring.setStrokeWidth(6);
  ring.setColor(SkColorSetARGB(0xcc, 0xff, 0x8a, 0x00));
  canvas->drawCircle(kWidth - 110, kHeight - 110, 70, ring);

  SkBitmap readback;
  readback.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  if (!surface->readPixels(readback.pixmap(), 0, 0) ||
      !writePng(readback.pixmap(), outDir / "composite.png")) {
    std::fprintf(stderr, "failed to write composite.png\n");
    return 1;
  }

  std::printf("wrote %s and %s\n",
              (outDir / "web_frame.png").string().c_str(),
              (outDir / "composite.png").string().c_str());
  return 0;
}
