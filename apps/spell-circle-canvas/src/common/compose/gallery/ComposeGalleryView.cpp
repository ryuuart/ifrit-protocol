#include "ComposeGalleryView.h"

#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtQuick/QQuickWindow>

#include <algorithm>
#include <cmath>

using namespace compose_gallery;

ComposeGalleryView::ComposeGalleryView(QQuickItem *parent)
    : QQuickPaintedItem(parent) {
  m_stage.showStats = false; // the QML sidebar is the stats display
  m_stage.activate(makeScene(0));
  m_timer.setInterval(16);
  QObject::connect(&m_timer, &QTimer::timeout, this, [this] {
    update();
    // Metrics for the sidebar, refreshed at display cadence.
    const Composer::Stats &cs = m_stage.composer->stats();
    m_metrics =
        QString("work %1 ms   p99 %2 ms\nheadroom ~%3 fps   presented %4 fps\n"
                "instances %5   pictures %6\ntextures %7   live nodes %8")
            .arg(m_stage.stats.average(), 0, 'f', 2)
            .arg(m_stage.stats.percentile(0.99), 0, 'f', 2)
            .arg(m_stage.stats.fps(), 0, 'f', 0)
            .arg(m_stage.stats.presentedFps(), 0, 'f', 1)
            .arg(cs.instances)
            .arg(cs.picturesLive)
            .arg(cs.texturesLive)
            .arg(cs.nodesPainted);
    emit metricsChanged();
  });
  m_timer.start();
}

void ComposeGalleryView::paint(QPainter *painter) {
  // Render at the item's device-pixel size and scale the CANVAS, not
  // the finished image: vectors re-rasterize crisply at any window
  // size and DPI, the aspect ratio is letterboxed instead of
  // stretched, and Cache::Texture nodes re-bake at the live scale.
  const qreal dpr =
      window() ? window()->effectiveDevicePixelRatio() : 1.0;
  const int w = std::max(1, (int)std::lround(width() * dpr));
  const int h = std::max(1, (int)std::lround(height() * dpr));
  QImage image(w, h, QImage::Format_RGBA8888_Premultiplied);
  sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(
      SkImageInfo::Make(w, h, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      image.bits(), image.bytesPerLine());
  SkCanvas &canvas = *surface->getCanvas();
  canvas.clear(SK_ColorBLACK);
  const float scale = std::min((float)w / kSceneSize.width(),
                               (float)h / kSceneSize.height());
  canvas.save();
  canvas.translate((w - kSceneSize.width() * scale) / 2,
                   (h - kSceneSize.height() * scale) / 2);
  canvas.scale(scale, scale);
  canvas.clipRect(SkRect::MakeWH(kSceneSize.width(), kSceneSize.height()));
  m_stage.frame(canvas);
  canvas.restore();
  image.setDevicePixelRatio(dpr);
  painter->drawImage(QPointF(0, 0), image);
  m_stage.markPresented();
}

void ComposeGalleryView::setSceneIndex(int index) {
  if (index == m_sceneIndex || index < 0 || index >= kGallerySceneCount)
    return;
  m_sceneIndex = index;
  m_stage.activate(makeScene(index));
  emit sceneIndexChanged();
}

void ComposeGalleryView::setPaused(bool paused) {
  if (paused == m_stage.clock.paused())
    return;
  m_stage.clock.setPaused(paused);
  emit pausedChanged();
}

void ComposeGalleryView::setTimeScale(double scale) {
  if (scale == m_stage.clock.timeScale())
    return;
  m_stage.clock.setTimeScale(scale);
  emit timeScaleChanged();
}
