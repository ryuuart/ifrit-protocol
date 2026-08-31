/** @file
 * The render thread's half of the live canvas, and the GUI thread's poll.
 */

#include "SketchbookView.h"

#ifdef SIGILSKETCH_BOOK_GPU
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilskia/qt/QtInterop.h>
#endif

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <rhi/qrhi.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/live/Host.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <QtCore/QByteArray>
#include <QtCore/QMetaObject>
#include <QtCore/QMutexLocker>
#include <QtCore/QSize>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

namespace sketch = sigil::sketch;

std::filesystem::path SketchbookView::sketchDir;
std::filesystem::path SketchbookView::assetsDir;
std::filesystem::path SketchbookView::flagsFile;
sketch::Host* SketchbookView::host = nullptr;
// QMutex's constructor does not throw
// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
QMutex SketchbookView::hostMutex;

namespace {

/** Which path frames are actually taking, published to the status bar. A
 *  sketch running full-screen live materials on the CPU raster fallback
 *  is slow for reasons that have nothing to do with the sketch, so the
 *  backend stays visible rather than inferred. Written on the render
 *  thread, read on the GUI thread's poll. */
std::atomic<int> g_backend{0};  // 0 unknown, 1 Graphite GPU, 2 CPU raster

sigil::weave::FontContext& fonts() {
  // Leaked deliberately: it owns Skia-backed state, and a static
  // destructor racing Skia teardown is a class of crash worth not
  // having.
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

}  // namespace

class SketchbookRenderer final : public QQuickRhiItemRenderer {
 public:
  void initialize(QRhiCommandBuffer* commandBuffer) override;
  void synchronize(QQuickRhiItem* item) override;
  void render(QRhiCommandBuffer* commandBuffer) override;

