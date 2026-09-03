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
#include <sigilmeasure/time/Stopwatch.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Sources.h>
#include <sigilsketch/live/Host.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <QtCore/QByteArray>
#include <QtCore/QMetaObject>
#include <QtCore/QMutexLocker>
#include <QtCore/QSize>
#include <QtGui/QKeySequence>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace motion = sigil::motion;

std::filesystem::path SketchbookView::sketchDir;
std::filesystem::path SketchbookView::assetsDir;
std::filesystem::path SketchbookView::flagsFile;
std::filesystem::path SketchbookView::sharedDir;
std::vector<std::filesystem::path> SketchbookView::externals;
sketch::Host* SketchbookView::host = nullptr;
sketch::Residency SketchbookView::sessions;
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

/** ONE INDEX OVER TWO LISTS: the registry first, the files this session
 *  was pointed at after it. An index below the registry's size selects a
 *  compiled-in sketch, one above it a file opened by path. */
int externalAt(int index) { return index - (int)sketch::registry().size(); }

/** What the sidebar and the metrics panel call the sketch at @p index —
 *  a registry entry's filed name, or the stem of the file the session was
 *  pointed at. Empty when the index selects neither. */
std::string nameOf(int index) {
  const auto& entries = sketch::registry();
  if (index >= 0 && index < (int)entries.size()) return entries[index].name;
  const int external = externalAt(index);
  if (external >= 0 && external < (int)SketchbookView::externals.size())
    return SketchbookView::externals[external].stem().string();
  return {};
}

/** HOW A CANVAS IS FITTED INTO A SURFACE: the scale that letterboxes a
 *  canvas of @p size into @p width by @p height, and where its top-left
 *  corner lands. The frame is drawn through this and a pointer is read
 *  back through its inverse, so it is written once. */
struct Fit {
  float scale = 1.0f;
  float x = 0.0f;
  float y = 0.0f;
};
Fit fitOf(SkSize size, float width, float height) {
  Fit fit;
  fit.scale = std::min(width / size.width(), height / size.height());
  fit.x = (width - size.width() * fit.scale) / 2;
  fit.y = (height - size.height() * fit.scale) / 2;
  return fit;
}

/** THE KEY AS A SKETCH READS IT: the name a keyboard spells it by, and
 *  the code p5 gives it. The keys p5 names get p5's numbers — the
 *  arrows, Enter, Escape, Backspace, Delete, Tab, the modifiers, the
 *  function keys — a key that types a character is that character, with
 *  a letter's code its upper-case ASCII the way p5 reports it, and any
 *  other key keeps Qt's name and number.
 *
 *  The modifiers are Qt's, which on macOS reports the Command key as
 *  Control: the key a shortcut is SPELLED with rather than the one the
 *  keyboard is engraved with. A sketch comparing against p5's numbers is
 *  comparing against the same key it would press to save a file. */
