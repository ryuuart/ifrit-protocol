#pragma once
#include <QCanvasPainter>
#include <QColor>
#include <QFont>
#include <QHash>
#include <QPainterPath>
#include <QString>

/**
 * Paints a string of text along an arbitrary QPainterPath, placing each
 * character tangent to the curve at its arc-length position.
 *
 * Placement is controlled by:
 *   - pathOffset [0, 1]: fraction of path length used as the anchor point.
 *   - pathAlignment: whether the anchor is the left edge, centre, or right
 *     edge of the text run.
 *   - perpendicularOffset: signed shift along the path normal — positive moves
 *     toward the left-of-travel normal (outward for clockwise paths).
 *   - baseline: which part of each glyph sits on the (offset) curve.
 *
 * Per-glyph advance widths are cached by QChar and cleared only when the font
 * changes, so textBoundingBox is paid at most once per unique character.
 *
 * The painter must be inside an active beginCanvasPainting block when any
 * method accepting a QCanvasPainter* is called.
 */
class TextPathPainter {
public:
    // Whether the left edge, centre, or right edge of the text run is pinned
    // to pathOffset. Mirrors QCanvasPainter::TextAlign semantics.
    enum class PathAlignment { Left, Center, Right };

    void setFont(const QFont &font);
    const QFont &font() const { return m_font; }

    // Conveniences for the most common font adjustments.
    void setFontSize(int pointSize);
    void setFontWeight(QFont::Weight weight);

    // Fraction [0, 1] of path length used as the text anchor.
    void setPathOffset(float t) { m_pathOffset = t; }
    float pathOffset() const { return m_pathOffset; }

    // Which edge of the text run is pinned to pathOffset.
    void setPathAlignment(PathAlignment alignment) { m_pathAlignment = alignment; }
    PathAlignment pathAlignment() const { return m_pathAlignment; }

    // Displacement perpendicular to the path tangent.
    // Positive shifts toward the left-of-travel normal (outward for CW paths).
    void setPerpendicularOffset(float offset) { m_perpendicularOffset = offset; }
    float perpendicularOffset() const { return m_perpendicularOffset; }

    // Which part of each glyph sits on the (offset) curve.
    void setBaseline(QCanvasPainter::TextBaseline baseline) { m_baseline = baseline; }
    QCanvasPainter::TextBaseline baseline() const { return m_baseline; }

    void setColor(const QColor &color) { m_color = color; }
    const QColor &color() const { return m_color; }

    // Returns total advance width of text, populating the glyph cache.
    float measure(QCanvasPainter *p, const QString &text);

    // Paints text anchored at pathOffset along path.
    // Caller is responsible for setting the desired fill style beforehand.
    void paint(QCanvasPainter *p, const QPainterPath &path, const QString &text);

private:
    // Returns advance width for ch; measures via textBoundingBox on first call.
    // Assumes the correct font is already set on p.
    float glyphAdvance(QCanvasPainter *p, QChar ch);

    QFont m_font;
    QColor m_color{Qt::white};
    QHash<QChar, float> m_glyphCache;
    float m_pathOffset = 0.5f;
    float m_perpendicularOffset = 0.0f;
    PathAlignment m_pathAlignment = PathAlignment::Center;
    QCanvasPainter::TextBaseline m_baseline = QCanvasPainter::TextBaseline::Middle;
};
