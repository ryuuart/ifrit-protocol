#pragma once

/** @file
 * @ingroup paint
 *
 * The decoration walk over a layout's runs: every band rectangle a
 * paragraph's decorations draw, emitted through a callback with its paint
 * already resolved, so a draw only has to put the rectangle on the canvas.
 * Runs on one line that share a style and a font merge into one band that
 * also covers the glue between words, and an underline that skips ink is
 * cut around every member run's glyph intercepts, which are memoized per
 * blob. Both draws of a layout — the immediate one and the batched one —
 * run over this same walk, once for the highlights that sit beneath the
 * glyphs and once for everything that sits above them.
 */

#include <include/core/SkFontMetrics.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkTextBlob.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "sigilweave/decoration/Decoration.h"
#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/layout/PositionedRun.h"
#include "sigilweave/paragraph/Paragraph.h"

namespace sigil::weave {
namespace detail {

// Skip-ink intercepts of `blob` with the band window `bounds` (two y
// values), memoized on exactly those inputs. Defined in Decoration.cpp.
const std::vector<SkScalar>& cachedIntercepts(const SkTextBlob& blob,
                                              const SkScalar bounds[2]);

// The paint a run draws with: the override when one is given, else its
// span's paint, else a default for an out-of-range style index. Defined in
// Decoration.cpp.
const PaintStyle& resolvePaint(const std::vector<StyleSpan>& spans,
                               uint32_t styleIndex,
                               const PaintStyle* overridePaint);

/// A run that can carry a decoration band: straight glyphs, along a line or
/// down a column. A run the layout TURNED carries none — a band would have
/// to follow the curve it rides.
inline bool decorableRun(const PositionedRun& run) {
  return !run.transformed && run.shaped && run.placeholderIndex < 0 && run.blob;
}

/// The coordinate of the axis a run's band is measured from: its baseline
/// along a line, its column axis down a column.
inline float decorationAxisOf(const PositionedRun& run) {
  return run.shaped->vertical ? run.origin.x() : run.origin.y();
}

/// Where the run's pen enters, along its own axis of travel.
inline float decorationEntryOf(const PositionedRun& run) {
  return run.shaped->vertical ? run.origin.y() : run.origin.x();
}

/// One band rectangle: `start`/`end` travel with the pen, `axis` names the
/// baseline or the column axis, and the band grows down from a baseline and
/// right from a column axis. Which side of the axis the band stands on is
/// already carried by `band.position`, signed by the resolve.
inline SkRect decorationBandRect(bool alongColumn, float start, float end,
                                 float axis,
                                 const ResolvedDecorationBand& band) {
  const float near = axis + band.position;
  return alongColumn
             ? SkRect::MakeLTRB(near, start, near + band.thickness, end)
             : SkRect::MakeLTRB(start, near, end, near + band.thickness);
}

// Emits every decoration rect for a layout through `emitRect(SkRect,
// const SkPaint &)` — the paint already resolved by decorationBandPaint(),
// so callers just draw. Shared by draw() (immediate) and drawBatched()
// (deferred past the glyph buckets so strikethroughs land above the
// batched glyphs).
//
// Decorations span the *decorated range*, not individual words: contiguous
// runs on one line sharing a style (and metrics identity) merge into one
// group whose band also covers the glue between words — CSS behavior, where
// an underlined sentence is one continuous line. Skip-ink intercepts still
// come from each run's own blob; the inter-word gaps have no ink, so they
// stay covered.
/// Which decoration kinds an emission pass covers: highlights paint
/// beneath every glyph pass, everything else above them.
enum class DecorationPhase : uint8_t { kBelowGlyphs, kAboveGlyphs };

inline bool decorationInPhase(const Decoration& decoration,
                              DecorationPhase phase) {
  const bool isHighlight = decoration.kind == Decoration::Kind::kHighlight;
  return phase == DecorationPhase::kBelowGlyphs ? isHighlight : !isHighlight;
}

template <typename EmitRect>
void forEachDecorationRect(const std::vector<PositionedRun>& runs,
                           const std::vector<StyleSpan>& spans,
                           const PaintStyle* overridePaint,
                           DecorationPhase phase, EmitRect&& emitRect) {
  for (size_t groupStart = 0; groupStart < runs.size();) {
    const PositionedRun& first = runs[groupStart];
    if (!decorableRun(first)) {
      ++groupStart;
      continue;
    }
    const PaintStyle& style =
        resolvePaint(spans, first.styleIndex, overridePaint);
    if (style.decorations.empty()) {
      ++groupStart;
      continue;
    }

    // Extend the group while runs stay visually contiguous in reading order
    // on the same line or column, with identical style and metrics identity
    // (same typeface + size ⇒ same band geometry). Bidi-reordered or
    // fallback-split neighbors simply start a new group, and so does a run
    // set the other way round — the two bands lie on different axes.
    const bool alongColumn = first.shaped->vertical;
    size_t groupEnd = groupStart + 1;
    while (groupEnd < runs.size()) {
      const PositionedRun& candidate = runs[groupEnd];
      const PositionedRun& previous = runs[groupEnd - 1];
      if (!decorableRun(candidate) || candidate.lineIndex != first.lineIndex ||
          candidate.styleIndex != first.styleIndex ||
          candidate.shaped->vertical != alongColumn ||
          candidate.shaped->typeface.get() != first.shaped->typeface.get() ||
          candidate.shaped->fontSize != first.shaped->fontSize ||
          decorationAxisOf(candidate) != decorationAxisOf(first) ||
          decorationEntryOf(candidate) < decorationEntryOf(previous))
        break;
      ++groupEnd;
    }

    const float groupStartX = decorationEntryOf(first);
    const float groupEndX =
        decorationEntryOf(runs[groupEnd - 1]) + runs[groupEnd - 1].advance;
    const SkFont font =
        makeFont(first.shaped->typeface, first.shaped->fontSize);
    SkFontMetrics metrics;
    font.getMetrics(&metrics);

    for (const Decoration& decoration : style.decorations) {
      if (!decorationInPhase(decoration, phase)) continue;
      const detail::ResolvedDecorationBand band = detail::resolveDecorationBand(
          decoration, metrics, style.foreground.getColor(), alongColumn);
      const SkPaint bandPaint = detail::decorationBandPaint(decoration, band);
      const float axis = decorationAxisOf(first);
      if (decoration.span == Decoration::Span::kPerWord) {
        // One band per word run: reuse the single-run segment geometry so
        // per-word skip-ink behaves exactly like the spanning form's.
        for (size_t runIndex = groupStart; runIndex < groupEnd; ++runIndex)
          for (const auto& [startX, endX] :
               detail::decorationSegments(runs[runIndex], decoration, band))
            emitRect(decorationBandRect(alongColumn, startX, endX, axis, band),
                     bandPaint);
        continue;
      }
      // Skip-ink reads a horizontal band window, so a column's band is
      // continuous — see decorationSegments.
      const bool skipInk = decoration.skipInk && !alongColumn &&
                           decoration.kind == Decoration::Kind::kUnderline;
      if (!skipInk) {
        emitRect(
            decorationBandRect(alongColumn, groupStartX, groupEndX, axis, band),
            bandPaint);
        continue;
      }
      const float top = axis + band.position;
      // One continuous band minus every member run's ink intercepts. Glue
      // between runs carries no ink, so it never interrupts the band.
      const SkScalar bounds[2] = {band.position,
                                  band.position + band.thickness};
      const float standoff = band.thickness;
      float cursor = groupStartX;
      for (size_t runIndex = groupStart; runIndex < groupEnd; ++runIndex) {
        const PositionedRun& run = runs[runIndex];
        const std::vector<SkScalar>& intercepts =
            cachedIntercepts(*run.blob, bounds);
        const int interceptCount = static_cast<int>(intercepts.size());
        if (interceptCount < 2) continue;
        for (int pair = 0; pair + 1 < interceptCount; pair += 2) {
          const float inkStart =
              run.origin.x() + intercepts[static_cast<size_t>(pair)] - standoff;
          const float inkEnd = run.origin.x() +
                               intercepts[static_cast<size_t>(pair) + 1] +
                               standoff;
          if (inkStart > cursor)
            emitRect(
                SkRect::MakeLTRB(cursor, top, std::min(inkStart, groupEndX),
                                 top + band.thickness),
                bandPaint);
          cursor = std::max(cursor, inkEnd);
        }
      }
      if (cursor < groupEndX)
        emitRect(SkRect::MakeLTRB(cursor, top, groupEndX, top + band.thickness),
                 bandPaint);
    }
    groupStart = groupEnd;
  }
}

}  // namespace detail
}  // namespace sigil::weave
