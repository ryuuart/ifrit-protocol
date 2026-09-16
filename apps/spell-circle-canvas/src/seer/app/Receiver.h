#pragma once

#include <QtQml/qqmlregistration.h>
#include <sigilseer/wire/Wires.h>

#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <functional>

#include "GraphicsConfig.h"
#include "SpellCircleModel.h"

/** One pinned scene consumer of Seer's wires. The session owns draining;
 *  this object owns accepted scene state, settings and receiver controls. */
class Receiver : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("Owned by the Seer session")
  Q_PROPERTY(SpellCircleModel* model READ model CONSTANT)
  Q_PROPERTY(GraphicsConfig* config READ config CONSTANT)
  Q_PROPERTY(QString uri READ uri WRITE setUri NOTIFY changed)
  Q_PROPERTY(int port READ port WRITE setPort NOTIFY changed)
  Q_PROPERTY(bool opened READ opened NOTIFY changed)
  Q_PROPERTY(bool listening READ listening NOTIFY changed)
  Q_PROPERTY(bool recorded READ recorded NOTIFY changed)
  Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
 public:
  using WireDeferrer = std::function<bool(std::function<void()>)>;
  Receiver(sigil::seer::Wires& wires, QObject* parent = nullptr,
           QString settingsDirectory = {}, QString importDirectory = {},
           WireDeferrer deferWireChange = {});
  SpellCircleModel* model() { return &m_model; }
  GraphicsConfig* config() { return &m_config; }
  QString uri() const { return m_uri; }
  int port() const;
  bool opened() const { return m_opened; }
  bool listening() const { return m_listening; }
  bool recorded() const;
  QString statusText() const { return m_status; }
  void setUri(const QString& uri);
  void setPort(int port);
  void accept(const sigil::io::Feed& feed, const sigil::io::Arrival& arrival);
  void refresh();
  Q_INVOKABLE void start();
  Q_INVOKABLE void stop();
  Q_INVOKABLE void beginSettings();
  Q_INVOKABLE void cancelSettings();
  Q_INVOKABLE bool saveSettings();
 signals:
  void changed();
  void wiresChanged();

 private:
  sigil::seer::Wires& m_wires;
  WireDeferrer m_deferWireChange;
  QString m_settingsDirectory;
  GraphicsConfig m_config;
  SpellCircleModel m_model;
  QString m_uri = QStringLiteral("udp://:27015");
  QString m_status = QStringLiteral("Stopped");
  bool m_opened = false;
  bool m_listening = false;
  QJsonObject m_editConfig;
  QString m_editUri;
  bool m_editListening = false;
};
