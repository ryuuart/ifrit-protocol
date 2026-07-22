#include "ComposeSketchView.h"

#ifdef SIGILCOMPOSE_SKETCH_GPU
#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#endif

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>

#include <QtCore/QMetaObject>
#include <QtCore/QMutexLocker>
#include <QtCore/QSize>
#include <rhi/qrhi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <thread>
#include <vector>

using sigil::compose::sketch::SketchHost;

SketchHost *ComposeSketchView::host = nullptr;
QMutex ComposeSketchView::hostMutex;

namespace {
// Which path frames actually take, for the status bar: the CPU-raster
// sketch host is exactly how full-screen live materials once read as a
// framework problem, so the backend stays visible at all times.
std::atomic<int> backendMode{0}; // 0 unknown, 1 Graphite GPU, 2 CPU raster
} // namespace

class ComposeSketchRenderer final : public QQuickRhiItemRenderer {
public:
  void initialize(QRhiCommandBuffer *commandBuffer) override;
  void synchronize(QQuickRhiItem *item) override;
  void render(QRhiCommandBuffer *commandBuffer) override;

private:
  void renderScene(SkCanvas &canvas, QSize pixelSize);
  void runPendingCaptures(); // hostMutex must be held

#ifdef SIGILCOMPOSE_SKETCH_GPU
  bool readbackGraphite(SkSurface &surface, const SkPixmap &out);
  // Declared before everything Skia so reverse destruction releases any
  // Graphite-backed images before tearing down the context.
  std::unique_ptr<SkiaGraphiteContext> m_graphiteContext;
#endif
  ComposeSketchView *m_view = nullptr;
  QRhi *m_rhi = nullptr;
  bool m_initialized = false;
  std::vector<uint32_t> m_rasterPixels;
  int m_pendingCaptures = 0;
};

void ComposeSketchRenderer::initialize(QRhiCommandBuffer * /*commandBuffer*/) {
  QRhi *currentRhi = rhi();
  if (m_initialized && m_rhi == currentRhi)
    return;

  QMutexLocker lock(&ComposeSketchView::hostMutex);
  SketchHost *host = ComposeSketchView::host;
#ifdef SIGILCOMPOSE_SKETCH_GPU
  m_graphiteContext.reset();
  if (host) {
    host->setCaptureBackend({});
    // A replacement QRhi invalidates every image the old context minted;
    // the composer's caches must re-record on the new backend.
    if (m_initialized && host->composer())
      host->composer()->purgeCaches();
  }
  // Metal only, like ComposeGallery: the Vulkan adapter does not yet hand
  // the image's final layout back to QRhi's state tracker.
  if (currentRhi && currentRhi->backend() == QRhi::Metal)
    m_graphiteContext = SkiaGraphiteContext::create(currentRhi);
  if (m_graphiteContext && host) {
    host->setCaptureBackend(
        {[this](const SkImageInfo &info) -> sk_sp<SkSurface> {
           if (!m_graphiteContext)
             return nullptr;
           return SkSurfaces::RenderTarget(m_graphiteContext->recorder(),
                                           info);
         },
         [this](SkSurface &surface, const SkPixmap &out) {
           return readbackGraphite(surface, out);
         }});
  }
  backendMode.store(m_graphiteContext ? 1 : 2);
#else
  backendMode.store(2);
#endif
  m_rhi = currentRhi;
  m_initialized = true;
  std::fprintf(stderr, "[compose sketch] renderer: %s\n",
               backendMode.load() == 1 ? "Graphite GPU"
                                       : "CPU raster fallback");
}

void ComposeSketchRenderer::synchronize(QQuickRhiItem *item) {
  auto *view = static_cast<ComposeSketchView *>(item);
  m_view = view;
  m_pendingCaptures += view->m_captureRequests;
  view->m_captureRequests = 0;
}

