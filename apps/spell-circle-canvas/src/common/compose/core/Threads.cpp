/** @file
 * The frame chain: a story threaded from one frame to the next — how many
 * lines each takes, the run that fills it, and the balance pass that
 * evens the last two.
 */

#include <algorithm>
#include <boost/container/flat_set.hpp>
#include <cmath>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

/** ONE STORY THROUGH AS MANY FRAMES AS IT WAS GIVEN.
 *
 *  Every other derived thing borrows a rect or a path from a node's BOX;
 *  a frame borrows where the frame before it STOPPED, which is an answer
 *  of that frame's own text layout rather than of its geometry. So the
 *  chain is walked here, in chain order, and each frame is laid out again
 *  at the measure it already resolved to before the next one is asked
 *  what it inherits — a chain of any length therefore settles inside this
 *  one pass, rather than one link per convergence round.
 *
 *  A CHAIN'S HEAD is a frame nothing threads into. A frame that threads
 *  into itself, or into a cycle, is dropped at the point the walk revisits
 *  it, which is the same rule the borrow family applies to a cyclic key. */
bool Composer::Impl::resolveThreads() {
  if (threadedInstances.empty()) return false;
  bool moved = false;
  // Which frames are somebody's target: the rest are chain heads.
  boost::container::flat_set<const Instance*> threadedInto;
  std::vector<Instance*> targets;
  for (Instance* inst : threadedInstances) {
    auto found = byKey.find(inst->description->textData->threadTo);
    if (found == byKey.end()) continue;
    if (threadedInto.insert(found->second).second)
      targets.push_back(found->second);
  }
  // A FRAME IS BOUNDED BY ITS OWN DEPTH, and the last link of a chain is a
  // frame although it threads nowhere: what it cannot hold has nowhere to
  // go, and unbounded it would draw past its box instead of running out
  // and taking the marker the leaf asked for. Only the walk knows which
  // leaf that is, so the fact is written onto the instance here — and off
  // again where a chain no longer reaches, which is what the kept list is
  // for.
  const auto bound = [&](Instance* frame, bool inChain) {
    if (frame->threadedInto == inChain) return;
    frame->threadedInto = inChain;
    frame->contentRev++;
    if (frame->yoga) YGNodeMarkDirty(frame->yoga);
    moved = true;
  };
  for (Instance* stale : threadTargets)
    if (!threadedInto.count(stale)) bound(stale, false);
  for (Instance* target : targets) bound(target, true);
  threadTargets = std::move(targets);
  boost::container::flat_set<const Instance*> visited;
  for (Instance* head : threadedInstances) {
    if (threadedInto.count(head)) continue;  // not a head
    uint32_t cursor = 0;
    // The story numbers its own lines, so each frame is told where in that
    // numbering its first line stands. Words, characters, sentences and
    // named runs are already the story's — every frame builds the whole
    // story's paragraph and resumes at a word — and the line is the one
    // address that was the frame's rather than the story's.
    uint32_t lineOffset = 0;
    std::vector<Instance*> chain;
    for (Instance* frame = head; frame;) {
      if (!visited.insert(frame).second)
        break;  // a cycle: stop where it closes
      const detail::TextData* text =
          frame->description && frame->description->textData
              ? &*frame->description->textData
              : nullptr;
      Instance* next = nullptr;
      if (text && !text->threadTo.empty()) {
        auto found = byKey.find(text->threadTo);
        if (found != byKey.end()) next = found->second;
      }
      // THE MEASURE THE NEXT FRAME SETS IN, for the widow rule: the lines
      // it counts are the remainder, and the remainder is what the next
      // frame will hold. Read off the box that frame resolved to on the
      // pass before this one — 0 the first time round, which is weave's
      // "not known" and leaves the count at this frame's own measure.
      // A frame that has not been laid out reports a measure that is not a
      // number; 0 is what weave reads as "not known", so the guard
      // compares what will be STORED and not the raw answer — comparing
      // the raw one is never equal, and the chain would report movement
      // every round until the convergence budget ran out.
      const float rawMeasure = next ? instanceRect(*next).width() : 0.0f;
      const float nextMeasure =
          std::isfinite(rawMeasure) && rawMeasure > 0 ? rawMeasure : 0.0f;
      if (frame->threadCursor != cursor ||
          frame->threadLineOffset != lineOffset ||
          frame->threadNextMeasure != nextMeasure) {
        frame->threadCursor = cursor;
        frame->threadLineOffset = lineOffset;
        frame->threadNextMeasure = nextMeasure;
        frame->contentRev++;
        if (frame->yoga) YGNodeMarkDirty(frame->yoga);
        moved = true;
      }
      // Re-fill at the box this frame RESOLVED to, so the next link reads
      // a remainder that belongs to this cursor. The box and not the
      // measure: a frame is bounded by its own depth, and the depth is an
      // answer of the layout that just ran.
      const SkRect box = instanceRect(*frame);
      if (box.isFinite() && box.width() > 0)
        layoutText(*frame, box.width(), box.height());
      cursor =
          frame->textLayout.overflowed()
              ? frame->textLayout.firstUnplacedWord
              : (frame->paragraph ? (uint32_t)frame->paragraph->words().size()
                                  : cursor);
      lineOffset += (uint32_t)std::max(frame->textLayout.lineCount, 0);
      chain.push_back(frame);
      frame = next;
    }
    // The story's own line count, which only the finished walk knows and
    // every frame of the chain needs: a cascade numbered over the story
    // spans the story's units, not the ones this frame happened to hold.
    for (Instance* link : chain) link->threadStoryLines = lineOffset;
    moved |= balanceRuns(chain);
  }
  return moved;
}

