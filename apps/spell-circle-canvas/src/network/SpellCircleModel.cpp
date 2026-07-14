#include "SpellCircleModel.h"
#include "SpellCircle_generated.h"
#include <QHash>
#include <spdlog/spdlog.h>

namespace {

// Caps the activity feed so a long-running session doesn't grow m_items
// (and the QML ListView backing it) without bound.
constexpr int kMaxFeedItems = 500;

// clear() trims the feed down to this many of the most recent entries
// instead of wiping it, so the sidebar list doesn't flash empty and refill.
constexpr int kClearKeepItems = kMaxFeedItems - 450;

QString utf8(const flatbuffers::String *string) {
  return string ? QString::fromUtf8(string->c_str()) : QString();
}

CircleComponent toCircleComponent(const SpellCircle::Circle *circle) {
  if (!circle)
    return {};
  return CircleComponent{
      .name = utf8(circle->name()),
      .centerX = circle->pos() ? circle->pos()->x() : 0.0f,
      .centerY = circle->pos() ? circle->pos()->y() : 0.0f,
      .radius = circle->radius(),
      .textStart = circle->text_start(),
      .active = circle->active(),
  };
}

entt::entity makeCircleEntity(entt::registry &registry,
                              const SpellCircle::Circle *circle) {
  const entt::entity entity = registry.create();
  registry.emplace<CircleComponent>(entity, toCircleComponent(circle));
  return entity;
}

/**
 * Returns the entity for `point`, creating it on first sight. FlatBuffers is
 * zero-copy, so if the packet embeds the same Point table under more than
 * one Edge/Box field (a vertex shared by several edges, say), every such
 * occurrence decodes to the same `point` pointer — `pointCache` lets those
 * occurrences resolve to one shared PointComponent entity instead of a
 * fresh duplicate per reference.
 */
entt::entity getOrCreatePointEntity(
    entt::registry &registry,
    QHash<const SpellCircle::Point *, entt::entity> &pointCache,
    const SpellCircle::Point *point) {
  if (!point)
    return entt::null;

  if (const auto cachedPoint = pointCache.constFind(point);
      cachedPoint != pointCache.constEnd())
    return cachedPoint.value();

  const entt::entity entity = registry.create();
  registry.emplace<PointComponent>(
      entity, PointComponent{
                  .value = utf8(point->value()),
                  .circle = toCircleComponent(point->circle()),
                  .position = point->position(),
              });
  pointCache.insert(point, entity);
  return entity;
}

} // namespace

SpellCircleModel::SpellCircleModel(QObject *parent)
    : QAbstractListModel(parent) {}

int SpellCircleModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_items.size();
}

QVariant SpellCircleModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_items.size())
    return {};

  const auto &item = m_items.at(index.row());
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
  if (!willTrimFeed && !m_hasGeometry)
    return;

  // The registry isn't row data — rowCount()/data() only ever look at
  // m_items — so clearing it needs no model reset/row signals of its own.
  m_registry.clear();
  m_hasGeometry = false;
  m_sceneWidth = 0.0f;
  m_sceneHeight = 0.0f;

  // Trim the feed down to its most recent entries rather than wiping it
  // outright: a beginResetModel() here would flash the sidebar list empty
  // and refill it, whereas removing just the tail (oldest first, since new
  // entries are prepended) leaves the remaining rows' delegates untouched.
  if (willTrimFeed) {
    beginRemoveRows({}, kClearKeepItems, m_items.size() - 1);
    m_items.resize(kClearKeepItems);
    endRemoveRows();
  }

  ++m_generation;
  emit geometryChanged();
}

void SpellCircleModel::onSpellCircleReceived(const QString &source,
                                             const QByteArray &payload) {
  const auto *scene = SpellCircle::GetScene(payload.constData());
  if (!scene)
    return;

  m_registry.clear();
  m_sceneWidth = scene->width();
  m_sceneHeight = scene->height();

  int circleCount = 0;
  int edgeCount = 0;
  int boxCount = 0;

  // Shared across edges/boxes so a Point referenced by more than one of them
  // resolves to a single entity rather than being duplicated per reference.
  QHash<const SpellCircle::Point *, entt::entity> pointCache;

  if (scene->circles()) {
    for (const auto *circle : *scene->circles()) {
      makeCircleEntity(m_registry, circle);
      ++circleCount;
    }
  }

  if (scene->edges()) {
    for (const auto *edge : *scene->edges()) {
      const entt::entity entity = m_registry.create();
      m_registry.emplace<EdgeComponent>(
          entity, EdgeComponent{
                      .first = getOrCreatePointEntity(m_registry, pointCache,
                                                      edge->first()),
                      .second = getOrCreatePointEntity(m_registry, pointCache,
                                                       edge->second()),
                  });
      ++edgeCount;
    }
  }

  if (scene->boxes()) {
    for (const auto *box : *scene->boxes()) {
      const entt::entity entity = m_registry.create();
      m_registry.emplace<BoxComponent>(
          entity, BoxComponent{
                      .value = utf8(box->value()),
                      .point = getOrCreatePointEntity(m_registry, pointCache,
                                                      box->point()),
                      .active = box->active(),
                  });
      ++boxCount;
    }
  }

  m_hasGeometry = circleCount > 0 || edgeCount > 0 || boxCount > 0;

  // One concise feed entry per received scene, rather than one per circle.
  const QString message = QStringLiteral("SpellCircle received — "
                                         "%1 circles, %2 edges, %3 boxes")
                              .arg(circleCount)
                              .arg(edgeCount)
                              .arg(boxCount);
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

  ++m_generation;
  emit geometryChanged();
}
