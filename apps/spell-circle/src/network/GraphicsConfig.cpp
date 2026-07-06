#include "GraphicsConfig.h"
#include <QCoreApplication>
#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <spdlog/spdlog.h>

void BoxStyleConfig::setWidth(qreal width) {
  if (qFuzzyCompare(m_width, width))
    return;
  m_width = width;
  emit changed();
}

void BoxStyleConfig::setHeight(qreal height) {
  if (qFuzzyCompare(m_height, height))
    return;
  m_height = height;
  emit changed();
}

void BoxStyleConfig::setPadding(qreal padding) {
  if (qFuzzyCompare(m_padding, padding))
    return;
  m_padding = padding;
  emit changed();
}

void BoxStyleConfig::setDistance(qreal distance) {
  if (qFuzzyCompare(m_distance, distance))
    return;
  m_distance = distance;
  emit changed();
}

void CanvasSizeConfig::setWidth(int width) {
  if (m_width == width)
    return;
  m_width = width;
  emit changed();
}

void CanvasSizeConfig::setHeight(int height) {
  if (m_height == height)
    return;
  m_height = height;
  emit changed();
}

GraphicsConfig::GraphicsConfig(QObject *parent)
    : QObject(parent), m_box(new BoxStyleConfig(this)),
      m_canvas(new CanvasSizeConfig(this)) {
  m_font.setBold(true);
  m_font.setPointSize(36);

  connect(m_box, &BoxStyleConfig::changed, this,
          &GraphicsConfig::bumpGeneration);
  connect(m_canvas, &CanvasSizeConfig::changed, this,
          &GraphicsConfig::bumpGeneration);

  load();
}

void GraphicsConfig::setColor(const QColor &color) {
  if (m_color == color)
    return;
  m_color = color;
  emit colorChanged();
  bumpGeneration();
}

void GraphicsConfig::setStrokeWidth(qreal strokeWidth) {
  if (qFuzzyCompare(m_strokeWidth, strokeWidth))
    return;
  m_strokeWidth = strokeWidth;
  emit strokeWidthChanged();
  bumpGeneration();
}

void GraphicsConfig::setScale(qreal scale) {
  if (qFuzzyCompare(m_scale, scale))
    return;
  m_scale = scale;
  emit scaleChanged();
  bumpGeneration();
}

void GraphicsConfig::setLabelOffset(qreal labelOffset) {
  if (qFuzzyCompare(m_labelOffset, labelOffset))
    return;
  m_labelOffset = labelOffset;
  emit labelOffsetChanged();
  bumpGeneration();
}

void GraphicsConfig::setFont(const QFont &font) {
  if (m_font == font)
    return;
  m_font = font;
  emit fontChanged();
  bumpGeneration();
}

void GraphicsConfig::bumpGeneration() {
  ++m_generation;
  emit generationChanged();
}

QString GraphicsConfig::configFilePath() {
  return QCoreApplication::applicationDirPath() + "/graphics_config.json";
}

bool GraphicsConfig::load() {
  QFile file(configFilePath());
  if (!file.open(QIODevice::ReadOnly)) {
    spdlog::info("GraphicsConfig: no config file at {}, using defaults",
                 configFilePath().toStdString());
    return false;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) {
    spdlog::warn("GraphicsConfig: malformed config file at {}",
                 configFilePath().toStdString());
    return false;
  }

  const QJsonObject obj = doc.object();
  if (obj.contains("color"))
    m_color = QColor(obj["color"].toString());
  if (obj.contains("strokeWidth"))
    m_strokeWidth = obj["strokeWidth"].toDouble(m_strokeWidth);
  if (obj.contains("scale"))
    m_scale = obj["scale"].toDouble(m_scale);
  if (obj.contains("labelOffset"))
    m_labelOffset = obj["labelOffset"].toDouble(m_labelOffset);

  if (obj.contains("box") && obj["box"].isObject()) {
    const QJsonObject boxObj = obj["box"].toObject();
    m_box->setWidth(boxObj.value("width").toDouble(m_box->width()));
    m_box->setHeight(boxObj.value("height").toDouble(m_box->height()));
    m_box->setPadding(boxObj.value("padding").toDouble(m_box->padding()));
    m_box->setDistance(boxObj.value("distance").toDouble(m_box->distance()));
  }

  if (obj.contains("canvas") && obj["canvas"].isObject()) {
    const QJsonObject canvasObj = obj["canvas"].toObject();
    m_canvas->setWidth(canvasObj.value("width").toInt(m_canvas->width()));
    m_canvas->setHeight(canvasObj.value("height").toInt(m_canvas->height()));
  }

  if (obj.contains("font") && obj["font"].isObject()) {
    const QJsonObject fontObj = obj["font"].toObject();
    const QString family = fontObj.value("family").toString(m_font.family());
    const qreal pointSize =
        fontObj.value("pointSize").toDouble(m_font.pointSizeF());
    const QString style = fontObj.value("style").toString();
    m_font = style.isEmpty()
                 ? QFont(family, qRound(pointSize))
                 : QFontDatabase::font(family, style, qRound(pointSize));
  }

  emit colorChanged();
  emit strokeWidthChanged();
  emit scaleChanged();
  emit labelOffsetChanged();
  emit fontChanged();
  bumpGeneration();
  return true;
}

bool GraphicsConfig::save() const {
  QJsonObject boxObj;
  boxObj["width"] = m_box->width();
  boxObj["height"] = m_box->height();
  boxObj["padding"] = m_box->padding();
  boxObj["distance"] = m_box->distance();

  QJsonObject canvasObj;
  canvasObj["width"] = m_canvas->width();
  canvasObj["height"] = m_canvas->height();

  QJsonObject fontObj;
  fontObj["family"] = m_font.family();
  fontObj["pointSize"] = m_font.pointSize();
  fontObj["style"] = QFontDatabase::styleString(m_font);

  QJsonObject obj;
  obj["color"] = m_color.name(QColor::HexRgb);
  obj["strokeWidth"] = m_strokeWidth;
  obj["scale"] = m_scale;
  obj["labelOffset"] = m_labelOffset;
  obj["box"] = boxObj;
  obj["canvas"] = canvasObj;
  obj["font"] = fontObj;

  QFile file(configFilePath());
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    spdlog::error("GraphicsConfig: failed to write config file at {}",
                  configFilePath().toStdString());
    return false;
  }
  file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
  return true;
}
