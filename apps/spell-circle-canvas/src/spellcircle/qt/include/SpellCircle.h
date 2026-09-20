#pragma once

/** @file
 * The QML item a receiver's canvas is: the scene model and the graphics
 * settings it watches, and the repaint it schedules when either moves.
 */

#include <QtCanvasPainter/QCanvasPainterItem>

#include "GraphicsConfig.h"
#include "SpellCircleModel.h"

/**
 * QML item that renders incoming spell circle geometry via QCanvasPainter.
 * Binds to a SpellCircleModel through the @c model property and a
 * GraphicsConfig through the @c config property, scheduling a repaint
 * whenever either changes.
 */
class SpellCircle : public QCanvasPainterItem {
 public:
  explicit SpellCircle(QQuickItem* parent = nullptr);

  /** Creates the render-thread renderer for this item. */
  QCanvasPainterItemRenderer* createItemRenderer() const override;

  /** Returns the scene model currently observed by this item. */
  SpellCircleModel* model() const { return m_model; }

  /** Accepts a plain QObject* for QML property compatibility; casts internally.
   */
  void setModel(QObject* model);

  /** Returns the graphics configuration currently observed by this item. */
  GraphicsConfig* config() const { return m_config; }

  /** Accepts a plain QObject* for QML property compatibility; casts internally.
   */
  void setConfig(QObject* config);

 signals:
  /** Another scene model was bound to this item. */
  void modelChanged();
  /** Another graphics configuration was bound to this item. */
  void configChanged();

 private:
  Q_OBJECT
  QML_ELEMENT
  /** The scene model this item draws from. */
  Q_PROPERTY(QObject* model READ model WRITE setModel NOTIFY modelChanged)
  /** The graphics settings this item draws with. */
  Q_PROPERTY(QObject* config READ config WRITE setConfig NOTIFY configChanged)

  SpellCircleModel* m_model = nullptr;
  GraphicsConfig* m_config = nullptr;
};
