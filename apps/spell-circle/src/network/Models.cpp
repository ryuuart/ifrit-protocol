#include "Models.h"
#include <spdlog/spdlog.h>

Models::Models(QObject *parent) : QObject(parent) {
  m_spellCircleModel = new SpellCircleModel(this);
  m_graphicsConfig = new GraphicsConfig(this);
  m_networkManager = new NetworkManager(NetworkManager::kDefaultPort, this);

  if (!m_networkManager->start()) {
    spdlog::error("NetworkManager failed to start");
  }

  QObject::connect(m_networkManager, &NetworkManager::spellCircleReceived,
                   m_spellCircleModel,
                   &SpellCircleModel::onSpellCircleReceived);

  m_spellCircleModel->setCanvasSize(m_graphicsConfig->canvas()->width(),
                                    m_graphicsConfig->canvas()->height());
  QObject::connect(m_graphicsConfig->canvas(), &CanvasSizeConfig::changed,
                   m_spellCircleModel, [this]() {
                     m_spellCircleModel->setCanvasSize(
                         m_graphicsConfig->canvas()->width(),
                         m_graphicsConfig->canvas()->height());
                   });
}
