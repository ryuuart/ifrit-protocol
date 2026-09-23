/** @file
 * What the render thread does with the presented session: opens it,
 * draws its frame into the item's texture, photographs it for the store
 * and publishes what it cost.
 */

#include "SketchbookRenderer.h"

#ifdef SIGILSKETCH_BOOK_GPU
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/PrecompileContext.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilskia/graphite/PaintOrder.h>
#include <sigilskia/qt/QtInterop.h>
#endif

#include <TexturePublisher.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <rhi/qrhi.h>
#include <sigilmeasure/time/Stopwatch.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Sources.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/plate/ThumbnailWriter.h>
#include <sigilsketch/plate/Thumbnails.h>
#include <sigilsketch/python/Python.h>
#include <sigilweave/fonts/FontContext.h>

#include <QtCore/QByteArray>
#include <QtCore/QMetaObject>
#include <QtCore/QString>
#include <QtCore/QMutexLocker>
#include <QtQuick/QQuickWindow>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "CanvasView.h"
#include "PipelineWarm.h"
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

SketchbookRenderer::SketchbookRenderer()
    : m_publishing(SketchbookView::publishAtStart) {}
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
  // The layer a held zoom is drawn into was made on the old backend too.
  m_paneLayer = {};
#ifdef SIGILSKETCH_BOOK_GPU
  // …and the warm-up goes before the context too: it is precompiling
  // through the very context below, on another thread.
  if (m_pipelineWarmup.valid()) m_pipelineWarmup.wait();
  m_graphiteContext.reset();
  // Metal only: the Vulkan adapter does not yet hand the image's final
  // layout back to QRhi's state tracker, so Qt's later sampling of it
  // would be undefined.
  if (currentRhi && currentRhi->backend() == QRhi::Metal)
    m_graphiteContext = sigil::skia::createGraphiteContext(currentRhi);
  g_backend.store(m_graphiteContext ? 1 : 2);
  // THE PROGRAMS THE FIRST SKETCH WOULD OTHERWISE WAIT FOR. Here and
  // not at startup, because the warm-up precompiles THROUGH a context
  // and this is where the window's first exists; on a worker, because
  // building a program costs on this thread exactly what waiting for it
  // does, which is what the warm-up is for.
  //
  // What crosses to the worker is the helper and a name, both taken
  // HERE: the context belongs to this thread, and a warm-up reaching
  // for it from another would be two threads inside one context. The
  // helper holds the context's shared half and is made to be spent
  // elsewhere.
  if (m_graphiteContext && m_graphiteContext->context()) {
    std::unique_ptr<skgpu::graphite::PrecompileContext> precompile =
        m_graphiteContext->makePrecompileContext();
    if (precompile) {
      m_pipelineWarmup = std::async(
          std::launch::async, warmStockPipelines, std::move(precompile),
          graphiteBackendName(*m_graphiteContext->context()));
      // The hold belongs to THIS warm-up. A replaced QRhi runs all of
      // this again against a new device, and a count left where the
      // last warm-up put it is a hold already spent: the frame after it
      // would draw straight away and pay for its own programs.
      m_framesHeldForWarmup = 0;
    }
  }
#else
  g_backend.store(2);
#endif
  m_rhi = currentRhi;
  m_presentsYUp = currentRhi && currentRhi->isYUpInFramebuffer();
  m_initialized = true;
  m_index = -1;
  m_frameCount = 0;
  m_metricsDirty = true;
  m_submitMsAverage = 0.0;
  std::fprintf(stderr, "[sketchbook] renderer: %s\n",
               g_backend.load() == 1 ? "Graphite GPU" : "CPU raster fallback");
  // A REPLACEMENT QRhi IS A REPLACEMENT DEVICE: the server stands on the
  // device its textures come from, so the old one goes with the context
  // above and a new one is stood up here for the run that asked to
  // publish. The canvas it was offering belonged to the old device too.
  m_publishedCanvas.reset();
  m_publishedSize = QSize();
  m_publisher.reset();
  if (m_publishing) startPublishing();
}

