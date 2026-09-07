/** @file
 * THE MARKER TEXT THAT DID NOT FIT ENDS WITH: the last placed line trimmed
 * back far enough for it, and the marker set the way the text it stands
 * for was set.
 */

#include <hb.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkTextBlob.h>
#include <unicode/ubidi.h>
#include <unicode/utf16.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "Blobs.h"
#include "ParagraphLayoutInternal.h"
#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/layout/ParagraphLayout.h"

namespace sigil::weave {

namespace detail {

// Overflow marker: trim the final placed line until the configured ellipsis
// fits, then append it as one more run (CSS text-overflow semantics). A
// line's marker lands at its end and a column's at its foot; a contour
// interval takes none, because there is no end to a loop.
void applyEllipsis(FontContext& fontContext, Paragraph& paragraph,
                   IntervalSequence& intervalSequence,
                   const ParagraphLayoutOptions& options,
                   ParagraphLayout& result) {
  if (result.runs.empty()) return;
  // Overflow means the breakers consumed every interval the geometry had,
  // so the final placed line sits on the last one.
  const FlatInterval* lastInterval = nullptr;
  for (size_t intervalIndex = 0; const FlatInterval* flatInterval =
                                     intervalSequence.intervalAt(intervalIndex);
       ++intervalIndex)
    lastInterval = flatInterval;
  if (!lastInterval || lastInterval->interval.contour.valid()) return;
  const SkVector direction = lastInterval->interval.direction;
  const bool alongColumn = direction.x() == 0 && direction.y() == 1;
  if (!alongColumn && (direction.x() != 1 || direction.y() != 0)) return;

  // Shape the marker in the style of the line's tail (fallback-resolved on
  // its first codepoint; cache-shared like every other word) — and, down a
  // column, in that tail's FORM. THE MARKER STANDS FOR THE TEXT THAT WAS
  // CUT, so it is set the way that text was set: a column of upright
  // glyphs ends in an upright marker, which TTB shaping gives the face's
  // own `vert` form when it has one, and a rotated Latin run ends in a
  // marker turned with the column exactly as the letters before it are.
  const int lineIndex = result.runs.back().lineIndex;
  const uint32_t styleIndex = result.runs.back().styleIndex;
  const uint32_t tailWord = result.runs.back().wordIndex;
  const bool uprightMarker = alongColumn && !result.runs.back().transformed;
  const StyleSpan& span = paragraph.spans()[styleIndex];
  UChar32 firstCodepoint;
  {
    size_t codeUnitIndex = 0;
    U16_NEXT(options.overflow.ellipsis.data(), codeUnitIndex,
             options.overflow.ellipsis.size(), firstCodepoint);
  }
  const char* languageTag = span.style.shaping.languageTag.empty()
                                ? nullptr
                                : span.style.shaping.languageTag.c_str();
  sk_sp<SkTypeface> typeface = fontContext.resolveTypeface(
      span.style.shaping.typeface, firstCodepoint, languageTag);
  if (!typeface) typeface = fontContext.defaultTypeface();
  ShapedWordRef marker = shapeWord(
      fontContext, span.style.shaping, typeface, options.overflow.ellipsis,
      static_cast<ScriptTag>(HB_SCRIPT_COMMON), false, uprightMarker);
  if (!marker || marker->glyphs.empty()) return;
  // Like a tab leader, the marker is the layout's own word and not one of
  // the paragraph's, so the layout is what keeps it alive for its run.
  result.shapedByTheLayout.push_back(marker);

  size_t lineBegin = result.runs.size();
  while (lineBegin > 0 && result.runs[lineBegin - 1].lineIndex == lineIndex)
    lineBegin--;
  // How far along the interval a run reaches, in the pen's own direction —
  // the one measurement the trim is made of, and the only thing about it
  // that the writing mode changes.
  auto runEnd = [&](const PositionedRun& run) {
    const float runWidth = run.shaped
                               ? run.advance
                               : (run.placeholderIndex >= 0
                                      ? paragraph
                                            .placeholders()[static_cast<size_t>(
                                                run.placeholderIndex)]
                                            .width
                                      : 0.0f);
    if (!alongColumn) return run.origin.x() + runWidth;
    // A ROTATED run's placement is baked into its blob and its origin is
    // the canvas origin, so only its pen offset says where down the column
    // it sits; its horizontal advance IS its travel down the column.
    if (run.transformed)
      return lastInterval->interval.origin.y() + run.penOffset + runWidth;
    // A TATE-CHU-YOKO run stands across the column and consumes its font
    // height, not the advance of however many digits it holds; its origin
    // is the baseline it stands on, so its foot is one descent below.
    if (run.shaped && !run.shaped->vertical) {
      SkFontMetrics metrics;
      makeFont(run.shaped->typeface, run.shaped->fontSize, run.shaped->scaleX,
               run.shaped->aliased)
          .getMetrics(&metrics);
      return run.origin.y() + metrics.fDescent;
    }
    return run.origin.y() + runWidth;
  };

  // Drop whole trailing words until the marker fits inside the interval.
  const float intervalStart = alongColumn ? lastInterval->interval.origin.y()
                                          : lastInterval->interval.origin.x();
  const float limit =
      intervalStart + lastInterval->interval.length - marker->advance + 0.25f;
  while (result.runs.size() > lineBegin && runEnd(result.runs.back()) > limit) {
    const uint32_t trailingWordIndex = result.runs.back().wordIndex;
    while (result.runs.size() > lineBegin &&
           result.runs.back().wordIndex == trailingWordIndex) {
      result.firstUnplacedWord =
          std::min(result.firstUnplacedWord, result.runs.back().wordIndex);
      result.runs.pop_back();
    }
  }

  PositionedRun run;
  run.shaped = marker.get();
  run.advance = marker->advance;
  run.styleIndex = styleIndex;
  run.wordIndex = tailWord;
  run.lineIndex = lineIndex;
  const bool afterARun = result.runs.size() > lineBegin;
  const float markerPen =
      afterARun ? runEnd(result.runs.back()) : intervalStart;
  // The marker names the interval it landed on and where along it, like
  // any other run. A COLUMN's metrics are read through that pair — a
  // column has no baseline to measure from the way a line has one — so a
  // marker that named neither would fall outside the column it ends.
  run.intervalIndex = lastInterval->index;
  run.penOffset = markerPen - intervalStart;
  if (!alongColumn) {
    run.blob = wordBlob(*marker);
    run.origin = {markerPen, afterARun ? result.runs.back().origin.y()
                                       : lastInterval->interval.origin.y()};
  } else if (uprightMarker) {
    run.blob = wordBlob(*marker);
    run.origin = {lastInterval->interval.origin.x(), markerPen};
  } else {
    run.blob = buildTransformedBlob(*marker, lastInterval->interval,
                                    markerPen - intervalStart,
                                    options.pathText.tangentRotationSteps);
    run.transformed = true;
  }
  if (!run.blob) return;
  result.runs.push_back(std::move(run));
  result.ellipsized = true;
}

}  // namespace detail

}  // namespace sigil::weave
