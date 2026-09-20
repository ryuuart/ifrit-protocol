#pragma once

/** @file
 * @ingroup weave-shaping
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

/** One line decoration — underline, strikethrough, overline or highlight
 * — as band geometry plus a band fill, resolved with a run's paint at
 * draw time, so adding, removing or recolouring one never re-shapes and
 * never relayouts. Thickness and position default to the font's own
 * metrics, making `PaintStyle::addDecoration({})` a correct underline. A
 * band spans the decorated RANGE by default, covering the glue between
 * words, and `Span::kPerWord` opts back into one band per word.
 * @silent the run is transformed, on a path or on a rotated interval: a
 * band would have to follow the curve it rides. */
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
  /** Which side of the run's own axis the band anchors on: an underline
   * and an overline are one band on opposite sides of it, so the opposite
   * side is the other one's anchor.
   * @silent the kind crosses the type rather than standing beside it — a
   * strikethrough, a highlight — or an explicit `offset` already names
   * the near edge. */
  enum class Side : uint8_t {
    kDefault,   ///< the kind's own side: an underline below the line and
                ///< right of the column, an overline above it and left
    kOpposite,  ///< the two swap: an underline above the line or left of
                ///< the column, an overline below it or right of it
  };

  Kind kind = Kind::kUnderline;       ///< only underlines honor `skipInk`
  Span span = Span::kDecoratedRange;  ///< continuous band vs one per word
  Side side = Side::kDefault;         ///< which side of the axis it takes
  /// SK_ColorTRANSPARENT → the resolved foreground paint's color — except
  /// for kHighlight, where an opaque foreground would hide the text, so it
  /// resolves to the foreground color at quarter alpha instead.
  SkColor color = SK_ColorTRANSPARENT;
  /// 0 → thickness from font metrics (kHighlight: ascent + descent),
  /// floored at 1px.
  float thickness = 0;
  /// 0 → position from font metrics on the side `side` names (kHighlight:
  /// the ascent line); otherwise the band's top edge in px relative to the
  /// baseline (positive below, Skia's y-grows-down convention), which
  /// names a side of its own and leaves `side` nothing to choose.
  float offset = 0;
  /// Underlines only: interrupt the line where glyph ink (descenders)
  /// crosses the band, via SkTextBlob::getIntercepts.
  bool skipInk = true;
  /// Full-vocabulary band fill, applied verbatim and taking precedence
  /// over `color`, whose resolution rules — the translucent highlight
  /// default included — then no longer apply. Band geometry is untouched:
  /// the paint fills the same rect segments, ink skipping included.
  std::optional<SkPaint> paint;

  /** Compares kind, span, side, fill (color and paint override), geometry
   * overrides, and ink skipping. */
  bool operator==(const Decoration&) const = default;
};

}  // namespace sigil::weave
