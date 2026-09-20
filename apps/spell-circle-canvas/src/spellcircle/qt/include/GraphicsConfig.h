#pragma once

/** @file
 * How a receiver draws the scenes it is sent — the colour, the stroke,
 * the type, the box geometry and the render-target size — as one
 * QML-reachable object the application owns.
 */

#include <QColor>
#include <QFont>
#include <QJsonObject>
#include <QObject>

#include "ReceiverDefaults.h"

/** Geometry of the labelled boxes a scene can attach to its points, exposed as
 *  a grouped QML property on GraphicsConfig (receiver.config.box.width,
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
  /** The width short labels share; a longer one widens its box. */
  Q_PROPERTY(qreal width READ width WRITE setWidth NOTIFY changed)
  /** Outer height, applied as given. */
  Q_PROPERTY(qreal height READ height WRITE setHeight NOTIFY changed)
  /** Gap between a box's border and its label. */
  Q_PROPERTY(qreal padding READ padding WRITE setPadding NOTIFY changed)
  /** Gap between a box's point and the box's nearest face. */
  Q_PROPERTY(qreal distance READ distance WRITE setDistance NOTIFY changed)

 public:
  explicit BoxStyleConfig(QObject* parent = nullptr) : QObject(parent) {}

  /** Minimum outer width of a box. A box whose label plus padding needs more
   *  than this grows to fit rather than clipping the text, so this sets the
   *  width short labels share, not a maximum. */
  qreal width() const { return m_width; }
  /** Sets the shared width; a value equal to the current one does
   *  nothing. */
  void setWidth(qreal width);

  /** Outer height of a box, applied as given — unlike the width, it does not
   *  grow for its content. */
  qreal height() const { return m_height; }
  /** Sets the outer height; a value equal to the current one does
   *  nothing. */
  void setHeight(qreal height);

  /** Gap kept between a box's border and the label text inside it. */
  qreal padding() const { return m_padding; }
  /** Sets the inner gap; a value equal to the current one does
   *  nothing. */
  void setPadding(qreal padding);

  /** The gap between the point a box is attached to and the box's nearest
   *  face, measured outward along the ray from the canvas center through that
   *  point — so a box always lands on the outside of the diagram, whichever
   *  side of it the point lies on, and a box that widens to fit a longer
   *  label grows away from its point rather than toward it. */
  qreal distance() const { return m_distance; }
  /** Sets the stand-off from the point; a value equal to the current
   *  one does nothing. */
  void setDistance(qreal distance);

 signals:
  /** Emitted when any value in this group actually moved. */
  void changed();

 private:
  qreal m_width = spellcircle::ReceiverDefaults::boxWidth;
  qreal m_height = spellcircle::ReceiverDefaults::boxHeight;
  qreal m_padding = spellcircle::ReceiverDefaults::boxPadding;
  qreal m_distance = spellcircle::ReceiverDefaults::boxDistance;
};

/** Size of the render target, in real pixels, exposed as a grouped QML property
 *  (receiver.config.canvas.width, and so on).
 *
 *  One setting feeds two consumers that must agree: it is both the coordinate
 *  space an incoming author-space scene is scaled into and the size of the
 *  offscreen framebuffer that scene is drawn to. Splitting them into separate
 *  settings would let the geometry and the surface disagree. */
class CanvasSizeConfig : public QObject {
  Q_OBJECT
  /** The smallest size either axis may be set to. */
  Q_PROPERTY(int minimum READ minimum CONSTANT)
  /** The largest size either axis may be set to. */
  Q_PROPERTY(int maximum READ maximum CONSTANT)
  /** Render-target width in pixels. */
  Q_PROPERTY(int width READ width WRITE setWidth NOTIFY changed)
  /** Render-target height in pixels. */
  Q_PROPERTY(int height READ height WRITE setHeight NOTIFY changed)

 public:
  explicit CanvasSizeConfig(QObject* parent = nullptr) : QObject(parent) {}

  /** The smallest size either axis may be set to. */
  int minimum() const {
    return spellcircle::ReceiverDefaults::minimumCanvasSize;
  }
  /** The largest size either axis may be set to. */
  int maximum() const {
    return spellcircle::ReceiverDefaults::maximumCanvasSize;
  }

  /** Render-target width in pixels. */
  int width() const { return m_width; }
  /** Sets the render-target width, held between minimum and maximum. */
  void setWidth(int width);

  /** Render-target height in pixels. */
  int height() const { return m_height; }
  /** Sets the render-target height, held between minimum and
   *  maximum. */
  void setHeight(int height);

 signals:
  /** Emitted when either axis actually moved. */
  void changed();

 private:
  int m_width = spellcircle::ReceiverDefaults::canvasWidth;
  int m_height = spellcircle::ReceiverDefaults::canvasHeight;
};

/**
 * How scenes are styled: accent color, stroke width, global scale, typography,
 * box geometry, and the render-target size. Reachable from QML as
 * the application-owned receiver.config. Lives on the GUI thread: the
 * render side does not read these accessors while drawing, it copies the values
 * across when generation() tells it something changed.
 *
 * Nothing in here describes a scene — a scene arrives over the network and says
 * only where things are and what they are called. Everything about how those
 * things look is a local setting, which is why the same scene renders
 * differently on two machines by design.
 *
 * This object owns values, not files. Its application takes and restores
 * snapshots for persistence and an editor's explicit accept/cancel.
 * Constructing a configuration always starts from the defaults below and
 * performs no IO.
 *
 * Every setter ignores a value equal to the current one, so a QML binding that
 * re-fires with the same number costs nothing. Anything that does change bumps
 * generation(), which is how renderers detect that they must rebuild.
 */
