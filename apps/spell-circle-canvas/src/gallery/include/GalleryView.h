#pragma once

// The gallery's canvas item: a QQuickRhiItem whose renderer draws the
// active TextFlow scene with Skia — Graphite on Qt's own Metal device/queue
// when available (the same GPU path as the SpellCircle app, via
// SkiaGraphiteContext/SkiaOffscreenSurface), falling back to Skia's CPU
// raster backend uploaded into the item's texture otherwise.
//
// Threading: the renderer (scenes, FontContext, Graphite) lives on the Qt
// Quick render thread. The GUI-side item keeps its own scene instances
// purely for metadata (names, default text, editability) and ships control
// state across in synchronize(), which runs with the GUI thread blocked.

#include "GalleryScenes.h"

#include <QElapsedTimer>
#include <QQuickRhiItem>
#include <QTimer>

#include <memory>
#include <vector>

class GalleryView : public QQuickRhiItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QStringList sceneNames READ sceneNames CONSTANT)
  Q_PROPERTY(int sceneIndex READ sceneIndex WRITE setSceneIndex NOTIFY
                 sceneIndexChanged)
  Q_PROPERTY(bool animating READ animating WRITE setAnimating NOTIFY
                 animatingChanged)
  Q_PROPERTY(bool textEditable READ textEditable NOTIFY sceneIndexChanged)
  Q_PROPERTY(
      QString sceneText READ sceneText WRITE setSceneText NOTIFY sceneTextChanged)
  Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY
                 fontFamilyChanged)
  Q_PROPERTY(
      qreal fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
  Q_PROPERTY(int fontWeight READ fontWeight WRITE setFontWeight NOTIFY
                 fontWeightChanged)
  Q_PROPERTY(int alignmentIndex READ alignmentIndex WRITE setAlignmentIndex
                 NOTIFY alignmentIndexChanged)
  Q_PROPERTY(int breakerIndex READ breakerIndex WRITE setBreakerIndex NOTIFY
                 breakerIndexChanged)
  Q_PROPERTY(bool gpuAvailable READ gpuAvailable CONSTANT)
  Q_PROPERTY(bool gpu READ gpu WRITE setGpu NOTIFY gpuChanged)
  Q_PROPERTY(QString stats READ stats NOTIFY statsChanged)

public:
  explicit GalleryView(QQuickItem *parent = nullptr);
  ~GalleryView() override;

  QQuickRhiItemRenderer *createRenderer() override;

  QStringList sceneNames() const;
  int sceneIndex() const { return m_sceneIndex; }
  void setSceneIndex(int index);
  bool animating() const { return m_animating; }
  void setAnimating(bool on);
  bool textEditable() const;
  QString sceneText() const { return m_sceneText; }
  void setSceneText(const QString &text);
  QString fontFamily() const { return m_fontFamily; }
  void setFontFamily(const QString &family);
  qreal fontSize() const { return m_fontSize; }
  void setFontSize(qreal size);
  int fontWeight() const { return m_fontWeight; }
  void setFontWeight(int weight);
  int alignmentIndex() const { return m_alignmentIndex; }
  void setAlignmentIndex(int index);
  int breakerIndex() const { return m_breakerIndex; }
  void setBreakerIndex(int index);
  bool gpuAvailable() const;
  bool gpu() const { return m_gpu; }
  void setGpu(bool on);
  QString stats() const { return m_stats; }

signals:
  void sceneIndexChanged();
  void animatingChanged();
  void sceneTextChanged();
  void fontFamilyChanged();
  void fontSizeChanged();
  void fontWeightChanged();
  void alignmentIndexChanged();
  void breakerIndexChanged();
  void gpuChanged();
  void statsChanged();

protected:
  void mousePressEvent(QMouseEvent *event) override;

private:
  friend class GalleryViewRenderer;

  // GUI-side scene instances: metadata only (never rendered with).
  std::vector<std::unique_ptr<gallery::Scene>> m_metaScenes;

  int m_sceneIndex = 0;
  bool m_animating = true;
  QString m_sceneText;
  QString m_fontFamily;
  qreal m_fontSize = 17.0;
  int m_fontWeight = 0; // 0 = font default; else a variable `wght` position
  int m_alignmentIndex = 3; // kJustify
  int m_breakerIndex = 0;   // kGreedy
  bool m_gpu = true;        // Graphite when available; raster otherwise
  std::vector<SkPoint> m_pendingClicks;

  QTimer m_timer;
  QString m_stats;
};
