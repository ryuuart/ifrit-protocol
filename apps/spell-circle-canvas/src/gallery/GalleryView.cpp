#include "GalleryView.h"

#include <textflowqt/TextFlowQt.h>

#ifdef TEXTFLOW_GALLERY_GPU
#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"
#endif

#include <include/core/SkCanvas.h>
#include <include/core/SkFontArguments.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypeface.h>
#include <include/ports/SkFontMgr_mac_ct.h>

#include <QFont>
#include <QMouseEvent>
#include <QQuickWindow>
#include <rhi/qrhi.h>

#include <chrono>

using namespace gallery;

// ── Render-thread side ─────────────────────────────────────────────────────

class GalleryViewRenderer : public QQuickRhiItemRenderer {
public:
  void initialize(QRhiCommandBuffer *cb) override;
  void synchronize(QQuickRhiItem *item) override;
  void render(QRhiCommandBuffer *cb) override;

private:
  void renderScene(SkCanvas *canvas, float dpr, QSize logical);

  std::vector<std::unique_ptr<Scene>> m_scenes;
  std::unique_ptr<textflow::FontContext> m_ctx;
#ifdef TEXTFLOW_GALLERY_GPU
  std::unique_ptr<SkiaGraphiteContext> m_graphite;
  bool m_graphiteTried = false;
#endif
  std::vector<uint32_t> m_rasterPixels; // CPU fallback framebuffer

  SceneParams m_params;
  QString m_paramText, m_paramFamily;
  int m_paramWeight = 0;
  int m_sceneIndex = 0;
  bool m_animating = true;
  bool m_useGpu = true;

  // Resolved (family, weight) → typeface, cached: re-cloning a variable
  // instance every frame would mint fresh uniqueIDs and defeat the shape
  // cache.
  QString m_resolvedFamily{QStringLiteral("\0")};
  int m_resolvedWeight = -1;
  sk_sp<SkTypeface> m_resolvedTypeface;
  std::vector<SkPoint> m_clicks;
  QSize m_logicalSize;

  QElapsedTimer m_clock;
  double m_pausedAt = 0;
  int m_frame = 0;
  int m_lastScene = -1;

  // Stats accumulated in render(), copied back in the next synchronize().
  double m_layoutUsEma = 0, m_recordUsEma = 0, m_submitUsEma = 0;
  double m_fpsEma = 0;
  uint64_t m_reshaped = 0;
  int m_runs = 0, m_glyphs = 0;
  QElapsedTimer m_interFrame;
  int m_statFrames = 0;
  bool m_statsDirty = false;
};

void GalleryViewRenderer::initialize(QRhiCommandBuffer * /*cb*/) {
#ifdef TEXTFLOW_GALLERY_GPU
  if (!m_graphiteTried) {
    m_graphiteTried = true;
    // Same pattern as the SpellCircle app: Graphite built on Qt's own Metal
    // device/queue, so its submissions order before Qt's scene-graph pass.
    m_graphite = SkiaGraphiteContext::create(rhi());
  }
#endif
}

void GalleryViewRenderer::synchronize(QQuickRhiItem *item) {
  auto *view = static_cast<GalleryView *>(item);
  if (m_scenes.empty())
    m_scenes = makeScenes();

  m_sceneIndex = view->m_sceneIndex;
  m_animating = view->m_animating;
  m_useGpu = view->m_gpu;
  m_paramWeight = view->m_fontWeight;
  m_paramText = view->m_sceneText;
  m_paramFamily = view->m_fontFamily;
  m_params.text = m_paramText;
  m_params.fontSize = static_cast<float>(view->m_fontSize);
  m_params.alignment =
      static_cast<textflow::Alignment>(std::clamp(view->m_alignmentIndex, 0, 3));
  m_params.breaker =
      static_cast<textflow::Breaker>(std::clamp(view->m_breakerIndex, 0, 1));
  m_clicks.insert(m_clicks.end(), view->m_pendingClicks.begin(),
                  view->m_pendingClicks.end());
  view->m_pendingClicks.clear();
  m_logicalSize = QSize(static_cast<int>(view->width()),
                        static_cast<int>(view->height()));

  // Ship last frame's stats back (GUI thread is blocked right now).
  if (m_statsDirty) {
    m_statsDirty = false;
    const char *mode =
#ifdef TEXTFLOW_GALLERY_GPU
        (m_graphite && m_useGpu) ? "Graphite GPU" : "CPU raster";
#else
        "CPU raster";
#endif
    view->m_stats =
        QStringLiteral(
            "%1 · %2 fps · layout %3 µs · record %4 ms · submit %5 ms\n"
            "%6 runs · %7 reshaped/frame%8")
            .arg(QLatin1String(mode))
            .arg(m_fpsEma, 0, 'f', 0)
            .arg(m_layoutUsEma, 0, 'f', 0)
            .arg(m_recordUsEma / 1000.0, 0, 'f', 2)
            .arg(m_submitUsEma / 1000.0, 0, 'f', 2)
            .arg(m_runs)
            .arg(m_reshaped)
            .arg(m_glyphs > 0
                     ? QStringLiteral(" · %1 letters").arg(m_glyphs)
                     : QString());
    QMetaObject::invokeMethod(view, &GalleryView::statsChanged,
                              Qt::QueuedConnection);
  }
}

