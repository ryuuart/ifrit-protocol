#pragma once
#include "SceneModel.h"
#include <QAbstractListModel>
#include <QDateTime>
#include <QElapsedTimer>
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

/**
 * List model that ingests incoming SpellCircle scene packets and exposes them
 * as QML-consumable feed items (one timestamped log entry per received scene,
 * capped to the 500 most recent so a long-running session doesn't grow the
 * feed without bound) and as a spellcircle::SceneDocument — the Qt-free entt
 * registry of scene entities shared with the native macOS app (see
 * src/scene/SceneModel.h). This model only routes packet data into the
 * document and presents the feed; positioning and scaling math is performed
 * by the renderer via spellcircle::resolveScene().
 */
class SpellCircleModel : public QAbstractListModel {
  Q_OBJECT
  // Smoothed incoming scene packet rate (Hz) for the activity panel's
  // status readouts; 0 until two packets have arrived.
  Q_PROPERTY(double scenesPerSecond READ scenesPerSecond NOTIFY
                 scenesPerSecondChanged)

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
  const spellcircle::SceneDocument &document() const { return m_document; }

  /** Incremented every time the registry is replaced. The renderer compares
   *  this against its own copy in synchronize() to skip redraws on zoom/pan. */
  int generation() const { return m_generation; }

  /** Smoothed incoming scene packet rate in Hz. */
  double scenesPerSecond() const { return m_scenesPerSecond; }

signals:
  /** Emitted after the scene registry is replaced with newly parsed data. */
  void geometryChanged();
  /** Emitted whenever the smoothed packet rate updates. */
  void scenesPerSecondChanged();

public slots:
  /**
   * Parses a FlatBuffers-encoded SpellCircle Scene from @p payload (already
   * verified by NetworkManager), replaces the current scene registry, and
   * prepends a single feed entry noting the receipt.
   */
  void onSpellCircleReceived(const QString &source, const QByteArray &payload);

  /** Removes all scene entities, and trims the feed down to its most recent
   *  entries (rather than wiping it outright) so the sidebar list doesn't
   *  flash empty and refill. */
  void clear();

private:
  QList<FeedItem> m_items;
  spellcircle::SceneDocument m_document;
  bool m_hasGeometry = false;
  int m_generation = 0;
  QElapsedTimer m_arrivalTimer;
  double m_scenesPerSecond = 0.0;
};
