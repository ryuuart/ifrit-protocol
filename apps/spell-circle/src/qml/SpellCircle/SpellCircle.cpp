#include "SpellCircle.h"
#include "SpellCircleRenderer.h"

SpellCircle::SpellCircle(QQuickItem *parent) : QCanvasPainterItem(parent) {}

QCanvasPainterItemRenderer *SpellCircle::createItemRenderer() const {
  return new SpellCircleRenderer;
}

void SpellCircle::setFeedModel(QObject *object) {
  FeedModel *model = qobject_cast<FeedModel *>(object);
  if (m_feedModel == model) {
    return;
  }
  if (m_feedModel)
    disconnect(m_feedModel, &FeedModel::circlesChanged, this,
               &SpellCircle::update);
  m_feedModel = model;
  if (m_feedModel)
    connect(m_feedModel, &FeedModel::circlesChanged, this,
            &SpellCircle::update);
  emit feedModelChanged();
  update();
}