void ComposeSketchRenderer::renderScene(SkCanvas &canvas, QSize pixelSize) {
  SketchHost *host = ComposeSketchView::host;
  const int w = pixelSize.width();
  const int h = pixelSize.height();
  canvas.clear(SkColorSetRGB(0x0b, 0x0a, 0x14)); // letterbox bars
  const SkSize scene = host->canvasSize(); // sketch-declared (ctx.canvas)
  const float scale =
      std::min((float)w / scene.width(), (float)h / scene.height());
  canvas.save();
  canvas.translate((w - scene.width() * scale) / 2,
                   (h - scene.height() * scale) / 2);
  canvas.scale(scale, scale);
  canvas.clipRect(SkRect::MakeWH(scene.width(), scene.height()));
  canvas.clear(host->background().toSkColor());
  if (!host->frame(canvas)) {
    // Nothing loaded yet — quiet dark canvas until the first build.
    canvas.clear(SkColorSetRGB(0x14, 0x12, 0x1e));
  }
  canvas.restore();
}

void ComposeSketchRenderer::runPendingCaptures() {
  SketchHost *host = ComposeSketchView::host;
  while (m_pendingCaptures > 0) {
    --m_pendingCaptures;
    QString result;
    if (host->live()) {
      namespace fs = std::filesystem;
      const fs::path dir = host->sketchPath().parent_path() / "captures";
      const std::string stem = host->sketchPath().stem().string();
      fs::path out;
      for (int n = 1; n < 10000; ++n) {
        char name[256];
        std::snprintf(name, sizeof name, "%s-%03d.png", stem.c_str(), n);
        out = dir / name;
        if (!fs::exists(out))
          break;
      }
      if (host->capture(out, 2.0f))
        result = QString::fromStdString(out.string());
    }
    if (m_view)
      QMetaObject::invokeMethod(
          m_view, [view = m_view, result] { emit view->captureReady(result); },
          Qt::QueuedConnection);
  }
}

#ifdef SIGILCOMPOSE_SKETCH_GPU
bool ComposeSketchRenderer::readbackGraphite(SkSurface &surface,
                                             const SkPixmap &out) {
  if (!m_graphiteContext)
    return false;
  if (auto recording = m_graphiteContext->recorder()->snap()) {
    skgpu::graphite::InsertRecordingInfo info;
    info.fRecording = recording.get();
    m_graphiteContext->context()->insertRecording(info);
  }
  struct ReadContext {
    std::unique_ptr<const SkImage::AsyncReadResult> result;
    bool called = false;
  } readContext;
  m_graphiteContext->context()->asyncRescaleAndReadPixels(
      &surface, out.info(), SkIRect::MakeWH(surface.width(), surface.height()),
      SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kNearest,
      [](SkImage::ReadPixelsContext context,
         std::unique_ptr<const SkImage::AsyncReadResult> result) {
        auto *read = static_cast<ReadContext *>(context);
        read->result = std::move(result);
        read->called = true;
      },
      &readContext);
  skgpu::graphite::SubmitInfo submitInfo;
  submitInfo.fSync = skgpu::graphite::SyncToCpu::kYes;
  m_graphiteContext->context()->submit(submitInfo);
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!readContext.called &&
         std::chrono::steady_clock::now() < deadline) {
    m_graphiteContext->context()->checkAsyncWorkCompletion();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (!readContext.result)
    return false;
  const auto *src = static_cast<const uint8_t *>(readContext.result->data(0));
  const size_t srcRowBytes = readContext.result->rowBytes(0);
  const size_t copyBytes = std::min(srcRowBytes, out.rowBytes());
  for (int y = 0; y < out.height(); ++y)
    std::memcpy(out.writable_addr(0, y), src + (size_t)y * srcRowBytes,
                copyBytes);
  return true;
}
#endif

