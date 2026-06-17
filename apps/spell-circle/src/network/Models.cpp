#include "Models.h"
#include <spdlog/spdlog.h>

Models::Models(QObject *parent) : QObject(parent) {
  m_feedModel = new FeedModel(this);
  m_networkManager = new NetworkManager(NetworkManager::kDefaultPort, this);

  if (!m_networkManager->start()) {
    spdlog::error("NetworkManager failed to start");
  }

  QObject::connect(m_networkManager, &NetworkManager::spellCircleReceived,
                   m_feedModel, &FeedModel::onSpellCircleReceived);
}