void SketchbookRenderer::synchronize(QQuickRhiItem* item) {
  auto* view = static_cast<SketchbookView*>(item);
  m_view = view;
  // THIS ITEM'S TEXTURE IS WRITTEN TOP DOWN WHATEVER THE BACKEND: Skia's
  // surfaces have their origin at the top left, and the raster path
  // uploads its rows in that order into a subresource whose destination
  // is the texture's top left. Qt mirrors the textured quad on a backend
  // whose framebuffers are y-up, which compensates a render pass — and
  // this item makes none, so unless the mirror is asked for a second
  // time the two do not cancel and the sketch presents upside down.
  // Asked for here rather than in the item's constructor because which
  // backend is behind it is only known once the QRhi is.
  if (view->isMirrorVerticallyEnabled() != m_presentsYUp)
    view->setMirrorVertically(m_presentsYUp);
  const bool pauseStarted = !m_paused && view->m_paused;
  m_paused = view->m_paused;
  m_timeScale = view->m_timeScale;
  m_clock.setPaused(m_paused);
  m_clock.setTimeScale(m_timeScale);
  m_requestedIndex = view->m_sketchIndex;
  if (view->m_replayIndex >= 0)
    m_replayIndex = std::exchange(view->m_replayIndex, -1);
  m_pendingCaptures += view->m_captureRequests;
  view->m_captureRequests = 0;
  if (m_publishing != view->m_publishing) {
    m_publishing = view->m_publishing;
    m_metricsDirty = true;
  }
  m_publicationRequest = view->m_publicationRequest;
  m_logicalSize = QSizeF(view->width(), view->height());
  m_canvasScale = (float)view->m_canvasScale;
  m_canvasOffset = view->m_canvasOffset;
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
  options.pythonLoader = &sketch::python::load;
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
  // A SKETCH THIS BINARY CARRIES reads the demo root whatever the window
  // was opened on: a file on the command line names no root for the
  // registry, only the assets beside itself.
  if (options.assetsDirectory.empty() && options.compiledIn)
    options.assetsDirectory = SIGIL_SKETCH_ASSET_DIR;
  options.sketchesDirectory = SketchbookView::sketchesDirectory;
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
  resetPresentation();
  return std::move(presented.evicted);
}

std::unique_ptr<sketch::Host> SketchbookRenderer::updateSession() {
  const int replayIndex = std::exchange(m_replayIndex, -1);
  if (m_index != m_requestedIndex) {
    m_index = m_requestedIndex;
    return openSketch(m_index);
  }
  if (replayIndex >= 0 && replayIndex == m_index && SketchbookView::host &&
      SketchbookView::host->restartSession())
    resetPresentation();
  return nullptr;
}

