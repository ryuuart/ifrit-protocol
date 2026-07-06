#pragma once
#include <QColor>
#include <QFont>
#include <QObject>

/** Box width/height/padding, exposed as a grouped QML property on
 *  GraphicsConfig (Models.graphicsConfig.box.width, etc.) — the same
 *  convention Qt itself uses for Rectangle.border. */
class BoxStyleConfig : public QObject {
  Q_OBJECT
  Q_PROPERTY(qreal width READ width WRITE setWidth NOTIFY changed)
  Q_PROPERTY(qreal height READ height WRITE setHeight NOTIFY changed)
  Q_PROPERTY(qreal padding READ padding WRITE setPadding NOTIFY changed)

public:
  explicit BoxStyleConfig(QObject *parent = nullptr) : QObject(parent) {}

  qreal width() const { return m_width; }
  void setWidth(qreal width);

  qreal height() const { return m_height; }
  void setHeight(qreal height);

  qreal padding() const { return m_padding; }
  void setPadding(qreal padding);

signals:
  void changed();

private:
  qreal m_width = 360.0;
  qreal m_height = 140.0;
  qreal m_padding = 16.0;
};

/** Native canvas width/height, exposed as a grouped QML property
 *  (Models.graphicsConfig.canvas.width, etc.). Drives both the coordinate
 *  space SpellCircleModel scales scenes into and SpellCircleRenderer's
 *  offscreen framebuffer dimensions, so the two stay in sync. */
class CanvasSizeConfig : public QObject {
  Q_OBJECT
  Q_PROPERTY(int width READ width WRITE setWidth NOTIFY changed)
  Q_PROPERTY(int height READ height WRITE setHeight NOTIFY changed)

public:
  explicit CanvasSizeConfig(QObject *parent = nullptr) : QObject(parent) {}

  int width() const { return m_width; }
  void setWidth(int width);

  int height() const { return m_height; }
  void setHeight(int height);

signals:
  void changed();

private:
  int m_width = 4000;
  int m_height = 4000;
};

/**
 * QML-exposed graphics style configuration for the SpellCircle renderer
 * (accent color, stroke width, global scale, typography, box geometry).
 * Owned by the Models singleton (Models.graphicsConfig) and consumed by
 * SpellCircle/SpellCircleRenderer the same way SpellCircleModel is.
 *
 * Loads from a JSON file next to the executable on construction, falling
 * back to the defaults below if the file is missing or invalid. Persists
 * only when save() is called explicitly (e.g. from a settings window).
 */
class GraphicsConfig : public QObject {
  Q_OBJECT
  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(qreal strokeWidth READ strokeWidth WRITE setStrokeWidth NOTIFY
                 strokeWidthChanged)
  Q_PROPERTY(qreal scale READ scale WRITE setScale NOTIFY scaleChanged)
  Q_PROPERTY(qreal labelOffset READ labelOffset WRITE setLabelOffset NOTIFY
                 labelOffsetChanged)
  Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
  Q_PROPERTY(BoxStyleConfig *box READ box CONSTANT)
  Q_PROPERTY(CanvasSizeConfig *canvas READ canvas CONSTANT)
  Q_PROPERTY(
      int generation READ generation NOTIFY generationChanged)

public:
  explicit GraphicsConfig(QObject *parent = nullptr);

  QColor color() const { return m_color; }
  void setColor(const QColor &color);

  qreal strokeWidth() const { return m_strokeWidth; }
  void setStrokeWidth(qreal strokeWidth);

  qreal scale() const { return m_scale; }
  void setScale(qreal scale);

  /** Perpendicular offset (px, pre-scale) applied to every circle's label
   *  along its text path — no longer authored per-circle over the wire, so
   *  this is the single knob for nudging all labels uniformly. */
  qreal labelOffset() const { return m_labelOffset; }
  void setLabelOffset(qreal labelOffset);

  QFont font() const { return m_font; }
  void setFont(const QFont &font);

  BoxStyleConfig *box() const { return m_box; }

  CanvasSizeConfig *canvas() const { return m_canvas; }

  int generation() const { return m_generation; }

  /** Reads the JSON config file next to the executable, if present.
   *  Returns false (leaving current values untouched) if the file is
   *  missing or malformed. */
  Q_INVOKABLE bool load();

  /** Writes the current configuration to the JSON file next to the
   *  executable. Returns false if the file could not be written. */
  Q_INVOKABLE bool save() const;

signals:
  void colorChanged();
  void strokeWidthChanged();
  void scaleChanged();
  void labelOffsetChanged();
  void fontChanged();
  void generationChanged();

private:
  static QString configFilePath();
  void bumpGeneration();

  QColor m_color{"#ff0000"};
  qreal m_strokeWidth = 4.0;
  qreal m_scale = 1.0;
  qreal m_labelOffset = 0.0;
  QFont m_font;
  BoxStyleConfig *m_box;
  CanvasSizeConfig *m_canvas;
  int m_generation = 0;
};
