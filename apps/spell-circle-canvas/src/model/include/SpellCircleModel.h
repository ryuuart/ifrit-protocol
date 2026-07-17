#pragma once
#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QtQml/qqml.h>
#include <QtQml/qqmlregistration.h>
#include <entt/entt.hpp>

/** A single timestamped message entry shown in the activity feed. */
struct FeedItem {
  QDateTime timestamp;
  QString source;
  QString message;
};

/** Mirrors SpellCircle.fbs `Circle` — a named circle at an author-space
 *  position, as decoded from the packet. Scaling to native coordinates and
 *  all other positioning math is left to the renderer. */
struct CircleComponent {
  QString name;
  float centerX = 0.0f;
  float centerY = 0.0f;
  uint32_t radius = 0;
  float textStart =
      0.0f;            // fraction [0, 1] where the label begins along the path
  float active = 0.0f; // background fill alpha/intensity [0, 1]; 0 = no fill
};

/** Mirrors SpellCircle.fbs `Point` — a fractional position along a circle's
 *  perimeter. The wire format embeds a full Circle per Point rather than
 *  referencing Scene.circles, so `circle` is a value copy used only to
 *  resolve this point's position at render time. A Point referenced by more
 *  than one Edge/Box in the same packet (FlatBuffers is zero-copy, so this
 *  shows up as the same underlying Point table under multiple fields) gets
 *  one entity, shared via the entt::entity handles below. */
struct PointComponent {
  QString value;
  CircleComponent circle;
  float position = 0.0f; // fraction [0, 1] clockwise from 12 o'clock
};

/** Mirrors SpellCircle.fbs `Edge` — a connector between two Point entities,
 *  which may or may not be distinct from other Edges'/Boxes' Point entities
 *  (see PointComponent). */
struct EdgeComponent {
  entt::entity first = entt::null;
  entt::entity second = entt::null;
};

/** Mirrors SpellCircle.fbs `Box` — a labelled box anchored to a Point
 *  entity, which may be shared with other Edges/Boxes (see PointComponent).
 */
struct BoxComponent {
  QString value;
  entt::entity point = entt::null;
  float active = 0.0f; // background fill alpha/intensity [0, 1]; 0 = no fill
};

/**
 * List model that ingests incoming SpellCircle scene packets and exposes them
 * as QML-consumable feed items (one timestamped log entry per received scene,
 * capped to the 500 most recent so a long-running session doesn't grow the
 * feed without bound) and as an entt::registry of scene entities mirroring
 * the flatbuffer schema (CircleComponent/PointComponent/EdgeComponent/
 * BoxComponent). This model only translates packet data into
 * entities/components; positioning and scaling math is performed by the
 * renderer, which queries the registry.
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

  /** Returns the number of activity-feed entries exposed to QML. */
  int rowCount(const QModelIndex &parent = {}) const override;
  /** Returns feed data for `index` and a value from `Roles`. */
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  /** Returns the QML-facing name for each custom model role. */
  QHash<int, QByteArray> roleNames() const override;

  /** Scene entities decoded from the most recently parsed packet. */
  const entt::registry &registry() const { return m_registry; }

  /** Author-space canvas dimensions from the most recently parsed Scene
   *  (Scene.width/height); 0 means coordinates are already native. */
  float sceneWidth() const { return m_sceneWidth; }
  float sceneHeight() const { return m_sceneHeight; }

  /** Incremented every time the registry is replaced. The renderer compares
   *  this against its own copy in synchronize() to skip redraws on zoom/pan. */
  int generation() const { return m_generation; }

signals:
  /** Emitted after the scene registry is replaced with newly parsed data. */
  void geometryChanged();

public slots:
  /**
   * Parses a FlatBuffers-encoded SpellCircle Scene from @p payload, replaces
   * the current scene registry, and prepends a single feed entry noting the
   * receipt.
   */
  void onSpellCircleReceived(const QString &source, const QByteArray &payload);

  /** Removes all scene entities, and trims the feed down to its most recent
   *  entries (rather than wiping it outright) so the sidebar list doesn't
   *  flash empty and refill. */
  void clear();

private:
  QList<FeedItem> m_items;
  entt::registry m_registry;
  bool m_hasGeometry = false;
  float m_sceneWidth = 0.0f;
  float m_sceneHeight = 0.0f;
  int m_generation = 0;
};
