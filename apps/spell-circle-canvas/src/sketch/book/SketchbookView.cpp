/** @file
 * The render thread's half of the live canvas, and the GUI thread's poll.
 */

#include "SketchbookView.h"

#include <sigilsketch/core/Fit.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/live/Host.h>

#include <QtCore/QMutexLocker>
#include <QtQuick/QQuickWindow>
#include <algorithm>
#include <cmath>
#include <string>

#include "Keys.h"
#include "SketchCatalog.h"
#include "SketchbookRenderer.h"

namespace sketch = sigil::sketch;

std::filesystem::path SketchbookView::assetsDir;
std::filesystem::path SketchbookView::flagsFile;
std::filesystem::path SketchbookView::sharedDir;
sketch::Host* SketchbookView::host = nullptr;
sigil::weave::FontContext* SketchbookView::fonts = nullptr;
sketch::Residency SketchbookView::sessions;
bool SketchbookView::oneSessionAtATime = false;
// QMutex's constructor does not throw
// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
QMutex SketchbookView::hostMutex;

namespace {

/** How long a resize gesture must be quiet before the render target
 *  follows it. Long enough that consecutive steps of one wheel spin or
 *  one drag fall inside it, short enough that letting go and looking is
 *  not a wait. */
constexpr int kResizeSettleMs = 180;

}  // namespace

SketchbookView::SketchbookView(QQuickItem* parent) : QQuickRhiItem(parent) {
  // We draw into colorTexture() directly; no QRhi render target or depth
  // buffer is needed for this item.
  setAutoRenderTarget(false);
  setAlphaBlending(false);
  m_settle.setSingleShot(true);
  m_settle.setInterval(kResizeSettleMs);
  QObject::connect(&m_settle, &QTimer::timeout, this,
                   [this] { settleRenderSize(); });
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

void SketchbookView::settleRenderSize() {
  m_settle.stop();
  // Nothing to pin to before the item has been laid out; the first real
  // geometry brings one.
  if (!(width() > 0) || !(height() > 0)) return;
  const qreal ratio = window() ? window()->effectiveDevicePixelRatio() : 1.0;
  // Truncate then multiply, which is how the item's own automatic sizing
  // reaches the same number, so a settle that lands on the size already
  // held is a no-op rather than a rebuild by one pixel.
  const int pixelWidth = std::max(1, (int)std::lround((int)width() * ratio));
  const int pixelHeight = std::max(1, (int)std::lround((int)height() * ratio));
  if (pixelWidth == fixedColorBufferWidth() &&
      pixelHeight == fixedColorBufferHeight())
    return;
  setFixedColorBufferWidth(pixelWidth);
  setFixedColorBufferHeight(pixelHeight);
}

void SketchbookView::geometryChange(const QRectF& newGeometry,
                                    const QRectF& oldGeometry) {
  QQuickRhiItem::geometryChange(newGeometry, oldGeometry);
  // A PURE TRANSLATION COSTS NOTHING: the frame on the texture is the
  // same frame wherever the item stands, so panning never reaches here.
  if (newGeometry.size() == oldGeometry.size()) return;
  // Every step defers, the first one included: a width and a height
  // arrive as two changes, so a gesture is never one event to recognise,
  // and the first frame of a resize is exactly the one worth not paying
  // for. Restarting the same single-shot timer is what keeps at most one
  // resize pending, with the last size the one that is taken.
  m_settle.start();
}

void SketchbookView::itemChange(ItemChange change, const ItemChangeData& data) {
  QQuickRhiItem::itemChange(change, data);
  if (change == ItemDevicePixelRatioHasChanged ||
      (change == ItemSceneChange && data.window))
    settleRenderSize();
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
  const sketch::Fit fit =
      sketch::fitInto(size, SkRect::MakeWH((float)width(), (float)height()));
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
