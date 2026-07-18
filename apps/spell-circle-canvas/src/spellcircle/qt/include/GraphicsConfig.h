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
  Q_PROPERTY(qreal distance READ distance WRITE setDistance NOTIFY changed)

public:
  explicit BoxStyleConfig(QObject *parent = nullptr) : QObject(parent) {}

  /** Returns the configured box width in author-space pixels. */
  qreal width() const { return m_width; }
  /** Sets the box width. */
  void setWidth(qreal width);

  /** Returns the configured box height in author-space pixels. */
  qreal height() const { return m_height; }
  /** Sets the box height. */
  void setHeight(qreal height);

  /** Returns the padding between box content and its border. */
  qreal padding() const { return m_padding; }
  /** Sets the box content padding. */
  void setPadding(qreal padding);

  /** Distance (px, pre-scale) the box's inner edge sits beyond its assigned
   *  point, measured outward along the ray from the canvas center through
   *  that point. */
  qreal distance() const { return m_distance; }
  /** Sets the outward distance from an anchor point to the box edge. */
  void setDistance(qreal distance);

signals:
  void changed();

private:
  qreal m_width = 360.0;
  qreal m_height = 140.0;
  qreal m_padding = 16.0;
  qreal m_distance = 40.0;
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

  /** Returns the native render-target width in pixels. */
  int width() const { return m_width; }
  /** Sets the native render-target width. */
  void setWidth(int width);

  /** Returns the native render-target height in pixels. */
  int height() const { return m_height; }
  /** Sets the native render-target height. */
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
  Q_PROPERTY(qreal pointDistance READ pointDistance WRITE setPointDistance
                 NOTIFY pointDistanceChanged)
  Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
  Q_PROPERTY(BoxStyleConfig *box READ box CONSTANT)
  Q_PROPERTY(CanvasSizeConfig *canvas READ canvas CONSTANT)
  Q_PROPERTY(int generation READ generation NOTIFY generationChanged)

public:
  explicit GraphicsConfig(QObject *parent = nullptr);

  /** Returns the accent color used for scene geometry and labels. */
  QColor color() const { return m_color; }
  /** Sets the scene accent color. */
  void setColor(const QColor &color);

  /** Returns geometry stroke width in author-space pixels. */
  qreal strokeWidth() const { return m_strokeWidth; }
  /** Sets geometry stroke width. */
  void setStrokeWidth(qreal strokeWidth);

  /** Returns the global geometry and typography scale. */
  qreal scale() const { return m_scale; }
  /** Sets the global geometry and typography scale. */
  void setScale(qreal scale);

  /** Perpendicular offset (px, pre-scale) applied to every circle's label
   *  along its text path — no longer authored per-circle over the wire, so
   *  this is the single knob for nudging all labels uniformly. */
  qreal labelOffset() const { return m_labelOffset; }
  /** Sets the global circle-label path offset. */
  void setLabelOffset(qreal labelOffset);

  /** Distance (px, pre-scale) a Point's own value label sits beyond its
   *  position, measured outward along the ray from the canvas center
   *  through that point — the same offset a Box uses (box.distance), but a
   *  separate knob since a point label isn't a box. */
  qreal pointDistance() const { return m_pointDistance; }
  /** Sets the outward distance used for standalone point labels. */
  void setPointDistance(qreal pointDistance);

  /** Returns the font shared by scene labels. */
  QFont font() const { return m_font; }
  /** Sets the font shared by scene labels. */
  void setFont(const QFont &font);

  /** Returns grouped box style configuration owned by this object. */
  BoxStyleConfig *box() const { return m_box; }

  /** Returns grouped native canvas dimensions owned by this object. */
  CanvasSizeConfig *canvas() const { return m_canvas; }

  /** Returns the revision used by renderers to detect configuration changes. */
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
  void pointDistanceChanged();
  void fontChanged();
  void generationChanged();

private:
  /** Returns the platform-specific persistent configuration path. */
  static QString configFilePath();
  /** Advances the revision and emits `generationChanged()`. */
  void bumpGeneration();

  QColor m_color{"#ff0000"};
  qreal m_strokeWidth = 4.0;
  qreal m_scale = 1.0;
  qreal m_labelOffset = 0.0;
  qreal m_pointDistance = 40.0;
  QFont m_font;
  BoxStyleConfig *m_box;
  CanvasSizeConfig *m_canvas;
  int m_generation = 0;
};