void GalleryViewRenderer::renderScene(SkCanvas *canvas, float dpr,
                                      QSize logical) {
  using Clock = std::chrono::steady_clock;
  if (!m_ctx)
    m_ctx = std::make_unique<textflow::FontContext>(
        SkFontMgr_New_CoreText(nullptr));

  if (m_sceneIndex != m_lastScene) {
    m_lastScene = m_sceneIndex;
    m_clock.restart();
    m_pausedAt = 0;
    m_frame = 0;
  }
  if (!m_clock.isValid())
    m_clock.start();

  Scene *scene = m_scenes[static_cast<size_t>(
                              std::clamp<int>(m_sceneIndex, 0,
                                              static_cast<int>(m_scenes.size()) - 1))]
                     .get();

  // Font family resolves on this thread (owns the FontContext). A weight
  // is a variable-font `wght` design position cloned onto the family
  // (Noto Sans when none is picked); shaping follows it because faceFor
  // mirrors the clone's design position into HarfBuzz.
  if (m_paramFamily.isEmpty() && m_paramWeight == 0) {
    m_params.typeface = nullptr;
  } else {
    if (m_paramFamily != m_resolvedFamily || m_paramWeight != m_resolvedWeight) {
      m_resolvedFamily = m_paramFamily;
      m_resolvedWeight = m_paramWeight;
      const QString family = m_paramFamily.isEmpty()
                                 ? QStringLiteral("Noto Sans")
                                 : m_paramFamily;
      sk_sp<SkTypeface> tf =
          textflowqt::toSkTypeface(m_ctx->fontMgr(), QFont(family));
      if (tf && m_paramWeight > 0) {
        const SkFontArguments::VariationPosition::Coordinate wght{
            SkSetFourByteTag('w', 'g', 'h', 't'),
            static_cast<float>(m_paramWeight)};
        SkFontArguments args;
        args.setVariationDesignPosition({&wght, 1});
        if (sk_sp<SkTypeface> varied = tf->makeClone(args))
          tf = std::move(varied);
      }
      m_resolvedTypeface = std::move(tf);
    }
    m_params.typeface = m_resolvedTypeface;
  }

  for (const SkPoint &click : m_clicks)
    scene->pointerPress(click);
  m_clicks.clear();

  canvas->save();
  canvas->scale(dpr, dpr);
  const double t =
      m_pausedAt + (m_animating ? m_clock.elapsed() / 1000.0 : 0.0);

  const uint64_t shapeCallsBefore = m_ctx->stats().shapeCalls;
  const auto t0 = Clock::now();
  const FrameStats frame = scene->render(
      canvas, {logical.width(), logical.height()}, t, m_frame, m_params,
      *m_ctx);
  const double recordUs =
      std::chrono::duration<double, std::micro>(Clock::now() - t0).count();
  canvas->restore();

  if (m_animating)
    m_frame++;

  m_reshaped = m_ctx->stats().shapeCalls - shapeCallsBefore;
  m_runs = frame.runs;
  m_glyphs = frame.glyphs;
  m_layoutUsEma = m_layoutUsEma == 0
                      ? frame.layoutUs
                      : m_layoutUsEma * 0.95 + frame.layoutUs * 0.05;
  m_recordUsEma =
      m_recordUsEma == 0 ? recordUs : m_recordUsEma * 0.95 + recordUs * 0.05;
  if (m_animating && m_interFrame.isValid()) {
    const double dtMs =
        static_cast<double>(m_interFrame.nsecsElapsed()) / 1e6;
    if (dtMs > 0.1) {
      const double fps = 1000.0 / dtMs;
      m_fpsEma = m_fpsEma == 0 ? fps : m_fpsEma * 0.95 + fps * 0.05;
    }
  }
  m_interFrame.restart();
  if (++m_statFrames % 15 == 0)
    m_statsDirty = true;
}

