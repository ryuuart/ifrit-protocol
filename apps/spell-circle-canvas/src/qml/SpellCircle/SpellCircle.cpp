#include "SpellCircle.h"
#include "SpellCircleRenderer.h"

SpellCircle::SpellCircle(QQuickItem *parent) : QCanvasPainterItem(parent) {}

QCanvasPainterItemRenderer *SpellCircle::createItemRenderer() const {
  return new SpellCircleRenderer;
}

void SpellCircle::setModel(QObject *object) {
  SpellCircleModel *model = qobject_cast<SpellCircleModel *>(object);
  if (m_model == model) {
    return;
  }
  if (m_model)
    disconnect(m_model, &SpellCircleModel::geometryChanged, this,
               &SpellCircle::update);
  m_model = model;
  if (m_model)
    connect(m_model, &SpellCircleModel::geometryChanged, this,
            &SpellCircle::update);
  emit modelChanged();
  update();
}

void SpellCircle::setConfig(QObject *object) {
  GraphicsConfig *config = qobject_cast<GraphicsConfig *>(object);
  if (m_config == config) {
    return;
  }
  if (m_config)
    disconnect(m_config, &GraphicsConfig::generationChanged, this,
               &SpellCircle::update);
  m_config = config;
  if (m_config)
    connect(m_config, &GraphicsConfig::generationChanged, this,
            &SpellCircle::update);
  emit configChanged();
  update();
}
