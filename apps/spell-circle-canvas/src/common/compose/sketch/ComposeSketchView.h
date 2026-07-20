#pragma once

#include "SketchHost.h"

#include <QtCore/QTimer>
#include <QtGui/QImage>
#include <QtQuick/QQuickPaintedItem>

#include <memory>

/** The live canvas: paints the SketchHost every frame (DPI-aware,
 *  letterboxed) and surfaces build state to QML. */
class ComposeSketchView : public QQuickPaintedItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QString status READ status NOTIFY stateChanged)
  Q_PROPERTY(QString errorLog READ errorLog NOTIFY stateChanged)
  Q_PROPERTY(bool compiling READ compiling NOTIFY stateChanged)
  Q_PROPERTY(QString state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString metrics READ metrics NOTIFY metricsChanged)

public:
  explicit ComposeSketchView(QQuickItem *parent = nullptr);

  void paint(QPainter *painter) override;

  QString status() const { return m_status; }
  QString errorLog() const { return m_errorLog; }
  bool compiling() const { return m_compiling; }
  QString state() const { return m_state; }
  QString metrics() const { return m_metrics; }

  /** Wired up by main() before the QML scene loads. */
  static sigil::compose::sketch::SketchHost *host;

signals:
  void stateChanged();
  void metricsChanged();

private:
  QTimer m_timer;
  QImage m_frame;
  QString m_status;
  QString m_errorLog;
  QString m_state = "waiting";
  QString m_metrics;
  bool m_compiling = false;
};
