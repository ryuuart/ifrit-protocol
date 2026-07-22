#pragma once

// The QML-embedded compose surface. QQuickRhiItem gives the render-thread
// counterpart ownership of the Composer and lets Skia Graphite draw straight
// into Qt Quick's texture. The CPU fallback still uploads one raster buffer,
// but avoids QQuickPaintedItem's extra QPainter backing-store copy.

#include <QQuickRhiItem>
#include <QVariantList>
#include <QVariantMap>

class ComposeGalleryRenderer;

class ComposeGalleryView : public QQuickRhiItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QVariantList scenes READ scenes CONSTANT)
  Q_PROPERTY(int sceneIndex READ sceneIndex WRITE setSceneIndex NOTIFY
                 sceneIndexChanged)
  Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
  Q_PROPERTY(double timeScale READ timeScale WRITE setTimeScale NOTIFY
                 timeScaleChanged)
  // Structured, not preformatted: the panel is narrow and its width is the
  // user's to drag, so QML owns the elision and the value formatting. A
  // fixed-width block composed here is what used to run off the sidebar.
  Q_PROPERTY(QVariantMap metrics READ metrics NOTIFY metricsChanged)
public:
  explicit ComposeGalleryView(QQuickItem *parent = nullptr);
  ~ComposeGalleryView() override;

  QQuickRhiItemRenderer *createRenderer() override;

  QVariantList scenes() const;
  int sceneIndex() const { return m_sceneIndex; }
  void setSceneIndex(int index);
  bool paused() const { return m_paused; }
  void setPaused(bool paused);
  double timeScale() const { return m_timeScale; }
  void setTimeScale(double scale);
  QVariantMap metrics() const { return m_metrics; }

signals:
  void sceneIndexChanged();
  void pausedChanged();
  void timeScaleChanged();
  void metricsChanged();

private:
  friend class ComposeGalleryRenderer;

  int m_sceneIndex = 0;
  bool m_paused = false;
  double m_timeScale = 1.0;
  QVariantMap m_metrics = {
      {QStringLiteral("backend"),
       QStringLiteral("hardware QRhi renderer required")}};
};
