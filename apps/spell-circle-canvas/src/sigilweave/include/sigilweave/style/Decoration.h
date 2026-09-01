#pragma once

/** @file
 * @ingroup shaping
 *
 * One line decoration — underline, strikethrough, overline or highlight
 * — as band geometry plus a band fill, resolved with a run's paint at
 * draw time. Paint-side on purpose: adding, removing or recolouring one
 * never re-shapes and never relayouts.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPaint.h>

#include <cstdint>
#include <optional>

namespace sigil::weave {

/** One line decoration (underline / strikethrough / overline) drawn with a
 * run's resolved paint at draw time.
 *
 * Decorations live on the paint side on purpose: adding, removing, or
 * recoloring one never re-shapes and never relayouts, exactly like paint
 * layers. Thickness and position default to the font's own metrics
 * (SkFontMetrics underline/strikeout values, with sensible fallbacks when a
 * face reports none), so the zero-argument spelling
 * `PaintStyle{...}.addDecoration({})` is a correct underline.
 *
 * By default a decoration spans the decorated range, not individual words:
 * contiguous same-style runs on a line merge into one continuous band that
 * also covers the glue between words (CSS behavior — an underlined sentence
 * is one line, a highlight reads like one marker stroke). Skip-ink breaks
 * come only from glyph ink, never from word gaps. `span = Span::kPerWord`
 * opts back into one band per word (spell-check squiggles, word chips).
 *
 * kHighlight is the background member of the family: a full-text-height
 * band (ascent to descent by default) drawn *beneath* every glyph pass, so
 * with the default range spanning it renders as a continuous highlighter
 * stroke behind the words and their gaps.
 *
 * A decoration separates two concerns: *band geometry* (kind, span,
 * thickness, offset, skipInk) and *band fill*. The fill has two spellings:
 * `color` is the lightweight one, and `paint` is the full SkPaint
 * vocabulary — shaders (PaintShaders.h presets animate per frame through
 * Paragraph::setPaint() without relayout, exactly like glyph paint), blend
 * modes, mask filters. Glyphs and decorations resolve their fills
 * independently, so a shaded highlight under plain-colored text — or the
 * reverse — needs no coordination between the two. Multi-pass band effects
 * compose the same way glyph passes do: stack several decorations with the
 * same geometry and different fills.
 *
 * Scope: decorations render on straight runs, set either way. Down a column
 * the band turns with the type — an underline runs beside the column on its
 * right, an overline on its left, a strikethrough down the column axis, and
 * a highlight covers the whole em box — and it draws through the glyphs'
 * ink, because skip-ink intercepts are cut out of a horizontal band window
 * that a column's band is not. Transformed runs (on a path, on a rotated
 * interval) carry no band at all: it would have to follow the curve they
 * ride.
 */
struct Decoration {
  /// Selects which font metric anchors the band by default. kHighlight is
  /// drawn beneath the glyph passes; the others above them.
  enum class Kind : uint8_t {
    kUnderline,
    kStrikethrough,
    kOverline,
    kHighlight,
  };
  /// How far one band extends along the line.
  enum class Span : uint8_t {
    kDecoratedRange,  ///< merge contiguous same-style runs, covering gaps
    kPerWord,         ///< one band per word run; breaks at every gap
  };

  Kind kind = Kind::kUnderline;       ///< only underlines honor `skipInk`
  Span span = Span::kDecoratedRange;  ///< continuous band vs one per word
  /// SK_ColorTRANSPARENT → the resolved foreground paint's color — except
  /// for kHighlight, where an opaque foreground would hide the text, so it
  /// resolves to the foreground color at quarter alpha instead.
  SkColor color = SK_ColorTRANSPARENT;
  /// 0 → thickness from font metrics (kHighlight: ascent + descent),
  /// floored at 1px.
  float thickness = 0;
  /// 0 → position from font metrics (kHighlight: the ascent line);
  /// otherwise the band's top edge in px relative to the baseline
  /// (positive below, Skia's y-grows-down convention).
  float offset = 0;
  /// Underlines only: interrupt the line where glyph ink (descenders)
  /// crosses the band, via SkTextBlob::getIntercepts.
  bool skipInk = true;
  /// Full-vocabulary band fill. When set it is applied verbatim — nothing
  /// is overridden, exactly like a PaintLayer wrapping a caller-configured
  /// SkPaint — and it takes precedence over `color`, whose resolution rules
  /// (including the translucent kHighlight default) no longer apply: alpha,
  /// anti-aliasing, and everything else are the caller's. Band geometry is
  /// untouched — the paint fills the same rect segments a plain color
  /// would, ink skipping included.
  std::optional<SkPaint> paint;

  /** Compares kind, span, fill (color and paint override), geometry
   * overrides, and ink skipping. */
  bool operator==(const Decoration&) const = default;
};

}  // namespace sigil::weave
