#include "Models.h"

Models::Models(QObject* parent) : QObject(parent) {
  m_spellCircleModel = new SpellCircleModel(this);
  m_graphicsConfig = new GraphicsConfig(this);
  m_networkManager = new NetworkManager(NetworkManager::kDefaultPort, this);
  m_networkManager->load();

  QObject::connect(m_networkManager, &NetworkManager::spellCircleReceived,
                   m_spellCircleModel,
                   &SpellCircleModel::onSpellCircleReceived);
}