void ComposeSketchRenderer::render(QRhiCommandBuffer *commandBuffer) {
  QRhiTexture *texture = colorTexture();
  SketchHost *host = ComposeSketchView::host;
  if (!texture || !host)
    return;
  const QSize pixelSize = texture->pixelSize();
  if (pixelSize.width() < 1 || pixelSize.height() < 1)
    return;

#ifdef SIGILCOMPOSE_SKETCH_GPU
  if (m_graphiteContext) {
    bool rendered = false;
    {
      SkiaOffscreenSurface surface(*m_graphiteContext, texture, pixelSize);
      if (SkCanvas *canvas = surface.canvas()) {
        QMutexLocker lock(&ComposeSketchView::hostMutex);
        renderScene(*canvas, pixelSize);
        surface.submit();
        host->markPresented();
        runPendingCaptures();
        rendered = true;
      }
    }
    if (rendered) {
      update();
      return;
    }
    // Latch the CPU fallback until Qt supplies a new QRhi — and purge the
    // composer's caches first: images minted by this context cannot replay
    // onto a raster canvas.
    std::fprintf(stderr, "[compose sketch] Graphite texture wrap failed; "
                         "switching to CPU raster\n");
    QMutexLocker lock(&ComposeSketchView::hostMutex);
    m_graphiteContext.reset();
    host->setCaptureBackend({});
    if (host->composer())
      host->composer()->purgeCaches();
    backendMode.store(2);
  }
#endif

  // Portable fallback: one reusable raster buffer and one explicit upload
  // (same shape as ComposeGallery's).
  m_rasterPixels.resize(static_cast<size_t>(pixelSize.width()) *
                        static_cast<size_t>(pixelSize.height()));
  sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(
      SkImageInfo::Make(pixelSize.width(), pixelSize.height(),
                        kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      m_rasterPixels.data(),
      static_cast<size_t>(pixelSize.width()) * sizeof(uint32_t));
  if (!surface)
    return;
  {
    QMutexLocker lock(&ComposeSketchView::hostMutex);
    renderScene(*surface->getCanvas(), pixelSize);
    host->markPresented();
    runPendingCaptures();
  }

  QRhiResourceUpdateBatch *batch = rhi()->nextResourceUpdateBatch();
  const QByteArray uploadBytes = QByteArray::fromRawData(
      reinterpret_cast<const char *>(m_rasterPixels.data()),
      static_cast<qsizetype>(m_rasterPixels.size() * sizeof(uint32_t)));
  QRhiTextureSubresourceUploadDescription sub(uploadBytes);
  batch->uploadTexture(texture, QRhiTextureUploadDescription({0, 0, sub}));
  commandBuffer->resourceUpdate(batch);
  update();
}

ComposeSketchView::ComposeSketchView(QQuickItem *parent)
    : QQuickRhiItem(parent) {
  setAutoRenderTarget(false);
  setAlphaBlending(false);
  m_timer.setInterval(16);
  QObject::connect(&m_timer, &QTimer::timeout, this, [this] {
    QMutexLocker lock(&hostMutex);
    if (!host)
      return;
    host->poll();
    const QString status = QString::fromStdString(host->status());
    const QString error = QString::fromStdString(host->errorLog());
    static const char *kStateNames[] = {"waiting", "compiling", "live",
                                        "failed"};
    const QString state = kStateNames[(int)host->state()];
    if (status != m_status || error != m_errorLog ||
        m_compiling != host->compiling() || state != m_state) {
      m_status = status;
      m_errorLog = error;
      m_compiling = host->compiling();
      m_state = state;
      emit stateChanged();
    }
    if (host->live()) {
      const int mode = backendMode.load();
      m_metrics = QString("%1work %2 ms (p99 %3)   %4 fps")
                      .arg(mode == 1   ? "Graphite GPU   "
                           : mode == 2 ? "CPU raster   "
                                       : "")
                      .arg(host->workMsAverage(), 0, 'f', 2)
                      .arg(host->workMsP99(), 0, 'f', 2)
                      .arg(host->presentedFps(), 0, 'f', 0);
      emit metricsChanged();
    }
    update();
  });
  m_timer.start();
}

ComposeSketchView::~ComposeSketchView() = default;

QQuickRhiItemRenderer *ComposeSketchView::createRenderer() {
  return new ComposeSketchRenderer;
}

void ComposeSketchView::capture() {
  ++m_captureRequests;
  update();
}
