#include "ComposeGalleryView.h"
#include "GalleryScenes.h"

#ifdef SIGILCOMPOSE_GALLERY_GPU
#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"
#endif

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <QByteArray>
#include <QMetaObject>
#include <QSize>
#include <rhi/qrhi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <vector>

using namespace compose_gallery;

class ComposeGalleryRenderer final : public QQuickRhiItemRenderer {
public:
  void initialize(QRhiCommandBuffer *commandBuffer) override;
  void synchronize(QQuickRhiItem *item) override;
  void render(QRhiCommandBuffer *commandBuffer) override;

private:
  void renderScene(SkCanvas &canvas, QSize pixelSize);

#ifdef SIGILCOMPOSE_GALLERY_GPU
  // Declared before the stage so reverse destruction releases every
  // Graphite-backed SkImage in Composer caches before tearing down context.
  std::unique_ptr<SkiaGraphiteContext> m_graphiteContext;
#endif
  std::unique_ptr<GalleryStage> m_stage;
  QRhi *m_rhi = nullptr;
  std::vector<uint32_t> m_rasterPixels;
  QSize m_logicalSize;
  int m_requestedSceneIndex = 0;
  int m_sceneIndex = -1;
  int m_statisticsFrameCount = 0;
  bool m_paused = false;
  bool m_metricsDirty = true;
  double m_submitMsAverage = 0.0;
};

void ComposeGalleryRenderer::initialize(QRhiCommandBuffer * /*commandBuffer*/) {
  QRhi *currentRhi = rhi();
  if (m_rhi == currentRhi && m_stage)
    return;

  // initialize() can be called again with a replacement QRhi. Destroy the
  // stage first because texture-cached elements can retain Graphite images
  // owned by the old context.
  m_stage.reset();
#ifdef SIGILCOMPOSE_GALLERY_GPU
  m_graphiteContext.reset();
  // Metal needs only same-queue ordering. The Vulkan adapter does not yet
  // hand the image's final layout back to QRhi's state tracker, so using it
  // here would make later Qt sampling undefined; keep the safe CPU fallback
  // on Vulkan until that interop contract is implemented.
  if (currentRhi && currentRhi->backend() == QRhi::Metal) {
    // Graphite uses Qt's native device and command queue. Its asynchronous
    // submission therefore completes before Qt samples this texture later on
    // the same queue, without a CPU wait or an intermediate copy.
    m_graphiteContext = SkiaGraphiteContext::create(currentRhi);
  }
#endif
  m_rhi = currentRhi;
  m_stage = std::make_unique<GalleryStage>();
  m_sceneIndex = -1;
  m_statisticsFrameCount = 0;
  m_metricsDirty = true;
  m_submitMsAverage = 0.0;
  std::fprintf(stderr, "[compose gallery] renderer: %s\n",
#ifdef SIGILCOMPOSE_GALLERY_GPU
               m_graphiteContext ? "Graphite GPU" : "CPU raster fallback"
#else
               "CPU raster fallback"
#endif
  );
}

