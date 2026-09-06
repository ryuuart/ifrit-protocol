/** @file
 * THE BLOCKS A TEXT IS SET IN and what a frame does with them: the words
 * between mandatory breaks resolved into blocks with their styles and
 * their air, the keeps enforced at the frame boundary, where the first
 * baseline sits, and what becomes of the room left over.
 */

#include <hb.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkTextBlob.h>
#include <unicode/ubidi.h>
#include <unicode/utf16.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

#include "Blobs.h"
#include "ParagraphLayoutInternal.h"
#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/layout/ParagraphLayout.h"

namespace sigil::weave {

namespace detail {

namespace {

/** How many lines the words in `[first, end)` would take at `measure`,
 *  counted no further than `cap`.
 *
 *  It is a greedy fit at ONE measure and nothing else. The measure is the
 *  NEXT frame's where the caller stated one
 *  (`ParagraphLayoutOptions::nextMeasure`) and the measure this frame's
 *  last line was set in otherwise, which is the same number for a chain of
 *  equal frames and the honest fallback when nobody holds the chain. The
 *  FIT stays greedy even for a block the optimizing breaker set, so a
 *  remainder whose hyphens or demerits would have bought it a line comes
 *  out a line long. Counting stops at `cap` because every caller only asks
 *  whether the remainder REACHES a number, and stopping there is what
 *  keeps the tail of a long block unshaped. */
int remainderLines(FontContext& fontContext, Paragraph& paragraph,
                   uint32_t first, uint32_t end, float measure, int cap) {
  if (first >= end || measure <= 0 || cap <= 0) return 0;
  const std::vector<Word>& words = paragraph.words();
  int lines = 1;
  float pen = 0;
  for (uint32_t wordIndex = first; wordIndex < end && wordIndex < words.size();
       ++wordIndex) {
    paragraph.ensureShapedTo(fontContext, wordIndex + 1);
    const Word& word = words[wordIndex];
    const float glue = wordIndex > first ? words[wordIndex - 1].spaceWidth : 0;
    if (pen > 0 && pen + glue + word.width > measure + kFitEpsilon) {
      if (++lines >= cap) return cap;
      pen = word.width;
    } else {
      pen += glue + word.width;
    }
  }
  return lines;
}

}  // namespace

/** The BLOCKS of a text — the words between mandatory breaks — each with
 *  its style resolved against the layout's own, its pitch resolved from its
 *  own first span, and the air before it resolved by the one spacing rule:
 *  THE GAP IS THE LARGER of the block before's `spaceAfter` and this
 *  block's `spaceBefore`, everywhere, the head of the flow included.
 *
 *  @p settings receives the resolved settings the blocks point at, one per
 *  run of blocks that resolves alike and none at all for a text whose
 *  blocks override nothing. It must outlive the blocks. */
std::vector<detail::Block> resolveBlocks(
    FontContext& fontContext, const Paragraph& paragraph,
    const ParagraphLayoutOptions& options,
    std::deque<ParagraphLayoutOptions>& settings) {
  const std::vector<Word>& words = paragraph.words();
  std::vector<detail::Block> blocks;
  uint32_t first = 0;
  for (uint32_t wordIndex = 0; wordIndex < words.size(); ++wordIndex)
    if (words[wordIndex].mandatoryBreakAfter) {
      blocks.push_back(
          detail::Block{static_cast<int>(blocks.size()), first, wordIndex + 1});
      first = wordIndex + 1;
    }
  if (first < words.size() || blocks.empty())
    blocks.push_back(detail::Block{static_cast<int>(blocks.size()), first,
                                   static_cast<uint32_t>(words.size())});

  const ParagraphStyle unstyled;
  // The style the setting at the back of `settings` was resolved from, so a
  // run of blocks set the same way shares one setting.
  const ParagraphStyle* resolvedFrom = nullptr;
  // The layout's own answer with the style list dropped, made at most once
  // and copied into every setting: a setting is the answer and never the
  // question, nothing downstream of here reads the list, and copying it
  // per setting is what would cost a long styled story the square of its
  // length.
  std::optional<ParagraphLayoutOptions> plain;
  float previousSpaceAfter = 0;
  for (size_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
    detail::Block& block = blocks[blockIndex];
    const ParagraphStyle& style = blockIndex < options.blocks.size()
                                      ? options.blocks[blockIndex]
                                      : unstyled;
    block.style = style;
    // A style that states none of the four overridable settings is set by
    // the layout's own, which the block reads where it stands.
    if (!style.alignment && !style.justification && !style.hyphenation &&
        !style.tabStops) {
      block.options = &options;
    } else if (resolvedFrom && resolvedFrom->alignment == style.alignment &&
               resolvedFrom->justification == style.justification &&
               resolvedFrom->hyphenation == style.hyphenation &&
               resolvedFrom->tabStops == style.tabStops) {
      block.options = &settings.back();
    } else {
      if (!plain) {
        plain = options;
        plain->blocks = {};
      }
      ParagraphLayoutOptions& setting = settings.emplace_back(*plain);
      if (style.alignment) setting.alignment = *style.alignment;
      if (style.justification) setting.justification = *style.justification;
      if (style.hyphenation) setting.hyphenation = *style.hyphenation;
      if (style.tabStops) setting.tabStops = *style.tabStops;
      block.options = &setting;
      resolvedFrom = &style;
    }

    const uint32_t textOffset =
        block.firstWord < words.size() ? words[block.firstWord].textBegin : 0;
    const Paragraph::Strut strut = paragraph.strutAt(fontContext, textOffset);
    // A stated line metric overrides what the FACE reports; a stated leading
    // overrides the pitch outright, and the extra it opens goes above the
    // line, which is where leading has always gone.
    float faceHeight = options.lineMetrics.height > 0
                           ? options.lineMetrics.height
                           : strut.height;
    float faceAscent = options.lineMetrics.ascent > 0
                           ? options.lineMetrics.ascent
                           : strut.ascent;
    // AN INLINE SLOT TALLER THAN THE TYPE OPENS THE LINES IT SITS IN. The
    // reserved box is one unbreakable word of the flow, and a word that
    // reaches further above the baseline than the face does — or further
    // below it — is a fact about the strut the block is set on. Nothing
    // chases it afterwards: the band is deep enough before a single break
    // is decided, which is the only order in which a box can be woven into
    // a line rather than drawn over it.
    //
    // The strut is the BLOCK's, so a slot opens every line of its own
    // block: bands are asked of the geometry before anyone knows which
    // words land on them, and a depth that varied line by line would have
    // to be decided after the break it decides.
    const std::vector<Placeholder>& placeholders = paragraph.placeholders();
    if (!placeholders.empty()) {
      float faceDescent = faceHeight - faceAscent;
      for (uint32_t wordIndex = block.firstWord;
           wordIndex < block.endWord && wordIndex < words.size(); ++wordIndex) {
        const int slot = words[wordIndex].placeholderIndex;
        if (slot < 0 || (size_t)slot >= placeholders.size()) continue;
        const Placeholder& box = placeholders[(size_t)slot];
        faceAscent = std::max(faceAscent, box.height - box.baselineDrop);
        faceDescent = std::max(faceDescent, box.baselineDrop);
      }
      faceHeight = std::max(faceHeight, faceAscent + faceDescent);
    }
    float pitch = faceHeight;
    float gridStep = 0;
    switch (style.leading.kind) {
      case Leading::Kind::kFace:
        break;
      case Leading::Kind::kMultiple:
        pitch = faceHeight * style.leading.value;
        break;
      case Leading::Kind::kAbsolute:
        pitch = style.leading.value;
        break;
      case Leading::Kind::kGrid:
        gridStep = style.leading.value;
        pitch = gridStep > 0 ? std::ceil(faceHeight / gridStep) * gridStep
                             : faceHeight;
        break;
    }
    // The reserved band is a layout input: it opens the pitch before
    // anything is broken, and `before` carries the baseline down inside the
    // band so the type stays where a reader expects it.
    const float reservedBefore =
        options.reserved.before + style.reserved.before;
    const float reservedAfter = options.reserved.after + style.reserved.after;
    block.pitch = pitch + reservedBefore + reservedAfter;
    // WHERE THE LEADING GOES. All of it above the line is the setting
    // convention; half above and half below is the web's, and a passage
    // that must sit optically centred in its own band wants that one.
    const float opened = pitch - faceHeight;
    block.ascent =
        (style.leading.kind == Leading::Kind::kFace
             ? faceAscent
             : faceAscent + opened * (style.halfLeading ? 0.5f : 1.0f)) +
        reservedBefore;
    block.gridStep = gridStep;
    block.lead = blockIndex == 0
                     ? style.spaceBefore
                     : std::max(previousSpaceAfter, style.spaceBefore);
    previousSpaceAfter = style.spaceAfter;
  }
  return blocks;
}

/** ENFORCES THE KEEPS AT THE FRAME BOUNDARY: lines the block may not leave
 *  behind are taken out of this fill and reported as overflow, so the next
 *  frame of the chain gets them.
 *
 *  A keep is a statement about a BOUNDARY — a widow stands at the head of
 *  the next frame, an orphan at the foot of this one, a kept-together pair
 *  straddles the join — so it is settled where the boundary is, once the
 *  fill has stopped, by moving lines forward. Nothing is weighed against
 *  spacing and no break is re-decided, which is why both breakers obey
 *  these identically: retracting a line is not a break decision.
 *
 *  A KEEP NEVER EMPTIES A FRAME. A retraction that would leave the fill
 *  with nothing is dropped, because the text it moved would arrive at the
 *  next frame in exactly the state that emptied this one and the chain
 *  would never advance.
 *
 *  Returns the depth the retracted lines had occupied, which the frame's
 *  vertical distribution must not spend. */
float enforceKeeps(FontContext& fontContext, Paragraph& paragraph,
                   const std::vector<PlacedBlock>& placed,
                   float remainderMeasure, ParagraphLayout& result) {
  if (!result.overflowed() || placed.empty()) return 0;

  float depthFreed = 0;
  // Retracts every run from `runIndex` on, reporting the first word of
  // them as where this frame ran out.
  const auto retractFrom = [&](size_t runIndex) {
    if (runIndex == 0 || runIndex >= result.runs.size()) return false;
    result.firstUnplacedWord =
        std::min(result.firstUnplacedWord, result.runs[runIndex].wordIndex);
    result.runs.erase(result.runs.begin() + (long)runIndex, result.runs.end());
    result.ellipsized = false;
    return true;
  };
  // The run each of a block's last `count` lines begins at.
  const auto runStartingLastLines = [&](const PlacedBlock& entry, int count) {
    size_t runIndex = entry.endRun;
    int lines = 0;
    while (runIndex > entry.firstRun && lines < count) {
      const int line = result.runs[runIndex - 1].lineIndex;
      while (runIndex > entry.firstRun &&
             result.runs[runIndex - 1].lineIndex == line)
        --runIndex;
      ++lines;
    }
    return runIndex;
  };

  size_t entryIndex = placed.size() - 1;
  const PlacedBlock& last = placed[entryIndex];
  const KeepOptions& keep = last.block->style.keep;
  const bool split = result.firstUnplacedWord < last.block->endWord &&
                     result.firstUnplacedWord > last.block->firstWord;

  int retractLines = 0;
  bool wholeBlock = false;
  if (split) {
    if (keep.allLinesTogether) {
      wholeBlock = true;
    } else if (keep.orphanLines > 0 && last.lines < keep.orphanLines) {
      wholeBlock = true;
    } else if (keep.widowLines > 0) {
      const int carried = remainderLines(
          fontContext, paragraph, result.firstUnplacedWord, last.block->endWord,
          remainderMeasure, keep.widowLines);
      retractLines = keep.widowLines - carried;
      // Every line pulled back out of this frame is a line the next frame
      // gains, so what the block keeps here must still satisfy its own
      // orphan rule; when it cannot, the block goes over whole.
      if (retractLines > 0 &&
          last.lines - retractLines < std::max(keep.orphanLines, 1))
        wholeBlock = true;
    }
  } else if (keep.withNext && last.block->endWord <= result.firstUnplacedWord) {
    // The block ended in this frame and the one after it begins in the
    // next: the pair the caller asked to keep together is exactly the pair
    // the boundary fell between.
    wholeBlock = true;
  }

  // Whole-block retractions cascade backwards: a block that leaves takes
  // the frame's boundary with it, and the block before it that asked to
  // keep with the next now sits against that boundary.
  while (wholeBlock || retractLines > 0) {
    const PlacedBlock& entry = placed[entryIndex];
    const int lines =
        wholeBlock ? entry.lines : std::min(retractLines, entry.lines);
    if (!retractFrom(wholeBlock ? entry.firstRun
                                : runStartingLastLines(entry, lines)))
      break;
    depthFreed += entry.block->pitch * static_cast<float>(lines);
    if (!wholeBlock) break;
    depthFreed += entry.block->lead;
    if (entryIndex == 0) break;
    --entryIndex;
    if (!placed[entryIndex].block->style.keep.withNext) break;
  }
  return depthFreed;
}

/** WHERE THE FLOW'S FIRST BAND BEGINS, from the first-baseline rule: the
 *  rule names where baseline 0 sits below the flow's near edge, and every
 *  later baseline follows at its own block's pitch, so seating the passage
 *  is one number applied once. */
float firstBandStart(const FrameOptions& frame, const Paragraph::Strut& strut,
                     float ascent, float pitch) {
  float baseline = 0;
  switch (frame.firstBaseline) {
    case FrameOptions::FirstBaseline::kAscent:
      baseline = ascent + frame.firstBaselineOffset;
      break;
    case FrameOptions::FirstBaseline::kCapHeight:
      baseline = strut.capHeight + frame.firstBaselineOffset;
      break;
    case FrameOptions::FirstBaseline::kXHeight:
      baseline = strut.xHeight + frame.firstBaselineOffset;
      break;
    case FrameOptions::FirstBaseline::kLeading:
      baseline = pitch + frame.firstBaselineOffset;
      break;
    case FrameOptions::FirstBaseline::kFixed:
      baseline = frame.firstBaselineOffset;
      break;
  }
  return baseline - ascent;
}

/** Spends the room a frame has left over on the lines it holds: nothing,
 *  half above, all above, or spread between them as extra leading.
 *
 *  A pure translation along the stacking axis, applied to the placed runs
 *  and to the intervals they report landing on, so a caller re-placing a
 *  run reads the geometry the glyphs are actually on. A run whose glyphs
 *  were baked per-glyph — a path or a rotated column — carries its
 *  placement inside its blob and cannot be moved, so a flow holding one is
 *  left where it stands. */
void distributeInFrame(const FrameOptions& frame, float usedDepth,
                       int lineCount, ParagraphLayout& layout) {
  if (frame.extent <= 0 || layout.runs.empty() || layout.intervals.empty())
    return;
  if (frame.distribute == FrameOptions::Distribute::kStart) return;
  const float leftover = frame.extent - usedDepth;
  if (leftover <= 0) return;
  for (const PositionedRun& run : layout.runs)
    if (run.transformed) return;
  const SkVector direction = layout.intervals.front().direction;
  SkVector stack{0, 1};  // lines stack down the page
  if (direction.x() == 0 && direction.y() == 1)
    stack = {-1, 0};  // columns advance right to left
  else if (direction.x() != 1 || direction.y() != 0)
    return;  // neither a line nor a column: nothing to distribute along

  float shift = 0;
  float perLine = 0;
  switch (frame.distribute) {
    case FrameOptions::Distribute::kStart:
      return;
    case FrameOptions::Distribute::kCenter:
      shift = leftover * 0.5f;
      break;
    case FrameOptions::Distribute::kEnd:
      shift = leftover;
      break;
    case FrameOptions::Distribute::kJustify:
      if (lineCount < 2) return;
      perLine = leftover / static_cast<float>(lineCount - 1);
      if (frame.maximumInterlineSpacing > 0)
        perLine = std::min(perLine, frame.maximumInterlineSpacing);
      break;
  }
  const auto move = [&](SkPoint& point, int lineIndex) {
    const float distance =
        shift + perLine * static_cast<float>(std::max(lineIndex, 0));
    point += SkVector{stack.x() * distance, stack.y() * distance};
  };
  for (PositionedRun& run : layout.runs) move(run.origin, run.lineIndex);
  for (size_t index = 0; index < layout.intervals.size(); ++index)
    move(layout.intervals[index].origin, static_cast<int>(index));
}

}  // namespace detail

}  // namespace sigil::weave
