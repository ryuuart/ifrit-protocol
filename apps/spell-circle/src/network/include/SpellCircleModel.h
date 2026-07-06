#pragma once
#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QtQml/qqml.h>
#include <QtQml/qqmlregistration.h>

/** A single timestamped message entry shown in the activity feed. */
struct FeedItem {
  QDateTime timestamp;
  QString source;
  QString message;
};

/** Position, size, and label placement of a spell circle decoded from a packet. */
struct CircleGeometry {
  QString name;
  float x = 0.0f;
  float y = 0.0f;
  uint32_t radius = 0;
  float textStart = 0.0f;   // fraction [0, 1] where the label begins along the path
  bool active = false;      // filled when true
};

/** A graph node placed at a fractional position along a circle's perimeter,
 *  resolved to absolute canvas coordinates. */
struct PointGeometry {
  QString value;
  float x = 0.0f;
  float y = 0.0f;
};

/** A straight connector between two resolved point positions. */
struct EdgeGeometry {
  float x1 = 0.0f;
  float y1 = 0.0f;
  float x2 = 0.0f;
  float y2 = 0.0f;
};

/** A labelled box anchored at a resolved point position. */
struct BoxGeometry {
  QString value;
  float x = 0.0f;
  float y = 0.0f;
  bool active = false; // filled when true
};

/**
 * List model that ingests incoming SpellCircle scene packets and exposes them
 * as QML-consumable feed items (one timestamped log entry per received scene)
 * and as resolved scene geometry (circles, points, edges, boxes) for rendering.
 */
class SpellCircleModel : public QAbstractListModel {
  Q_OBJECT

public:
  /** Roles exposed to QML delegates: timestamp (ISO string), source (ip:port),
   *  and message (human-readable scene summary). */
  enum Roles {
    TimestampRole = Qt::UserRole + 1,
    SourceRole,
    MessageRole,
  };
  Q_ENUM(Roles)

  explicit SpellCircleModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  /** Geometry extracted from the most recently parsed scene. */
  const QList<CircleGeometry> &circles() const { return m_circles; }
  const QList<PointGeometry> &points() const { return m_points; }
  const QList<EdgeGeometry> &edges() const { return m_edges; }
  const QList<BoxGeometry> &boxes() const { return m_boxes; }

  /** Incremented every time geometry is replaced. The renderer compares this
   *  against its own copy in synchronize() to skip redraws on zoom/pan. */
  int generation() const { return m_generation; }

  /** Width/height (px) of the native coordinate space scenes are scaled
   *  into. Must match GraphicsConfig::canvas / the renderer's framebuffer. */
  int canvasWidth() const { return m_canvasWidth; }
  int canvasHeight() const { return m_canvasHeight; }

  /** Updates the scaling target for future onSpellCircleReceived() calls.
   *  Existing geometry was scaled into the old dimensions and would render at
   *  the wrong scale/position in the new ones, so it's discarded rather than
   *  kept stale; the feed log is left untouched. */
  void setCanvasSize(int width, int height);

signals:
  /** Emitted after the scene geometry is replaced with newly parsed data. */
  void geometryChanged();

public slots:
  /**
   * Parses a FlatBuffers-encoded SpellCircle Scene from @p payload, replaces the
   * current scene geometry, and prepends a single feed entry noting the receipt.
   */
  void onSpellCircleReceived(const QString &source, const QByteArray &payload);

  /** Removes all feed entries and scene geometry from the model. */
  void clear();

private:
  QList<FeedItem> m_items;
  QList<CircleGeometry> m_circles;
  QList<PointGeometry> m_points;
  QList<EdgeGeometry> m_edges;
  QList<BoxGeometry> m_boxes;
  int m_generation = 0;
  int m_canvasWidth = 4000;
  int m_canvasHeight = 4000;
};
