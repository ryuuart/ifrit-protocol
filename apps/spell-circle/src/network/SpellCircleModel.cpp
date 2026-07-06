#include "SpellCircleModel.h"
#include "SpellCircle_generated.h"
#include <QPointF>
#include <cmath>
#include <spdlog/spdlog.h>

namespace {

/**
 * Per-scene factor mapping author-space coordinates onto the native texture.
 * `sx`/`sy` scale x/y; `s` is the uniform factor used for scalar quantities
 * (radii) that cannot represent a non-square stretch.
 */
struct SceneScale {
  float sx = 1.0f;
  float sy = 1.0f;
  float s = 1.0f;
};

/**
 * Resolves a Point to absolute canvas coordinates, scaled into native texture
 * space. A point carries its own circle, and `position` is the fraction [0, 1]
 * of the way around the circle's perimeter, measured clockwise from the top
 * (12 o'clock).
 */
QPointF resolvePoint(const SpellCircle::Point *point, const SceneScale &scale) {
  if (!point || !point->circle())
    return {};
  const auto *circle = point->circle();
  const float cx = circle->pos() ? circle->pos()->x() : 0.0f;
  const float cy = circle->pos() ? circle->pos()->y() : 0.0f;
  const float r = static_cast<float>(circle->radius());
  const double theta =
      static_cast<double>(point->position()) * 2.0 * M_PI - M_PI_2;
  return {(cx + r * static_cast<float>(std::cos(theta))) * scale.sx,
          (cy + r * static_cast<float>(std::sin(theta))) * scale.sy};
}

QString utf8(const flatbuffers::String *s) {
  return s ? QString::fromUtf8(s->c_str()) : QString();
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

void SpellCircleModel::setCanvasSize(int width, int height) {
  if (m_canvasWidth == width && m_canvasHeight == height)
    return;
  m_canvasWidth = width;
  m_canvasHeight = height;

  const bool hadGeometry = !m_circles.isEmpty() || !m_points.isEmpty() ||
                           !m_edges.isEmpty() || !m_boxes.isEmpty();
  if (!hadGeometry)
    return;
  m_circles.clear();
  m_points.clear();
  m_edges.clear();
  m_boxes.clear();
  ++m_generation;
  emit geometryChanged();
}

void SpellCircleModel::clear() {
  const bool hadGeometry = !m_circles.isEmpty() || !m_points.isEmpty() ||
                           !m_edges.isEmpty() || !m_boxes.isEmpty();
  if (m_items.isEmpty() && !hadGeometry)
    return;
  beginResetModel();
  m_items.clear();
  m_circles.clear();
  m_points.clear();
  m_edges.clear();
  m_boxes.clear();
  endResetModel();
  ++m_generation;
  emit geometryChanged();
}

void SpellCircleModel::onSpellCircleReceived(const QString &source,
                                             const QByteArray &payload) {
  const auto *scene = SpellCircle::GetScene(payload.constData());
  if (!scene)
    return;

  m_circles.clear();
  m_points.clear();
  m_edges.clear();
  m_boxes.clear();

  // Scenes may be authored in a smaller coordinate space and scaled up to the
  // native texture. width/height of 0 mean coordinates are already native.
  SceneScale scale;
  if (scene->width() > 0.0f)
    scale.sx = static_cast<float>(m_canvasWidth) / scene->width();
  if (scene->height() > 0.0f)
    scale.sy = static_cast<float>(m_canvasHeight) / scene->height();
  scale.s = scale.sx;

  if (scene->circles()) {
    m_circles.reserve(static_cast<int>(scene->circles()->size()));
    for (const auto *circle : *scene->circles()) {
      m_circles.append(CircleGeometry{
          .name = utf8(circle->name()),
          .x = (circle->pos() ? circle->pos()->x() : 0.0f) * scale.sx,
          .y = (circle->pos() ? circle->pos()->y() : 0.0f) * scale.sy,
          .radius = static_cast<uint32_t>(
              std::lround(circle->radius() * scale.s)),
          .textStart = circle->text_start(),
          .active = circle->active(),
      });
    }
  }

  if (scene->edges()) {
    m_edges.reserve(static_cast<int>(scene->edges()->size()));
    for (const auto *edge : *scene->edges()) {
      const QPointF a = resolvePoint(edge->first(), scale);
      const QPointF b = resolvePoint(edge->second(), scale);
      m_edges.append(EdgeGeometry{
          .x1 = static_cast<float>(a.x()),
          .y1 = static_cast<float>(a.y()),
          .x2 = static_cast<float>(b.x()),
          .y2 = static_cast<float>(b.y()),
      });
      if (edge->first())
        m_points.append(PointGeometry{.value = utf8(edge->first()->value()),
                                      .x = static_cast<float>(a.x()),
                                      .y = static_cast<float>(a.y())});
      if (edge->second())
        m_points.append(PointGeometry{.value = utf8(edge->second()->value()),
                                      .x = static_cast<float>(b.x()),
                                      .y = static_cast<float>(b.y())});
    }
  }

  if (scene->boxes()) {
    m_boxes.reserve(static_cast<int>(scene->boxes()->size()));
    for (const auto *box : *scene->boxes()) {
      const QPointF p = resolvePoint(box->point(), scale);
      m_boxes.append(BoxGeometry{
          .value = utf8(box->value()),
          .x = static_cast<float>(p.x()),
          .y = static_cast<float>(p.y()),
          .active = box->active(),
      });
      if (box->point())
        m_points.append(PointGeometry{.value = utf8(box->point()->value()),
                                      .x = static_cast<float>(p.x()),
                                      .y = static_cast<float>(p.y())});
    }
  }

  // One concise feed entry per received scene, rather than one per circle.
  const QString message = QStringLiteral("SpellCircle received — "
                                         "%1 circles, %2 edges, %3 boxes")
                              .arg(m_circles.size())
                              .arg(m_edges.size())
                              .arg(m_boxes.size());
  beginInsertRows({}, 0, 0);
  m_items.prepend(FeedItem{
      .timestamp = QDateTime::currentDateTime(),
      .source = source,
      .message = message,
  });
  endInsertRows();

  ++m_generation;
  emit geometryChanged();
}
