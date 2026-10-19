/** @file
 * The pane a window shows its host through, and the renderer that draws
 * a frame of the host into it.
 */

#include "WindowCanvas.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkRect.h>
#include <rhi/qrhi.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilsketch/core/Session.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/window/WindowPainter.h>

#include <QtCore/QMutexLocker>
#include <QtGui/QMouseEvent>
#include <QtQuick/QQuickWindow>
#include <utility>

namespace sigil::sketch {

namespace {

/** DRAWS THE PRESENTED HOST INTO THE ITEM'S TEXTURE, a frame per call,
 *  on the window's clock. */
class WindowCanvasRenderer final : public QQuickRhiItemRenderer {
 public:
  explicit WindowCanvasRenderer(std::shared_ptr<PresentedHost> presented)
      : m_presented(std::move(presented)) {}

  void initialize(QRhiCommandBuffer* /*commandBuffer*/) override {
    QRhi* current = rhi();
    if (m_initialized && current == m_rhi) return;
    m_painter.initialize(current);
    m_rhi = current;
    m_presentsYUp = current && current->isYUpInFramebuffer();
    m_initialized = true;
    // A host's capture path belongs to the context just replaced.
    m_installedFor = nullptr;
  }

  void synchronize(QQuickRhiItem* item) override {
    // THE TEXTURE IS WRITTEN TOP DOWN WHATEVER THE BACKEND: Skia's
    // surfaces have their origin at the top left, and Qt mirrors the
    // textured quad on a backend whose framebuffers are y-up, which this
    // item's drawing does not compensate for.
    if (item->isMirrorVerticallyEnabled() != m_presentsYUp)
      item->setMirrorVertically(m_presentsYUp);
    m_deviceRatio = item->window()
                        ? (float)item->window()->effectiveDevicePixelRatio()
                        : 1.0f;
  }

  void render(QRhiCommandBuffer* commandBuffer) override {
    QRhiTexture* texture = colorTexture();
    if (!texture || texture->pixelSize().isEmpty()) {
      update();
      return;
    }
    const QSize pixels = texture->pixelSize();
    const SkSize pane =
        SkSize::Make((float)pixels.width(), (float)pixels.height());
    (void)m_painter.paint(rhi(), texture, commandBuffer, [&](SkCanvas& canvas) {
      QMutexLocker lock(&m_presented->mutex);
      Host* host = m_presented->host;
      if (host != m_installedFor) {
        m_installedFor = host;
        if (host) m_painter.installCaptureBackend(*host);
        m_clock.restart();
      }
      if (host) host->poll();
      if (!host || !host->live()) {
        canvas.clear(SkColor4f::FromColor(kPaneMatte));
        return;
      }
      // Bakes belong to the canvas the sketch declared, taken at the
      // screen's density once, not at whatever the pane magnifies it to.
      if (Session* session = host->session())
        session->setBakeDensity(m_deviceRatio);
      drawPane(canvas, pane, host->canvasSize(), CanvasView{},
               [&](SkCanvas& into) {
                 // The ground as the sketch stated it, alpha and all.
                 into.clear(material::skia::toSkColor(host->background()));
                 (void)host->frame(into, m_clock.advance().count());
               });
      host->markPresented();
    });
    update();
  }

 private:
  std::shared_ptr<PresentedHost> m_presented;
  WindowPainter m_painter;
  /** Wall time, stall-clamped: the frame advances by what elapsed, and a
   *  pause in the pumping resumes at the next frame instead of jumping
   *  every motion forward by its length. */
  motion::Engine m_clock;
  /** The host whose captures were last routed through this context. */
  Host* m_installedFor = nullptr;
  QRhi* m_rhi = nullptr;
  bool m_initialized = false;
  bool m_presentsYUp = false;
  float m_deviceRatio = 1.0f;
};

}  // namespace

WindowCanvas::WindowCanvas(std::shared_ptr<PresentedHost> presented,
                           QQuickItem* parent)
    : QQuickRhiItem(parent), m_presented(std::move(presented)) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
}

QQuickRhiItemRenderer* WindowCanvas::createRenderer() {
  return new WindowCanvasRenderer(m_presented);
}

void WindowCanvas::mousePressEvent(QMouseEvent* event) {
  m_pressed = true;
  pointer(event->position(), true);
}

void WindowCanvas::mouseMoveEvent(QMouseEvent* event) {
  pointer(event->position(), m_pressed);
}

void WindowCanvas::mouseReleaseEvent(QMouseEvent* event) {
  m_pressed = false;
  pointer(event->position(), false);
}

void WindowCanvas::hoverMoveEvent(QHoverEvent* event) {
  pointer(event->position(), m_pressed);
}

void WindowCanvas::pointer(QPointF position, bool pressed) {
  QMutexLocker lock(&m_presented->mutex);
  Host* host = m_presented->host;
  Session* session = host && host->live() ? host->session() : nullptr;
  if (!session) {
    // A press begun while nothing was live is not the next sketch's.
    (void)m_pointerGate.admit(pressed, false);
    return;
  }
  // The placement the frame is drawn with, in the item's own units.
  const SkSize size = host->canvasSize();
  const Placement placement = placeCanvas(
      size, SkSize::Make((float)width(), (float)height()), CanvasView{});
  if (!(placement.scale > 0.0f)) return;
  const SkPoint point =
      canvasPointAt(placement, {(float)position.x(), (float)position.y()});
  const bool onCanvas = SkRect::MakeSize(size).contains(point.x(), point.y());
  if (m_pointerGate.admit(pressed, onCanvas))
    session->pointer(point.x(), point.y(), pressed);
}

}  // namespace sigil::sketch
