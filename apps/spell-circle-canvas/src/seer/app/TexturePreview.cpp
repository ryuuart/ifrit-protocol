#include "TexturePreview.h"

#include <QtCore/QMetaObject>
#include <utility>

TexturePreview::TexturePreview(QQuickItem* parent) : QQuickItem(parent) {
  setFlag(ItemHasContents);
  connect(&m_frame, &QTimer::timeout, this, [this] {
    if (isVisible() && !m_paused && !m_source.isEmpty()) update();
  });
  m_frame.start(16);
}

void TexturePreview::setSource(const QString& source) {
  if (m_source == source) return;
  m_source = source;
  m_capturePath.clear();
  m_connected = false;
  m_size = {};
  m_frames = 0;
  m_status.clear();
  emit sourceChanged();
  emit frameChanged();
  update();
}

void TexturePreview::setApplication(const QString& application) {
  if (m_application == application) return;
  m_application = application;
  m_capturePath.clear();
  m_connected = false;
  m_size = {};
  m_frames = 0;
  m_status.clear();
  emit sourceChanged();
  emit frameChanged();
  update();
}

void TexturePreview::setPaused(bool paused) {
  if (m_paused == paused) return;
  m_paused = paused;
  emit pausedChanged();
  update();
}

void TexturePreview::saveFrame(const QUrl& file) {
  if (!file.isLocalFile() || file.toLocalFile().isEmpty()) return;
  m_capturePath = file.toLocalFile();
  update();
}

void TexturePreview::reportFrame(bool connected, QSize size, qulonglong frames,
                                 QString status) {
  const auto source = m_source;
  const auto application = m_application;
  QMetaObject::invokeMethod(
      this,
      [this, connected, size, frames, status = std::move(status), source,
       application] {
        if (m_source != source || m_application != application) return;
        if (m_connected == connected && m_size == size && m_frames == frames &&
            m_status == status)
          return;
        m_connected = connected;
        m_size = size;
        m_frames = frames;
        m_status = status;
        emit frameChanged();
      },
      Qt::QueuedConnection);
}

#ifndef Q_OS_MACOS
QSGNode* TexturePreview::updatePaintNode(QSGNode* node, UpdatePaintNodeData*) {
  delete node;
  reportFrame(false, {}, 0,
              QStringLiteral("Texture reception requires macOS with Metal."));
  if (!m_capturePath.isEmpty()) {
    const auto path = std::exchange(m_capturePath, {});
    QMetaObject::invokeMethod(
        this, [this, path] { emit saved(path, false); }, Qt::QueuedConnection);
  }
  return nullptr;
}
#endif
