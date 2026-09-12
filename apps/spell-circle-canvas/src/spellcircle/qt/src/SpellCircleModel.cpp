#include "SpellCircleModel.h"

#include <spdlog/spdlog.h>

namespace {

// Caps the activity feed so a long-running session doesn't grow m_items
// (and the QML ListView backing it) without bound.
constexpr int kMaxFeedItems = 500;

// clear() trims the feed down to this many of the most recent entries
// instead of wiping it, so the sidebar list doesn't flash empty and refill.
constexpr int kClearKeepItems = kMaxFeedItems - 450;

}  // namespace

SpellCircleModel::SpellCircleModel(QObject* parent)
    : QAbstractListModel(parent) {
  m_rateTimer.setInterval(250);
  connect(&m_rateTimer, &QTimer::timeout, this,
          &SpellCircleModel::updatePacketRate);
  m_rateTimer.start();
}

int SpellCircleModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) return 0;
  return m_items.size();
}

QVariant SpellCircleModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() >= m_items.size()) return {};

  const auto& item = m_items.at(index.row());
  switch (static_cast<Roles>(role)) {
    case TimestampRole:
      return item.timestamp.toString(Qt::ISODateWithMs);
    case SourceRole:
      return item.source;
    case MessageRole:
      return item.message;
  }
  return {};
}

QHash<int, QByteArray> SpellCircleModel::roleNames() const {
  return {
      {TimestampRole, "timestamp"},
      {SourceRole, "source"},
      {MessageRole, "message"},
  };
}

void SpellCircleModel::clear() {
  const bool willTrimFeed = m_items.size() > kClearKeepItems;
  const uint64_t generation = m_session.generation();

  // The document isn't row data — rowCount()/data() only ever look at
  // m_items — so clearing it needs no model reset/row signals of its own.
  m_session.clear();
  updatePacketRate();

  // Trim the feed down to its most recent entries rather than wiping it
  // outright: a beginResetModel() here would flash the sidebar list empty
  // and refill it, whereas removing just the tail (oldest first, since new
  // entries are prepended) leaves the remaining rows' delegates untouched.
  if (willTrimFeed) {
    beginRemoveRows({}, kClearKeepItems, m_items.size() - 1);
    m_items.resize(kClearKeepItems);
    endRemoveRows();
  }

  if (m_session.generation() != generation) emit geometryChanged();
}

void SpellCircleModel::onSpellCircleReceived(
    const QString& source, const QByteArray& payload,
    std::chrono::steady_clock::time_point receivedAt) {
  const spellcircle::SceneUpdate update = m_session.ingest(
      payload.constData(), static_cast<size_t>(payload.size()), receivedAt);
  if (update == spellcircle::SceneUpdate::Invalid) {
    spdlog::warn("Dropped invalid SpellCircle buffer from {}",
                 source.toStdString());
    return;
  }
  updatePacketRate();
  const spellcircle::SceneStats& stats = m_session.stats();

  // One concise feed entry per received scene, rather than one per circle.
  const QString message = QStringLiteral(
                              "SpellCircle received — "
                              "%1 circles, %2 edges, %3 boxes")
                              .arg(stats.circles)
                              .arg(stats.edges)
                              .arg(stats.boxes);
  beginInsertRows({}, 0, 0);
  m_items.prepend(FeedItem{
      .timestamp = QDateTime::currentDateTime(),
      .source = source,
      .message = message,
  });
  endInsertRows();

  if (m_items.size() > kMaxFeedItems) {
    beginRemoveRows({}, kMaxFeedItems, m_items.size() - 1);
    m_items.resize(kMaxFeedItems);
    endRemoveRows();
  }

  if (update == spellcircle::SceneUpdate::Changed) emit geometryChanged();
}

void SpellCircleModel::updatePacketRate() {
  const double rate = m_session.packetRate();
  if (m_scenesPerSecond == rate) return;
  m_scenesPerSecond = rate;
  emit scenesPerSecondChanged();
}
