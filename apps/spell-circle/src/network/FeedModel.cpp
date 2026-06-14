#include "FeedModel.h"
#include "SpellCircle_generated.h"
#include <spdlog/spdlog.h>

FeedModel::FeedModel(QObject *parent) : QAbstractListModel(parent) {}

int FeedModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_items.size();
}

QVariant FeedModel::data(const QModelIndex &index, int role) const {
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

QHash<int, QByteArray> FeedModel::roleNames() const {
  return {
      {TimestampRole, "timestamp"},
      {SourceRole, "source"},
      {MessageRole, "message"},
  };
}

void FeedModel::onSpellCircleReceived(const QString &source,
                                      const QByteArray &payload) {
  const auto *sc = SpellCircle::GetScene(payload.constData());
  if (!sc || !sc->circles() || sc->circles()->size() == 0)
    return;

  const int count = static_cast<int>(sc->circles()->size());
  const QDateTime now = QDateTime::currentDateTime();

  m_circles.clear();
  m_circles.reserve(count);

  beginInsertRows({}, 0, count - 1);
  for (int i = count - 1; i >= 0; --i) {
    const auto *circle =
        sc->circles()->Get(static_cast<flatbuffers::uoffset_t>(i));
    const QString name = circle->name()
                             ? QString::fromUtf8(circle->name()->c_str())
                             : QStringLiteral("?");
    QString msg;
    if (circle->pos()) {
      msg = QStringLiteral("%1 @ (%2, %3) r=%4")
                .arg(name)
                .arg(circle->pos()->x(), 0, 'f', 1)
                .arg(circle->pos()->y(), 0, 'f', 1)
                .arg(circle->radius());
    } else {
      msg = QStringLiteral("%1 r=%2").arg(name).arg(circle->radius());
    }
    m_items.prepend(FeedItem{
        .timestamp = now,
        .source = source,
        .message = msg,
    });

    m_circles.append(CircleGeometry{
        .name   = name,
        .x      = circle->pos() ? circle->pos()->x() : 0.0f,
        .y      = circle->pos() ? circle->pos()->y() : 0.0f,
        .radius = circle->radius(),
    });
  }
  endInsertRows();
  emit circlesChanged();
}
