#pragma once
#include <QColor>
#include <QFont>
#include <QObject>

/** Geometry of the labelled boxes a scene can attach to its points, exposed as
 *  a grouped QML property on GraphicsConfig (Models.graphicsConfig.box.width,
 *  and so on) — the grouping convention Qt itself uses for Rectangle.border.
 *
 *  Lengths here are pre-scale: each is multiplied by GraphicsConfig's global
 *  scale and the result used directly as canvas pixels. They are NOT resized by
 *  the scene's own author-space-to-canvas conversion, so a box stays the same
 *  size on screen no matter what coordinate space a sender authored in. Any
 *  change emits changed(), which GraphicsConfig turns into a generation bump so
 *  renderers notice. */
class BoxStyleConfig : public QObject {
  Q_OBJECT
  Q_PROPERTY(qreal width READ width WRITE setWidth NOTIFY changed)
  Q_PROPERTY(qreal height READ height WRITE setHeight NOTIFY changed)
  Q_PROPERTY(qreal padding READ padding WRITE setPadding NOTIFY changed)
  Q_PROPERTY(qreal distance READ distance WRITE setDistance NOTIFY changed)

public:
  explicit BoxStyleConfig(QObject *parent = nullptr) : QObject(parent) {}

  /** Minimum outer width of a box. A box whose label plus padding needs more
   *  than this grows to fit rather than clipping the text, so this sets the
   *  width short labels share, not a maximum. */
  qreal width() const { return m_width; }
  void setWidth(qreal width);

  /** Outer height of a box, applied as given — unlike the width, it does not
   *  grow for its content. */
  qreal height() const { return m_height; }
  void setHeight(qreal height);

  /** Gap kept between a box's border and the label text inside it. */
  qreal padding() const { return m_padding; }
  void setPadding(qreal padding);

  /** The gap between the point a box is attached to and the box's nearest
   *  face, measured outward along the ray from the canvas center through that
   *  point — so a box always lands on the outside of the diagram, whichever
   *  side of it the point lies on, and a box that widens to fit a longer
   *  label grows away from its point rather than toward it. */
  qreal distance() const { return m_distance; }
  void setDistance(qreal distance);

signals:
  void changed();

private:
  qreal m_width = 360.0;
  qreal m_height = 140.0;
  qreal m_padding = 16.0;
  qreal m_distance = 40.0;
};

/** Size of the render target, in real pixels, exposed as a grouped QML property
 *  (Models.graphicsConfig.canvas.width, and so on).
 *
 *  One setting feeds two consumers that must agree: it is both the coordinate
 *  space an incoming author-space scene is scaled into and the size of the
 *  offscreen framebuffer that scene is drawn to. Splitting them into separate
 *  settings would let the geometry and the surface disagree. */
class CanvasSizeConfig : public QObject {
  Q_OBJECT
  Q_PROPERTY(int width READ width WRITE setWidth NOTIFY changed)
  Q_PROPERTY(int height READ height WRITE setHeight NOTIFY changed)

public:
  explicit CanvasSizeConfig(QObject *parent = nullptr) : QObject(parent) {}

  /** Render-target width in pixels. */
  int width() const { return m_width; }
  void setWidth(int width);

  /** Render-target height in pixels. */
  int height() const { return m_height; }
  void setHeight(int height);

signals:
  void changed();

private:
  int m_width = 4000;
  int m_height = 4000;
};

