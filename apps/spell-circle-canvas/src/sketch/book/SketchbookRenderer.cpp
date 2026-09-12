/** @file
 * What the render thread does with the presented session: opens it,
 * draws its frame into the item's texture, photographs it for the store
 * and publishes what it cost.
 */

#include "SketchbookRenderer.h"

#ifdef SIGILSKETCH_BOOK_GPU
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilskia/graphite/PaintOrder.h>
#include <sigilskia/qt/QtInterop.h>
#endif

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <rhi/qrhi.h>
#include <sigilmeasure/time/Stopwatch.h>
#include <sigilsketch/core/Fit.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Sources.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/plate/Thumbnails.h>
#include <sigilweave/fonts/FontContext.h>

#include <QtCore/QByteArray>
#include <QtCore/QMetaObject>
#include <QtCore/QMutexLocker>
#include <QtQuick/QQuickWindow>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#include "SketchCatalog.h"
#include "SketchbookView.h"

namespace sketch = sigil::sketch;
namespace motion = sigil::motion;

namespace {

/** Which path frames are actually taking, published to the status bar. A
 *  sketch running full-screen live materials on the CPU raster fallback
 *  is slow for reasons that have nothing to do with the sketch, so the
 *  backend stays visible rather than inferred. Written on the render
 *  thread, read on the GUI thread's poll. */
std::atomic<int> g_backend{0};  // 0 unknown, 1 Graphite GPU, 2 CPU raster

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
  if (external >= 0 && external < (int)SketchCatalog::externals.size())
    return SketchCatalog::externals[external].stem().string();
  return {};
}

}  // namespace

SketchbookRenderer::SketchbookRenderer() = default;
SketchbookRenderer::~SketchbookRenderer() = default;

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
  m_logicalSize = QSizeF(view->width(), view->height());
  m_deviceRatio = view->window()
                      ? (float)view->window()->effectiveDevicePixelRatio()
                      : 1.0f;
  if (pauseStarted) m_metricsDirty = true;
  if (view->m_orbitDirty) {
    view->m_orbitDirty = false;
    QMutexLocker lock(&SketchbookView::hostMutex);
    if (SketchbookView::host)
      if (sketch::Session* session = SketchbookView::host->session())
        session->viewpoint(view->m_yawDeg, view->m_pitchDeg, view->m_distance);
  }
}

std::unique_ptr<sketch::Host> SketchbookRenderer::openSketch(int index) {
  const auto& entries = sketch::registry();
  sketch::Host::Options options;
  if (index >= 0 && index < (int)entries.size()) {
    // A sketch this binary carries opens instantly: the host starts from
    // the compiled-in entry and builds only once the file changes.
    options.compiledIn = &entries[index];
    options.sketchPath =
        sketch::sourceOf(SketchCatalog::sketchDirectory, entries[index].key);
  } else if (const int external = externalAt(index);
             external >= 0 && external < (int)SketchCatalog::externals.size()) {
    // A file this binary does not carry has to be built to be seen, so
    // it opens on the compiler rather than on an entry.
    options.sketchPath = SketchCatalog::externals[external];
  } else {
    return nullptr;
  }
  if (!SketchbookView::fonts) return nullptr;  // nothing shapes text yet
  options.assetsDirectory = SketchbookView::assetsDirectory;
  options.flagsFile = SketchbookView::flagsFile;
  // The file is the session's name: it is what distinguishes a registry
  // entry from every other, and a file opened by path from every other.
  const std::string key = options.sketchPath.string();
  sketch::Residency::Presented presented =
      SketchbookView::sessions.present(key, [this, &options] {
        auto host = std::make_unique<sketch::Host>(std::move(options),
                                                   *SketchbookView::fonts);
        installCaptureBackend(*host);
        return host;
      });
  SketchbookView::host = presented.host;
  if (!SketchbookView::host) return std::move(presented.evicted);
  // Residency keeps the expensive host/compiler warm, but PRESENTATION is
  // a fresh run. In particular, a retained Composer's mount transitions have
  // already finished; merely resuming it makes entrance-heavy sketches look
  // inert when revisited.
  if (!presented.opened) SketchbookView::host->restartSession();
  m_frameCount = 0;
  m_sceneSeconds = 0.0;
  m_thumbnailTaken = false;
  m_submitMsAverage = 0.0;
  m_metricsDirty = true;
  m_clock = motion::FrameClock{};  // a new sketch starts at its own zero
  // synchronize() applied these before openSketch(). Replacing the clock
  // above must not silently unpause it or return it to normal speed.
  m_clock.setPaused(m_paused);
  m_clock.setTimeScale(m_timeScale);
  const bool orbits = SketchbookView::host->session() &&
                      SketchbookView::host->session()->hasViewpoint();
  // THE ITEM'S OWN STATE IS WRITTEN ON THE GUI THREAD. This runs on the
  // render thread, outside synchronize(), while the GUI thread reads the
  // same members through the property getters — so the value travels in
  // the queued call rather than being assigned here and announced after.
  if (SketchbookView* view = m_view)
    QMetaObject::invokeMethod(
        view,
        [view, orbits] {
          view->m_orbitable = orbits;
          emit view->sketchIndexChanged();
        },
        Qt::QueuedConnection);
  return std::move(presented.evicted);
}

