#pragma once
#include "../../network/FeedModel.h"
#include <QtCanvasPainter/QCanvasPainterItem>

class SpellCircle : public QCanvasPainterItem {
public:
  SpellCircle(QQuickItem *parent = nullptr);

  QCanvasPainterItemRenderer *createItemRenderer() const override;

  FeedModel *model() const { return m_feedModel; }
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
