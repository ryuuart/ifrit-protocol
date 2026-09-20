#pragma once

/** @file
 * @ingroup weave-paint
 *
 * A decoration resolved against a run: the band it occupies once the
 * font's metrics have filled in what the style left at zero, the paint
 * that band draws with, and the spans its band actually covers once
 * skip-ink has cut it. Deterministic geometry over a PositionedRun,
 * exposed so a test can check a band without drawing it.
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

/** Resolves a decoration's thickness, position and colour against font
 * metrics: explicit values win, and zeros fall back to the face's
 * underline and strikeout metrics, a mid-x-height strikethrough or the
 * ascent line for overlines, thickness floored at one pixel.
 * @p alongColumn measures from the em box instead, and
 * `Decoration::offset` is then signed ACROSS the column, positive right. */
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
 * actually draws for @p run — one span covering the run's advance, minus
 * glyph-ink intercepts grown by one thickness of standoff when the
 * decoration skips ink. A COLUMN RUN ALWAYS ANSWERS ONE SPAN.
 * @silent the run is transformed or a placeholder, which answer with no
 * spans at all. */
[[nodiscard]] std::vector<std::pair<float, float>> decorationSegments(
    const PositionedRun& run, const Decoration& decoration,
    const ResolvedDecorationBand& band);

}  // namespace detail

}  // namespace sigil::weave