void SketchbookRenderer::resetPresentation() {
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
  const SkColor colour =
      sigil::material::skia::toSkColor(specification.background).toSkColor();
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
  // WHAT THE FRAME IS LEAVING UNDER, while it is leaving: the name a
  // subscriber binds to, read off the publisher rather than remembered.
  if (m_publisher) {
    const std::string_view published = m_publisher->name();
    metrics.insert(
        QStringLiteral("publish"),
        QString::fromUtf8(published.data(), (qsizetype)published.size()));
  }
  metrics.insert(QStringLiteral("counters"),
                 QString::fromStdString(session->counters()));
  QVariantList lanes;
  for (const sketch::LaneCost& lane : session->lanes()) {
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

void SketchbookRenderer::paintFrame(SkCanvas& canvas, sketch::Host& host) {
  // The ground as the sketch stated it, ALPHA AND ALL. A sketch may
  // ground itself in nothing at all, and the pixels it does not draw on
  // then carry no colour and no coverage — which is what a subscriber
  // composites the sketch over its own scene by.
  canvas.clear(sigil::material::skia::toSkColor(host.background()));
  // Wall time, scaled, pausable and stall-clamped: the frame the reader
  // sees advances by what actually elapsed, not by a nominal step.
  // Bakes belong to the canvas the sketch declared, not to how far this
  // window has magnified it: taken at the screen's density, once.
  if (sketch::Session* session = host.session())
    session->setBakeDensity(m_deviceRatio);
  const double step = m_clock.tick();
  host.frame(canvas, step);
  m_sceneSeconds += step;
}

sk_sp<SkImage> SketchbookRenderer::renderPublication(
    [[maybe_unused]] const QRhiTexture& window) {
#ifdef SIGILSKETCH_BOOK_GPU
  if (!m_publisher || !m_graphiteContext || !m_rhi) return nullptr;
  sketch::Host* host = SketchbookView::host;
  if (!host || !host->live()) return nullptr;
  const SkSize canvas = host->canvasSize();
  if (!(canvas.width() > 0) || !(canvas.height() > 0)) return nullptr;
  // ONE TEXTURE PIXEL PER CANVAS UNIT: what a subscriber receives is the
  // size the sketch declared, so a scene built around a 1920x1080 canvas
  // arrives as 1920x1080 however large or small this window happens to
  // be. A fractional extent is rounded UP, because a canvas offered
  // smaller than it declared is missing a strip of itself.
  const QSize pixels((int)std::ceil(canvas.width()),
                     (int)std::ceil(canvas.height()));
  if (!m_publishedCanvas || m_publishedSize != pixels) {
    m_publishedCanvas.reset();
    m_publishedSize = QSize();
    std::unique_ptr<QRhiTexture> made(m_rhi->newTexture(
        window.format(), pixels, 1, QRhiTexture::RenderTarget));
    if (!made || !made->create()) {
      stopPublishing();
      refusePublishing(
          QStringLiteral("This canvas is too large for a publication texture "
                         "on this graphics backend."));
      return nullptr;
    }
    m_publishedCanvas = std::move(made);
    m_publishedSize = pixels;
  }
  sigil::skia::OffscreenSurface surface = sigil::skia::wrapTexture(
      *m_graphiteContext, m_publishedCanvas.get(), pixels);
  SkCanvas* into = surface.canvas();
  if (!into) {
    stopPublishing();
    refusePublishing(QStringLiteral(
        "The sketch's canvas could not be drawn into a texture of its own."));
    return nullptr;
  }
  // ASKED FOR BEFORE ANYTHING IS DRAWN, and the frame is abandoned when
  // the answer is null: the texture is offered AND shown, so a texture
  // this backend will not let the window sample is one the sketch has
  // not been stepped for — leaving the tick to the ordinary path rather
  // than spending it on a frame nobody can see.
  sk_sp<SkImage> shown = SkSurfaces::AsImage(sk_ref_sp(surface.surface()));
  if (!shown) {
    stopPublishing();
    refusePublishing(QStringLiteral(
        "A published canvas cannot be sampled back on this graphics "
        "backend."));
    return nullptr;
  }
  paintFrame(*into, *host);
  // Submitted HERE, before the window records the draw that samples it:
  // Graphite rides the queue this window presents on, and work on one
  // queue runs in the order it was committed.
  surface.submit();
  return shown;
#else
  return nullptr;
#endif
}

void SketchbookRenderer::drawSketch(SkCanvas& canvas, QSize pixelSize,
                                    const SkImage* published) {
  sketch::Host* host = SketchbookView::host;
  if (!host || !host->live()) {
    // Nothing to show, so nothing is covered: the pane shows its own
    // ground until a sketch is live.
    canvas.clear(SK_ColorTRANSPARENT);
    return;
  }
  // THE TEXTURE IS THE PANE. It is sized from the item, the item fills
  // the pane, and a zoom or a pan moves the view below rather than the
  // item — so the pixels a frame covers are the pixels on screen at any
  // zoom, the texture never outgrows what the device can allocate, and
  // the canvas off the pane is clipped away before it is drawn. The view
  // is spelled in item units and drawn in pixels, and the ratio between
  // the two is the texture's own.
  const SkSize pane =
      SkSize::Make((float)pixelSize.width(), (float)pixelSize.height());
  const float pixelsPerUnit = m_logicalSize.width() > 0
                                  ? pane.width() / (float)m_logicalSize.width()
                                  : 1.0f;
  const CanvasView view =
      CanvasView{m_canvasScale,
                 {(float)m_canvasOffset.x(), (float)m_canvasOffset.y()}}
          .scaled(pixelsPerUnit);
  const SkSize canvasSize = host->canvasSize();
  // WHILE A ZOOM MOVES, THE SKETCH IS DRAWN AT THE SCALE IT STOOD AT, and
  // the frame is magnified to the view: what the sketch keeps at the
  // scale it is drawn at stays kept through the gesture instead of being
  // made again at each step of it. A session just opened starts from the
  // view's own scale. A published frame is an image already drawn at
  // the canvas's own pixels, so it is shown at the view's.
  if (host->session() != m_heldSession) {
    m_zoomHold.release();
    m_heldSession = host->session();
  }
  const float drawnScale =
      published ? 0.0f
                : m_zoomHold.drawnScale(
                      placeCanvas(canvasSize, pane, view).scale,
                      ZoomHold::Clock::now());
  // The SKETCH's own canvas is placed on the pane and never stretched to
  // it: a sketch declares its own dimensions and they do not share an
  // aspect ratio, so stretching one to fill would distort what it shows.
  drawPane(
      canvas, pane, canvasSize, view,
      [this, host, published](SkCanvas& into) {
        if (published)
          // THE FRAME THAT HAS ALREADY LEFT, shown rather than drawn
          // again. It stands at the canvas's own pixels and the view
          // magnifies it, which is the one difference a publishing
          // window shows.
          into.drawImage(published, 0, 0,
                         SkSamplingOptions(SkFilterMode::kLinear));
        else
          paintFrame(into, *host);
      },
      drawnScale, &m_paneLayer);
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
  // ONLY THE REPAINT AND THE READBACK NEED THIS THREAD. The pixels that
  // come back name no device; the PNG over them and the walk that
  // removes the spent stills are pure CPU work over memory nobody else
  // can see, and for a still full of grain they are most of the cost.
  // Inside a frame they are a hitch on exactly the sketches worth
  // looking at, so they go to a worker and this frame carries on.
  SkBitmap pixels = host->still(scale);
  if (pixels.isNull()) return;
  if (!m_thumbnailWriter)
    m_thumbnailWriter =
        std::make_unique<sketch::ThumbnailWriter>([this](int index) {
          // ON THE WORKER, which this renderer outlives: its destructor
          // finishes the write in flight. The report is left here for
          // the render thread to pass on, because the view it is passed
          // to is the render thread's to name — this thread holds no
          // claim that the item still stands.
          const std::lock_guard lock(m_writtenMutex);
          m_written.push_back(index);
        });
  m_thumbnailWriter->write(m_index, std::move(pixels), out,
                           SketchCatalog::thumbnailDirectory, entry.name);
}

void SketchbookRenderer::reportWrittenThumbnails() {
  std::vector<int> written;
  {
    const std::lock_guard lock(m_writtenMutex);
    written.swap(m_written);
  }
  if (written.empty() || !m_view) return;
  for (int index : written)
    QMetaObject::invokeMethod(
        m_view, [view = m_view, index] { emit view->thumbnailCaptured(index); },
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

void SketchbookRenderer::startPublishing() {
  if (m_publisher) return;
  m_metricsDirty = true;
  // Only the Graphite frame path submits a texture to the publisher.
#ifdef SIGILSKETCH_BOOK_GPU
  if (m_graphiteContext)
    m_publisher =
        ifrit::qt::createPublisher(m_rhi, SketchbookView::publishName);
#endif
  if (m_publisher) {
    std::fprintf(stderr, "[sketchbook] publishing as \"%s\"\n",
                 SketchbookView::publishName.c_str());
    return;
  }
  QString reason = QStringLiteral(
      "This window uses CPU rendering. Frame publishing requires a supported "
      "GPU renderer.");
#ifdef SIGILSKETCH_BOOK_GPU
  if (m_graphiteContext)
    reason = QStringLiteral(
        "A frame publisher could not be opened for this graphics backend.");
#endif
  refusePublishing(std::move(reason));
}

void SketchbookRenderer::refusePublishing(QString reason) {
  // Initialization can precede synchronization with the view. Leave the
  // request pending until its refusal can reach the window.
  SketchbookView* view = m_view;
  if (!view) return;
  m_publishing = false;
  std::fprintf(stderr, "[sketchbook] publish: %s\n",
               reason.toUtf8().constData());
  const auto request = m_publicationRequest;
  QMetaObject::invokeMethod(
      view,
      [view, request, reason = std::move(reason)] {
        if (request != view->m_publicationRequest || !view->m_publishing)
          return;
        view->m_publishing = false;
        view->m_publicationError = reason;
        emit view->publishingChanged();
      },
      Qt::QueuedConnection);
}

void SketchbookRenderer::stopPublishing() {
  // The canvas texture goes with the publication it was standing for:
  // nothing is offering frames, so nothing needs a frame's worth of the
  // device held open.
  m_publishedCanvas.reset();
  m_publishedSize = QSize();
  if (!m_publisher) return;
  std::fprintf(stderr, "[sketchbook] publishing stopped\n");
  m_publisher.reset();
  m_metricsDirty = true;
}

void SketchbookRenderer::publishFrame(QRhiTexture* texture,
                                      QRhiCommandBuffer* commandBuffer,
                                      QSize pixelSize) {
  if (!m_publisher) return;
  // The drawing has already been submitted on this device's queue and
  // the buffer below is the one Qt commits after render() returns, so
  // the copy is ordered behind the frame it is copying.
  ifrit::qt::publishFrame(*m_publisher, texture, commandBuffer, pixelSize);
}

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
#ifdef SIGILSKETCH_BOOK_GPU
  // NOTHING IS DRAWN UNTIL THE PROGRAMS ARE STANDING. Standing them up
  // on a worker only helps a frame that waits for it: a first frame
  // recorded beside the warm-up asks for the very programs it is
  // building and builds them itself, which is the stall moved rather
  // than removed. So the frame is SKIPPED rather than blocked — this
  // thread stays free, Qt presents what it already has, and the next
  // frame asks again.
  //
  // Bounded, because a canvas that never draws is worse than a hitch:
  // past the hold the sketch is drawn and pays for its own programs.
  if (m_pipelineWarmup.valid()) {
    using namespace std::chrono_literals;
    if (m_pipelineWarmup.wait_for(0s) == std::future_status::ready)
      // Dropped rather than got: a warm-up that threw is a warm-up that
      // did not happen, and rethrowing it here would end the frame loop
      // over a hitch.
      m_pipelineWarmup = std::future<void>();
    else if (++m_framesHeldForWarmup <= kFramesHeldForWarmup) {
      update();
      return;
    }
  }
#endif
  if (m_publishing && !m_publisher) startPublishing();
  if (!m_publishing && m_publisher) stopPublishing();

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
    bool offered = false;
    {
      sigil::skia::OffscreenSurface surface =
          sigil::skia::wrapTexture(*m_graphiteContext, texture, pixelSize);
      if (SkCanvas* canvas = surface.canvas()) {
        QMutexLocker lock(&SketchbookView::hostMutex);
        evicted = updateSession();
        // THE SKETCH IS DRAWN ONCE, and where a subscriber is waiting it
        // is drawn into its own canvas first — the window then shows
        // that frame instead of stepping the sketch a second time.
        const sk_sp<SkImage> published = renderPublication(*texture);
        offered = published != nullptr;
        drawSketch(*canvas, pixelSize, published.get());
        const sigil::measure::Stopwatch submitWatch;
        surface.submit();
        const double submitMs = submitWatch.elapsedMs();
        m_submitMsAverage = m_submitMsAverage == 0.0
                                ? submitMs
                                : m_submitMsAverage * 0.95 + submitMs * 0.05;
        refreshThumbnail();
        reportWrittenThumbnails();
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
      // WHAT LEAVES IS THE SKETCH'S OWN CANVAS. A frame that could not
      // be drawn into one is not offered at all: a subscriber handed
      // this window's texture instead would receive the matte around
      // the sketch, and a size that follows the window and its zoom.
      if (offered)
        publishFrame(m_publishedCanvas.get(), commandBuffer, m_publishedSize);
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
    evicted = updateSession();
    drawSketch(*surface->getCanvas(), pixelSize, nullptr);
    refreshThumbnail();
    reportWrittenThumbnails();
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
