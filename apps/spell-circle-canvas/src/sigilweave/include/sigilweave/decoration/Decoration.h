#pragma once

/** @file
 * @ingroup paint
 *
 * A decoration resolved against a run: the band an underline, strikethrough,
 * overline or highlight occupies once the font's metrics have filled in
 * what the style left at zero, the paint that band draws with, and the
 * x spans a run's band actually covers once skip-ink has cut it around the
 * glyphs' descenders. Deterministic geometry over a PositionedRun, exposed
 * so a test can check the band without drawing it; the draws in the paint
 * feature run over these same functions.
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
/// geometry (top edge relative to the baseline, px, y-down) and color.
struct ResolvedDecorationBand {
  float position = 0;             ///< band top, relative to the baseline
  float thickness = 1;            ///< band height, px; floored at 1
  SkColor color = SK_ColorBLACK;  ///< resolved draw color, never transparent
};

/** Resolves a decoration's thickness, position, and color against font
 * metrics (deterministic geometry, exposed for tests): explicit values win;
 * zeros fall back to the face's underline/strikeout metrics, a mid-x-height
 * strikethrough, or the ascent line for overlines, with a 1px thickness
 * floor throughout. */
[[nodiscard]] ResolvedDecorationBand resolveDecorationBand(
    const Decoration& decoration, const SkFontMetrics& metrics,
    SkColor foregroundColor);

/** Resolves the paint a decoration band draws with — the fill concern,
 * separate from the band geometry above: the decoration's `paint` override
 * verbatim when present, otherwise an anti-aliased fill of the band's
 * resolved color. */
[[nodiscard]] SkPaint decorationBandPaint(const Decoration& decoration,
                                          const ResolvedDecorationBand& band);

/** Returns the absolute x spans the decoration actually draws for `run` —
 * one span covering the run's advance, minus glyph-ink intercepts (grown by
 * one thickness of standoff) when the decoration skips ink. Empty for
 * transformed, vertical, and placeholder runs. */
[[nodiscard]] std::vector<std::pair<float, float>> decorationSegments(
    const PositionedRun& run, const Decoration& decoration,
    const ResolvedDecorationBand& band);

}  // namespace detail

}  // namespace sigil::weave
