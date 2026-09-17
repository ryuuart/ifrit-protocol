#include "Receiver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <optional>

namespace {
QString storageRoot(const QString& supplied) {
  return supplied.isEmpty() ? QDir(QStandardPaths::writableLocation(
                                       QStandardPaths::AppConfigLocation))
                                  .filePath("receiver")
                            : supplied;
}
QString importRoot(const QString& supplied) {
  return supplied.isEmpty() ? QFileInfo(QStandardPaths::writableLocation(
                                            QStandardPaths::AppConfigLocation))
                                  .dir()
                                  .filePath("SpellCircle")
                            : supplied;
}

std::optional<QJsonObject> readSettings(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
  const auto document = QJsonDocument::fromJson(file.readAll());
  if (!document.isObject()) return std::nullopt;
  return document.object();
}

/** The first existing file owns the answer, even if it cannot be decoded.
 *  An unreadable current file must not silently restore another directory's
 *  settings. Importing never changes or writes any of these files. */
std::optional<QJsonObject> importSettings(const QString& name,
                                          const QString& settingsDirectory,
                                          const QString& importDirectory) {
  for (const auto& directory : {settingsDirectory, importDirectory,
                                QCoreApplication::applicationDirPath()}) {
    const QString path = QDir(directory).filePath(name);
    if (QFile::exists(path)) return readSettings(path);
  }
  return std::nullopt;
}
}  // namespace

Receiver::Receiver(sigil::seer::Wires& wires, QObject* parent,
                   QString settingsDirectory, QString importDirectory,
                   WireDeferrer deferWireChange)
    : QObject(parent),
      m_wires(wires),
      m_deferWireChange(std::move(deferWireChange)),
      m_settingsDirectory(storageRoot(settingsDirectory)),
      m_config(this),
      m_model(this) {
  if (const auto values = readSettings(
          QDir(m_settingsDirectory).filePath("receiver_config.json"))) {
    const auto graphics = values->value("graphics");
    const QString source = values->value("uri").toString();
    if (graphics.isObject() && !source.isEmpty()) {
      m_config.restore(graphics.toObject());
      m_uri = source;
      return;
    }
  }
  const QString imported = importRoot(importDirectory);
  if (const auto graphics =
          importSettings("graphics_config.json", m_settingsDirectory, imported))
    m_config.restore(*graphics);
  if (const auto network = importSettings("network_config.json",
                                          m_settingsDirectory, imported)) {
    const QString source = network->value("uri").toString();
    if (!source.isEmpty())
      m_uri = source;
    else {
      const int port =
          network->value("port").toInt(spellcircle::ReceiverDefaults::port);
      if (port > 0 && port <= 65535) m_uri = QString("udp://:%1").arg(port);
    }
  }
}

int Receiver::port() const {
  return QUrl(m_uri).port(spellcircle::ReceiverDefaults::port);
}
bool Receiver::recorded() const {
  return m_wires.recorded(m_uri.toStdString());
}

void Receiver::setUri(const QString& uri) {
  if (m_deferWireChange && m_deferWireChange([this, uri] { setUri(uri); }))
    return;
  const QString next = uri.trimmed();
  if (next.isEmpty() || next == m_uri) return;
  const bool running = m_listening;
  if (running) m_wires.close(m_uri.toStdString());
  m_uri = next;
  if (running)
    start();
  else {
    refresh();
    emit changed();
  }
  emit wiresChanged();
}

void Receiver::setPort(int port) {
  if (port > 0 && port <= 65535) setUri(QString("udp://:%1").arg(port));
}

void Receiver::start() {
  if (m_deferWireChange && m_deferWireChange([this] { start(); })) return;
  const auto uri = m_uri.toStdString();
  if (const auto previous = m_wires.feed(uri);
      previous && (previous->closed() || !previous->error().empty()))
    m_wires.close(uri);
  m_wires.open(uri);
  m_opened = true;
  refresh();
  emit changed();
  emit wiresChanged();
}

void Receiver::stop() {
  if (m_deferWireChange && m_deferWireChange([this] { stop(); })) return;
  m_wires.close(m_uri.toStdString());
  refresh();
  emit wiresChanged();
}

void Receiver::refresh() {
  bool listening = false;
  QString status;
  {
    const auto feed = m_wires.feed(m_uri.toStdString());
    listening = feed && !feed->closed() && feed->error().empty();
    if (feed && !feed->error().empty())
      status = QString::fromStdString(feed->error());
    else if (recorded())
      status = listening ? "Playing recording · " + m_uri
                         : "Recording stopped · " + m_uri;
    else
      status = listening ? "Listening · " + m_uri : "Stopped · " + m_uri;
  }
  if (m_listening == listening && m_status == status) return;
  m_listening = listening;
  m_status = status;
  emit changed();
}

void Receiver::accept(const sigil::io::Feed& feed,
                      const sigil::io::Arrival& arrival) {
  if (!arrival.bytes) return;
  QString from = QString::fromStdString(arrival.from);
  const qsizetype scheme = from.indexOf("://");
  if (scheme >= 0) from = from.mid(scheme + 3);
  if (from.isEmpty()) from = recorded() ? "recording" : m_uri;
  const auto& bytes = arrival.bytes->bytes;
  m_model.onSpellCircleReceived(
      from,
      QByteArray::fromRawData(reinterpret_cast<const char*>(bytes.data()),
                              static_cast<qsizetype>(bytes.size())),
      feed.receivedAt(arrival));
}

void Receiver::beginSettings() {
  m_editConfig = m_config.snapshot();
  m_editUri = m_uri;
  m_editListening = m_listening;
}

void Receiver::cancelSettings() {
  if (m_deferWireChange && m_deferWireChange([this] { cancelSettings(); }))
    return;
  if (m_editUri.isEmpty()) return;
  m_config.restore(m_editConfig);
  if (m_listening && !m_editListening) stop();
  setUri(m_editUri);
  if (m_editListening && !m_listening) start();
  m_editUri.clear();
}

bool Receiver::saveSettings() {
  if (!QDir().mkpath(m_settingsDirectory)) return false;
  QSaveFile file(QDir(m_settingsDirectory).filePath("receiver_config.json"));
  if (!file.open(QIODevice::WriteOnly)) return false;
  const QByteArray bytes =
      QJsonDocument(
          QJsonObject{{"uri", m_uri}, {"graphics", m_config.snapshot()}})
          .toJson();
  if (file.write(bytes) != bytes.size() || !file.commit()) return false;
  m_editUri.clear();
  return true;
}
