#pragma once

/** @file
 * @ingroup paint
 *
 * A decoration resolved against a run: the band an underline, strikethrough,
 * overline or highlight occupies once the font's metrics have filled in
 * what the style left at zero, the paint that band draws with, and the
 * spans along the run's own axis its band actually covers once skip-ink has
 * cut it around the glyphs' descenders. Deterministic geometry over a
 * PositionedRun, exposed so a test can check the band without drawing it;
 * the draws in the paint feature run over these same functions.
 */

#include <include/core/SkColor.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkPaint.h>

#include <utility>
#include <vector>

#include "sigilweave/layout/PositionedRun.h"
#include "sigilweave/style/Style.h"

namespace sigil::weave {

namespace detail {

/// A Decoration resolved against one run's font metrics: concrete band
/// geometry (the near edge measured across the run's own axis, px) and
/// color. Along a line that axis is the baseline and the band grows down;
/// down a column it is the column axis and the band grows right.
struct ResolvedDecorationBand {
  float position = 0;   ///< near edge, relative to the run's own axis
  float thickness = 1;  ///< band depth across that axis, px; floored at 1
  SkColor color = SK_ColorBLACK;  ///< resolved draw color, never transparent
};

/** Resolves a decoration's thickness, position, and color against font
 * metrics (deterministic geometry, exposed for tests): explicit values win;
 * zeros fall back to the face's underline/strikeout metrics, a mid-x-height
 * strikethrough, or the ascent line for overlines, with a 1px thickness
 * floor throughout.
 *
 * @p alongColumn resolves the band for a run set DOWN A COLUMN instead. A
 * column has no baseline — an upright glyph's em box is centred on the
 * column axis — so the face's underline and strikeout metrics have nothing
 * to measure from and the em box does the measuring instead: an underline
 * stands clear of the box on the RIGHT of the column, which is the side a
 * vertical setting reads its emphasis line on, an overline on the left, a
 * strikethrough down the axis itself, and a highlight across the whole box.
 * `Decoration::offset` still overrides, and is then a signed distance ACROSS
 * the column (positive to the right). */
[[nodiscard]] ResolvedDecorationBand resolveDecorationBand(
    const Decoration& decoration, const SkFontMetrics& metrics,
    SkColor foregroundColor, bool alongColumn = false);

/** Resolves the paint a decoration band draws with — the fill concern,
 * separate from the band geometry above: the decoration's `paint` override
 * verbatim when present, otherwise an anti-aliased fill of the band's
 * resolved color. */
[[nodiscard]] SkPaint decorationBandPaint(const Decoration& decoration,
                                          const ResolvedDecorationBand& band);

/** Returns the absolute spans along the run's own axis the decoration
 * actually draws for `run` — one span covering the run's advance, minus
 * glyph-ink intercepts (grown by one thickness of standoff) when the
 * decoration skips ink. A COLUMN RUN ALWAYS ANSWERS ONE SPAN: intercepts
 * are cut out of a horizontal band, which a column's band is not, so a
 * vertical underline draws through its glyphs' ink rather than around it.
 * Empty for transformed and placeholder runs. */
[[nodiscard]] std::vector<std::pair<float, float>> decorationSegments(
    const PositionedRun& run, const Decoration& decoration,
    const ResolvedDecorationBand& band);

}  // namespace detail

}  // namespace sigil::weave
