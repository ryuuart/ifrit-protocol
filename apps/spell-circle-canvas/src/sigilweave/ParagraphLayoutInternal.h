#pragma once

// Internals shared across the layout translation units: the interval
// sequence and placement the greedy (LineBreak.cpp) and Knuth-Plass
// (KnuthPlass.cpp) breakers both drive, and the decoration walk the
// immediate and batched draws (Paint.cpp) both run over the bands
// Decoration.cpp resolves.

#include <include/core/SkFontMetrics.h>
#include <include/core/SkPaint.h>
#include <include/core/SkTextBlob.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "sigilweave/Flow.h"
#include "sigilweave/Paragraph.h"
#include "sigilweave/ParagraphLayout.h"
#include "sigilweave/Shaper.h"

namespace sigil::weave {
namespace detail {

struct FlatInterval {
  LineInterval interval;
  int sourceLineIndex = 0;
  /// Position in the flattened sequence. Carried on the interval rather
  /// than threaded through the placement calls, so the number a run
  /// reports and the number the breakers used are the same number.
  int index = 0;
};

// Flattens a FlowGeometry's lines into one ordered sequence of intervals,
// fetched lazily. Both breakers consume geometry exclusively through this,
// so break decisions and placement always agree on interval numbering.
class IntervalSequence {
 public:
  IntervalSequence(FlowGeometry& geometry, float lineHeight, float ascent,
                   float minimumWidth = 0)
      : m_geometry(geometry),
        m_lineHeight(lineHeight),
        m_ascent(ascent),
        m_minimumWidth(minimumWidth) {}

  /** Returns a flattened interval, fetching source lines on demand. */
  const FlatInterval* intervalAt(size_t intervalIndex) {
    while (intervalIndex >= m_flatIntervals.size() && !m_geometryExhausted)
      fetchLine();
    return intervalIndex < m_flatIntervals.size()
               ? &m_flatIntervals[intervalIndex]
               : nullptr;
  }

  /** Returns whether every source line has the same single measure. */
  bool uniform() const { return m_geometry.uniformIntervals(); }

  /** Returns every interval fetched so far, in index order. */
  const std::vector<FlatInterval>& flattened() const { return m_flatIntervals; }

 private:
  /** Fetches and flattens the next source line into the interval cache. */
  void fetchLine() {
    m_sourceLineIntervals.clear();
    if (!m_geometry.lineIntervals(m_nextLineIndex, m_lineHeight, m_ascent,
                                  m_sourceLineIntervals)) {
      m_geometryExhausted = true;
      return;
    }
    for (const LineInterval& interval : m_sourceLineIntervals)
      if (interval.length >= m_minimumWidth)
        m_flatIntervals.push_back({interval, m_nextLineIndex,
                                   static_cast<int>(m_flatIntervals.size())});
    m_nextLineIndex++;
  }

