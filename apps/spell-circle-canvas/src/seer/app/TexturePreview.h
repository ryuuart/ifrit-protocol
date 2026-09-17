#pragma once

#include <QtQml/qqmlregistration.h>

#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtQuick/QQuickItem>

// Native textures enter Qt's scene graph on its own device and remain GPU
// resident.
class TexturePreview : public QQuickItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
  Q_PROPERTY(QString application READ application WRITE setApplication NOTIFY
                 sourceChanged)
  Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
  Q_PROPERTY(bool connected READ connected NOTIFY frameChanged)
  Q_PROPERTY(QSize frameSize READ frameSize NOTIFY frameChanged)
  Q_PROPERTY(qulonglong frames READ frames NOTIFY frameChanged)
  Q_PROPERTY(QString status READ status NOTIFY frameChanged)
 public:
  explicit TexturePreview(QQuickItem* parent = nullptr);
  QString source() const { return m_source; }
  QString application() const { return m_application; }
  bool paused() const { return m_paused; }
  bool connected() const { return m_connected; }
  QSize frameSize() const { return m_size; }
  qulonglong frames() const { return m_frames; }
  QString status() const { return m_status; }
  void setSource(const QString& source);
  void setApplication(const QString& application);
  void setPaused(bool paused);
  Q_INVOKABLE void saveFrame(const QUrl& file);
 signals:
  void sourceChanged();
  void pausedChanged();
  void frameChanged();
  void saved(const QString& path, bool success);

 protected:
  QSGNode* updatePaintNode(QSGNode* node, UpdatePaintNodeData*) override;
  void reportFrame(bool connected, QSize size, qulonglong frames,
                   QString status);
  QString m_capturePath;

 private:
  QString m_source;
  QString m_application;
  QString m_status;
  QSize m_size;
  qulonglong m_frames = 0;
  bool m_paused = false;
  bool m_connected = false;
  QTimer m_frame{this};
};
