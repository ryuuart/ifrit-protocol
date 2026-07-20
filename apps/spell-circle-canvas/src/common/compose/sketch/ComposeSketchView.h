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

public:
  explicit ComposeSketchView(QQuickItem *parent = nullptr);

  void paint(QPainter *painter) override;

  QString status() const { return m_status; }
  QString errorLog() const { return m_errorLog; }
  bool compiling() const { return m_compiling; }

  /** Wired up by main() before the QML scene loads. */
  static ifrit::compose::sketch::SketchHost *host;

signals:
  void stateChanged();

private:
  QTimer m_timer;
  QImage m_frame;
  QString m_status;
  QString m_errorLog;
  bool m_compiling = false;
};
