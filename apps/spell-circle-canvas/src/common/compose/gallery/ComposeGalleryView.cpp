#include "ComposeGalleryView.h"

#include <QtGui/QImage>
#include <QtGui/QPainter>

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
  const int w = (int)kSceneSize.width();
  const int h = (int)kSceneSize.height();
  QImage image(w, h, QImage::Format_RGBA8888_Premultiplied);
  sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(
      SkImageInfo::Make(w, h, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      image.bits(), image.bytesPerLine());
  surface->getCanvas()->clear(SK_ColorBLACK);
  m_stage.frame(*surface->getCanvas());
  painter->drawImage(QRectF(0, 0, width(), height()), image);
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
