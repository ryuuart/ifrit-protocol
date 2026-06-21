#pragma once
#include "FeedModel.h"
#include <QtCanvasPainter/QCanvasPainterItem>

/**
 * QML item that renders incoming spell circle geometry via QCanvasPainter.
 * Binds to a FeedModel through the @c model property and schedules a repaint
 * on each circlesChanged signal.
 */
class SpellCircle : public QCanvasPainterItem {
public:
  SpellCircle(QQuickItem *parent = nullptr);

  /** Creates the render-thread renderer for this item. */
  QCanvasPainterItemRenderer *createItemRenderer() const override;

  FeedModel *model() const { return m_feedModel; }

  /** Accepts a plain QObject* for QML property compatibility; casts to FeedModel internally. */
  void setFeedModel(QObject *model);

signals:
  void feedModelChanged();

private:
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(
      QObject *model READ model WRITE setFeedModel NOTIFY feedModelChanged)

  FeedModel *m_feedModel = nullptr;
};
