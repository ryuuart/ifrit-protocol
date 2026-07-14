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
  Q_PROPERTY(
      bool animating READ animating WRITE setAnimating NOTIFY animatingChanged)
  Q_PROPERTY(bool textEditable READ textEditable NOTIFY sceneIndexChanged)
  Q_PROPERTY(QString sceneText READ sceneText WRITE setSceneText NOTIFY
                 sceneTextChanged)
  Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY
                 fontFamilyChanged)
  Q_PROPERTY(
      qreal fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
  Q_PROPERTY(int fontWeight READ fontWeight WRITE setFontWeight NOTIFY
                 fontWeightChanged)
  Q_PROPERTY(int alignmentIndex READ alignmentIndex WRITE setAlignmentIndex
                 NOTIFY alignmentIndexChanged)
  Q_PROPERTY(int lineBreakStrategyIndex READ lineBreakStrategyIndex WRITE
                 setLineBreakStrategyIndex NOTIFY lineBreakStrategyIndexChanged)
  Q_PROPERTY(bool gpuAvailable READ gpuAvailable CONSTANT)
  Q_PROPERTY(bool gpu READ gpu WRITE setGpu NOTIFY gpuChanged)
  Q_PROPERTY(QString stats READ stats NOTIFY statsChanged)

public:
  explicit GalleryView(QQuickItem *parent = nullptr);
  ~GalleryView() override;

  /** Creates the render-thread counterpart for this item. */
  QQuickRhiItemRenderer *createRenderer() override;

  /** Returns gallery scene names in switcher order. */
  QStringList sceneNames() const;
  /** Returns the active scene index. */
  int sceneIndex() const { return m_sceneIndex; }
  /** Selects a scene index after clamping it to the available range. */
  void setSceneIndex(int index);
  /** Returns whether time and frame counters advance. */
  bool animating() const { return m_animating; }
  /** Enables or pauses animation. */
  void setAnimating(bool enabled);
  /** Returns whether the active scene accepts custom text. */
  bool textEditable() const;
  /** Returns the current scene text override. */
  QString sceneText() const { return m_sceneText; }
  /** Sets the current scene text override. */
  void setSceneText(const QString &text);
  /** Returns the selected font family. */
  QString fontFamily() const { return m_fontFamily; }
  /** Selects a font family for scenes that use panel typography. */
  void setFontFamily(const QString &family);
  /** Returns the selected font size in points. */
  qreal fontSize() const { return m_fontSize; }
  /** Selects the font size in points. */
  void setFontSize(qreal size);
  /** Returns the selected variable-font weight, or zero for automatic. */
  int fontWeight() const { return m_fontWeight; }
  /** Selects a variable-font weight, or zero for automatic. */
  void setFontWeight(int weight);
  /** Returns the index into the gallery's text-alignment choices. */
  int alignmentIndex() const { return m_alignmentIndex; }
  /** Selects one of the gallery's text alignments. */
  void setAlignmentIndex(int index);
  /** Returns the index into the available line-breaking strategies. */
  int lineBreakStrategyIndex() const { return m_lineBreakStrategyIndex; }
  /** Selects the line-breaking strategy. */
  void setLineBreakStrategyIndex(int index);
  /** Returns whether the current Qt renderer supports the GPU path. */
  bool gpuAvailable() const;
  /** Returns whether GPU drawing is requested. */
  bool gpu() const { return m_gpu; }
  /** Enables or disables GPU drawing when available. */
  void setGpu(bool enabled);
  /** Returns the latest formatted rendering statistics. */
  QString stats() const { return m_stats; }

signals:
  void sceneIndexChanged();
  void animatingChanged();
  void sceneTextChanged();
  void fontFamilyChanged();
  void fontSizeChanged();
  void fontWeightChanged();
  void alignmentIndexChanged();
  void lineBreakStrategyIndexChanged();
  void gpuChanged();
  void statsChanged();

protected:
  /** Queues a click for delivery to the render-thread scene. */
  void mousePressEvent(QMouseEvent *event) override;

private:
  friend class GalleryViewRenderer;

  // GUI-side scene instances: metadata only (never rendered with).
  std::vector<std::unique_ptr<gallery::Scene>> m_sceneMetadata;

  int m_sceneIndex = 0;
  bool m_animating = true;
  QString m_sceneText;
  QString m_fontFamily;
  qreal m_fontSize = 17.0;
  int m_fontWeight = 0;     // 0 = font default; else a variable `wght` position
  int m_alignmentIndex = 3; // kJustify
  int m_lineBreakStrategyIndex = 0; // kGreedy
  bool m_gpu = true;                // Graphite when available
  std::vector<SkPoint> m_pendingClicks;

  QTimer m_timer;
  QString m_stats;
};
