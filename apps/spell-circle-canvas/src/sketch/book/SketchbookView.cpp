/** @file
 * The render thread's half of the live canvas, and the GUI thread's poll.
 */

#include "SketchbookView.h"

#include <WindowChrome.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/live/Host.h>

#include <QtCore/QMutexLocker>
#include <QtQuick/QQuickWindow>
#include <algorithm>
#include <cmath>
#include <string>

#include "CanvasView.h"
#include "Keys.h"
#include "SketchCatalog.h"
#include "SketchbookRenderer.h"

namespace sketch = sigil::sketch;

std::filesystem::path SketchbookView::assetsDirectory;
std::filesystem::path SketchbookView::sketchesDirectory;
std::filesystem::path SketchbookView::flagsFile;
std::string SketchbookView::publishName = "Sketchbook";
bool SketchbookView::publishAtStart = false;
sketch::Host* SketchbookView::host = nullptr;
sigil::weave::FontContext* SketchbookView::fonts = nullptr;
sketch::Residency SketchbookView::sessions;
bool SketchbookView::oneSessionAtATime = false;
// QMutex's constructor does not throw
// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
QMutex SketchbookView::hostMutex;

SketchbookView::SketchbookView(QQuickItem* parent)
    : QQuickRhiItem(parent), m_publishing(publishAtStart) {
  // We draw into colorTexture() directly; no QRhi render target or depth
  // buffer is needed for this item.
  setAutoRenderTarget(false);
  // BLENDED, because the item covers the whole pane and the frame covers
  // only the canvas: around it the pane's own ground shows through.
  setAlphaBlending(true);
  m_timer.setInterval(16);
  QObject::connect(&m_timer, &QTimer::timeout, this, [this] {
    {
      QMutexLocker lock(&hostMutex);
      if (host) {
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
      }
    }
    // Ask for a frame WHETHER OR NOT a host is loaded. A sketch that
    // failed to load leaves no host; if the tick returned here without
    // this, render() would never be called again and selecting another
    // sketch could not reopen — the window would be stuck on the failure.
    update();
  });
  m_timer.start();
}

SketchbookView::~SketchbookView() = default;

QQuickRhiItemRenderer* SketchbookView::createRenderer() {
  return new SketchbookRenderer;
}

void SketchbookView::itemChange(ItemChange change, const ItemChangeData& data) {
  QQuickRhiItem::itemChange(change, data);
  if (change == ItemSceneChange && data.window && m_publishing)
    WindowChrome::keepRendering(data.window);
}

void SketchbookView::setCanvasScale(qreal scale) {
  if (!std::isfinite(scale) || scale < 0.0) scale = 0.0;
  if (scale == m_canvasScale) return;
  m_canvasScale = scale;
  emit canvasViewChanged();
  update();
}

void SketchbookView::setCanvasOffset(const QPointF& offset) {
  if (!std::isfinite(offset.x()) || !std::isfinite(offset.y())) return;
  if (offset == m_canvasOffset) return;
  m_canvasOffset = offset;
  emit canvasViewChanged();
  update();
}

void SketchbookView::setSketchIndex(int index) {
  if (index == m_sketchIndex || index < 0 ||
      index >=
          (int)(sketch::registry().size() + SketchCatalog::externals.size()))
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

void SketchbookView::replay() {
  if (m_sketchIndex < 0) return;
  m_replayIndex = m_sketchIndex;
  update();
}

void SketchbookView::setPublishing(bool publishing) {
  if (publishing == m_publishing) return;
  m_publishing = publishing;
  ++m_publicationRequest;
  m_publicationError.clear();
  if (publishing && window()) WindowChrome::keepRendering(window());
  emit publishingChanged();
  update();  // the renderer reads it on the synchronize this asks for
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
  // THE SAME PLACEMENT THE FRAME IS DRAWN WITH, in this item's own
  // units: the canvas under the reader's view of it, so a point on the
  // item is a point on the declared canvas by the inverse of that view.
  const SkSize size = host->canvasSize();
  const sketch::Placement placement = placeCanvas(
      size, SkSize::Make((float)width(), (float)height()),
      CanvasView{(float)m_canvasScale,
                 {(float)m_canvasOffset.x(), (float)m_canvasOffset.y()}});
  if (!(placement.scale > 0.0f)) return;
  const float canvasX = ((float)x - placement.x) / placement.scale;
  const float canvasY = ((float)y - placement.y) / placement.scale;
  // The item is the whole pane and the canvas may be a part of it: a
  // pointer off the canvas reaches the sketch only as the end of a press
  // that began on it.
  const bool onCanvas =
      SkRect::MakeSize(size).contains(canvasX, canvasY);
  if (!m_pointerHeld && !onCanvas) return;
  m_pointerHeld = pressed;
  session->pointer(canvasX, canvasY, pressed);
}

void SketchbookView::key(int qtKey, const QString& text, bool pressed) {
  const auto [name, code] = keyAs(qtKey, text);
  QMutexLocker lock(&hostMutex);
  if (!host || !host->live()) return;
  if (sketch::Session* session = host->session())
    session->key(name, code, pressed);
}
