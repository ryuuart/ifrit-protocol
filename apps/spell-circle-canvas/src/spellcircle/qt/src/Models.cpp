#include "Models.h"

#include <utility>

Models::Models(boost::asio::any_io_executor executor, QObject* parent)
    : QObject(parent) {
  m_spellCircleModel = new SpellCircleModel(this);
  m_graphicsConfig = new GraphicsConfig(this);
  m_networkManager = new NetworkManager(std::move(executor),
                                        NetworkManager::kDefaultPort, this);
  m_networkManager->load();

  QObject::connect(m_networkManager, &NetworkManager::spellCircleReceived,
                   m_spellCircleModel,
                   &SpellCircleModel::onSpellCircleReceived);
}
