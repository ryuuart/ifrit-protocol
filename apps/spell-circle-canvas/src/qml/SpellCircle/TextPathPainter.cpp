#include "TextPathPainter.h"
#include <QCanvasPainter>
#include <algorithm>
#include <cmath>

static constexpr float kPi = 3.14159265358979323846f;

// Builds a polyline approximation of the curve offset by perpOffset along the
// left-of-travel normal at each sample. Arc-length parameterisation on this
// curve keeps glyph spacing uniform — without it, glyphs spread apart as the
// offset grows because the same angular span covers more arc on a larger-radius
// offset curve (2π(R+d) vs 2πR for a circle).
static QPainterPath buildOffsetPath(const QPainterPath &path,
                                    float perpOffset) {
  const float pathLen = static_cast<float>(path.length());
  // One sample per ~3 px keeps the chord-arc error well below 0.01 px.
  const int n = std::clamp(static_cast<int>(pathLen / 3.0f), 60, 1000);

  QPainterPath result;
  result.setCachingEnabled(true);
  for (int i = 0; i <= n; ++i) {
    const double t = static_cast<double>(i) / n;
    const QPointF pos = path.pointAtPercent(t);
    const float angle =
        -static_cast<float>(path.angleAtPercent(t)) * kPi / 180.0f;
    // Left-of-travel normal: (sin θ, -cos θ)
    const float ox = static_cast<float>(pos.x()) + std::sin(angle) * perpOffset;
    const float oy = static_cast<float>(pos.y()) - std::cos(angle) * perpOffset;
    if (i == 0)
      result.moveTo(ox, oy);
    else
      result.lineTo(ox, oy);
  }
  return result;
}

void TextPathPainter::setFont(const QFont &font) {
  if (m_font != font) {
    m_font = font;
    m_glyphCache.clear();
  }
}

void TextPathPainter::setFontSize(int pointSize) {
  QFont f = m_font;
  f.setPointSize(pointSize);
  setFont(f);
}

void TextPathPainter::setFontWeight(QFont::Weight weight) {
  QFont f = m_font;
  f.setWeight(weight);
  setFont(f);
}

float TextPathPainter::glyphAdvance(QCanvasPainter *p, QChar ch) {
  auto it = m_glyphCache.constFind(ch);
  if (it != m_glyphCache.constEnd())
    return it.value();
  const float advance =
      static_cast<float>(p->textBoundingBox(QString(ch), 0.0f, 0.0f).width());
  m_glyphCache.insert(ch, advance);
  return advance;
}

float TextPathPainter::measure(QCanvasPainter *p, const QString &text) {
  p->setFont(m_font);
  float total = 0.0f;
  for (QChar ch : text)
    total += glyphAdvance(p, ch);
  return total;
}

void TextPathPainter::paint(QCanvasPainter *p, const QPainterPath &path,
                            const QString &text) {
  if (text.isEmpty())
    return;

  const float pathLen = static_cast<float>(path.length());
  if (pathLen <= 0.0f)
    return;

  p->setFillStyle(m_color);
  p->setFont(m_font);
  p->setTextBaseline(m_baseline);
  p->setTextAlign(QCanvasPainter::TextAlign::Left);

  // When there is a perpendicular offset, build the offset curve and measure
  // arc length on it. Glyphs are then placed at the correct positions on the
  // curve they actually sit on, not on the original path.
  const bool hasOffset = !qFuzzyIsNull(m_perpendicularOffset);
  QPainterPath offsetPath;
  if (hasOffset)
    offsetPath = buildOffsetPath(path, m_perpendicularOffset);

  const QPainterPath &arcPath = hasOffset ? offsetPath : path;
  const float arcLen = static_cast<float>(arcPath.length());
  if (arcLen <= 0.0f)
    return;

  float totalWidth = 0.0f;
  for (QChar ch : text)
    totalWidth += glyphAdvance(p, ch);

  const float anchor = m_pathOffset * arcLen;
  float arcPos;
  switch (m_pathAlignment) {
  case PathAlignment::Left:
    arcPos = anchor;
    break;
  case PathAlignment::Right:
    arcPos = anchor - totalWidth;
    break;
  default:
    arcPos = anchor - totalWidth * 0.5f;
    break;
  }

  for (QChar ch : text) {
    const float advance = glyphAdvance(p, ch);
    const float charCenter = arcPos + advance * 0.5f;
    arcPos += advance;

    if (charCenter < 0.0f || charCenter > arcLen)
      continue;

    const double t =
        static_cast<double>(charCenter) / static_cast<double>(arcLen);

    // Position from the offset path — correct arc-length spacing.
    const QPointF pos = arcPath.pointAtPercent(t);

    // Angle from the original smooth path — avoids polyline stairstepping
    // while remaining accurate since t maps to the same angular position
    // on concentric curves.
    const float angle =
        -static_cast<float>(path.angleAtPercent(t)) * kPi / 180.0f;

    p->save();
    p->translate(static_cast<float>(pos.x()), static_cast<float>(pos.y()));
    p->rotate(angle);
    p->fillText(QString(ch), -advance * 0.5f, 0.0f);
    p->restore();
  }
}