void SketchbookRenderer::installCaptureBackend(sketch::Host& host) {
#ifdef SIGILSKETCH_BOOK_GPU
  if (!m_graphiteContext) return;
  host.setCaptureBackend(
      {[this](const SkImageInfo& info) -> sk_sp<SkSurface> {
         if (!m_graphiteContext) return nullptr;
         return SkSurfaces::RenderTarget(m_graphiteContext->recorder(), info);
       },
       [this](SkSurface& surface, const SkPixmap& out) {
         return readbackGraphite(surface, out);
       },
       [this](SkSurface& surface) -> SkCanvas* {
         if (!m_graphiteContext) return nullptr;
         m_captureCanvas = std::make_unique<sigil::skia::PaintOrderCanvas>(
             *m_graphiteContext, surface.getCanvas());
         return m_captureCanvas.get();
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
  // WHICH RUNTIME THIS SKETCH DREW THROUGH, known only once it has been
  // built: a file opened by path learns its kind here, so the row in the
  // browser stops reading "not yet compiled" under a sketch that is live.
  if (const std::string_view runtime = SketchbookView::host->kind();
      !runtime.empty())
    metrics.insert(
        QStringLiteral("runtime"),
        QString::fromUtf8(runtime.data(), (qsizetype)runtime.size()));
  // WHAT THE BODY DECLARED, which is only knowable once it has run: a
  // sketch states its size, its ground and the moment it is worth
  // photographing from inside its own setup. The browser keeps what it
  // is told here, so a row reads its canvas back after a look at
  // something else.
  metrics.insert(QStringLiteral("sketchIndex"), m_index);
  const sketch::CanvasSpecification& specification = session->canvas();
  const SkSize size = specification.size;
  metrics.insert(
      QStringLiteral("canvas"),
      QStringLiteral("%1x%2").arg((int)size.width()).arg((int)size.height()));
  metrics.insert(QStringLiteral("moment"), specification.captureSeconds);
  const SkColor colour = specification.background.toSkColor();
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
  // THE MAP TRAVELS IN THE CALL. It is implicitly shared, and this is the
  // render thread: assigning it here and announcing it after would let
  // the GUI thread copy a map while its buckets were being replaced.
  SketchbookView* view = m_view;
  QMetaObject::invokeMethod(
      view,
      [view, metrics = std::move(metrics)]() mutable {
        view->m_metrics = std::move(metrics);
        emit view->metricsChanged();
      },
      Qt::QueuedConnection);

  // WHERE THE SKETCH IS SEEN FROM, published whether or not a pointer
  // has moved it: a drag reads this at the moment it starts, so the
  // first one continues the sketch's own framing and every one after it
  // continues where the last left off.
  if (const std::optional<sigil::geometry::mesh::camera::Orbit> orbit =
          session->orbit())
    QMetaObject::invokeMethod(
        view,
        [view, seen = *orbit] {
          view->m_orbit = seen;
          emit view->orbitChanged();
        },
        Qt::QueuedConnection);
}

void SketchbookRenderer::drawSketch(SkCanvas& canvas, QSize pixelSize) {
  sketch::Host* host = SketchbookView::host;
  // WHAT THE SCENE GRAPH WILL DO TO THIS TEXTURE, UNDONE IN ADVANCE. The
  // texture is stretched over the item whatever resolution it stands at,
  // so the frame is composed into a rectangle of the ITEM'S SHAPE and
  // that rectangle is scaled to fill the texture. A zoom grows the item
  // without changing its shape, so the two rectangles are the same one
  // and the matrix below is bit for bit the matrix of the frame before —
  // which is what lets every cached raster in the scene stand while the
  // gesture runs, since a cache asks whether its node is exactly where it
  // was. Only a shape that has really changed is compensated, and only
  // until the resolution settles on it.
  float width = (float)pixelSize.width();
  float height = (float)pixelSize.height();
  const double itemAspect = m_logicalSize.height() > 0
                                ? m_logicalSize.width() / m_logicalSize.height()
                                : 0.0;
  const double heldAspect =
      (double)pixelSize.width() / (double)pixelSize.height();
  if (itemAspect > 0 && std::abs(itemAspect - heldAspect) > 0.002 * heldAspect)
    height = (float)((double)width / itemAspect);
  // Letterbox to the SKETCH's own canvas rather than to the item: a
  // sketch declares its own dimensions and they do not share an aspect
  // ratio, so stretching one to fill would distort what it shows. The
  // matte around it stays dark so the sketch's own edge reads; inside
  // the clip its declared background takes over.
  canvas.clear(SkColorSetRGB(0x0b, 0x0a, 0x14));
  if (!host || !host->live()) return;
  const SkSize size = host->canvasSize();
  const sketch::Fit fit = sketch::fitInto(size, SkRect::MakeWH(width, height));
  canvas.save();
  // The compensation above, applied. It is the identity whenever the item
  // and the texture agree in shape, which is every frame of a zoom.
  canvas.scale((float)pixelSize.width() / width,
               (float)pixelSize.height() / height);
  canvas.translate(fit.x, fit.y);
  canvas.scale(fit.scale, fit.scale);
  canvas.clipRect(SkRect::MakeWH(size.width(), size.height()));
  canvas.clear(host->background().toSkColor());
  // Wall time, scaled, pausable and stall-clamped: the frame the reader
  // sees advances by what actually elapsed, not by a nominal step.
  // Bakes belong to the canvas the sketch declared, not to how far this
  // window has magnified it: taken at the screen's density, once.
  if (sketch::Session* session = host->session())
    session->setBakeDensity(m_deviceRatio);
  const double step = m_clock.tick();
  host->frame(canvas, step);
  m_sceneSeconds += step;
  canvas.restore();
  host->markPresented();
  if (++m_frameCount % 15 == 0) m_metricsDirty = true;
}

void SketchbookRenderer::refreshThumbnail() {
  // ONCE PER SKETCH OPENED, AT THE MOMENT THE SKETCH NAMED. A sketch
  // states from inside its own setup when a still of it is worth taking;
  // a sketch that names none is photographed after a second, by which
  // time whatever it mounts with has arrived. What the store then holds
  // is the frame the reader was looking at, which is why nothing needs
  // to be re-rendered in the background to keep it current.
  constexpr double kSettledSeconds = 1.0;
  if (m_thumbnailTaken || SketchCatalog::thumbnailDirectory.empty()) return;
  const auto& entries = sketch::registry();
  // A file opened by path has no row in the store: the store is keyed by
  // a registry sketch's filed name, and two drafts may share a stem.
  if (m_index < 0 || m_index >= (int)entries.size()) return;
  sketch::Host* host = SketchbookView::host;
  if (!host || !host->live()) return;
  sketch::Session* session = host->session();
  if (!session) return;
  const sketch::CanvasSpecification& specification = session->canvas();
  const double moment = specification.captureSeconds > 0
                            ? specification.captureSeconds
                            : kSettledSeconds;
  if (m_sceneSeconds < moment) return;
  // Taken whether or not it lands: a sketch whose still cannot be
  // written is not one to try again on every frame after its moment.
  m_thumbnailTaken = true;

  const sketch::Entry& entry = entries[m_index];
  const std::filesystem::path source =
      sketch::sourceOf(SketchCatalog::sketchDirectory, entry.key);
  const std::string key = sketch::thumbnailKey(source);
  const std::filesystem::path out =
      sketch::thumbnailFile(SketchCatalog::thumbnailDirectory, entry.name, key);
  const SkSize size = specification.size;
  const float longest = std::max(size.width(), size.height());
  if (!(longest > 0)) return;
  const float scale = std::min(1.0f, (float)sketch::kThumbnailWidth / longest);
  std::error_code code;
  std::filesystem::create_directories(out.parent_path(), code);
  if (!host->capture(out, scale)) return;
  sketch::pruneThumbnails(SketchCatalog::thumbnailDirectory, entry.name, out);
  if (m_view)
    QMetaObject::invokeMethod(
        m_view,
        [view = m_view, index = m_index] {
          emit view->thumbnailCaptured(index);
        },
        Qt::QueuedConnection);
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
      for (uint64_t n = 1;; ++n) {
        const std::string number = std::to_string(n);
        const std::string padding(number.size() < 3 ? 3 - number.size() : 0,
                                  '0');
        out = dir / (stem + "-" + padding + number + ".png");
        if (!fs::exists(out)) break;
      }
      // A CAPTURE IS PHOTOGRAPHED AT ITS OWN DENSITY. The live frame's
      // bakes are taken at the screen's, which is the resolution the
      // reader is looking at; a still written at twice that would blit
      // them up. So the density is raised for the photograph and put
      // back after, which costs the scene one re-bake each way — a price
      // an explicitly asked-for still can pay and a frame cannot.
      sketch::Session* session = host->session();
      if (session) session->setBakeDensity(2.0f * m_deviceRatio);
      const bool wrote = host->capture(out, 2.0f);
      if (session) session->setBakeDensity(m_deviceRatio);
      if (wrote) result = QString::fromStdString(out.string());
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
  if (!texture || !(m_logicalSize.width() > 0) ||
      !(m_logicalSize.height() > 0)) {
    update();  // keep asking for frames until there is something to draw into
    return;
  }
  const QSize pixelSize = texture->pixelSize();
  if (pixelSize.width() < 1 || pixelSize.height() < 1) {
    update();
    return;
  }

  // ONE SESSION AT A TIME: the outgoing one goes BEFORE the next opens,
  // so that letting it go is not work inside the frames of the sketch
  // that follows it. Outside the lock for the reason every release of a
  // host is: ~Host waits on the build it may be in the middle of. The
  // requested index is written on this thread, so reading it here needs
  // nothing held.
  if (SketchbookView::oneSessionAtATime && m_index != m_requestedIndex) {
    std::unique_ptr<sketch::Host> leaving;
    {
      QMutexLocker lock(&SketchbookView::hostMutex);
      leaving = SketchbookView::sessions.dropPresented();
      SketchbookView::host = SketchbookView::sessions.presented();
    }
    leaving.reset();
  }

#ifdef SIGILSKETCH_BOOK_GPU
  if (m_graphiteContext) {
    bool rendered = false;
    // LET GO OF AN EVICTED SESSION OUTSIDE THE LOCK: ~Host waits on the
    // build it may be in the middle of, which is a compiler run, and
    // this lock is the one every frame and every poll takes.
    std::unique_ptr<sketch::Host> evicted;
    {
      sigil::skia::OffscreenSurface surface =
          sigil::skia::wrapTexture(*m_graphiteContext, texture, pixelSize);
      if (SkCanvas* canvas = surface.canvas()) {
        QMutexLocker lock(&SketchbookView::hostMutex);
        if (m_index != m_requestedIndex) {
          m_index = m_requestedIndex;
          evicted = openSketch(m_index);
        }
        drawSketch(*canvas, pixelSize);
        const sigil::measure::Stopwatch submitWatch;
        surface.submit();
        const double submitMs = submitWatch.elapsedMs();
        m_submitMsAverage = m_submitMsAverage == 0.0
                                ? submitMs
                                : m_submitMsAverage * 0.95 + submitMs * 0.05;
        refreshThumbnail();
        runPendingCaptures();
        if (m_metricsDirty) {
          m_metricsDirty = false;
          publishMetrics();
        }
        rendered = true;
      }
    }
    evicted.reset();
    if (rendered) {
      update();
      return;
    }
    // A FAILED FRAME IS ONE SESSION'S FAILURE, not the context's. The
    // texture could not be wrapped for the sketch on screen — which a bad
    // pipeline in that one sketch can cause — so drop THAT session and
    // keep the shared Graphite context and every other resident warm,
    // rather than tearing the context down and making one sketch's bad
    // frame cost every other its state. m_index falls behind the request
    // so the next selection reopens, and update() is re-requested so the
    // window keeps asking for frames instead of going dark. A genuine QRhi
    // replacement is a different event, handled in initialize().
    std::fprintf(stderr,
                 "[sketchbook] Graphite frame failed for the current sketch; "
                 "dropping its session and keeping the context\n");
    std::unique_ptr<sketch::Host> dropped;
    {
      QMutexLocker lock(&SketchbookView::hostMutex);
      dropped = SketchbookView::sessions.dropPresented();
      SketchbookView::host = SketchbookView::sessions.presented();
      m_index = -1;
    }
    dropped.reset();  // outside the lock, for the reason above
    update();
    return;
  }
#endif

  // Portable fallback: one reusable raster buffer and one explicit
  // upload, so the frame is copied only by the backend's staging upload.
  m_rasterPixels.resize((size_t)pixelSize.width() * (size_t)pixelSize.height());
  sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(
      SkImageInfo::Make(pixelSize.width(), pixelSize.height(),
                        kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      m_rasterPixels.data(), (size_t)pixelSize.width() * sizeof(uint32_t));
  if (!surface) {
    update();
    return;
  }
  std::unique_ptr<sketch::Host> evicted;
  {
    QMutexLocker lock(&SketchbookView::hostMutex);
    if (m_index != m_requestedIndex) {
      m_index = m_requestedIndex;
      evicted = openSketch(m_index);
    }
    drawSketch(*surface->getCanvas(), pixelSize);
    refreshThumbnail();
    runPendingCaptures();
    if (m_metricsDirty) {
      m_metricsDirty = false;
      publishMetrics();
    }
  }
  evicted.reset();  // outside the lock: ~Host waits on its build

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
