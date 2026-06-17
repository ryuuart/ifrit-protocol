#pragma once
#include "FeedModel.h"
#include <QtCanvasPainter/QCanvasPainterItemRenderer>

class SpellCircleRenderer : public QCanvasPainterItemRenderer {
public:
  void synchronize(QCanvasPainterItem *item) override;
  void initializeResources(QCanvasPainter *p) override;
  void prePaint(QCanvasPainter *p) override;
  void paint(QCanvasPainter *p) override;

private:
  QList<CircleGeometry> m_circles;
};