std::pair<std::string, int> keyAs(int qtKey, const QString& text) {
  switch (qtKey) {
    case Qt::Key_Left: return {"ArrowLeft", 37};
    case Qt::Key_Up: return {"ArrowUp", 38};
    case Qt::Key_Right: return {"ArrowRight", 39};
    case Qt::Key_Down: return {"ArrowDown", 40};
    case Qt::Key_Return:
    case Qt::Key_Enter: return {"Enter", 13};
    case Qt::Key_Escape: return {"Escape", 27};
    case Qt::Key_Backspace: return {"Backspace", 8};
    case Qt::Key_Delete: return {"Delete", 46};
    case Qt::Key_Tab: return {"Tab", 9};
    case Qt::Key_Space: return {" ", 32};
    case Qt::Key_Shift: return {"Shift", 16};
    case Qt::Key_Control: return {"Control", 17};
    case Qt::Key_Alt: return {"Alt", 18};
    case Qt::Key_Meta: return {"Meta", 91};
    case Qt::Key_Home: return {"Home", 36};
    case Qt::Key_End: return {"End", 35};
    case Qt::Key_PageUp: return {"PageUp", 33};
    case Qt::Key_PageDown: return {"PageDown", 34};
    case Qt::Key_Insert: return {"Insert", 45};
    default: break;
  }
  if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F12)
    return {"F" + std::to_string(qtKey - Qt::Key_F1 + 1),
            112 + (qtKey - Qt::Key_F1)};
  // The character it types — but only when it typed one a reader would
  // recognise: a chord over a letter types a control character, and a
  // sketch asked to compare against \x01 has been told which key was
  // pressed in a way it cannot use. Qt's own key is that letter.
  if (!text.isEmpty() && text.at(0).unicode() >= 0x20) {
    const QChar first = text.at(0);
    const int code = first.isLetter() ? first.toUpper().unicode()
                                      : (int)first.unicode();
    return {text.toStdString(), code};
  }
  if (qtKey >= 0x20 && qtKey <= 0x7e) {
    const QChar typed = QChar(qtKey);
    return {QString(typed.toLower()).toStdString(),
            (int)typed.toUpper().unicode()};
  }
  return {QKeySequence(qtKey).toString().toStdString(), qtKey};
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
  /** Routes a session's captures through this renderer's own context.
   *  Once live frames render on the device, the runtime's caches hold
   *  device-backed images that cannot replay onto a raster canvas, so
   *  every session gets this as it is built rather than only the first. */
  void installCaptureBackend(sketch::Host& host);

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
  /** The clock the presented frame advances by. The pause and the time
   *  scale are the view's published properties, pushed into it on every
   *  synchronize; what the clock adds on top is the stall clamp, so
   *  dragging the window or stopping in a debugger resumes at the next
   *  frame instead of jumping every animation forward by the length of
   *  the pause. */
  motion::FrameClock m_clock;
};

void SketchbookRenderer::initialize(QRhiCommandBuffer* /*commandBuffer*/) {
  QRhi* currentRhi = rhi();
  if (m_initialized && m_rhi == currentRhi) return;

  QMutexLocker lock(&SketchbookView::hostMutex);
  // A REPLACEMENT QRhi INVALIDATES EVERY IMAGE THE OLD CONTEXT MINTED,
  // and a resident session holds them: its caches cannot replay onto a
  // backend that never saw them. So the whole set goes, and it goes
  // before the context does — released after it, the images it holds
  // would be freed against a context that is no longer there.
  SketchbookView::sessions.clear();
  SketchbookView::host = nullptr;
#ifdef SIGILSKETCH_BOOK_GPU
  m_graphiteContext.reset();
  // Metal only: the Vulkan adapter does not yet hand the image's final
  // layout back to QRhi's state tracker, so Qt's later sampling of it
  // would be undefined.
  if (currentRhi && currentRhi->backend() == QRhi::Metal)
    m_graphiteContext = sigil::skia::createGraphiteContext(currentRhi);
  g_backend.store(m_graphiteContext ? 1 : 2);
#else
  g_backend.store(2);
#endif
  m_rhi = currentRhi;
  m_initialized = true;
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
  m_clock.setPaused(m_paused);
  m_clock.setTimeScale(m_timeScale);
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
        session->viewpoint(view->m_yawDeg, view->m_pitchDeg, view->m_distance);
  }
}

