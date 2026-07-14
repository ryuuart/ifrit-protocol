#include "QTextPathPainter.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

/**
 * Builds a polyline approximation offset along the curve's left-hand normal.
 *
 * Measuring the approximation gives uniform glyph spacing on the curve the
 * glyphs occupy. Using the source path's length would spread glyphs apart as
 * the offset radius grows (2*pi*(radius + offset) versus 2*pi*radius).
 */
QPainterPath buildOffsetPath(const QPainterPath &path,
                             float perpendicularOffset) {
  const float pathLength = static_cast<float>(path.length());

  // One sample per roughly three pixels keeps chord-arc error below 0.01 px
  // while the clamp bounds work for both tiny and extremely long paths.
  const int sampleCount =
      std::clamp(static_cast<int>(pathLength / 3.0f), 60, 1000);

  QPainterPath offsetPath;
  offsetPath.setCachingEnabled(true);
  for (int sampleIndex = 0; sampleIndex <= sampleCount; ++sampleIndex) {
    const double pathFraction = static_cast<double>(sampleIndex) / sampleCount;
    const QPointF position = path.pointAtPercent(pathFraction);
    const float tangentAngle =
        -static_cast<float>(path.angleAtPercent(pathFraction)) *
        std::numbers::pi_v<float> / 180.0f;

    // The left-of-travel normal for tangent (cos angle, sin angle) is
    // (sin angle, -cos angle) in Qt's downward-positive coordinate system.
    const float offsetX = static_cast<float>(position.x()) +
                          std::sin(tangentAngle) * perpendicularOffset;
    const float offsetY = static_cast<float>(position.y()) -
                          std::cos(tangentAngle) * perpendicularOffset;
    if (sampleIndex == 0)
      offsetPath.moveTo(offsetX, offsetY);
    else
      offsetPath.lineTo(offsetX, offsetY);
  }
  return offsetPath;
}

} // namespace

void QTextPathPainter::setFont(const QFont &font) {
  if (m_font != font) {
    m_font = font;
    m_glyphAdvances.clear();
  }
}

void QTextPathPainter::setFontSize(int pointSize) {
  QFont updatedFont = m_font;
  updatedFont.setPointSize(pointSize);
  setFont(updatedFont);
}

void QTextPathPainter::setFontWeight(QFont::Weight weight) {
  QFont updatedFont = m_font;
  updatedFont.setWeight(weight);
  setFont(updatedFont);
}

float QTextPathPainter::glyphAdvance(QCanvasPainter *painter, QChar character) {
  const auto cachedAdvance = m_glyphAdvances.constFind(character);
  if (cachedAdvance != m_glyphAdvances.constEnd())
    return cachedAdvance.value();

  const float advance = static_cast<float>(
      painter->textBoundingBox(QString(character), 0.0f, 0.0f).width());
  m_glyphAdvances.insert(character, advance);
  return advance;
}

float QTextPathPainter::measure(QCanvasPainter *painter, const QString &text) {
  painter->setFont(m_font);
  float totalAdvance = 0.0f;
  for (QChar character : text)
    totalAdvance += glyphAdvance(painter, character);
  return totalAdvance;
}

void QTextPathPainter::paint(QCanvasPainter *painter, const QPainterPath &path,
                             const QString &text) {
  if (text.isEmpty() || path.isEmpty())
    return;

  painter->setFillStyle(m_color);
  painter->setFont(m_font);
  painter->setTextBaseline(m_baseline);
  painter->setTextAlign(QCanvasPainter::TextAlign::Left);

  // Arc length must be measured on the offset curve to preserve spacing.
  const bool hasPerpendicularOffset = !qFuzzyIsNull(m_perpendicularOffset);
  QPainterPath offsetPath;
  if (hasPerpendicularOffset)
    offsetPath = buildOffsetPath(path, m_perpendicularOffset);

  const QPainterPath &placementPath =
      hasPerpendicularOffset ? offsetPath : path;
  const float placementPathLength = static_cast<float>(placementPath.length());
  if (placementPathLength <= 0.0f)
    return;

  const float textAdvance = measure(painter, text);
  const float anchorPosition = m_pathOffset * placementPathLength;
  float currentPosition = anchorPosition;
  switch (m_pathAlignment) {
  case PathAlignment::Center:
    currentPosition -= textAdvance * 0.5f;
    break;
  case PathAlignment::Right:
    currentPosition -= textAdvance;
    break;
  case PathAlignment::Left:
    break;
  }

  for (QChar character : text) {
    const float advance = glyphAdvance(painter, character);
    const float glyphCenter = currentPosition + advance * 0.5f;
    currentPosition += advance;

    if (glyphCenter < 0.0f || glyphCenter > placementPathLength)
      continue;

    const double pathFraction = static_cast<double>(glyphCenter) /
                                static_cast<double>(placementPathLength);
    const QPointF position = placementPath.pointAtPercent(pathFraction);

    // Query the smooth source path for rotation. The offset approximation is
    // deliberately used only for distance, avoiding polyline stair-stepping.
    const float tangentAngle =
        -static_cast<float>(path.angleAtPercent(pathFraction)) *
        std::numbers::pi_v<float> / 180.0f;

    painter->save();
    painter->translate(static_cast<float>(position.x()),
                       static_cast<float>(position.y()));
    painter->rotate(tangentAngle);
    painter->fillText(QString(character), -advance * 0.5f, 0.0f);
    painter->restore();
  }
}