void ComposeGalleryRenderer::synchronize(QQuickRhiItem *item) {
  auto *view = static_cast<ComposeGalleryView *>(item);
  const bool pauseStarted = !m_paused && view->m_paused;
  m_paused = view->m_paused;

  // Scene construction can generate atlases and pictures. Copy only the
  // request here; activate at the start of render() so Qt does not keep the
  // GUI thread blocked on that work during synchronize().
  m_requestedSceneIndex = view->m_sceneIndex;

  // If the repaint timer stopped while paused, consume that wall-clock span
  // before unpausing so FrameClock cannot deliver a clamped catch-up jump.
  if (m_stage) {
    if (m_stage->clock.paused() && !m_paused) {
      m_stage->clock.tick();
      // Do not count the wall-clock pause as one extremely late frame.
      m_stage->resetPresentationCadence();
    }
    m_stage->clock.setPaused(m_paused);
    m_stage->clock.setTimeScale(view->m_timeScale);
  }
  m_logicalSize =
      QSize(std::max(1, static_cast<int>(std::lround(view->width()))),
            std::max(1, static_cast<int>(std::lround(view->height()))));

  // Publish the last completed frame immediately when the animation stops,
  // even when the 15-frame periodic metrics boundary has not been reached.
  if (pauseStarted)
    m_metricsDirty = true;

  // synchronize() runs on the render thread while Qt blocks the GUI thread,
  // making this the safe hand-off point for last frame's metrics.
  if (m_metricsDirty && m_stage && m_stage->composer) {
    m_metricsDirty = false;
    const Composer::Stats &cs = m_stage->composer->stats();
    const char *backend =
#ifdef SIGILCOMPOSE_GALLERY_GPU
        m_graphiteContext ? "Graphite GPU" : "CPU raster";
#else
        "CPU raster";
#endif
    view->m_metrics =
        QString("%1   compose %2 ms   p99 %3 ms\n"
                "submit %4 ms   render cadence %5 fps\n"
                "instances %6   pictures %7\ntextures %8   live nodes %9")
            .arg(QLatin1String(backend))
            .arg(m_stage->stats.average(), 0, 'f', 2)
            .arg(m_stage->stats.percentile(0.99), 0, 'f', 2)
            .arg(m_submitMsAverage, 0, 'f', 2)
            .arg(m_stage->stats.presentedFps(), 0, 'f', 1)
            .arg(cs.instances)
            .arg(cs.picturesLive)
            .arg(cs.texturesLive)
            .arg(cs.nodesPainted);
    QMetaObject::invokeMethod(view, &ComposeGalleryView::metricsChanged,
                              Qt::QueuedConnection);
  }
}

void ComposeGalleryRenderer::renderScene(SkCanvas &canvas, QSize pixelSize) {
  if (!m_stage)
    return;
  canvas.clear(SK_ColorBLACK);
  const float scale =
      std::min(static_cast<float>(pixelSize.width()) / kSceneSize.width(),
               static_cast<float>(pixelSize.height()) / kSceneSize.height());
  canvas.save();
  canvas.translate(
      (static_cast<float>(pixelSize.width()) - kSceneSize.width() * scale) /
          2.0f,
      (static_cast<float>(pixelSize.height()) - kSceneSize.height() * scale) /
          2.0f);
  canvas.scale(scale, scale);
  canvas.clipRect(SkRect::MakeWH(kSceneSize.width(), kSceneSize.height()));
  m_stage->frame(canvas);
  canvas.restore();
  m_stage->markPresented();
  if (++m_statisticsFrameCount % 15 == 0)
    m_metricsDirty = true;
}