class GraphicsConfig : public QObject {
  Q_OBJECT
  /** The one colour every part of a scene is drawn in. */
  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  /** Stroke width for circles, edges and box borders. */
  Q_PROPERTY(qreal strokeWidth READ strokeWidth WRITE setStrokeWidth NOTIFY
                 strokeWidthChanged)
  /** Multiplier over every length and the font size here. */
  Q_PROPERTY(qreal scale READ scale WRITE setScale NOTIFY scaleChanged)
  /** How far a circle's curved label sits off its circle. */
  Q_PROPERTY(qreal labelOffset READ labelOffset WRITE setLabelOffset NOTIFY
                 labelOffsetChanged)
  /** How far a point's own label sits from its point. */
  Q_PROPERTY(qreal pointDistance READ pointDistance WRITE setPointDistance
                 NOTIFY pointDistanceChanged)
  /** The one font every label is drawn with. */
  Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
  /** The box geometry group. */
  Q_PROPERTY(BoxStyleConfig* box READ box CONSTANT)
  /** The render-target size group. */
  Q_PROPERTY(CanvasSizeConfig* canvas READ canvas CONSTANT)
  /** A counter a renderer compares against to know it must rebuild. */
  Q_PROPERTY(int generation READ generation NOTIFY generationChanged)

 public:
  explicit GraphicsConfig(QObject* parent = nullptr);

  /** A value snapshot for a settings editor's explicit accept/cancel. */
  QJsonObject snapshot() const;
  /** Applies a snapshot's values, leaving any this object does not
   *  know standing. */
  void restore(const QJsonObject& values);

  /** The single color a scene is drawn in: circles, edges, box borders, and
   *  label text, with fills using the same color at a per-item alpha. A scene
   *  cannot override it — the sender chooses shapes, the receiver chooses how
   *  they look. */
  QColor color() const { return m_color; }
  /** Sets the accent colour; a value equal to the current one does
   *  nothing. */
  void setColor(const QColor& color);

  /** Stroke width for circles, edges, and box borders. */
  qreal strokeWidth() const { return m_strokeWidth; }
  /** Sets the stroke width; a value equal to the current one does
   *  nothing. */
  void setStrokeWidth(qreal strokeWidth);

  /** Multiplier applied to every length here and to the font point size on the
   *  way to the renderer, so one knob retunes stroke weight, text, and box
   * sizes together after changing the render-target size. It does not scale the
   * scene itself — geometry is fitted to the canvas from the author-space
   * dimensions the sender supplies. */
  qreal scale() const { return m_scale; }
  /** Sets the multiplier; a value equal to the current one does
   *  nothing. */
  void setScale(qreal scale);

  /** Shifts every circle's curved label off its circle, perpendicular to the
   *  text path: positive outward, negative inward, zero sitting the glyphs'
   *  optical centers on the circle itself. The wire format carries no
   *  per-circle equivalent, so this moves all labels together or none. */
  qreal labelOffset() const { return m_labelOffset; }
  /** Sets the label offset; a value equal to the current one does
   *  nothing. */
  void setLabelOffset(qreal labelOffset);

  /** How far the center of a point's own value label sits from the point,
   *  measured outward along the ray from the canvas center — the same placement
   *  rule as box.distance, kept as a separate knob because a bare line of text
   *  needs different clearance than a bordered box. */
  qreal pointDistance() const { return m_pointDistance; }
  /** Sets the point-label stand-off; a value equal to the current one
   *  does nothing. */
  void setPointDistance(qreal pointDistance);

  /** The one font every label in the scene is drawn with; its point size is
   *  multiplied by scale() before use. */
  QFont font() const { return m_font; }
  /** Sets the label font; a value equal to the current one does
   *  nothing. */
  void setFont(const QFont& font);

  /** Box geometry group. Owned by this object and alive as long as it is,
   *  so QML may hold the pointer. */
  BoxStyleConfig* box() const { return m_box; }

  /** Render-target size group. Owned by this object and alive as long as it is,
   *  so QML may hold the pointer. */
  CanvasSizeConfig* canvas() const { return m_canvas; }

  /** Counter incremented on every change to any value here, including those in
   *  the two groups above. A renderer compares it against the value it last
   *  drew with, which is cheaper and less error-prone than subscribing to each
   *  individual change signal. Only equality is meaningful; the magnitude
   *  carries nothing. */
  int generation() const { return m_generation; }

 signals:
  /** @name Change notifications
   *  One per value, emitted only when that value actually moved. A
   *  renderer watches `generationChanged()` alone rather than all of
   *  them.
   *  @{ */
  /** The accent colour moved. */
  void colorChanged();
  /** The stroke width moved. */
  void strokeWidthChanged();
  /** The global multiplier moved. */
  void scaleChanged();
  /** The curved-label offset moved. */
  void labelOffsetChanged();
  /** The point-label stand-off moved. */
  void pointDistanceChanged();
  /** The label font moved. */
  void fontChanged();
  /** Something here moved, including inside the two groups. */
  void generationChanged();
  /** @} */

 private:
  /** Advances generation() and emits generationChanged(). Every setter that
   *  actually changes a value calls this, as does a restored snapshot; the
   *  grouped objects' changed() signals are connected to it. */
  void bumpGeneration();

  QColor m_color = QColor::fromRgba(spellcircle::ReceiverDefaults::color);
  qreal m_strokeWidth = spellcircle::ReceiverDefaults::strokeWidth;
  qreal m_scale = spellcircle::ReceiverDefaults::scale;
  qreal m_labelOffset = spellcircle::ReceiverDefaults::labelOffset;
  qreal m_pointDistance = spellcircle::ReceiverDefaults::pointDistance;
  QFont m_font;
  BoxStyleConfig* m_box;
  CanvasSizeConfig* m_canvas;
  int m_generation = 0;
};
