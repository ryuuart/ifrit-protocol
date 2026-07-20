#include "ComposeSketchView.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <QtGui/QPainter>
#include <QtQuick/QQuickWindow>

#include <algorithm>
#include <cmath>

using sigil::compose::sketch::SketchHost;

SketchHost *ComposeSketchView::host = nullptr;

ComposeSketchView::ComposeSketchView(QQuickItem *parent)
    : QQuickPaintedItem(parent) {
  m_timer.setInterval(16);
  QObject::connect(&m_timer, &QTimer::timeout, this, [this] {
    if (!host)
      return;
    host->poll();
    const QString status = QString::fromStdString(host->status());
    const QString error = QString::fromStdString(host->errorLog());
    static const char *kStateNames[] = {"waiting", "compiling", "live",
                                        "failed"};
    const QString state = kStateNames[(int)host->state()];
    if (status != m_status || error != m_errorLog ||
        m_compiling != host->compiling() || state != m_state) {
      m_status = status;
      m_errorLog = error;
      m_compiling = host->compiling();
      m_state = state;
      emit stateChanged();
    }
    if (host->live()) {
      m_metrics = QString("work %1 ms (p99 %2)   %3 fps")
                      .arg(host->workMsAverage(), 0, 'f', 2)
                      .arg(host->workMsP99(), 0, 'f', 2)
                      .arg(host->presentedFps(), 0, 'f', 0);
      emit metricsChanged();
    }
    update();
  });
  m_timer.start();
}

void ComposeSketchView::paint(QPainter *painter) {
  if (!host)
    return;
  const qreal dpr =
      window() ? window()->effectiveDevicePixelRatio() : 1.0;
  const int w = std::max(1, (int)std::lround(width() * dpr));
  const int h = std::max(1, (int)std::lround(height() * dpr));
  if (m_frame.width() != w || m_frame.height() != h)
    m_frame = QImage(w, h, QImage::Format_RGBA8888_Premultiplied);
  sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(
      SkImageInfo::Make(w, h, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      m_frame.bits(), m_frame.bytesPerLine());
  SkCanvas &canvas = *surface->getCanvas();
  canvas.clear(SkColorSetRGB(0x0b, 0x0a, 0x14)); // letterbox bars
  const SkSize scene = host->canvasSize(); // sketch-declared (ctx.canvas)
  const float scale =
      std::min((float)w / scene.width(), (float)h / scene.height());
  canvas.save();
  canvas.translate((w - scene.width() * scale) / 2,
                   (h - scene.height() * scale) / 2);
  canvas.scale(scale, scale);
  canvas.clipRect(SkRect::MakeWH(scene.width(), scene.height()));
  canvas.clear(host->background().toSkColor());
  if (!host->frame(canvas)) {
    // Nothing loaded yet — quiet dark canvas until the first build.
    canvas.clear(SkColorSetRGB(0x14, 0x12, 0x1e));
  }
  canvas.restore();
  m_frame.setDevicePixelRatio(dpr);
  painter->drawImage(QPointF(0, 0), m_frame);
  host->markPresented();
}
