#pragma once
#include "GraphicsConfig.h"
#include "SpellCircleModel.h"
#include <QtCanvasPainter/QCanvasPainterItem>

/**
 * QML item that renders incoming spell circle geometry via QCanvasPainter.
 * Binds to a SpellCircleModel through the @c model property and a
 * GraphicsConfig through the @c config property, scheduling a repaint
 * whenever either changes.
 */
class SpellCircle : public QCanvasPainterItem {
public:
  SpellCircle(QQuickItem *parent = nullptr);

  /** Creates the render-thread renderer for this item. */
  QCanvasPainterItemRenderer *createItemRenderer() const override;

  SpellCircleModel *model() const { return m_model; }

  /** Accepts a plain QObject* for QML property compatibility; casts internally. */
  void setModel(QObject *model);

  GraphicsConfig *config() const { return m_config; }

  /** Accepts a plain QObject* for QML property compatibility; casts internally. */
  void setConfig(QObject *config);

signals:
  void modelChanged();
  void configChanged();

private:
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QObject *model READ model WRITE setModel NOTIFY modelChanged)
  Q_PROPERTY(QObject *config READ config WRITE setConfig NOTIFY configChanged)

  SpellCircleModel *m_model = nullptr;
  GraphicsConfig *m_config = nullptr;
};