  FlowGeometry& m_geometry;
  float m_lineHeight;
  float m_ascent;
  float m_minimumWidth;
  std::vector<FlatInterval> m_flatIntervals;
  std::vector<LineInterval> m_sourceLineIntervals;
  int m_nextLineIndex = 0;
  bool m_geometryExhausted = false;
};

// Natural (unjustified) width of a half-open word range on one line: content
// widths plus inter-word glue, the last word's trailing space excluded.
float naturalWidth(const std::vector<Word>& words, uint32_t firstWordIndex,
                   uint32_t endWordIndex);

// Whether tab stops are configured at all (ParagraphLayoutOptions::tabStops).
// Defined in the header (as is glueAfter below) rather than in one of the
// breaker translation units: both are called from the innermost loops of
// both breakers, and both must stay inlinable there.
inline bool tabStopsActive(const ParagraphLayoutOptions& options) {
  return !options.tabStops.positions.empty() || options.tabStops.interval > 0;
}

// The glue width after one word with the pen at `penPosition` (relative to
// the line interval's start): the distance to the next tab stop for tab
// gaps, the measured whitespace otherwise. Both breakers and placement
// resolve stops through this one function so they always agree on widths.
inline float glueAfter(const Word& word, float penPosition,
                       const ParagraphLayoutOptions& options) {
  if (!word.tabAfter || !tabStopsActive(options)) return word.spaceWidth;
  constexpr float kMinTabAdvance = 0.5f;  // a stop the pen already reached
                                          // is not "the next" stop
  for (const float stop : options.tabStops.positions)
    if (stop >= penPosition + kMinTabAdvance) return stop - penPosition;
  if (options.tabStops.interval > 0) {
    const float base = options.tabStops.positions.empty()
                           ? 0.0f
                           : options.tabStops.positions.back();
    const float distance = std::max(penPosition - base, 0.0f);
    const float repeats =
        std::floor(distance / options.tabStops.interval) + 1.0f;
    const float stop = base + repeats * options.tabStops.interval;
    if (stop >= penPosition + kMinTabAdvance) return stop - penPosition;
    return stop + options.tabStops.interval - penPosition;
  }
  return word.spaceWidth;  // stops exhausted: tab degrades to a space
}

// Places a half-open word range into `interval` with the given alignment,
// appending PositionedRuns to `out`. Pure arithmetic over cached ShapedWords:
// straight horizontal intervals reuse each word's shared blob; rotated and
// contour intervals bake per-glyph RSXforms. When `hyphenBreakTaken`, the
// line ends at a soft hyphen and the word's hyphen glyph is rendered.
void placeWords(const std::vector<Word>& words, uint32_t firstWordIndex,
                uint32_t endWordIndex, const FlatInterval& interval,
                TextAlignment alignment, bool lastLine, bool hyphenBreakTaken,
                const ParagraphLayoutOptions& options, ParagraphLayout& out);

// Whether a non-final break before `endWordIndex` lands on a soft hyphen.
inline bool hyphenTakenAt(const std::vector<Word>& words, uint32_t endWordIndex,
                          bool lineIsFinal,
                          const ParagraphLayoutOptions& options) {
  return options.hyphenation.enabled && !lineIsFinal && endWordIndex > 0 &&
         endWordIndex <= words.size() && words[endWordIndex - 1].hyphenBreak &&
         words[endWordIndex - 1].hyphenGlyph &&
         !words[endWordIndex - 1].mandatoryBreakAfter;
}

// Knuth-Plass entry: breaks + places every word that fits. Takes the
// paragraph (not just its words) so it can pull lazy shaping along its own
// frontier. Defined in KnuthPlass.cpp.
ParagraphLayout knuthPlassLayout(FontContext& fontContext, Paragraph& paragraph,
                                 IntervalSequence& intervalSequence,
                                 const ParagraphLayoutOptions& options);

// ── Drawing ───────────────────────────────────────────────────────────

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

/// A run that can carry a decoration band: straight horizontal glyphs.
inline bool decorableRun(const PositionedRun& run) {
  return !run.transformed && run.shaped && !run.shaped->vertical &&
         run.placeholderIndex < 0 && run.blob;
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

    // Extend the group while runs stay visually contiguous left-to-right on
    // the same line with identical style and metrics identity (same
    // typeface + size ⇒ same band geometry). Bidi-reordered or
    // fallback-split neighbors simply start a new group.
    size_t groupEnd = groupStart + 1;
    while (groupEnd < runs.size()) {
      const PositionedRun& candidate = runs[groupEnd];
      const PositionedRun& previous = runs[groupEnd - 1];
      if (!decorableRun(candidate) || candidate.lineIndex != first.lineIndex ||
          candidate.styleIndex != first.styleIndex ||
          candidate.shaped->typeface.get() != first.shaped->typeface.get() ||
          candidate.shaped->fontSize != first.shaped->fontSize ||
          candidate.origin.y() != first.origin.y() ||
          candidate.origin.x() < previous.origin.x())
        break;
      ++groupEnd;
    }

    const float groupStartX = first.origin.x();
    const float groupEndX =
        runs[groupEnd - 1].origin.x() + runs[groupEnd - 1].shaped->advance;
    const SkFont font =
        makeFont(first.shaped->typeface, first.shaped->fontSize);
    SkFontMetrics metrics;
    font.getMetrics(&metrics);

    for (const Decoration& decoration : style.decorations) {
      if (!decorationInPhase(decoration, phase)) continue;
      const detail::ResolvedDecorationBand band = detail::resolveDecorationBand(
          decoration, metrics, style.foreground.getColor());
      const SkPaint bandPaint = detail::decorationBandPaint(decoration, band);
      const float top = first.origin.y() + band.position;
      if (decoration.span == Decoration::Span::kPerWord) {
        // One band per word run: reuse the single-run segment geometry so
        // per-word skip-ink behaves exactly like the spanning form's.
        for (size_t runIndex = groupStart; runIndex < groupEnd; ++runIndex)
          for (const auto& [startX, endX] :
               detail::decorationSegments(runs[runIndex], decoration, band))
            emitRect(SkRect::MakeLTRB(startX, top, endX, top + band.thickness),
                     bandPaint);
        continue;
      }
      const bool skipInk =
          decoration.skipInk && decoration.kind == Decoration::Kind::kUnderline;
      if (!skipInk) {
        emitRect(
            SkRect::MakeLTRB(groupStartX, top, groupEndX, top + band.thickness),
            bandPaint);
        continue;
      }
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
                               intercepts[static_cast<size_t>(pair + 1)] +
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