 private:
  void drawSketch(SkCanvas& canvas, QSize pixelSize);
  void runPendingCaptures();   // hostMutex must be held
  void openSketch(int index);  // hostMutex must be held
  void publishMetrics();       // hostMutex must be held

#ifdef SIGILSKETCH_BOOK_GPU
  bool readbackGraphite(SkSurface& surface, const SkPixmap& out);
  // Declared before everything Skia so reverse destruction releases any
  // Graphite-backed images before the context goes.
  std::unique_ptr<sigil::skia::GraphiteContext> m_graphiteContext;
#endif
  SketchbookView* m_view = nullptr;
  QRhi* m_rhi = nullptr;
  bool m_initialized = false;
  std::vector<uint32_t> m_rasterPixels;
  QSize m_logicalSize;
  int m_requestedIndex = 0;
  int m_index = -1;
  int m_pendingCaptures = 0;
  int m_frameCount = 0;
  bool m_paused = false;
  bool m_metricsDirty = true;
  double m_timeScale = 1.0;
  double m_submitMsAverage = 0.0;
  std::chrono::steady_clock::time_point m_lastFrame;
};

void SketchbookRenderer::initialize(QRhiCommandBuffer* /*commandBuffer*/) {
  QRhi* currentRhi = rhi();
  if (m_initialized && m_rhi == currentRhi) return;

  QMutexLocker lock(&SketchbookView::hostMutex);
#ifdef SIGILSKETCH_BOOK_GPU
  m_graphiteContext.reset();
  if (SketchbookView::host) SketchbookView::host->setCaptureBackend({});
  // Metal only: the Vulkan adapter does not yet hand the image's final
  // layout back to QRhi's state tracker, so Qt's later sampling of it
  // would be undefined.
  if (currentRhi && currentRhi->backend() == QRhi::Metal)
    m_graphiteContext = sigil::skia::createGraphiteContext(currentRhi);
  if (m_graphiteContext && SketchbookView::host) {
    SketchbookView::host->setCaptureBackend(
        {[this](const SkImageInfo& info) -> sk_sp<SkSurface> {
           if (!m_graphiteContext) return nullptr;
           return SkSurfaces::RenderTarget(m_graphiteContext->recorder(), info);
         },
         [this](SkSurface& surface, const SkPixmap& out) {
           return readbackGraphite(surface, out);
         }});
  }
  g_backend.store(m_graphiteContext ? 1 : 2);
#else
  g_backend.store(2);
#endif
  m_rhi = currentRhi;
  m_initialized = true;
  // A replacement QRhi invalidates every image the old context minted, so
  // the sketch is reopened rather than replayed onto a backend that never
  // saw its caches.
  m_index = -1;
  m_frameCount = 0;
  m_metricsDirty = true;
  m_submitMsAverage = 0.0;
  std::fprintf(stderr, "[sketchbook] renderer: %s\n",
               g_backend.load() == 1 ? "Graphite GPU" : "CPU raster fallback");
}

void SketchbookRenderer::synchronize(QQuickRhiItem* item) {
  auto* view = static_cast<SketchbookView*>(item);
  m_view = view;
  const bool pauseStarted = !m_paused && view->m_paused;
  m_paused = view->m_paused;
  m_timeScale = view->m_timeScale;
  m_requestedIndex = view->m_sketchIndex;
  m_pendingCaptures += view->m_captureRequests;
  view->m_captureRequests = 0;
  m_logicalSize = QSize(std::max(1, (int)std::lround(view->width())),
                        std::max(1, (int)std::lround(view->height())));
  if (pauseStarted) m_metricsDirty = true;
  if (view->m_orbitDirty) {
    view->m_orbitDirty = false;
    QMutexLocker lock(&SketchbookView::hostMutex);
    if (SketchbookView::host)
      if (sketch::Session* session = SketchbookView::host->session())
        session->viewpoint(view->m_yawDeg, view->m_pitchDeg,
                           view->m_distance > 0 ? view->m_distance : 480.0f);
  }
}

void SketchbookRenderer::openSketch(int index) {
  const auto& entries = sketch::registry();
  if (index < 0 || index >= (int)entries.size()) return;
  const sketch::Entry& entry = entries[index];
  sketch::Host::Options options;
  options.sketchPath =
      SketchbookView::sketchDir / (std::string(entry.key) + ".cpp");
  options.assetsDir = SketchbookView::assetsDir;
  options.flagsFile = SketchbookView::flagsFile;
  options.compiledIn = &entry;
  delete SketchbookView::host;
  SketchbookView::host = new sketch::Host(std::move(options), fonts());
  m_frameCount = 0;
  m_metricsDirty = true;
  m_submitMsAverage = 0.0;
  m_lastFrame = {};
  const bool orbits = SketchbookView::host->session() &&
                      SketchbookView::host->session()->hasViewpoint();
  if (m_view) {
    m_view->m_orbitable = orbits;
    QMetaObject::invokeMethod(m_view, &SketchbookView::sketchIndexChanged,
                              Qt::QueuedConnection);
  }
}

void SketchbookRenderer::publishMetrics() {
  if (!m_view || !SketchbookView::host) return;
  sketch::Session* session = SketchbookView::host->session();
  if (!session) return;
  const auto& entries = sketch::registry();
  const char* backend = g_backend.load() == 1 ? "Graphite GPU" : "CPU raster";
  QVariantMap metrics;
  metrics.insert(QStringLiteral("backend"), QLatin1String(backend));
  if (m_index >= 0 && m_index < (int)entries.size())
    metrics.insert(QStringLiteral("sketch"),
                   QString::fromUtf8(entries[m_index].name));
  const SkSize size = session->canvas().size;
  metrics.insert(
      QStringLiteral("canvas"),
      QStringLiteral("%1x%2").arg((int)size.width()).arg((int)size.height()));
  // fps: what the window actually presents. headroom: what the frame's
  // work alone would allow — the number a frame-time floor is judged on.
  metrics.insert(QStringLiteral("fps"), SketchbookView::host->presentedFps());
  const double work = SketchbookView::host->workMsAverage();
  metrics.insert(QStringLiteral("workMs"), work);
  metrics.insert(QStringLiteral("p99Ms"), SketchbookView::host->workMsP99());
  metrics.insert(QStringLiteral("headroomFps"), work > 0 ? 1000.0 / work : 0.0);
  metrics.insert(QStringLiteral("submitMs"), m_submitMsAverage);
  metrics.insert(QStringLiteral("counters"),
                 QString::fromStdString(session->counters()));
  QVariantList lanes;
  for (const sketch::Lane& lane : session->lanes()) {
    QVariantMap row;
    row.insert(QStringLiteral("name"), QString::fromUtf8(lane.name));
    row.insert(QStringLiteral("ms"), lane.ms);
    lanes.push_back(row);
  }
  metrics.insert(QStringLiteral("lanes"), lanes);
  m_view->m_metrics = std::move(metrics);
  QMetaObject::invokeMethod(m_view, &SketchbookView::metricsChanged,
                            Qt::QueuedConnection);
}

void SketchbookRenderer::drawSketch(SkCanvas& canvas, QSize pixelSize) {
  sketch::Host* host = SketchbookView::host;
  const int width = pixelSize.width();
  const int height = pixelSize.height();
  // Letterbox to the SKETCH's own canvas rather than to the item: a
  // sketch declares its own dimensions and they do not share an aspect
  // ratio, so stretching one to fill would distort what it shows. The
  // matte around it stays dark so the sketch's own edge reads; inside
  // the clip its declared background takes over.
  canvas.clear(SkColorSetRGB(0x0b, 0x0a, 0x14));
  if (!host || !host->live()) return;
  const SkSize size = host->canvasSize();
  const float scale =
      std::min((float)width / size.width(), (float)height / size.height());
  canvas.save();
  canvas.translate((width - size.width() * scale) / 2,
                   (height - size.height() * scale) / 2);
  canvas.scale(scale, scale);
  canvas.clipRect(SkRect::MakeWH(size.width(), size.height()));
  canvas.clear(host->background().toSkColor());
  // Wall time, scaled and pausable: the frame the reader sees advances
  // by what actually elapsed, not by a nominal step.
  const auto now = std::chrono::steady_clock::now();
  double dt = 0.0;
  if (m_lastFrame.time_since_epoch().count() != 0 && !m_paused)
    dt = std::chrono::duration<double>(now - m_lastFrame).count() * m_timeScale;
  m_lastFrame = now;
  host->frame(canvas, dt);
  canvas.restore();
  host->markPresented();
  if (++m_frameCount % 15 == 0) m_metricsDirty = true;
}

void SketchbookRenderer::runPendingCaptures() {
  sketch::Host* host = SketchbookView::host;
  while (m_pendingCaptures > 0) {
    --m_pendingCaptures;
    QString result;
    if (host && host->live()) {
      namespace fs = std::filesystem;
      const fs::path dir = host->sketchPath().parent_path() / "captures";
      const std::string stem = host->sketchPath().stem().string();
      fs::path out;
      for (int n = 1; n < 10000; ++n) {
        char name[256];
        std::snprintf(name, sizeof name, "%s-%03d.png", stem.c_str(), n);
        out = dir / name;
        if (!fs::exists(out)) break;
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

#ifdef SIGILSKETCH_BOOK_GPU
bool SketchbookRenderer::readbackGraphite(SkSurface& surface,
                                          const SkPixmap& out) {
  if (!m_graphiteContext) return false;
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
        auto* read = static_cast<ReadContext*>(context);
        read->result = std::move(result);
        read->called = true;
      },
      &readContext);
  skgpu::graphite::SubmitInfo submitInfo;
  submitInfo.fSync = skgpu::graphite::SyncToCpu::kYes;
  m_graphiteContext->context()->submit(submitInfo);
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!readContext.called && std::chrono::steady_clock::now() < deadline) {
    m_graphiteContext->context()->checkAsyncWorkCompletion();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (!readContext.result) return false;
  const auto* src = static_cast<const uint8_t*>(readContext.result->data(0));
  const size_t srcRowBytes = readContext.result->rowBytes(0);
  const size_t copyBytes = std::min(srcRowBytes, out.rowBytes());
  for (int y = 0; y < out.height(); ++y)
    std::memcpy(out.writable_addr(0, y), src + (size_t)y * srcRowBytes,
                copyBytes);
  return true;
}
#endif

void SketchbookRenderer::render(QRhiCommandBuffer* commandBuffer) {
  using Clock = std::chrono::steady_clock;
  QRhiTexture* texture = colorTexture();
  if (!texture || m_logicalSize.width() < 1 || m_logicalSize.height() < 1)
    return;
  const QSize pixelSize = texture->pixelSize();
  if (pixelSize.width() < 1 || pixelSize.height() < 1) return;

#ifdef SIGILSKETCH_BOOK_GPU
  if (m_graphiteContext) {
    bool rendered = false;
    {
      sigil::skia::OffscreenSurface surface =
          sigil::skia::wrapTexture(*m_graphiteContext, texture, pixelSize);
      if (SkCanvas* canvas = surface.canvas()) {
        QMutexLocker lock(&SketchbookView::hostMutex);
        if (m_index != m_requestedIndex) {
          m_index = m_requestedIndex;
          openSketch(m_index);
        }
        drawSketch(*canvas, pixelSize);
        const auto submitStart = Clock::now();
        surface.submit();
        const double submitMs = std::chrono::duration<double, std::milli>(
                                    Clock::now() - submitStart)
                                    .count();
        m_submitMsAverage = m_submitMsAverage == 0.0
                                ? submitMs
                                : m_submitMsAverage * 0.95 + submitMs * 0.05;
        runPendingCaptures();
        if (m_metricsDirty) {
          m_metricsDirty = false;
          publishMetrics();
        }
        rendered = true;
      }
    }
    if (rendered) {
      update();
      return;
    }
    // Latch the CPU fallback until Qt supplies a new QRhi: images minted
    // by this context cannot replay onto a raster canvas, so the sketch
    // is reopened rather than replayed.
    std::fprintf(stderr,
                 "[sketchbook] Graphite texture wrap failed; switching to "
                 "CPU raster\n");
    QMutexLocker lock(&SketchbookView::hostMutex);
    m_graphiteContext.reset();
    if (SketchbookView::host) SketchbookView::host->setCaptureBackend({});
    g_backend.store(2);
    m_index = -1;
  }
#endif

  // Portable fallback: one reusable raster buffer and one explicit
  // upload, so the frame is copied only by the backend's staging upload.
  m_rasterPixels.resize((size_t)pixelSize.width() * (size_t)pixelSize.height());
  sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(
      SkImageInfo::Make(pixelSize.width(), pixelSize.height(),
                        kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      m_rasterPixels.data(), (size_t)pixelSize.width() * sizeof(uint32_t));
  if (!surface) return;
  {
    QMutexLocker lock(&SketchbookView::hostMutex);
    if (m_index != m_requestedIndex) {
      m_index = m_requestedIndex;
      openSketch(m_index);
    }
    drawSketch(*surface->getCanvas(), pixelSize);
    runPendingCaptures();
    if (m_metricsDirty) {
      m_metricsDirty = false;
      publishMetrics();
    }
  }

  const auto submitStart = Clock::now();
  QRhiResourceUpdateBatch* batch = rhi()->nextResourceUpdateBatch();
  // fromRawData keeps this upload view non-owning; the render-thread
  // buffer stays stable through QRhi's endFrame.
  const QByteArray uploadBytes = QByteArray::fromRawData(
      reinterpret_cast<const char*>(m_rasterPixels.data()),
      (qsizetype)m_rasterPixels.size() * (qsizetype)sizeof(uint32_t));
  QRhiTextureSubresourceUploadDescription sub(uploadBytes);
  batch->uploadTexture(texture, QRhiTextureUploadDescription({0, 0, sub}));
  commandBuffer->resourceUpdate(batch);
  const double submitMs =
      std::chrono::duration<double, std::milli>(Clock::now() - submitStart)
          .count();
  m_submitMsAverage = m_submitMsAverage == 0.0
                          ? submitMs
                          : m_submitMsAverage * 0.95 + submitMs * 0.05;
  update();
}

SketchbookView::SketchbookView(QQuickItem* parent) : QQuickRhiItem(parent) {
  // We draw into colorTexture() directly; no QRhi render target or depth
  // buffer is needed for this item.
  setAutoRenderTarget(false);
  setAlphaBlending(false);
  m_timer.setInterval(16);
  QObject::connect(&m_timer, &QTimer::timeout, this, [this] {
    QMutexLocker lock(&hostMutex);
    if (!host) return;
    host->poll();
    const QString status = QString::fromStdString(host->status());
    const QString error = QString::fromStdString(host->errorLog());
    static const char* kStateNames[] = {"waiting", "compiling", "live",
                                        "failed"};
    const QString state = kStateNames[(int)host->state()];
    if (status != m_status || error != m_errorLog || state != m_state) {
      m_status = status;
      m_errorLog = error;
      m_state = state;
      emit stateChanged();
    }
    update();
  });
  m_timer.start();
}

SketchbookView::~SketchbookView() = default;

QQuickRhiItemRenderer* SketchbookView::createRenderer() {
  return new SketchbookRenderer;
}

QVariantList SketchbookView::sketches() const {
  QVariantList result;
  const auto& entries = sketch::registry();
  result.reserve((qsizetype)entries.size());
  for (int i = 0; i < (int)entries.size(); ++i) {
    const sketch::Entry& entry = entries[i];
    QVariantMap item;
    // The index travels with the row: the sidebar groups and filters, so
    // a row's position says nothing about which sketch it selects.
    item.insert(QStringLiteral("sketchIndex"), i);
    item.insert(QStringLiteral("name"),
                QString::fromStdString(sketch::title(entry.name)));
    item.insert(QStringLiteral("category"), QString::fromUtf8(entry.category));
    item.insert(QStringLiteral("tag"), QString::fromUtf8(entry.blurb));
    // A sketch also answers to its file stem — the thing you have open in
    // an editor when you want to find it here.
    item.insert(QStringLiteral("key"), QString::fromUtf8(entry.key));
    result.push_back(item);
  }
  return result;
}

void SketchbookView::setSketchIndex(int index) {
  if (index == m_sketchIndex || index < 0 ||
      index >= (int)sketch::registry().size())
    return;
  m_sketchIndex = index;
  emit sketchIndexChanged();
  update();
}

void SketchbookView::setPaused(bool paused) {
  if (paused == m_paused) return;
  m_paused = paused;
  emit pausedChanged();
  update();
}

void SketchbookView::setTimeScale(double scale) {
  if (!std::isfinite(scale)) return;
  scale = std::clamp(scale, 0.0, 16.0);
  if (scale == m_timeScale) return;
  m_timeScale = scale;
  emit timeScaleChanged();
  update();
}

void SketchbookView::capture() {
  ++m_captureRequests;
  update();
}

void SketchbookView::orbit(float yawDeg, float pitchDeg, float distance) {
  m_yawDeg = yawDeg;
  m_pitchDeg = std::clamp(pitchDeg, -85.0f, 85.0f);
  m_distance = distance;
  m_orbitDirty = true;
  update();
}
