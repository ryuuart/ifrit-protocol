#pragma once
#include <QtQml/qqml.h>
#include <QtQml/qqmlregistration.h>

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QTimer>
#include <chrono>
#include <cstdint>

#include "SceneSession.h"

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
 * registry of scene entities shared with the native macOS app. This model
 * only routes packet data into the document and presents the feed;
 * positioning and scaling math is performed by the renderer via
 * spellcircle::resolveScene().
 */
class SpellCircleModel : public QAbstractListModel {
  Q_OBJECT
  // Incoming valid scene packet rate (Hz), measured at transport receipt.
  Q_PROPERTY(
      double scenesPerSecond READ scenesPerSecond NOTIFY scenesPerSecondChanged)

 public:
  /** Roles exposed to QML delegates: timestamp (ISO string), source (ip:port),
   *  and message (human-readable scene summary). */
  enum Roles {
    TimestampRole = Qt::UserRole + 1,
    SourceRole,
    MessageRole,
  };
  Q_ENUM(Roles)

  explicit SpellCircleModel(QObject* parent = nullptr);

  /** Returns the number of activity-feed entries exposed to QML. */
  int rowCount(const QModelIndex& parent = {}) const override;
  /** Returns feed data for `index` and a value from `Roles`. */
  QVariant data(const QModelIndex& index,
                int role = Qt::DisplayRole) const override;
  /** Returns the QML-facing name for each custom model role. */
  QHash<int, QByteArray> roleNames() const override;

  /** Scene entities decoded from the most recently parsed packet. */
  const spellcircle::SceneDocument& document() const {
    return m_session.document();
  }

  /** Incremented every time the registry is replaced. The renderer compares
   *  this against its own copy in synchronize() to skip redraws on zoom/pan. */
  uint64_t generation() const { return m_session.generation(); }

  /** Incoming valid scene packet rate in Hz. */
  double scenesPerSecond() const { return m_scenesPerSecond; }

 signals:
  /** Emitted after the scene registry is replaced with newly parsed data. */
  void geometryChanged();
  /** Emitted whenever the packet rate updates, including expiry to zero. */
  void scenesPerSecondChanged();

 public slots:
  /**
   * Verifies a FlatBuffers-encoded scene through the shared session, updates
   * its document only when the payload changes, and prepends one feed entry
   * for every valid receipt. The packet rate uses the transport's timestamp.
   */
  void onSpellCircleReceived(const QString& source, const QByteArray& payload,
                             std::chrono::steady_clock::time_point receivedAt);

  /** Removes all scene entities, and trims the feed down to its most recent
   *  entries (rather than wiping it outright) so the sidebar list doesn't
   *  flash empty and refill. */
  void clear();

 private:
  void updatePacketRate();

  QList<FeedItem> m_items;
  spellcircle::SceneSession m_session;
  QTimer m_rateTimer;
  double m_scenesPerSecond = 0.0;
};
