/** @file
 * layoutParagraph itself: the pass that resolves the blocks, wraps the
 * geometry in whatever the options ask of it, hands each block to a
 * breaker, enforces the keeps and seats the result in its frame — and
 * layoutSingleLine, the one-line entry beside it.
 */

#include <include/core/SkTextBlob.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

#include "ParagraphLayoutInternal.h"
#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/layout/ParagraphLayout.h"

namespace sigil::weave {

namespace detail {

/// Clamps any FlowGeometry to its first `maxLines` lines
/// (OverflowOptions::maxLines): geometry "exhausts" at the limit, so the
/// existing overflow reporting and ellipsis machinery handle the rest with
/// zero breaker changes, for greedy and Knuth-Plass alike.
class LineLimitedGeometry final : public FlowGeometry {
 public:
  LineLimitedGeometry(FlowGeometry& inner, int maxLines)
      : m_inner(inner), m_maxLines(maxLines) {}

  using FlowGeometry::lineIntervals;
  bool lineIntervals(const LineRequest& request,
                     std::vector<LineInterval>& intervals) override {
    return request.index < m_maxLines &&
           m_inner.lineIntervals(request, intervals);
  }
  bool uniformIntervals() const override { return m_inner.uniformIntervals(); }

 private:
  FlowGeometry& m_inner;
  int m_maxLines;
};

}  // namespace detail

ParagraphLayout layoutParagraph(FontContext& fontContext, Paragraph& paragraph,
                                FlowGeometry& geometry,
                                const ParagraphLayoutOptions& options,
                                uint32_t firstWord) {
  using namespace detail;

  LineLimitedGeometry clampedGeometry(geometry, options.overflow.maxLines);
  FlowGeometry* effectiveGeometry =
      options.overflow.maxLines > 0
          ? static_cast<FlowGeometry*>(&clampedGeometry)
          : &geometry;

  // Whether a soft hyphen is a break opportunity is decided during
  // segmentation, so the option reaches the paragraph before it analyzes;
  // disabled, the two halves fuse into one unbreakable word. Where else
  // inside a word a break may fall is the same kind of fact and reaches it
  // the same way. Setting either to what the paragraph already holds is
  // free.
  paragraph.setSoftHyphenBreaks(options.hyphenation.enabled);
  paragraph.setHyphenator(options.hyphenation.patterns,
                          options.hyphenation.limits);
  paragraph.setKinsoku(options.kinsoku);

  // Segmentation only; the breakers pull HarfBuzz shaping just ahead of
  // their own frontier, so text past the geometry never shapes at all.
  paragraph.ensureAnalyzed(fontContext);

  ParagraphLayout result;
  const std::vector<Word>& words = paragraph.words();
  if (words.empty()) return result;
  // THE RUNS ARE THE ONE THING THIS FUNCTION GROWS WITHOUT BOUND, and a
  // run carries two reference-counted handles, so every doubling moves
  // every run already placed one by one and asks the allocator for a
  // block twice the size of the one it releases. A text sets at least one
  // run per word it places, so the words from the cursor on are the count
  // to ask for. A text longer than its geometry asks for more than it
  // will place, which costs it nothing: the pages a reservation never
  // writes to are never faulted in.
  result.runs.reserve(words.size() - std::min<size_t>(firstWord, words.size()));

  // The room a mojikumi table and tsume put after each word, resolved once
  // for the whole text and read by both breakers and by placement. Empty,
  // and free, for a layout that asked for neither.
  static thread_local std::vector<float> mojikumiRoom;
  resolveMojikumi(paragraph, options, mojikumiRoom);

  // The settings the blocks are set under, one per run of blocks that
  // resolves alike. Declared before the blocks so that it outlives them,
  // because every block points at one of these or at `options` itself.
  std::deque<ParagraphLayoutOptions> blockSettings;
  std::vector<Block> blocks =
      resolveBlocks(fontContext, paragraph, options, blockSettings);
  for (Block& block : blocks) block.mojikumiAfter = mojikumiRoom;
  // RESUMING: the blocks are numbered from the start of the text, so a
  // frame in the middle of a chain reads the same style for the same
  // block. What changes is where the fill begins — the blocks already
  // placed are dropped and the one the cursor sits in starts at the
  // cursor.
  bool openingBlockResumed = false;
  if (firstWord > 0) {
    size_t firstBlock = 0;
    while (firstBlock < blocks.size() &&
           blocks[firstBlock].endWord <= firstWord)
      ++firstBlock;
    if (firstBlock >= blocks.size()) return result;
    blocks.erase(blocks.begin(), blocks.begin() + (long)firstBlock);
    const uint32_t blockStart = blocks.front().firstWord;
    blocks.front().firstWord = std::max(blocks.front().firstWord, firstWord);
    openingBlockResumed = blocks.front().firstWord != blockStart;
    // A block resumed part-way opens no air of its own: the gap it asked
    // for was spent where it began, in the frame before this one.
    blocks.front().lead = 0;
  }
  const Paragraph::Strut strut = paragraph.strutAt(
      fontContext, blocks.front().firstWord < words.size()
                       ? words[blocks.front().firstWord].textBegin
                       : 0);

  // THE INITIAL LETTER, resolved before a line is asked for, because the
  // notch it cuts is part of the geometry every line is broken against.
  // One per pass: a block whose opening a frame before this one already
  // set is resumed and never re-opened, and the initial belongs to the
  // frame the block began in.
  InitialLetterPlan initialPlan;
  for (size_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
    const Block& candidate = blocks[blockIndex];
    if (candidate.style.initial.lines <= 0) continue;
    if (blockIndex == 0 && openingBlockResumed) break;
    if (candidate.firstWord >= words.size()) break;
    initialPlan = planInitialLetter(
        fontContext, paragraph, candidate,
        paragraph.strutAt(fontContext, words[candidate.firstWord].textBegin));
    break;
  }
  std::optional<InitialLetterGeometry> initialGeometry;
  if (initialPlan.active()) {
    initialGeometry.emplace(*effectiveGeometry, initialPlan);
    effectiveGeometry = &*initialGeometry;
    // The initial took the head of its block's opening word, so the fill
    // starts past it; what is left of that word is the initial's to place.
    for (Block& initialBlock : blocks)
      if (initialBlock.index == initialPlan.blockIndex)
        initialBlock.firstWord = initialPlan.wordIndex + 1;
  }

  IntervalSequence intervalSequence(
      *effectiveGeometry, blocks.front().pitch, blocks.front().ascent,
      options.lineBreakStrategy == LineBreakStrategy::kKnuthPlass
          ? options.knuthPlass.minimumIntervalWidth
          : 0.0f);
  const float firstBand = firstBandStart(
      options.frame, strut, blocks.front().ascent, blocks.front().pitch);
  intervalSequence.seatFirstBand(firstBand);

  // The geometry a caller needs to re-place a transformed run at draw time:
  // the intervals the layout actually consumed, in the numbering the runs
  // report, plus the snapping the placement used. Recorded on the way out,
  // because "which interval" is only meaningful next to the interval list
  // it indexes.
  const auto recordGeometry = [&](ParagraphLayout& layout) {
    layout.tangentRotationSteps = options.pathText.tangentRotationSteps;
    layout.linePitch = blocks.front().pitch;
    layout.intervals.reserve(intervalSequence.flattened().size());
    for (const FlatInterval& flat : intervalSequence.flattened())
      layout.intervals.push_back(flat.interval);
  };

  const bool optimizing =
      options.lineBreakStrategy == LineBreakStrategy::kKnuthPlass;
  size_t nextInterval = 0;
  int lastLineUsed = -1;
  std::vector<PlacedBlock> placedBlocks;
  // The cheapened copies a degraded frame is set from, and the settings
  // they are set under — a block that degrades is set from ITS copy, and
  // everything downstream reads the setting the lines were actually made
  // under. A degraded block is the one block that does not share its
  // setting with anything, because what it drops is decided while the frame
  // is being set rather than when the styles were resolved; the settings
  // stand before the blocks so that they outlive them. Deques because a
  // record points at one: they allocate nothing until a block degrades, and
  // never move what they hold.
  std::deque<ParagraphLayoutOptions> cheapSettings;
  std::deque<Block> cheapBlocks;
  float lastMeasure = 0;
  for (size_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
    const Block& block = blocks[blockIndex];
    if (block.firstWord >= block.endWord) {
      // A block whose whole opening the initial took sets no line of its
      // own. Its band is still asked for, here where the fill has reached
      // it, so the initial stands where its block begins rather than at
      // the head of the frame.
      if (initialGeometry && !initialGeometry->seated() &&
          block.index == initialPlan.blockIndex) {
        intervalSequence.openBlock(block.index, block.pitch, block.ascent,
                                   block.lead, block.gridStep,
                                   block.style.indent);
        intervalSequence.intervalAt(nextInterval);
      }
      continue;
    }
    // A block that must start a frame ends one it did not start: the fill
    // stops here and the block arrives at the head of the next.
    if (block.style.keep.startInNextFrame && !result.runs.empty()) {
      result.firstUnplacedWord = block.firstWord;
      break;
    }
    intervalSequence.openBlock(block.index, block.pitch, block.ascent,
                               block.lead, block.gridStep, block.style.indent);
    intervalSequence.setUniformBlocks(block.style.indent.firstLine == 0 &&
                                      block.style.indent.lastLine == 0);
    uint32_t overflowWord = ~0u;
    size_t lastIntervalUsed = SIZE_MAX;
    const size_t firstRun = result.runs.size();
    bool outOfBudget = false;
    // A block whose lines this thread has already decided the ends of, for
    // these words at this measure under this setting, is placed from that
    // decision: deciding is the expensive half of composing a paragraph and
    // a moving text asks for the same decision frame after frame.
    const BreakList* kept = nullptr;
    if (optimizing && options.live && intervalSequence.uniform()) {
      const FlatInterval* first = intervalSequence.intervalAt(nextInterval);
      if (first)
        kept = breakStore().find(BreakKey{
            paragraph.identity(), paragraph.wordRevision(), block.firstWord,
            block.endWord, quantisedMeasure(first->interval.length),
            breakSetting(block)});
    }
    if (kept) {
      ++result.reusedBlocks;
      placeBreaks(fontContext, paragraph, intervalSequence, block, *kept,
                  result, lastIntervalUsed, overflowWord);
    } else if (optimizing)
      knuthPlassBlock(fontContext, paragraph, intervalSequence, block,
                      nextInterval, result, lastIntervalUsed, overflowWord,
                      outOfBudget);
    // WHAT A DEGRADE ACTUALLY DROPS. The composer ran out of budget on
    // this block, so the frame is set greedily rather than late — and
    // greedily means the whole setting, not the breaker alone. The
    // controls that cost a frame something go with it: the hyphens (a
    // break the greedy fitter would have to reserve room for and then
    // weigh), the justification passes past the word gaps (letter spacing
    // and glyph scaling, both a second and third fitting of every line),
    // and the widow rule (the one keep that has to count lines the frame
    // cannot see, which means shaping past its own end). The keeps that
    // cost nothing — orphans, keep-with-next, all-lines-together — are
    // enforced as they always are. Everything is back the next frame the
    // budget is met.
    const Block* setFrom = &block;
    if (outOfBudget) {
      ++result.degradedBlocks;
      overflowWord = ~0u;
      cheapBlocks.push_back(block);
      Block& cheap = cheapBlocks.back();
      ParagraphLayoutOptions& cheapened =
          cheapSettings.emplace_back(*block.options);
      cheap.options = &cheapened;
      cheapened.hyphenation.enabled = false;
      JustificationOptions& justification = cheapened.justification;
      justification.letterSpacingMinimum = justification.letterSpacing;
      justification.letterSpacingMaximum = justification.letterSpacing;
      justification.glyphScaleMinimum = justification.glyphScale;
      justification.glyphScaleMaximum = justification.glyphScale;
      cheap.style.keep.widowLines = 0;
      setFrom = &cheap;
    }
    if ((!optimizing && !kept) || outOfBudget)
      lastIntervalUsed =
          greedyBlock(fontContext, paragraph, intervalSequence, *setFrom,
                      nextInterval, result, overflowWord);
    if (result.runs.size() > firstRun) {
      PlacedBlock entry{setFrom, firstRun, result.runs.size(), 1};
      for (size_t runIndex = firstRun + 1; runIndex < result.runs.size();
           ++runIndex)
        if (result.runs[runIndex].lineIndex !=
            result.runs[runIndex - 1].lineIndex)
          ++entry.lines;
      placedBlocks.push_back(entry);
    }
    if (lastIntervalUsed != SIZE_MAX) {
      const FlatInterval* used = intervalSequence.intervalAt(lastIntervalUsed);
      if (used) {
        lastLineUsed = std::max(lastLineUsed, used->sourceLineIndex);
        lastMeasure = used->interval.length;
      }
      // A block never shares a band with the one before it: whatever is
      // left of the line this block ended on belongs to no one.
      nextInterval = intervalSequence.pastSourceLine(lastIntervalUsed);
    }
    if (overflowWord != ~0u || (lastIntervalUsed == SIZE_MAX &&
                                !intervalSequence.intervalAt(nextInterval))) {
      result.firstUnplacedWord =
          overflowWord != ~0u ? overflowWord : block.firstWord;
      break;
    }
  }

  const float depthFreed = enforceKeeps(
      fontContext, paragraph, placedBlocks,
      options.nextMeasure > 0 ? options.nextMeasure : lastMeasure, result);
  result.lineCount = lastLineUsed + 1;
  if (depthFreed > 0) {
    int highestLine = -1;
    for (const PositionedRun& run : result.runs)
      highestLine = std::max(highestLine, run.lineIndex);
    result.lineCount = std::min(result.lineCount, highestLine + 1);
  }
  if (!options.overflow.ellipsis.empty() && result.overflowed())
    applyEllipsis(fontContext, paragraph, intervalSequence, options, result);
  if (initialGeometry) {
    // A block whose whole opening the initial took places no line of its
    // own, so the band it stands on has to be asked for outright.
    if (!initialGeometry->seated()) {
      static thread_local std::vector<LineInterval> seatScratch;
      initialGeometry->lineIntervals(
          LineRequest{0, firstBand, initialPlan.pitch, initialPlan.ascent,
                      initialPlan.blockIndex, 0},
          seatScratch);
    }
    placeInitialLetter(initialPlan, *initialGeometry, result);
  }
  recordGeometry(result);
  distributeInFrame(options.frame, intervalSequence.bandCursor() - depthFreed,
                    result.lineCount, result);
  return result;
}

ParagraphLayout layoutSingleLine(FontContext& fontContext, Paragraph& paragraph,
                                 SkPoint baselineOrigin,
                                 const PathTextOptions& pathText) {
  const float availableWidth = paragraph.naturalWidth(fontContext) + 1.0f;
  LineSetFlow singleLineFlow({{{baselineOrigin, {1, 0}, availableWidth}}});
  ParagraphLayoutOptions options;
  options.pathText = pathText;
  return layoutParagraph(fontContext, paragraph, singleLineFlow, options);
}

}  // namespace sigil::weave