void GalleryViewRenderer::render(QRhiCommandBuffer *cb) {
  using Clock = std::chrono::steady_clock;
  QRhiTexture *texture = colorTexture();
  if (!texture || m_scenes.empty() || m_logicalSize.width() < 8 ||
      m_logicalSize.height() < 8)
    return;
  const QSize pixelSize = texture->pixelSize();
  const float dpr = static_cast<float>(pixelSize.width()) /
                    static_cast<float>(m_logicalSize.width());

#ifdef TEXTFLOW_GALLERY_GPU
  if (m_graphite && m_useGpu) {
    // GPU: record straight into the item's texture, submit asynchronously —
    // Qt's scene-graph pass on the same Metal queue orders after it.
    SkiaOffscreenSurface surface(*m_graphite, texture, pixelSize);
    if (SkCanvas *canvas = surface.canvas()) {
      renderScene(canvas, dpr, m_logicalSize);
      const auto s0 = Clock::now();
      surface.submit();
      const double submitUs =
          std::chrono::duration<double, std::micro>(Clock::now() - s0)
              .count();
      m_submitUsEma = m_submitUsEma == 0
                          ? submitUs
                          : m_submitUsEma * 0.95 + submitUs * 0.05;
      // Nothing recorded on `cb` itself; the texture is ready for the
      // scene graph once Graphite's work executes ahead of it in queue
      // order. cb is still needed for the fallback path below.
      return;
    }
  }
#endif

  // CPU fallback: raster into a pixel buffer, upload into the texture.
  m_rasterPixels.resize(static_cast<size_t>(pixelSize.width()) *
                        static_cast<size_t>(pixelSize.height()));
  sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(
      SkImageInfo::Make(pixelSize.width(), pixelSize.height(),
                        kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      m_rasterPixels.data(),
      static_cast<size_t>(pixelSize.width()) * sizeof(uint32_t));
  if (!surface)
    return;
  renderScene(surface->getCanvas(), dpr, m_logicalSize);

  const auto s0 = Clock::now();
  QRhiResourceUpdateBatch *batch = rhi()->nextResourceUpdateBatch();
  QRhiTextureSubresourceUploadDescription sub(
      m_rasterPixels.data(), m_rasterPixels.size() * sizeof(uint32_t));
  batch->uploadTexture(texture,
                       QRhiTextureUploadDescription({0, 0, sub}));
  cb->resourceUpdate(batch);
  const double submitUs =
      std::chrono::duration<double, std::micro>(Clock::now() - s0).count();
  m_submitUsEma = m_submitUsEma == 0 ? submitUs
                                     : m_submitUsEma * 0.95 + submitUs * 0.05;
}

// ── GUI-thread side ────────────────────────────────────────────────────────

GalleryView::GalleryView(QQuickItem *parent) : QQuickRhiItem(parent) {
  m_metaScenes = makeScenes();
  setAcceptedMouseButtons(Qt::LeftButton);
  m_timer.setInterval(16);
  connect(&m_timer, &QTimer::timeout, this, [this] { update(); });
  m_timer.start();
}

GalleryView::~GalleryView() = default;

QQuickRhiItemRenderer *GalleryView::createRenderer() {
  return new GalleryViewRenderer;
}

QStringList GalleryView::sceneNames() const {
  QStringList names;
  for (const auto &scene : m_metaScenes)
    names << scene->name();
  return names;
}

void GalleryView::setSceneIndex(int index) {
  if (index == m_sceneIndex || index < 0 ||
      index >= static_cast<int>(m_metaScenes.size()))
    return;
  m_sceneIndex = index;
  m_sceneText.clear();
  emit sceneIndexChanged();
  emit sceneTextChanged();
  update();
}

void GalleryView::setAnimating(bool on) {
  if (on == m_animating)
    return;
  m_animating = on;
  if (on)
    m_timer.start();
  else
    m_timer.stop();
  emit animatingChanged();
  update();
}

bool GalleryView::textEditable() const {
  if (m_sceneIndex < 0 || m_sceneIndex >= static_cast<int>(m_metaScenes.size()))
    return false;
  return m_metaScenes[static_cast<size_t>(m_sceneIndex)]->supportsTextEdit();
}

void GalleryView::setSceneText(const QString &text) {
  if (text == m_sceneText)
    return;
  m_sceneText = text;
  emit sceneTextChanged();
  update();
}

void GalleryView::setFontFamily(const QString &family) {
  if (family == m_fontFamily)
    return;
  m_fontFamily = family;
  emit fontFamilyChanged();
  update();
}

void GalleryView::setFontSize(qreal size) {
  const qreal s = std::clamp(size, 8.0, 64.0);
  if (s == m_fontSize)
    return;
  m_fontSize = s;
  emit fontSizeChanged();
  update();
}

void GalleryView::setFontWeight(int weight) {
  const int w = weight <= 0 ? 0 : std::clamp(weight, 100, 900);
  if (w == m_fontWeight)
    return;
  m_fontWeight = w;
  emit fontWeightChanged();
  update();
}

void GalleryView::setAlignmentIndex(int index) {
  index = std::clamp(index, 0, 3);
  if (index == m_alignmentIndex)
    return;
  m_alignmentIndex = index;
  emit alignmentIndexChanged();
  update();
}

void GalleryView::setBreakerIndex(int index) {
  index = std::clamp(index, 0, 1);
  if (index == m_breakerIndex)
    return;
  m_breakerIndex = index;
  emit breakerIndexChanged();
  update();
}

bool GalleryView::gpuAvailable() const {
#ifdef TEXTFLOW_GALLERY_GPU
  return true;
#else
  return false;
#endif
}

void GalleryView::setGpu(bool on) {
  if (on == m_gpu)
    return;
  m_gpu = on;
  emit gpuChanged();
  update();
}

void GalleryView::mousePressEvent(QMouseEvent *event) {
  m_pendingClicks.push_back({static_cast<float>(event->position().x()),
                             static_cast<float>(event->position().y())});
  event->accept();
  update();
}
