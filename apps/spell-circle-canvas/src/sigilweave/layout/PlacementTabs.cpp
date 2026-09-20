/** @file
 * WHAT A TAB STOP OPENS AND WHAT FILLS IT: how much of the cell a stop
 * opened stands before the character it aligns on, which is what a
 * character-aligned stop pulls the cell back by, and the leader set
 * across the gap between the tab and the stop.
 */

#include <hb.h>
#include <include/core/SkTextBlob.h>
#include <unicode/utf16.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ParagraphLayoutInternal.h"
#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/layout/ParagraphLayout.h"

namespace sigil::weave {

namespace detail {

float widthBeforeAlignCharacter(const Paragraph& paragraph,
                                const std::vector<Word>& words,
                                const std::vector<uint32_t>& visualWordOrder,
                                size_t tabVisualIndex, char16_t alignOn,
                                float cellWidth) {
  const std::u16string& text = paragraph.text();
  float before = 0;
  for (size_t visualIndex = tabVisualIndex + 1;
       visualIndex < visualWordOrder.size(); ++visualIndex) {
    const Word& word = words[visualWordOrder[visualIndex]];
    const size_t found = text.find(alignOn, word.textBegin) < word.textEnd
                             ? text.find(alignOn, word.textBegin)
                             : std::u16string::npos;
    if (found != std::u16string::npos) {
      const auto target = static_cast<uint32_t>(found);
      for (const WordSegment& segment : word.segments()) {
        const ShapedWord& shaped = *segment.shaped;
        for (size_t glyphIndex = 0; glyphIndex < shaped.glyphs.size();
             ++glyphIndex) {
          if (segment.textBegin + shaped.clusters[glyphIndex] >= target)
            return before;
          before += shaped.advances[glyphIndex];
        }
      }
      return before;
    }
    before += word.width;
    if (word.tabAfter) break;  // the cell ends at the next tab
    if (visualIndex + 1 < visualWordOrder.size()) before += word.spaceWidth;
  }
  return cellWidth;
}

/** Sets a stop's leader across the gap it opened: the string repeated as
 *  many whole times as fit, BUTTED AGAINST THE STOP so the run of dots
 *  meets the figure it leads to and the ragged end falls where the eye
 *  starts rather than where it lands. Set in the style of the text before
 *  the tab, and only along a straight horizontal line — a leader down a
 *  column is a different convention and this is not it. */
void emitLeader(FontContext& fontContext, const Paragraph& paragraph,
                ParagraphLayout& result, const FlatInterval& flatInterval,
                const Word& word, uint32_t wordIndex, const TabStop& stop,
                float gapStart, float gapEnd) {
  if (gapEnd - gapStart <= 0 || flatInterval.interval.contour.valid()) return;
  if (flatInterval.interval.direction.x() != 1 ||
      flatInterval.interval.direction.y() != 0)
    return;
  const uint32_t styleIndex =
      word.segments().empty() ? 0 : word.segments().back().styleIndex;
  if (styleIndex >= paragraph.spans().size()) return;
  const StyleSpan& span = paragraph.spans()[styleIndex];
  UChar32 firstCodepoint = 0;
  {
    size_t codeUnitIndex = 0;
    U16_NEXT(stop.leader.data(), codeUnitIndex, stop.leader.size(),
             firstCodepoint);
  }
  const char* languageTag = span.style.shaping.languageTag.empty()
                                ? nullptr
                                : span.style.shaping.languageTag.c_str();
  sk_sp<SkTypeface> typeface = fontContext.resolveTypeface(
      span.style.shaping.typeface, firstCodepoint, languageTag);
  if (!typeface) typeface = fontContext.defaultTypeface();
  const ShapedWordReference leader =
      shapeWord(fontContext, span.style.shaping, typeface, stop.leader,
                static_cast<ScriptTag>(HB_SCRIPT_COMMON), false, false);
  if (!leader || leader->glyphs.empty() || leader->advance <= 0) return;
  // The leader is shaped HERE and lives nowhere in the paragraph, so the
  // layout keeps the handle its runs borrow from.
  LayoutAccess::retain(result, leader);
  const ShapedWord* const leaderWord = leader.get();
  const auto repeats =
      static_cast<int>(std::floor((gapEnd - gapStart) / leader->advance));
  float pen = gapEnd - static_cast<float>(repeats) * leader->advance;
  for (int repeat = 0; repeat < repeats; ++repeat) {
    PositionedRun run;
    run.blob = wordBlob(*leader);
    run.shaped = leaderWord;
    run.advance = leader->advance;
    run.styleIndex = styleIndex;
    // A leader belongs to the WORD BEFORE ITS TAB: it is set in that word's
    // style, on that word's line, and everything that reads a run back —
    // the line bands, the choreography walk — asks a run which word it
    // came from.
    run.wordIndex = wordIndex;
    run.lineIndex = flatInterval.sourceLineIndex;
    run.intervalIndex = flatInterval.index;
    run.penOffset = pen;
    run.origin = flatInterval.interval.origin + SkVector{pen, 0};
    if (run.blob) result.runs.push_back(std::move(run));
    pen += leader->advance;
  }
}

}  // namespace detail

}  // namespace sigil::weave