/** THE CHAIN FILLED FROM @p first AT ONE DEPTH: every frame of the run
 *  re-laid out at its own measure and this depth, each resuming where the
 *  one before it stopped.
 *
 *  It answers with how many lines the run placed and whether the last of
 *  them still had something left over — the two facts a depth is judged
 *  by — and it leaves the run laid out at that depth, so the caller ends
 *  by filling at the depth it chose. */
Composer::Impl::ChainFill Composer::Impl::fillRun(
    const std::vector<Instance*>& run, size_t first, size_t last, float depth,
    uint32_t cursor) {
  ChainFill filled;
  // A frame with no box to fill holds nothing, so a run that ENDS on one
  // has not been shown to hold what it was asked to hold: the verdict is
  // the last filled frame's, and a skipped tail overrides it.
  bool skippedTail = false;
  for (size_t i = first; i < last; ++i) {
    Instance* frame = run[i];
    const SkRect box = instanceRect(*frame);
    if (!box.isFinite() || box.width() <= 0) {
      skippedTail = true;
      continue;
    }
    skippedTail = false;
    frame->threadCursor = cursor;
    layoutText(*frame, box.width(), depth);
    filled.lines += (uint32_t)std::max(frame->textLayout.lineCount, 0);
    filled.overflowed = frame->textLayout.overflowed();
    cursor =
        filled.overflowed
            ? frame->textLayout.firstUnplacedWord
            : (frame->paragraph ? (uint32_t)frame->paragraph->words().size()
                                : cursor);
  }
  if (skippedTail) filled.overflowed = true;
  filled.cursor = cursor;
  return filled;
}

/** EVERY BALANCED RUN OF ONE CHAIN, SHORTENED TO WHAT IT HOLDS.
 *
 *  A run opens at a frame that declares it and closes before the next one
 *  that does. Its depth is found by halving: the frames' declared depth is
 *  the ceiling, nothing is the floor, and each trial fills the run again
 *  and asks whether it still holds what it was asked to hold — all of the
 *  story, or the story down to a stated line. The shallowest depth that
 *  does is the answer, and every frame of the run resolves to it.
 *
 *  A FIXED NUMBER OF HALVINGS rather than the exact turnover, so the
 *  answer is a hair deeper than the tightest one and the cost is bounded
 *  whatever the story is. The ceiling is the DECLARED depth and never the
 *  resolved one: read the resolved depth and each round would halve what
 *  the round before it chose, and the columns would close on nothing. */
bool Composer::Impl::balanceRuns(const std::vector<Instance*>& chain) {
  bool moved = false;
  for (size_t first = 0; first < chain.size(); ++first) {
    const detail::TextData* opens =
        chain[first]->description && chain[first]->description->textData
            ? &*chain[first]->description->textData
            : nullptr;
    if (!opens || !opens->balanceChain) continue;
    size_t last = first + 1;
    while (last < chain.size()) {
      const detail::TextData* text =
          chain[last]->description && chain[last]->description->textData
              ? &*chain[last]->description->textData
              : nullptr;
      if (text && text->balanceChain) break;
      ++last;
    }
    const Dim declared = chain[first]->description->layout.height;
    if (declared.unit != Dim::Unit::Px || declared.value <= 0) continue;
    const uint32_t cursor = chain[first]->threadCursor;
    const uint32_t through = opens->balanceThroughLine;
    // The line the run is asked to reach is the STORY's, so what the run
    // itself placed is counted from where the run starts in that
    // numbering — which is the one thing that lets a second run be asked
    // for a line number and not for a count of its own.
    const uint32_t opensAt = chain[first]->threadLineOffset;
    const auto holds = [&](float depth) {
      const ChainFill filled = fillRun(chain, first, last, depth, cursor);
      return through == ~0u ? !filled.overflowed
                            : opensAt + filled.lines > through;
    };
    float tooShallow = 0.0f;
    float deepEnough = declared.value;
    if (holds(deepEnough))
      for (int step = 0; step < kBalanceSteps; ++step) {
        const float trial = (tooShallow + deepEnough) * 0.5f;
        if (holds(trial))
          deepEnough = trial;
        else
          tooShallow = trial;
      }
    const ChainFill settled = fillRun(chain, first, last, deepEnough, cursor);
    // WHERE THE RUN STOPPED IS WHERE THE NEXT ONE STARTS: balancing moved
    // the boundary, and a frame below it that kept the pre-balance cursor
    // would resume at the wrong word.
    if (last < chain.size() && chain[last]->threadCursor != settled.cursor) {
      chain[last]->threadCursor = settled.cursor;
      chain[last]->contentRev++;
      if (chain[last]->yoga) YGNodeMarkDirty(chain[last]->yoga);
      moved = true;
    }
    for (size_t i = first; i < last; ++i) {
      if (!chain[i]->yoga) continue;
      if (std::abs(instanceRect(*chain[i]).height() - deepEnough) > 0.25f)
        moved = true;
      YGNodeStyleSetHeight(chain[i]->yoga, deepEnough);
    }
    first = last - 1;
  }
  return moved;
}

}  // namespace sigil::compose