void SketchbookRenderer::openSketch(int index) {
  const auto& entries = sketch::registry();
  sketch::Host::Options options;
  if (index >= 0 && index < (int)entries.size()) {
    // A sketch this binary carries opens instantly: the host starts from
    // the compiled-in entry and builds only once the file changes.
    options.compiledIn = &entries[index];
    options.sketchPath =
        sketch::sourceOf(SketchbookView::sketchDir, entries[index].key);
  } else if (const int external = externalAt(index);
             external >= 0 &&
             external < (int)SketchbookView::externals.size()) {
    // A file this binary does not carry has to be built to be seen, so
    // it opens on the compiler rather than on an entry.
    options.sketchPath = SketchbookView::externals[external];
  } else {
    return;
  }
  options.assetsDir = SketchbookView::assetsDir;
  options.flagsFile = SketchbookView::flagsFile;
  options.sharedDir = SketchbookView::sharedDir;
  // The file is the session's name: it is what distinguishes a registry
  // entry from every other, and a file opened by path from every other.
  const std::string key = options.sketchPath.string();
  const sketch::Residency::Presented presented =
      SketchbookView::sessions.present(key, [this, &options] {
        auto host = std::make_unique<sketch::Host>(std::move(options), fonts());
        installCaptureBackend(*host);
        return host;
      });
  SketchbookView::host = presented.host;
  if (!SketchbookView::host) return;
  // Residency keeps the expensive host/compiler warm, but PRESENTATION is
  // a fresh run. In particular, a retained Composer's mount transitions have
  // already finished; merely resuming it makes entrance-heavy sketches look
  // inert when revisited.
  if (!presented.opened) SketchbookView::host->restartSession();
  m_frameCount = 0;
  m_submitMsAverage = 0.0;
  m_metricsDirty = true;
  m_clock = motion::FrameClock{};  // a new sketch starts at its own zero
  // synchronize() applied these before openSketch(). Replacing the clock
  // above must not silently unpause it or return it to normal speed.
  m_clock.setPaused(m_paused);
  m_clock.setTimeScale(m_timeScale);
  const bool orbits = SketchbookView::host->session() &&
                      SketchbookView::host->session()->hasViewpoint();
  if (m_view) {
    m_view->m_orbitable = orbits;
    QMetaObject::invokeMethod(m_view, &SketchbookView::sketchIndexChanged,
                              Qt::QueuedConnection);
  }
}

void SketchbookRenderer::installCaptureBackend(sketch::Host& host) {
#ifdef SIGILSKETCH_BOOK_GPU
  if (!m_graphiteContext) return;
  host.setCaptureBackend({[this](const SkImageInfo& info) -> sk_sp<SkSurface> {
                            if (!m_graphiteContext) return nullptr;
                            return SkSurfaces::RenderTarget(
                                m_graphiteContext->recorder(), info);
                          },
                          [this](SkSurface& surface, const SkPixmap& out) {
                            return readbackGraphite(surface, out);
                          }});
#else
  (void)host;
#endif
}