void ComposeGalleryRenderer::render(QRhiCommandBuffer *commandBuffer) {
  using Clock = std::chrono::steady_clock;
  QRhiTexture *texture = colorTexture();
  if (!texture || !m_stage || m_logicalSize.width() < 1 ||
      m_logicalSize.height() < 1)
    return;

  if (m_sceneIndex != m_requestedSceneIndex) {
    m_sceneIndex = m_requestedSceneIndex;
    m_stage->activate(makeScene(m_sceneIndex));
    m_stage->showStats = false; // the QML sidebar owns the stats display
    m_submitMsAverage = 0.0;
    m_statisticsFrameCount = 0;
    m_metricsDirty = true;
  }
  if (!m_stage->scene)
    return;
  const QSize pixelSize = texture->pixelSize();

#ifdef SIGILCOMPOSE_GALLERY_GPU
  if (m_graphiteContext) {
    {
      SkiaOffscreenSurface surface(*m_graphiteContext, texture, pixelSize);
      if (SkCanvas *canvas = surface.canvas()) {
        renderScene(*canvas, pixelSize);
        const auto submitStart = Clock::now();
        surface.submit();
        const double submitMs = std::chrono::duration<double, std::milli>(
                                    Clock::now() - submitStart)
                                    .count();
        m_submitMsAverage = m_submitMsAverage == 0.0
                                ? submitMs
                                : m_submitMsAverage * 0.95 + submitMs * 0.05;
        if (!m_paused)
          update();
        return;
      }
    }

    // Do not mix a raster canvas with Graphite-backed Cache::Texture images
    // retained by the stage. A failed external-texture wrap latches the safe
    // fallback until Qt supplies a new QRhi, rebuilding all scene caches only
    // after the temporary surface has released its context reference.
    std::fprintf(stderr,
                 "[compose gallery] Graphite texture wrap failed; switching "
                 "to CPU raster\n");
    const bool wasPaused = m_stage->clock.paused();
    const double timeScale = m_stage->clock.timeScale();
    m_stage.reset();
    m_graphiteContext.reset();
    m_stage = std::make_unique<GalleryStage>();
    m_stage->clock.setPaused(wasPaused);
    m_stage->clock.setTimeScale(timeScale);
    m_sceneIndex = m_requestedSceneIndex;
    m_stage->activate(makeScene(m_sceneIndex));
    m_stage->showStats = false;
    m_statisticsFrameCount = 0;
    m_metricsDirty = true;
    m_submitMsAverage = 0.0;
  }
#endif

  // Portable fallback: one reusable raster buffer and one explicit upload.
  // This remains substantially leaner than QQuickPaintedItem, which added a
  // second QPainter backing-store copy before Qt uploaded the result.
  m_rasterPixels.resize(static_cast<size_t>(pixelSize.width()) *
                        static_cast<size_t>(pixelSize.height()));
  sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(
      SkImageInfo::Make(pixelSize.width(), pixelSize.height(),
                        kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      m_rasterPixels.data(),
      static_cast<size_t>(pixelSize.width()) * sizeof(uint32_t));
  if (!surface)
    return;
  renderScene(*surface->getCanvas(), pixelSize);

  const auto submitStart = Clock::now();
  QRhiResourceUpdateBatch *batch = rhi()->nextResourceUpdateBatch();
  // fromRawData keeps this upload view non-owning. The render-thread buffer
  // remains stable through QRhi's endFrame, avoiding an otherwise full-frame
  // QByteArray copy before the backend's unavoidable staging upload.
  const QByteArray uploadBytes = QByteArray::fromRawData(
      reinterpret_cast<const char *>(m_rasterPixels.data()),
      static_cast<qsizetype>(m_rasterPixels.size() * sizeof(uint32_t)));
  QRhiTextureSubresourceUploadDescription sub(uploadBytes);
  batch->uploadTexture(texture, QRhiTextureUploadDescription({0, 0, sub}));
  commandBuffer->resourceUpdate(batch);
  const double submitMs =
      std::chrono::duration<double, std::milli>(Clock::now() - submitStart)
          .count();
  m_submitMsAverage = m_submitMsAverage == 0.0
                          ? submitMs
                          : m_submitMsAverage * 0.95 + submitMs * 0.05;
  if (!m_paused)
    update();
}

ComposeGalleryView::ComposeGalleryView(QQuickItem *parent)
    : QQuickRhiItem(parent) {
  // We draw into colorTexture() directly; no QRhi render target or depth
  // buffer is needed for this item.
  setAutoRenderTarget(false);
  setAlphaBlending(false);
}

ComposeGalleryView::~ComposeGalleryView() = default;

QQuickRhiItemRenderer *ComposeGalleryView::createRenderer() {
  return new ComposeGalleryRenderer;
}

QVariantList ComposeGalleryView::scenes() const {
  QVariantList result;
  result.reserve(kGallerySceneCount);
  for (const SceneInfo &scene : kScenes) {
    QVariantMap item;
    item.insert(QStringLiteral("name"), QString::fromUtf8(scene.name));
    item.insert(QStringLiteral("category"), QString::fromUtf8(scene.category));
    item.insert(QStringLiteral("tag"), QString::fromUtf8(scene.catalog));
    result.push_back(std::move(item));
  }
  return result;
}

void ComposeGalleryView::setSceneIndex(int index) {
  if (index == m_sceneIndex || index < 0 || index >= kGallerySceneCount)
    return;
  m_sceneIndex = index;
  emit sceneIndexChanged();
  update();
}

void ComposeGalleryView::setPaused(bool paused) {
  if (paused == m_paused)
    return;
  m_paused = paused;
  emit pausedChanged();
  update();
}

void ComposeGalleryView::setTimeScale(double scale) {
  if (!std::isfinite(scale))
    return;
  scale = std::clamp(scale, 0.0, 16.0);
  if (scale == m_timeScale)
    return;
  m_timeScale = scale;
  emit timeScaleChanged();
  update();
}
