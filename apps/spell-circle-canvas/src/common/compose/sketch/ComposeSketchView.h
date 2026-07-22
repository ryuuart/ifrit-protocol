#pragma once

#include "SketchHost.h"

#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtQuick/QQuickRhiItem>

/** The live canvas: a QQuickRhiItem in the same mold as ComposeGallery —
 *  frames render on the render thread, through the shared Skia Graphite
 *  context straight into the item's texture when the QRhi backend is
 *  supported, with an explicit raster-and-upload fallback elsewhere.
 *  Build state surfaces to QML; captures run async on the render thread
 *  (result via captureReady). */
class ComposeSketchView : public QQuickRhiItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QString status READ status NOTIFY stateChanged)
  Q_PROPERTY(QString errorLog READ errorLog NOTIFY stateChanged)
  Q_PROPERTY(bool compiling READ compiling NOTIFY stateChanged)
  Q_PROPERTY(QString state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString metrics READ metrics NOTIFY metricsChanged)

public:
  explicit ComposeSketchView(QQuickItem *parent = nullptr);
  ~ComposeSketchView() override;

  QQuickRhiItemRenderer *createRenderer() override;

  /** Requests a capture of the current frame; render-thread work, so the
   *  saved path (or "" on failure) arrives via captureReady(). Writes to
   *  <sketch dir>/captures/<stem>-NNN.png at 2x. */
  Q_INVOKABLE void capture();

  QString status() const { return m_status; }
  QString errorLog() const { return m_errorLog; }
  bool compiling() const { return m_compiling; }
  QString state() const { return m_state; }
  QString metrics() const { return m_metrics; }

  /** Wired up by main() before the QML scene loads. */
  static sigil::compose::sketch::SketchHost *host;
  /** The GUI thread polls/reloads while the render thread frames —
   *  every host access on either side takes this. */
  static QMutex hostMutex;

signals:
  void stateChanged();
  void metricsChanged();
  void captureReady(const QString &path);

private:
  friend class ComposeSketchRenderer;
  QTimer m_timer;
  QString m_status;
  QString m_errorLog;
  QString m_state = "waiting";
  QString m_metrics;
  bool m_compiling = false;
  int m_captureRequests = 0; // consumed by the renderer in synchronize()
};
