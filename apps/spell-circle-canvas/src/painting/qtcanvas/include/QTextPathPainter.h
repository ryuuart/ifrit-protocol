#pragma once

#include <QCanvasPainter>
#include <QColor>
#include <QFont>
#include <QHash>
#include <QPainterPath>
#include <QString>

/**
 * Paints text along an arbitrary QPainterPath using QCanvasPainter.
 *
 * Each glyph is positioned by arc length and rotated tangent to the curve.
 * The configured path offset selects an anchor, path alignment selects the
 * text edge attached to that anchor, and perpendicular offset moves glyphs
 * along the left-of-travel path normal.
 *
 * Per-glyph advances are cached until the font changes. The painter must be
 * inside an active beginCanvasPainting block when measure() or paint() runs.
 */
class QTextPathPainter {
public:
  /** Edge of the text run pinned to the configured path offset. */
  enum class PathAlignment { Left, Center, Right };

  /** Selects the font and invalidates cached glyph advances when it changes. */
  void setFont(const QFont &font);
  /** Returns the font used for measurement and painting. */
  const QFont &font() const { return m_font; }

  /** Changes only the active font's point size. */
  void setFontSize(int pointSize);
  /** Changes only the active font's weight. */
  void setFontWeight(QFont::Weight weight);

  /** Selects the normalized [0, 1] path position used as the text anchor. */
  void setPathOffset(float pathOffset) { m_pathOffset = pathOffset; }
  /** Returns the normalized anchor position along the path. */
  float pathOffset() const { return m_pathOffset; }

  /** Selects which edge of the text run is pinned to pathOffset(). */
  void setPathAlignment(PathAlignment alignment) {
    m_pathAlignment = alignment;
  }
  /** Returns the text edge pinned to the anchor. */
  PathAlignment pathAlignment() const { return m_pathAlignment; }

  /** Selects displacement along the left-of-travel path normal. */
  void setPerpendicularOffset(float offset) { m_perpendicularOffset = offset; }
  /** Returns displacement along the path normal. */
  float perpendicularOffset() const { return m_perpendicularOffset; }

  /** Selects which part of each glyph sits on the offset curve. */
  void setBaseline(QCanvasPainter::TextBaseline baseline) {
    m_baseline = baseline;
  }
  /** Returns the glyph baseline mode used for painting. */
  QCanvasPainter::TextBaseline baseline() const { return m_baseline; }

  /** Selects the text color. */
  void setColor(const QColor &color) { m_color = color; }
  /** Returns the selected text color. */
  const QColor &color() const { return m_color; }

  /** Returns total text advance, populating the glyph-advance cache. */
  float measure(QCanvasPainter *painter, const QString &text);

  /** Paints text along path, anchored at the configured path offset. */
  void paint(QCanvasPainter *painter, const QPainterPath &path,
             const QString &text);

private:
  /** Returns a cached glyph advance, measuring it on the first request. */
  float glyphAdvance(QCanvasPainter *painter, QChar character);

  QFont m_font;
  QColor m_color{Qt::white};
  QHash<QChar, float> m_glyphAdvances;
  float m_pathOffset = 0.5f;
  float m_perpendicularOffset = 0.0f;
  PathAlignment m_pathAlignment = PathAlignment::Center;
  QCanvasPainter::TextBaseline m_baseline =
      QCanvasPainter::TextBaseline::Middle;
};
