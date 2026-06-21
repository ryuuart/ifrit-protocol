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

/** Position and size of a spell circle decoded from a network packet. */
struct CircleGeometry {
  QString name;
  float x = 0.0f;
  float y = 0.0f;
  uint32_t radius = 0;
};

/**
 * List model that ingests incoming SpellCircle scene packets and exposes them
 * as QML-consumable feed items (timestamped log entries) and circle geometry.
 */
class FeedModel : public QAbstractListModel {
  Q_OBJECT

public:
  /** Roles exposed to QML delegates: timestamp (ISO string), source (ip:port),
   *  and message (human-readable circle description). */
  enum Roles {
    TimestampRole = Qt::UserRole + 1,
    SourceRole,
    MessageRole,
  };
  Q_ENUM(Roles)

  explicit FeedModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  /** Returns the circle geometries extracted from the most recently parsed scene. */
  const QList<CircleGeometry> &circles() const { return m_circles; }

signals:
  /** Emitted after m_circles is replaced with data from a newly parsed scene. */
  void circlesChanged();

public slots:
  /**
   * Parses a FlatBuffers-encoded SpellCircle Scene from @p payload, prepends
   * a feed entry for each circle, and replaces the current circle geometry list.
   */
  void onSpellCircleReceived(const QString &source, const QByteArray &payload);

private:
  QList<FeedItem> m_items;
  QList<CircleGeometry> m_circles;
};
