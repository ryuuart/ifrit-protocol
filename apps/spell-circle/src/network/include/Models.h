#pragma once
#include "GraphicsConfig.h"
#include "NetworkManager.h"
#include "SpellCircleModel.h"
#include <QObject>
#include <QtQml/QtQml>

/**
 * QML singleton that owns the application's SpellCircleModel, GraphicsConfig,
 * and NetworkManager, wiring received UDP packets into the model.
 */
class Models : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(Models)
  QML_SINGLETON

  Q_PROPERTY(SpellCircleModel *spellCircleModel READ spellCircleModel)
  Q_PROPERTY(GraphicsConfig *graphicsConfig READ graphicsConfig)
public:
  Models(QObject *parent = nullptr);

  /** Returns the singleton SpellCircleModel instance. */
  SpellCircleModel *spellCircleModel() const { return m_spellCircleModel; }

  /** Returns the singleton GraphicsConfig instance. */
  GraphicsConfig *graphicsConfig() const { return m_graphicsConfig; }

signals:

private:
  SpellCircleModel *m_spellCircleModel;
  GraphicsConfig *m_graphicsConfig;
  NetworkManager *m_networkManager;
};
