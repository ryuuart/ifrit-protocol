#include "SpellCircleRenderer.h"
#include "SkiaSceneBackend.h"
#include "SpellCircle.h"
#include "TexturePublisher.h"
#include <spdlog/spdlog.h>

SpellCircleRenderer::SpellCircleRenderer() {
  m_font.setBold(true);
  m_font.setPointSize(36);
}

SpellCircleRenderer::~SpellCircleRenderer() = default;

void SpellCircleRenderer::synchronize(QCanvasPainterItem *item) {
  auto *spellCircleItem = static_cast<SpellCircle *>(item);

  bool needsResolve = false;

  const int modelGeneration =
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
      GraphicsConfig *config = spellCircleItem->config();
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
  if (needsResolve)
    resolveGeometry(spellCircleItem->model());
}

void SpellCircleRenderer::resolveGeometry(SpellCircleModel *model) {
  if (!model) {
    m_resolved.clear();
    return;
  }
  m_resolved =
      spellcircle::resolveScene(model->document(),
                                static_cast<float>(m_canvasWidth),
                                static_cast<float>(m_canvasHeight));
}

void SpellCircleRenderer::initializeResources(QCanvasPainter *painter) {
  static_cast<void>(painter);
  // Both factories inspect the active QRhi backend themselves and return
  // null when they have no implementation for it: publishing is optional,
  // and without a Skia backend the canvas stays empty (the QCanvasPainter
  // fallback drawing path is gone).
  m_publisher = createTexturePublisher(rhi(), "SpellCircle");
  m_sceneBackend = createSkiaSceneBackend(rhi());
  if (!m_sceneBackend)
    spdlog::warn("No Skia scene backend for the active graphics API; the "
                 "canvas will not render scene content");
}

void SpellCircleRenderer::prePaint(QCanvasPainter *painter) {
  if (!m_sceneBackend || !m_geometryDirty)
    return;
  m_geometryDirty = false;

  // ── Syphon canvas ─────────────────────────────────────────────────────────
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
  m_displayImage = m_sceneBackend->drawScene(
      *this, painter, m_canvas,
      QSize(m_allocatedCanvasWidth, m_allocatedCanvasHeight));
}

void SpellCircleRenderer::paint(QCanvasPainter *painter) {
  if (!m_displayImage.isNull())
    painter->drawImage(m_displayImage, 0, 0, width(), height());
}

void SpellCircleRenderer::render(QRhiCommandBuffer *commandBuffer) {
  QCanvasPainterItemRenderer::render(commandBuffer);
  if (m_publisher && !m_canvas.isNull() && m_canvas.texture())
    m_publisher->publishFrame(m_canvas.texture(), commandBuffer,
                              m_allocatedCanvasWidth, m_allocatedCanvasHeight);
}
