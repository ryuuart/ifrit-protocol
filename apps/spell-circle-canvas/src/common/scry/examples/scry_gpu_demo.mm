// GPU SigilScry demo — the all-hardware pipeline: Ultralight renders an
// HTML/CSS layout through the Metal GPUDriver, the published MTLTexture is
// wrapped zero-copy as a Graphite SkImage, and everything is composited on
// a Graphite surface sharing the same device/queue. The only CPU copy is
// the final readback for the PNG (out dir: first argument, default
// ./scry_demo_out).

#import <Metal/Metal.h>

#include <sigilscry/WebEngine.h>
#include <sigilscry/WebImage.h>
#include <sigilscry/WebView.h>

#include "SkiaGraphiteContext.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkStream.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkGradient.h>
#include <include/encode/SkPngEncoder.h>
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
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
  h1 { grid-column: 1 / -1; margin: 0; font-size: 40px; font-weight: 800;
       color: #7ee8ff; letter-spacing: -0.5px; }
  h1 em { color: #b18cff; font-style: normal; }
  .card {
    background: rgba(16, 20, 34, 0.82); border-radius: 16px; padding: 18px;
    border: 1px solid rgba(126, 232, 255, 0.25);
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.5);
    display: flex; flex-direction: column; justify-content: space-between;
  }
  .card h2 { margin: 0 0 8px; font-size: 20px; color: #eef3ff; }
  .card p { margin: 0; font-size: 14px; line-height: 1.5; color: #a9b4cc; }
  .tag {
    align-self: flex-start; margin-top: 12px; padding: 4px 10px;
    border-radius: 999px; font-size: 12px; font-weight: 600; color: #061018;
    background: linear-gradient(135deg, #7ee8ff, #b18cff);
  }
  .wide { grid-column: span 2; }
</style></head>
<body><div class="board">
  <h1>Ifrit Protocol &mdash; <em>Ultralight on Metal</em></h1>
  <div class="card wide"><div>
    <h2>Rendered by the GPU, composited by the GPU</h2>
    <p>Ultralight executed its fill and path shaders through SigilScry's
       Metal GPUDriver, into an MTLTexture on the same device and command
       queue as the Graphite canvas underneath.</p></div>
    <span class="tag">GPUDriver + Graphite</span></div>
  <div class="card"><div>
    <h2>Zero copy</h2>
    <p>The published texture is wrapped as a texture-backed SkImage &mdash;
       no pixels cross the PCIe/CPU boundary until this PNG's readback.</p></div>
    <span class="tag">WrapTexture</span></div>
  <div class="card"><div>
    <h2>One queue</h2>
    <p>Web render, publish blit, and canvas compositing all ride one
       MTLCommandQueue, so ordering is implicit.</p></div>
    <span class="tag">MTLCommandQueue</span></div>
  <div class="card"><div>
    <h2>Same API as the CPU path</h2>
    <p>WebView::draw() detects the Graphite canvas and wraps the texture;
       frame(recorder) and frameVersion() behave identically in both
       modes.</p></div>
    <span class="tag">WebView::draw()</span></div>
  <div class="card" style="align-items:center"><div style="text-align:center">
    <h2>And the other way</h2>
    <img src="sigil.imgsrc" style="width:120px;height:120px" />
    <p>This sigil was drawn by Skia/Graphite into a WebImage texture and
       composited into this page by Ultralight.</p></div>
    <span class="tag">WebImage</span></div>
</div></body></html>
)html";

/** Draws a spell-circle-ish sigil with Graphite into @p canvas. */
void drawSigil(SkCanvas *canvas, int size) {
  canvas->clear(SkColorSetARGB(0xff, 0x0b, 0x14, 0x24));
  SkPaint ring;
  ring.setAntiAlias(true);
  ring.setStyle(SkPaint::kStroke_Style);
  const float center = size / 2.0f;
  for (int i = 0; i < 3; ++i) {
    ring.setStrokeWidth(3.0f - i);
    ring.setColor(i % 2 ? SkColorSetRGB(0xb1, 0x8c, 0xff)
                        : SkColorSetRGB(0x7e, 0xe8, 0xff));
    canvas->drawCircle(center, center, center * (0.9f - 0.22f * i), ring);
  }
  SkPaint chord;
  chord.setAntiAlias(true);
  chord.setStrokeWidth(1.5f);
  chord.setColor(SkColorSetARGB(0xaa, 0x7e, 0xe8, 0xff));
  for (int i = 0; i < 6; ++i) {
    float angleA = (float)(i * 2.0 * M_PI / 6.0);
    float angleB = (float)(((i + 2) % 6) * 2.0 * M_PI / 6.0);
    float radius = center * 0.9f;
    canvas->drawLine(center + radius * std::cos(angleA),
                     center + radius * std::sin(angleA),
                     center + radius * std::cos(angleB),
                     center + radius * std::sin(angleB), chord);
  }
}

} // namespace

int main(int argc, char **argv) {
  std::filesystem::path outDir = argc > 1 ? argv[1] : "scry_demo_out";
  std::filesystem::create_directories(outDir);

  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  id<MTLCommandQueue> queue = [device newCommandQueue];
  if (!device || !queue) {
    std::fprintf(stderr, "no Metal device\n");
    return 1;
  }

  WebEngineConfig config;
  config.metalDevice = (void *)device;
  config.metalCommandQueue = (void *)queue;
  auto engine = WebEngine::create(config);
  if (!engine) {
    std::fprintf(stderr, "engine bring-up failed\n");
    return 1;
  }

  auto graphite =
      SkiaGraphiteContext::createMetal((void *)device, (void *)queue);
  if (!graphite) {
    std::fprintf(stderr, "Graphite bring-up failed\n");
    return 1;
  }

  // The reverse compositing direction: draw a sigil with Graphite into a
  // WebImage the page displays via <img src="sigil.imgsrc">. paint()
  // wraps the slot texture on the engine's own recorder and handles the
  // flush + invalidate.
  auto sigil = engine->createImage("sigil", 200, 200);
  if (!sigil->paint([](SkCanvas &canvas) { drawSigil(&canvas, 200); })) {
    std::fprintf(stderr, "sigil paint failed\n");
    return 1;
  }

  auto view = engine->createView(kWidth, kHeight);
  view->loadHTML(kDemoHtml);

  // Threaded engine: wait for the page to publish and settle (a few
  // stable versions in a row).
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  uint64_t stableVersion = 0;
  int stableTicks = 0;
  while (std::chrono::steady_clock::now() < deadline && stableTicks < 10) {
    uint64_t version = view->frameVersion();
    stableTicks = (version > 0 && version == stableVersion) ? stableTicks + 1
                                                            : 0;
    stableVersion = version;
    std::this_thread::sleep_for(std::chrono::milliseconds(32));
  }
  if (stableVersion == 0) {
    std::fprintf(stderr, "page never painted\n");
    return 1;
  }

  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
      graphite->recorder(), SkImageInfo::MakeN32Premul(kWidth, kHeight));
  SkCanvas *canvas = surface->getCanvas();

  SkPoint gradientSpan[2] = {{0, 0},
                             {(float)kWidth, (float)kHeight}};
  SkColor4f gradientColors[2] = {
      SkColor4f::FromColor(SkColorSetRGB(0x05, 0x10, 0x1e)),
      SkColor4f::FromColor(SkColorSetRGB(0x1c, 0x10, 0x38))};
  SkPaint backdrop;
  backdrop.setShader(SkShaders::LinearGradient(
      gradientSpan, SkGradient({gradientColors, SkTileMode::kClamp}, {})));
  canvas->drawPaint(backdrop);

  view->draw(*canvas, SkRect::MakeWH(kWidth, kHeight));

  SkPaint ring;
  ring.setAntiAlias(true);
  ring.setStyle(SkPaint::kStroke_Style);
  ring.setStrokeWidth(6);
  ring.setColor(SkColorSetARGB(0xcc, 0x7e, 0xe8, 0xff));
  canvas->drawCircle(kWidth - 110, kHeight - 110, 70, ring);

  // Flush the Graphite work, then read the composited surface back for
  // the PNG (the one CPU copy in this pipeline).
  auto recording = graphite->recorder()->snap();
  if (recording) {
    skgpu::graphite::InsertRecordingInfo info;
    info.fRecording = recording.get();
    graphite->context()->insertRecording(info);
  }

  struct ReadContext {
    std::unique_ptr<const SkImage::AsyncReadResult> result;
  } readContext;
  graphite->context()->asyncRescaleAndReadPixels(
      surface.get(), SkImageInfo::MakeN32Premul(kWidth, kHeight),
      SkIRect::MakeWH(kWidth, kHeight), SkImage::RescaleGamma::kSrc,
      SkImage::RescaleMode::kNearest,
      [](SkImage::ReadPixelsContext context,
         std::unique_ptr<const SkImage::AsyncReadResult> result) {
        static_cast<ReadContext *>(context)->result = std::move(result);
      },
      &readContext);
  skgpu::graphite::SubmitInfo submitInfo;
  submitInfo.fSync = skgpu::graphite::SyncToCpu::kYes;
  graphite->context()->submit(submitInfo);
  if (!readContext.result) {
    std::fprintf(stderr, "readback failed\n");
    return 1;
  }

  SkBitmap readback;
  readback.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  const char *src = static_cast<const char *>(readContext.result->data(0));
  for (int y = 0; y < kHeight; ++y)
    std::memcpy(readback.getAddr32(0, y),
                src + y * readContext.result->rowBytes(0),
                (size_t)kWidth * 4);

  std::filesystem::path outPath = outDir / "composite_gpu.png";
  SkFILEWStream stream(outPath.string().c_str());
  if (!stream.isValid() ||
      !SkPngEncoder::Encode(&stream, readback.pixmap(), {})) {
    std::fprintf(stderr, "failed to write %s\n", outPath.string().c_str());
    return 1;
  }

  std::printf("wrote %s\n", outPath.string().c_str());
  return 0;
}
