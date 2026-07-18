#pragma once
#include "GraphicsConfig.h"
#include "NetworkManager.h"
#include "SpellCircleModel.h"
#include <QObject>
#include <QtQml/QtQml>

/** QML tooling registration for the application-owned scene model. */
struct SpellCircleModelQmlType {
  Q_GADGET
  QML_FOREIGN(SpellCircleModel)
  QML_ANONYMOUS
};

/** QML tooling registration for the application-owned graphics settings. */
struct GraphicsConfigQmlType {
  Q_GADGET
  QML_FOREIGN(GraphicsConfig)
  QML_ANONYMOUS
};

/** QML tooling registration for the grouped box-style property. */
struct BoxStyleConfigQmlType {
  Q_GADGET
  QML_FOREIGN(BoxStyleConfig)
  QML_ANONYMOUS
};

/** QML tooling registration for the grouped canvas-size property. */
struct CanvasSizeConfigQmlType {
  Q_GADGET
  QML_FOREIGN(CanvasSizeConfig)
  QML_ANONYMOUS
};

/** QML tooling registration for the application-owned UDP receiver. */
struct NetworkManagerQmlType {
  Q_GADGET
  QML_FOREIGN(NetworkManager)
  QML_ANONYMOUS
};

/**
 * QML singleton that owns the application's SpellCircleModel, GraphicsConfig,
 * and NetworkManager, wiring received UDP packets into the model.
 */
class Models : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(Models)
  QML_SINGLETON

  Q_PROPERTY(SpellCircleModel *spellCircleModel READ spellCircleModel CONSTANT)
  Q_PROPERTY(GraphicsConfig *graphicsConfig READ graphicsConfig CONSTANT)
  Q_PROPERTY(NetworkManager *networkManager READ networkManager CONSTANT)
public:
  /** Constructs and wires the model, graphics configuration, and receiver. */
  explicit Models(QObject *parent = nullptr);

  /** Returns the singleton SpellCircleModel instance. */
  SpellCircleModel *spellCircleModel() const { return m_spellCircleModel; }

  /** Returns the singleton GraphicsConfig instance. */
  GraphicsConfig *graphicsConfig() const { return m_graphicsConfig; }

  /** Returns the singleton NetworkManager instance. */
  NetworkManager *networkManager() const { return m_networkManager; }

private:
  SpellCircleModel *m_spellCircleModel;
  GraphicsConfig *m_graphicsConfig;
  NetworkManager *m_networkManager;
};
