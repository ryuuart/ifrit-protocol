#include "GraphicsConfig.h"

#include <QFontDatabase>
#include <QJsonObject>
#include <algorithm>

void BoxStyleConfig::setWidth(qreal width) {
  if (qFuzzyCompare(m_width, width)) return;
  m_width = width;
  emit changed();
}

void BoxStyleConfig::setHeight(qreal height) {
  if (qFuzzyCompare(m_height, height)) return;
  m_height = height;
  emit changed();
}

void BoxStyleConfig::setPadding(qreal padding) {
  if (qFuzzyCompare(m_padding, padding)) return;
  m_padding = padding;
  emit changed();
}

void BoxStyleConfig::setDistance(qreal distance) {
  if (qFuzzyCompare(m_distance, distance)) return;
  m_distance = distance;
  emit changed();
}

void CanvasSizeConfig::setWidth(int width) {
  width = std::clamp(width, minimum(), maximum());
  if (m_width == width) return;
  m_width = width;
  emit changed();
}

void CanvasSizeConfig::setHeight(int height) {
  height = std::clamp(height, minimum(), maximum());
  if (m_height == height) return;
  m_height = height;
  emit changed();
}

GraphicsConfig::GraphicsConfig(QObject* parent)
    : QObject(parent),
      m_box(new BoxStyleConfig(this)),
      m_canvas(new CanvasSizeConfig(this)) {
  if (*spellcircle::ReceiverDefaults::fontFamily)
    m_font.setFamily(
        QString::fromUtf8(spellcircle::ReceiverDefaults::fontFamily));
  m_font.setWeight(
      static_cast<QFont::Weight>(spellcircle::ReceiverDefaults::fontWeight));
  m_font.setItalic(spellcircle::ReceiverDefaults::fontItalic);
  m_font.setPointSize(spellcircle::ReceiverDefaults::fontSize);

  connect(m_box, &BoxStyleConfig::changed, this,
          &GraphicsConfig::bumpGeneration);
  connect(m_canvas, &CanvasSizeConfig::changed, this,
          &GraphicsConfig::bumpGeneration);
}

void GraphicsConfig::setColor(const QColor& color) {
  if (m_color == color) return;
  m_color = color;
  emit colorChanged();
  bumpGeneration();
}

void GraphicsConfig::setStrokeWidth(qreal strokeWidth) {
  if (qFuzzyCompare(m_strokeWidth, strokeWidth)) return;
  m_strokeWidth = strokeWidth;
  emit strokeWidthChanged();
  bumpGeneration();
}

void GraphicsConfig::setScale(qreal scale) {
  if (qFuzzyCompare(m_scale, scale)) return;
  m_scale = scale;
  emit scaleChanged();
  bumpGeneration();
}

void GraphicsConfig::setLabelOffset(qreal labelOffset) {
  if (qFuzzyCompare(m_labelOffset, labelOffset)) return;
  m_labelOffset = labelOffset;
  emit labelOffsetChanged();
  bumpGeneration();
}

void GraphicsConfig::setPointDistance(qreal pointDistance) {
  if (qFuzzyCompare(m_pointDistance, pointDistance)) return;
  m_pointDistance = pointDistance;
  emit pointDistanceChanged();
  bumpGeneration();
}

void GraphicsConfig::setFont(const QFont& font) {
  if (m_font == font) return;
  m_font = font;
  emit fontChanged();
  bumpGeneration();
}

void GraphicsConfig::bumpGeneration() {
  ++m_generation;
  emit generationChanged();
}

void GraphicsConfig::restore(const QJsonObject& rootObject) {
  if (rootObject.contains("color"))
    m_color = QColor(rootObject["color"].toString());
  if (rootObject.contains("strokeWidth"))
    m_strokeWidth = rootObject["strokeWidth"].toDouble(m_strokeWidth);
  if (rootObject.contains("scale"))
    m_scale = rootObject["scale"].toDouble(m_scale);
  if (rootObject.contains("labelOffset"))
    m_labelOffset = rootObject["labelOffset"].toDouble(m_labelOffset);
  if (rootObject.contains("pointDistance"))
    m_pointDistance = rootObject["pointDistance"].toDouble(m_pointDistance);

  if (rootObject.contains("box") && rootObject["box"].isObject()) {
    const QJsonObject boxObject = rootObject["box"].toObject();
    m_box->setWidth(boxObject.value("width").toDouble(m_box->width()));
    m_box->setHeight(boxObject.value("height").toDouble(m_box->height()));
    m_box->setPadding(boxObject.value("padding").toDouble(m_box->padding()));
    m_box->setDistance(boxObject.value("distance").toDouble(m_box->distance()));
  }

  if (rootObject.contains("canvas") && rootObject["canvas"].isObject()) {
    const QJsonObject canvasObject = rootObject["canvas"].toObject();
    m_canvas->setWidth(canvasObject.value("width").toInt(m_canvas->width()));
    m_canvas->setHeight(canvasObject.value("height").toInt(m_canvas->height()));
  }

  if (rootObject.contains("font") && rootObject["font"].isObject()) {
    const QJsonObject fontObject = rootObject["font"].toObject();
    const QString family = fontObject.value("family").toString(m_font.family());
    const qreal pointSize =
        fontObject.value("pointSize").toDouble(m_font.pointSizeF());
    const QString style = fontObject.value("style").toString();
    m_font = style.isEmpty()
                 ? QFont(family, qRound(pointSize))
                 : QFontDatabase::font(family, style, qRound(pointSize));
  }

  emit colorChanged();
  emit strokeWidthChanged();
  emit scaleChanged();
  emit labelOffsetChanged();
  emit pointDistanceChanged();
  emit fontChanged();
  bumpGeneration();
}

QJsonObject GraphicsConfig::snapshot() const {
  QJsonObject boxObject;
  boxObject["width"] = m_box->width();
  boxObject["height"] = m_box->height();
  boxObject["padding"] = m_box->padding();
  boxObject["distance"] = m_box->distance();

  QJsonObject canvasObject;
  canvasObject["width"] = m_canvas->width();
  canvasObject["height"] = m_canvas->height();

  QJsonObject fontObject;
  fontObject["family"] = m_font.family();
  fontObject["pointSize"] = m_font.pointSize();
  fontObject["style"] = QFontDatabase::styleString(m_font);

  QJsonObject rootObject;
  rootObject["color"] = m_color.name(QColor::HexRgb);
  rootObject["strokeWidth"] = m_strokeWidth;
  rootObject["scale"] = m_scale;
  rootObject["labelOffset"] = m_labelOffset;
  rootObject["pointDistance"] = m_pointDistance;
  rootObject["box"] = boxObject;
  rootObject["canvas"] = canvasObject;
  rootObject["font"] = fontObject;

  return rootObject;
}
