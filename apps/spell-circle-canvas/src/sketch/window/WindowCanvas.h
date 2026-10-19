#pragma once

/** @file
 * The item a window shows its host through: the host's canvas fitted into
 * the item, drawn into the item's own texture by a `WindowPainter`.
 */

#include <sigilsketch/window/CanvasView.h>

#include <QtCore/QMutex>
#include <QtQuick/QQuickRhiItem>
#include <memory>

namespace sigil::sketch {

class Host;

/** WHICH HOST IS ON SCREEN, shared by the window that owns the host, the
 *  item and the item's renderer. Every read and write takes the mutex,
 *  and the window lets a host go only once it has stepped out of here. */
struct PresentedHost {
  QMutex mutex;
  Host* host = nullptr;
};

/** THE PANE: one host's canvas, fitted, centred and never stretched,
 *  over the ground the sketch declared, with the pointer handed to the
 *  sketch in its own canvas units. */
class WindowCanvas final : public QQuickRhiItem {
  Q_OBJECT

 public:
  WindowCanvas(std::shared_ptr<PresentedHost> presented,
               QQuickItem* parent = nullptr);
  QQuickRhiItemRenderer* createRenderer() override;

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void hoverMoveEvent(QHoverEvent* event) override;

 private:
  /** Hands the sketch the point under the pointer, where the gate admits
   *  it. */
  void pointer(QPointF position, bool pressed);

  std::shared_ptr<PresentedHost> m_presented;
  PointerGate m_pointerGate;
  bool m_pressed = false;
};

}  // namespace sigil::sketch