void SketchbookRenderer::publishMetrics() {
  if (!m_view || !SketchbookView::host) return;
  sketch::Session* session = SketchbookView::host->session();
  if (!session) return;
  const char* backend = g_backend.load() == 1 ? "Graphite GPU" : "CPU raster";
  QVariantMap metrics;
  metrics.insert(QStringLiteral("backend"), QLatin1String(backend));
  if (const std::string name = nameOf(m_index); !name.empty())
    metrics.insert(QStringLiteral("sketch"), QString::fromStdString(name));
  // WHAT THE BODY DECLARED, which is only knowable once it has run: a
  // sketch states its size, its ground and the moment it is worth
  // photographing from inside its own setup. The browser keeps what it
  // is told here, so a row reads its canvas back after a look at
  // something else.
  metrics.insert(QStringLiteral("sketchIndex"), m_index);
  const sketch::CanvasSpec& spec = session->canvas();
  const SkSize size = spec.size;
  metrics.insert(
      QStringLiteral("canvas"),
      QStringLiteral("%1x%2").arg((int)size.width()).arg((int)size.height()));
  metrics.insert(QStringLiteral("moment"), spec.captureSeconds);
  const SkColor colour = spec.background.toSkColor();
  metrics.insert(QStringLiteral("background"),
                 QStringLiteral("#%1").arg((uint)(colour & 0x00ffffffU), 6, 16,
                                           QLatin1Char('0')));
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

  // WHERE THE SKETCH IS SEEN FROM, published whether or not a pointer
  // has moved it: a drag reads this at the moment it starts, so the
  // first one continues the sketch's own framing and every one after it
  // continues where the last left off.
  if (const std::optional<sketch::Orbit> orbit = session->orbit()) {
    m_view->m_orbit = *orbit;
    QMetaObject::invokeMethod(m_view, &SketchbookView::orbitChanged,
                              Qt::QueuedConnection);
  }
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
  const Fit fit = fitOf(size, (float)width, (float)height);
  canvas.save();
  canvas.translate(fit.x, fit.y);
  canvas.scale(fit.scale, fit.scale);
  canvas.clipRect(SkRect::MakeWH(size.width(), size.height()));
  canvas.clear(host->background().toSkColor());
  // Wall time, scaled, pausable and stall-clamped: the frame the reader
  // sees advances by what actually elapsed, not by a nominal step.
  host->frame(canvas, m_clock.tick());
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
        const sigil::measure::Stopwatch submitWatch;
        surface.submit();
        const double submitMs = submitWatch.elapsedMs();
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
    // by this context cannot replay onto a raster canvas, so every
    // resident session goes rather than being replayed — and goes before
    // the context that made its images does.
    std::fprintf(stderr,
                 "[sketchbook] Graphite texture wrap failed; switching to "
                 "CPU raster\n");
    QMutexLocker lock(&SketchbookView::hostMutex);
    SketchbookView::sessions.clear();
    SketchbookView::host = nullptr;
    m_graphiteContext.reset();
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

  const sigil::measure::Stopwatch submitWatch;
  QRhiResourceUpdateBatch* batch = rhi()->nextResourceUpdateBatch();
  // fromRawData keeps this upload view non-owning; the render-thread
  // buffer stays stable through QRhi's endFrame.
  const QByteArray uploadBytes = QByteArray::fromRawData(
      reinterpret_cast<const char*>(m_rasterPixels.data()),
      (qsizetype)m_rasterPixels.size() * (qsizetype)sizeof(uint32_t));
  QRhiTextureSubresourceUploadDescription sub(uploadBytes);
  batch->uploadTexture(texture, QRhiTextureUploadDescription({0, 0, sub}));
  commandBuffer->resourceUpdate(batch);
  const double submitMs = submitWatch.elapsedMs();
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

void SketchbookView::setSketchIndex(int index) {
  if (index == m_sketchIndex || index < 0 ||
      index >= (int)(sketch::registry().size() + externals.size()))
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
  // A distance of nothing is not a viewpoint: a drag that arrived before
  // the running sketch had said where it stands is left alone rather
  // than answered with a number of this host's own.
  if (!(distance > 0.0f)) return;
  m_yawDeg = yawDeg;
  m_pitchDeg = std::clamp(pitchDeg, -85.0f, 85.0f);
  m_distance = distance;
  m_orbitDirty = true;
  update();
}

void SketchbookView::pointer(qreal x, qreal y, bool pressed) {
  // Under the same lock the render thread draws under, the way the poll
  // is: the session is one object, and the frame it is drawing is not
  // interrupted by a point arriving.
  QMutexLocker lock(&hostMutex);
  if (!host || !host->live()) return;
  sketch::Session* session = host->session();
  if (!session) return;
  // THE SAME FIT THE FRAME IS DRAWN WITH, in this item's own units: the
  // canvas letterboxed into the item, so a point on the item is a point
  // on the declared canvas by the inverse of that fit.
  const SkSize size = host->canvasSize();
  const auto fit = fitOf(size, (float)width(), (float)height());
  session->pointer(((float)x - fit.x) / fit.scale,
                   ((float)y - fit.y) / fit.scale, pressed);
}

void SketchbookView::key(int qtKey, const QString& text, bool pressed) {
  const auto [name, code] = keyAs(qtKey, text);
  QMutexLocker lock(&hostMutex);
  if (!host || !host->live()) return;
  if (sketch::Session* session = host->session())
    session->key(name, code, pressed);
}