/**
 * How scenes are styled: accent color, stroke width, global scale, typography,
 * box geometry, and the render-target size. Reachable from QML as
 * Models.graphicsConfig, which owns the instance. Lives on the GUI thread: the
 * render side does not read these accessors while drawing, it copies the values
 * across when generation() tells it something changed.
 *
 * Nothing in here describes a scene — a scene arrives over the network and says
 * only where things are and what they are called. Everything about how those
 * things look is a local setting, which is why the same scene renders
 * differently on two machines by design.
 *
 * Reads graphics_config.json during construction — from the per-user
 * application config directory, or, when no file exists there yet, from the
 * directory holding the executable — keeping the defaults below wherever the
 * file is missing, unreadable, or malformed: startup never fails over
 * configuration. Writing is never automatic: values changed at runtime are
 * lost unless save() is called, so a settings window can experiment and then
 * discard.
 *
 * Every setter ignores a value equal to the current one, so a QML binding that
 * re-fires with the same number costs nothing. Anything that does change bumps
 * generation(), which is how renderers detect that they must rebuild.
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

  /** The single color a scene is drawn in: circles, edges, box borders, and
   *  label text, with fills using the same color at a per-item alpha. A scene
   *  cannot override it — the sender chooses shapes, the receiver chooses how
   *  they look. */
  QColor color() const { return m_color; }
  void setColor(const QColor &color);

  /** Stroke width for circles, edges, and box borders. */
  qreal strokeWidth() const { return m_strokeWidth; }
  void setStrokeWidth(qreal strokeWidth);

  /** Multiplier applied to every length here and to the font point size on the
   *  way to the renderer, so one knob retunes stroke weight, text, and box sizes
   *  together after changing the render-target size. It does not scale the scene
   *  itself — geometry is fitted to the canvas from the author-space dimensions
   *  the sender supplies. */
  qreal scale() const { return m_scale; }
  void setScale(qreal scale);

  /** Shifts every circle's curved label off its circle, perpendicular to the
   *  text path: positive outward, negative inward, zero sitting the glyphs'
   *  optical centers on the circle itself. The wire format carries no
   *  per-circle equivalent, so this moves all labels together or none. */
  qreal labelOffset() const { return m_labelOffset; }
  void setLabelOffset(qreal labelOffset);

  /** How far the center of a point's own value label sits from the point,
   *  measured outward along the ray from the canvas center — the same placement
   *  rule as box.distance, kept as a separate knob because a bare line of text
   *  needs different clearance than a bordered box. */
  qreal pointDistance() const { return m_pointDistance; }
  void setPointDistance(qreal pointDistance);

  /** The one font every label in the scene is drawn with; its point size is
   *  multiplied by scale() before use. */
  QFont font() const { return m_font; }
  void setFont(const QFont &font);

  /** Box geometry group. Owned by this object and alive as long as it is,
   *  so QML may hold the pointer. */
  BoxStyleConfig *box() const { return m_box; }

  /** Render-target size group. Owned by this object and alive as long as it is,
   *  so QML may hold the pointer. */
  CanvasSizeConfig *canvas() const { return m_canvas; }

  /** Counter incremented on every change to any value here, including those in
   *  the two groups above. A renderer compares it against the value it last
   *  drew with, which is cheaper and less error-prone than subscribing to each
   *  individual change signal. Only equality is meaningful; the magnitude
   *  carries nothing. */
  int generation() const { return m_generation; }

  /** Reads graphics_config.json from the per-user config directory, falling
   *  back to the copy beside the executable when the per-user file does not
   *  exist yet. Returns false, leaving all current values untouched, when no
   *  file is found or the one found is not a JSON object; keys the file omits
   *  keep their current values. Called during construction, and callable
   *  again to reload after an external edit. */
  Q_INVOKABLE bool load();

  /** Writes the current values to graphics_config.json in the per-user config
   *  directory, creating that directory if needed and overwriting the file.
   *  Returns false if the file could not be opened for writing. This is the
   *  only thing that persists anything — nothing here saves on change or at
   *  shutdown. */
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
  /** The path save() writes and load() prefers: graphics_config.json under
   *  the per-user application config directory
   *  (QStandardPaths::AppConfigLocation) — writable, outside the application
   *  bundle, and surviving a reinstall of the app. */
  static QString configFilePath();
  /** graphics_config.json in the directory holding the running executable,
   *  which on macOS is inside the .app bundle. Read only, and only when
   *  configFilePath() does not exist yet: a file that ended up here keeps
   *  loading until the next save() writes the per-user path, which then
   *  takes precedence. */
  static QString legacyConfigFilePath();
  /** Advances generation() and emits generationChanged(). Every setter that
   *  actually changes a value calls this, as does a successful load(); the
   *  grouped objects' changed() signals are connected to it. */
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
