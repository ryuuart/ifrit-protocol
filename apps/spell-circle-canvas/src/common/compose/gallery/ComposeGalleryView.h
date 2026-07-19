#pragma once

// The QML-embedded compose surface: a QQuickPaintedItem hosting a
// GalleryStage, driven by a repaint timer, exposing scene selection,
// clock controls, and live frame metrics to the sidebar.

#include "GalleryScenes.h"

#include <QtQuick/QQuickPaintedItem>

#include <QtCore/QTimer>

class ComposeGalleryView : public QQuickPaintedItem {
  Q_OBJECT
  Q_PROPERTY(int sceneIndex READ sceneIndex WRITE setSceneIndex NOTIFY
                 sceneIndexChanged)
  Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
  Q_PROPERTY(double timeScale READ timeScale WRITE setTimeScale NOTIFY
                 timeScaleChanged)
  Q_PROPERTY(QString metrics READ metrics NOTIFY metricsChanged)
  QML_ELEMENT

public:
  explicit ComposeGalleryView(QQuickItem *parent = nullptr);

  void paint(QPainter *painter) override;

  int sceneIndex() const { return m_sceneIndex; }
  void setSceneIndex(int index);
  bool paused() const { return m_stage.clock.paused(); }
  void setPaused(bool paused);
  double timeScale() const { return m_stage.clock.timeScale(); }
  void setTimeScale(double scale);
  QString metrics() const { return m_metrics; }

signals:
  void sceneIndexChanged();
  void pausedChanged();
  void timeScaleChanged();
  void metricsChanged();

private:
  compose_gallery::GalleryStage m_stage;
  QTimer m_timer;
  int m_sceneIndex = 0;
  QString m_metrics;
};
