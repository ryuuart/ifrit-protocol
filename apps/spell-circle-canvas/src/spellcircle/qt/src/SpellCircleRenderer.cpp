#include "SpellCircleRenderer.h"

#include <TexturePublisher.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <sigilmeasure/time/FrameTimer.h>
#include <sigilskia/qt/QtInterop.h>
#include <sigilweave/qt/SigilWeaveQt.h>
#include <spdlog/spdlog.h>

#include "SceneRenderer.h"
#include "SpellCircle.h"

struct SpellCircleRenderer::RenderState {
  explicit RenderState(std::unique_ptr<sigil::skia::GraphiteContext> context)
      : context(std::move(context)) {}

  std::unique_ptr<sigil::skia::GraphiteContext> context;
  // The drawer's font context and label caches stay on their creating thread.
  spellcircle::SceneRenderer scene;
  uint64_t frame = 0;
  sigil::measure::FrameTimer timing;
};

SpellCircleRenderer::SpellCircleRenderer() {
  m_font.setBold(true);
  m_font.setPointSize(36);
}

SpellCircleRenderer::~SpellCircleRenderer() = default;

void SpellCircleRenderer::synchronize(QCanvasPainterItem* item) {
  auto* spellCircleItem = static_cast<SpellCircle*>(item);

  bool needsResolve = false;

  const uint64_t modelGeneration =
      spellCircleItem->model() ? spellCircleItem->model()->generation() : 0;
  if (modelGeneration != m_knownModelGeneration) {
    m_knownModelGeneration = modelGeneration;
    m_geometryDirty = true;
    needsResolve = true;
  }

  const int configGeneration =
      spellCircleItem->config() ? spellCircleItem->config()->generation() : 0;
  if (configGeneration != m_knownConfigGeneration) {
    m_knownConfigGeneration = configGeneration;
    m_geometryDirty = true;
    needsResolve = true;
    if (spellCircleItem->config()) {
      GraphicsConfig* config = spellCircleItem->config();
      m_accentColor = config->color();
      m_strokeWidth = config->strokeWidth();
      m_scale = config->scale();
      m_labelOffset = config->labelOffset();
      m_pointDistance = config->pointDistance();
      m_canvasWidth = config->canvas()->width();
      m_canvasHeight = config->canvas()->height();
      m_font = config->font();
      m_boxWidth = config->box()->width();
      m_boxHeight = config->box()->height();
      m_boxPadding = config->box()->padding();
      m_boxDistance = config->box()->distance();
    }
  }

  // Resolved positions depend on both the scene (model) and the native
  // canvas size (config), so either changing requires re-resolving.
  if (needsResolve) resolveGeometry(spellCircleItem->model());
}

void SpellCircleRenderer::resolveGeometry(SpellCircleModel* model) {
  if (!model) {
    m_resolved.clear();
    return;
  }
  m_resolved = spellcircle::resolveScene(model->document(),
                                         static_cast<float>(m_canvasWidth),
                                         static_cast<float>(m_canvasHeight));
}

void SpellCircleRenderer::initializeResources(QCanvasPainter* painter) {
  static_cast<void>(painter);
  m_publisher = ifrit::qt::createPublisher(rhi(), "SpellCircle");
  if (auto context = sigil::skia::createGraphiteContext(rhi()))
    m_renderState = std::make_unique<RenderState>(std::move(context));
  else {
    m_renderState.reset();
    spdlog::warn(
        "No Skia scene backend for the active graphics API; the "
        "canvas will not render scene content");
  }
}

void SpellCircleRenderer::prePaint(QCanvasPainter* painter) {
  if (!m_renderState || !m_geometryDirty) return;
  m_geometryDirty = false;

  // Full native resolution for external publishing; updated at the same rate
  // as the display cache (once per scene update, not on every zoom/pan).
  // Reassigning m_canvas drops the old QCanvasOffscreenCanvas's reference to
  // its GPU texture (releasing it once unreferenced) and allocates a fresh
  // one at the new dimensions whenever canvasWidth/Height has changed.
  if (m_canvas.isNull() || m_allocatedCanvasWidth != m_canvasWidth ||
      m_allocatedCanvasHeight != m_canvasHeight) {
    m_canvas = painter->createCanvas(QSize(m_canvasWidth, m_canvasHeight));
    m_allocatedCanvasWidth = m_canvasWidth;
    m_allocatedCanvasHeight = m_canvasHeight;
  }
  m_displayImage = drawScene(painter);
}

QCanvasImage SpellCircleRenderer::drawScene(QCanvasPainter* painter) {
  auto& state = *m_renderState;
  state.timing.begin();
  auto surface = sigil::skia::wrapTexture(
      *state.context, m_canvas.texture(),
      QSize(m_allocatedCanvasWidth, m_allocatedCanvasHeight));
  SkCanvas* canvas = surface.canvas();
  if (!canvas) return {};
  canvas->clear(SK_ColorTRANSPARENT);

  spellcircle::SceneStyle style;
  style.accentColor = sigil::weave::qt::toSkColor(m_accentColor);
  style.strokeWidth = static_cast<float>(m_strokeWidth * m_scale);
  style.labelOffset = static_cast<float>(m_labelOffset * m_scale);
  style.pointDistance = static_cast<float>(m_pointDistance * m_scale);
  style.boxWidth = static_cast<float>(m_boxWidth * m_scale);
  style.boxHeight = static_cast<float>(m_boxHeight * m_scale);
  style.boxPadding = static_cast<float>(m_boxPadding * m_scale);
  style.boxDistance = static_cast<float>(m_boxDistance * m_scale);
  style.fontSize = static_cast<float>(m_font.pointSizeF() * m_scale);
  style.typeface = sigil::weave::qt::toSkTypeface(
      state.scene.fontContext().fontManager(), m_font);
  state.scene.draw(canvas, m_resolved, style);

  state.timing.composed();
  surface.submit();
  state.timing.finished();
  if (state.frame++ % 600 == 0) {
    const double recordMs = state.timing.work().last();
    const double submitMs = state.timing.frame().last() - recordMs;
    spdlog::info(
        "drawScene: record {:.0f} us (mean {:.0f}), submit {:.0f} "
        "us (mean {:.0f})",
        recordMs * 1000.0, state.timing.work().mean() * 1000.0,
        submitMs * 1000.0,
        (state.timing.frame().mean() - state.timing.work().mean()) * 1000.0);
  }

  // The texture already contains blended, premultiplied pixels; the image
  // flag prevents drawImage from multiplying translucent colors a second time.
  return painter->addImage(m_canvas,
                           QCanvasPainter::ImageFlag::GenerateMipmaps |
                               QCanvasPainter::ImageFlag::Premultiplied);
}

void SpellCircleRenderer::paint(QCanvasPainter* painter) {
  if (!m_displayImage.isNull())
    painter->drawImage(m_displayImage, 0, 0, width(), height());
}

void SpellCircleRenderer::render(QRhiCommandBuffer* commandBuffer) {
  QCanvasPainterItemRenderer::render(commandBuffer);
  if (m_publisher && !m_canvas.isNull() && m_canvas.texture())
    ifrit::qt::publishFrame(*m_publisher, m_canvas.texture(), commandBuffer,
                            {m_allocatedCanvasWidth, m_allocatedCanvasHeight});
}
