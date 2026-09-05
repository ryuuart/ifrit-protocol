/** @file
 * Optimal line breaking over a sequence of intervals: TeX's demerits with
 * saturating badness, a lifeline break when no feasible one survives, an
 * emergency rerun with each line's own width as stretch, and path merging
 * on uniform geometry that keeps the active list bounded by the measure.
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

#include "ParagraphLayoutInternal.h"
#include "sigilweave/layout/ParagraphLayout.h"

namespace sigil::weave {
namespace detail {

namespace {

constexpr float kInfDemerits = std::numeric_limits<float>::max() / 4;
constexpr float kLinePenalty = 10.0f;
// Badness saturates (TeX's INF_BAD, scaled): a stretch-free underfull line
// is terrible but must stay finite, or demerits overflow to inf and poison
// every surviving path — which once dropped whole paragraphs on narrow,
// hyphen-heavy measures.
constexpr float kMaxBadness = 1e7f;

struct Node {
  uint32_t breakAt = 0;   // line starts at word `breakAt`
  uint32_t interval = 0;  // interval index the *next* line will occupy
  float demerits = 0;
  int32_t previousNode = -1;  // arena index of the predecessor node
};

// Folds one more value into a running hash.
template <typename T>
void foldInto(uint64_t& hash, const T& value) {
  static_assert(std::is_trivially_copyable_v<T>);
  const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
  for (size_t index = 0; index < sizeof(T); ++index) {
    hash ^= bytes[index];
    hash *= 0x100000001B3ull;  // FNV-1a
  }
}

}  // namespace

BreakStore& breakStore() {
  static thread_local BreakStore store;
  return store;
}

uint64_t breakSetting(const Block& block) {
  const ParagraphLayoutOptions& options = *block.options;
  uint64_t hash = 0xCBF29CE484222325ull;
  foldInto(hash, options.alignment);
  foldInto(hash, options.justification.wordSpacing);
  foldInto(hash, options.justification.spaceStretch);
  foldInto(hash, options.justification.spaceShrink);
  foldInto(hash, options.justification.justifyLastLine);
  foldInto(hash, options.justification.lastLineAlignment);
  foldInto(hash, options.justification.expandIdeographicGaps);
  foldInto(hash, options.hyphenation.enabled);
  foldInto(hash, options.hyphenation.penalty);
  foldInto(hash, options.hyphenation.consecutiveLimit);
  foldInto(hash, options.hyphenation.zone);
  foldInto(hash, options.hyphenation.lastWordOfBlock);
  foldInto(hash, options.knuthPlass.tolerance);
  foldInto(hash, options.knuthPlass.minimumIntervalWidth);
  foldInto(hash, options.tabStops.interval);
  for (const TabStop& stop : options.tabStops.stops) {
    foldInto(hash, stop.position);
    foldInto(hash, stop.align);
    foldInto(hash, stop.alignOn);
  }
  foldInto(hash, block.style.balanceRaggedLines);
  foldInto(hash, block.style.indent.lastLine);
  return hash;
}

int32_t quantisedMeasure(float measure) {
  // THE WHOLE PIXEL BELOW THE MEASURE, never the one above: a line broken
  // to fit a narrower measure than it is set in cannot overflow the one it
  // is set in. A width animating across a range therefore decides its
  // breaks once per pixel it crosses, and the fraction of a pixel it is
  // between two of them is spent where the alignment spends it.
  return static_cast<int32_t>(std::floor(measure));
}

// Classic Knuth-Plass optimal line breaking over word boxes and glue, with
// per-line (per-interval) widths from the flow geometry. Two robustness
// twists borrowed from TeX:
//  - when no feasible break survives at some boundary, the least-bad
//    candidate is force-accepted (loose before overfull) so the breaker
//    always terminates with a full layout;
//  - if that lifeline ever had to accept an *overfull* line — text past the
//    measure — the whole pass is redone with \emergencystretch (each line's
//    own width added to its stretchability), which turns loose lines into
//    real break nodes so overfull is never forced unless a single box is
//    wider than the measure.
// And one of scale: paths stop dead when the geometry runs out, and the
// prefix sums fill lazily behind the DP frontier, so a paragraph that
// vastly overflows its flow costs what *fits*, not its total length —
// otherwise every relayout of a huge paragraph in a small box would break
// thousands of lines only for placement to discard them.
void knuthPlassBlock(FontContext& fontContext, Paragraph& paragraph,
                     IntervalSequence& intervalSequence, const Block& block,
                     size_t firstInterval, ParagraphLayout& result,
                     size_t& lastIntervalUsed, uint32_t& overflowWord,
                     bool& outOfBudget) {
  const ParagraphLayoutOptions& options = *block.options;
  using Clock = std::chrono::steady_clock;
  outOfBudget = false;
  // The moment this block must be composed by, when a budget was set.
  std::optional<Clock::time_point> budgetExpiry;
  if (options.knuthPlass.budgetMicroseconds > 0)
    budgetExpiry =
        Clock::now() + std::chrono::microseconds(static_cast<int64_t>(
                           options.knuthPlass.budgetMicroseconds));
  const std::vector<Word>& words = paragraph.words();
  const uint32_t base = block.firstWord;
  const uint32_t wordCount = block.endWord;
  // HOW OFTEN THE BUDGET IS READ: the clock costs more than a breakpoint
  // does, so it is read at a fixed FRACTION of the block rather than every
  // so many breakpoints. A fixed count reads it zero times on every block
  // shorter than that count — which is every ordinary paragraph — and a
  // budget stated over one would then bound nothing at all.
  constexpr uint32_t kBudgetChecksPerBlock = 8;
  const uint32_t budgetCheckStride = std::max(
      1u, (wordCount > base ? wordCount - base : 1u) / kBudgetChecksPerBlock);
  lastIntervalUsed = SIZE_MAX;
  if (base >= wordCount) return;
  if (!intervalSequence.intervalAt(firstInterval)) {
    overflowWord = base;
    return;
  }
  // WHAT A MOVING TEXT IS BROKEN AGAINST: its own measure, and not the
  // frame's supply of lines. A block set in a uniform measure has one
  // number for every line it could ever have, so the break decisions are a
  // function of the words and that number alone — which is what makes them
  // worth keeping between frames, and what makes a frame that changes only
  // in DEPTH change which lines it holds without moving one of them. It
  // also means the geometry is not walked at all while the breaks are
  // being decided; the lines are asked for as they are placed.
  std::optional<float> liveMeasure;
  if (options.live && intervalSequence.uniform()) {
    if (const FlatInterval* first = intervalSequence.intervalAt(firstInterval))
      liveMeasure = first->interval.length;
  }

  // BALANCED RAGGED LINES are the same optimization with one freedom
  // withdrawn: the last line of a block is scored like every other rather
  // than absorbing whatever slack is left, so the breaker has to spread the
  // words instead of dumping the remainder on the final line.
  const bool balance = block.style.balanceRaggedLines;

  // Prefix sums: content width, and glue width/stretch/shrink per gap
  // (gap i sits after word i; the last word's "gap" is never on a line).
  // Extended on demand up to the furthest boundary the DP visits.
  static thread_local std::vector<float> prefixWidth, prefixGlue, prefixStretch,
      prefixShrink;
  // Words followed by a tab gap, ascending; only filled when stops are
  // active. Tab glue is pen-dependent, so it stays out of the prefix sums —
  // lineNatural resolves it per candidate line instead.
  static thread_local std::vector<uint32_t> tabGapIndices;
  prefixWidth.assign(1, 0);
  prefixGlue.assign(1, 0);
  prefixStretch.assign(1, 0);
  prefixShrink.assign(1, 0);
  tabGapIndices.clear();
  const bool tabAware = tabStopsActive(options);
  const bool spacedByTable = !block.mojikumiAfter.empty();
  // Every prefix array is indexed from the block's first word, so a block
  // in the middle of a text costs what IT holds and not what precedes it.
  const auto atWord = [&](uint32_t wordIndex) { return wordIndex - base; };
  auto ensurePrefixSums = [&](uint32_t endWordIndex) {
    for (uint32_t wordIndex =
             base + static_cast<uint32_t>(prefixWidth.size()) - 1;
         wordIndex < endWordIndex; ++wordIndex) {
      prefixWidth.push_back(prefixWidth[atWord(wordIndex)] +
                            words[wordIndex].width);
      float glue = 0;
      float stretch = 0;
      float shrink = 0;
      if (tabAware && words[wordIndex].tabAfter) {
        // Tab gaps are rigid (columns pin to stops) and positional; the
        // width they'll actually take is resolved in lineNatural.
        tabGapIndices.push_back(wordIndex);
      } else if (words[wordIndex].spaceWidth > 0) {
        glue = words[wordIndex].spaceWidth * options.justification.wordSpacing;
        stretch = glue * options.justification.spaceStretch;
        shrink = glue * options.justification.spaceShrink;
      } else if (options.justification.expandIdeographicGaps &&
                 wordIndex + 1 < wordCount &&
                 (words[wordIndex].ideographic ||
                  words[wordIndex + 1].ideographic)) {
        const float fontSize =
            words[wordIndex].segments().empty()
                ? 16.0f
                : words[wordIndex].segments()[0].shaped->fontSize;
        stretch = fontSize * 0.25f;
        shrink = fontSize * 0.03f;
      }
      // The room a mojikumi table or tsume puts after this word is part of
      // the gap and none of it is elastic: a table states a distance and a
      // justified line spends its slack in the gaps the face gave it. A
      // layout that asked for neither answers the question once, here,
      // rather than once per word.
      if (spacedByTable) glue += mojikumiAfter(block, wordIndex);
      prefixGlue.push_back(prefixGlue[atWord(wordIndex)] + glue);
      prefixStretch.push_back(prefixStretch[atWord(wordIndex)] + stretch);
      prefixShrink.push_back(prefixShrink[atWord(wordIndex)] + shrink);
    }
  };

  // Tab gaps interior to a candidate line [lineStart, lineEnd): gap indices
  // in [lineStart, lineEnd - 1) — the break-side gap is never on the line.
  auto tabGapsInLine = [&](uint32_t lineStart, uint32_t lineEnd) {
    const auto first =
        std::lower_bound(tabGapIndices.begin(), tabGapIndices.end(), lineStart);
    const auto last = std::lower_bound(first, tabGapIndices.end(), lineEnd - 1);
    return std::span<const uint32_t>(first, last);
  };

  // WHAT THE DP READS OUT OF THE SETTING, read once. The pass below calls
  // paragraph.ensureShapedTo() as its frontier advances, so nothing the
  // compiler can see keeps the setting still: every field left inside the
  // loop is loaded again on every candidate line.
  const bool hyphenating = options.hyphenation.enabled;
  const float hyphenPenalty = options.hyphenation.penalty;
  const float tolerance = options.knuthPlass.tolerance;

  // Extra width when the line ends on a discretionary (soft-hyphen) break.
  auto hyphenWidthAt = [&](uint32_t breakIndex) -> float {
    return hyphenating &&
                   hyphenTakenAt(words, breakIndex, breakIndex == wordCount,
                                 options)
               ? words[breakIndex - 1].hyphenGlyph->advance
               : 0.0f;
  };

  // Natural width and elasticity of a line holding a half-open word range.
  // These are the DP loop's hottest calls, so they stay slim enough to
  // inline; the tab corrections live in one flat, `tabAware`-guarded block
  // at their call site instead (nesting them here de-inlines the lot; the
  // bench ledger owns the cost).
  auto lineNatural = [&](uint32_t lineStart, uint32_t lineEnd) {
    return (prefixWidth[atWord(lineEnd)] - prefixWidth[atWord(lineStart)]) +
           (prefixGlue[atWord(lineEnd - 1)] - prefixGlue[atWord(lineStart)]) +
           hyphenWidthAt(lineEnd);
  };
  auto lineStretch = [&](uint32_t lineStart, uint32_t lineEnd) {
    return prefixStretch[atWord(lineEnd - 1)] -
           prefixStretch[atWord(lineStart)];
  };
  auto lineShrink = [&](uint32_t lineStart, uint32_t lineEnd) {
    return prefixShrink[atWord(lineEnd - 1)] - prefixShrink[atWord(lineStart)];
  };
  // Natural width of a line that contains tab gaps: tab-separated segments
  // accumulate from the prefix sums (tab gaps contributed zero there); each
  // tab then jumps the pen to its stop through the same glueAfter placement
  // will use, so the breaker's width for a candidate line is exactly the
  // width it renders at.
  auto tabResolvedNatural = [&](uint32_t lineStart, uint32_t lineEnd,
                                std::span<const uint32_t> tabs) {
    float pen = 0;
    uint32_t segmentStart = lineStart;
    for (const uint32_t tabIndex : tabs) {
      pen += (prefixWidth[atWord(tabIndex + 1)] -
              prefixWidth[atWord(segmentStart)]) +
             (prefixGlue[atWord(tabIndex)] - prefixGlue[atWord(segmentStart)]);
      pen += glueAfter(words[tabIndex], pen, options);
      segmentStart = tabIndex + 1;
    }
    return pen +
           (prefixWidth[atWord(lineEnd)] - prefixWidth[atWord(segmentStart)]) +
           (prefixGlue[atWord(lineEnd - 1)] -
            prefixGlue[atWord(segmentStart)]) +
           hyphenWidthAt(lineEnd);
  };

  // Shrink is only real when placement will actually render the line
  // justified: ragged lines (and demoted last lines) render at natural
  // width, so a "feasible shrunk" break there would leak past the measure.
  const bool justify = options.alignment == TextAlignment::kJustify;
  const float zone = options.hyphenation.zone;
  const bool lastLineJustify =
      justify &&
      (options.justification.justifyLastLine ||
       options.justification.lastLineAlignment == TextAlignment::kJustify);

  // THE DP'S WORKING MEMORY, HELD BY THE THREAD RATHER THAN THE CALL.
  // The composer is meant to run on moving text, which means running every
  // frame, and a breaker that allocates four vectors per block per frame
  // spends its budget in the allocator. Cleared and refilled, never freed:
  // after the first block of the first frame the capacity is the capacity
  // the text needs.
  static thread_local std::vector<Node> arena;
  static thread_local std::vector<int32_t> active;
  static thread_local std::vector<int32_t> nextActive;
  // Pairs are (next interval index, node arena index).
  static thread_local std::vector<std::pair<uint32_t, int32_t>> newNodes;
  const bool uniform = intervalSequence.uniform();
  // THE ACTIVE LIST IS WINDOWED. A path whose line is already overfull is
  // retired where it is found, and on a uniform measure every path that
  // reached one breakpoint faces one future and merges, so the list is
  // bounded by the geometry rather than by the text. This is the floor
  // under the case neither of those bounds: a geometry of many differently
  // sized intervals, where the lowest-demerit candidates are kept and the
  // rest dropped, so one frame's work cannot grow with the paragraph.
  constexpr size_t kMaxActiveNodes = 64;

  // One full DP pass. Returns the best terminal arena index (-1 if nothing
  // could be placed), reports whether the lifeline ever had to force an
  // overfull line, and — when the geometry ran out before the text did —
  // the first word that no longer fit.
  // WHAT A BALANCED BLOCK NARROWS BY, as a fraction of each interval's own
  // length (see the search below); 1 is every interval at its full width,
  // which is what an unbalanced block is broken against. It is a FRACTION
  // and not a width because a line an exclusion cut short is already
  // shorter than the rest and taking the same number of pixels off both
  // would tighten it by more of itself: an even rag over unequal lines is
  // every line giving up the same PROPORTION. Placement never sees it —
  // a balanced line is broken short and then set in the interval it
  // actually landed in, so a centred block stays centred on the real
  // measure.
  float balanceFraction = 1.0f;

  auto runPass = [&](bool useEmergencyStretch, bool& forcedOverfull,
                     uint32_t& firstUnplacedWord) -> int32_t {
    forcedOverfull = false;
    firstUnplacedWord = ~0u;
    arena.clear();
    arena.push_back({base, static_cast<uint32_t>(firstInterval), 0, -1});
    active = {0};

    // The path that placed the most text before running out of geometry —
    // the fallback result when no path reaches the end of the paragraph.
    int32_t bestEnd = -1;
    auto considerEnd = [&](int32_t nodeIndex) {
      if (bestEnd < 0 || arena[nodeIndex].breakAt > arena[bestEnd].breakAt ||
          (arena[nodeIndex].breakAt == arena[bestEnd].breakAt &&
           arena[nodeIndex].demerits < arena[bestEnd].demerits))
        bestEnd = nodeIndex;
    };

    for (uint32_t breakIndex = base + 1;
         breakIndex <= wordCount && !active.empty(); ++breakIndex) {
      if (budgetExpiry && (breakIndex - base) % budgetCheckStride == 0 &&
          Clock::now() >= *budgetExpiry) {
        outOfBudget = true;
        return -1;
      }
      // Shape only as the dynamic-programming frontier advances.
      paragraph.ensureShapedTo(fontContext, breakIndex);
      ensurePrefixSums(breakIndex);
      // The hyphen a break here would render is a fact about the BREAK, so
      // it is settled once and not once per candidate line ending on it.
      const float breakHyphenWidth = hyphenWidthAt(breakIndex);
      // Within a block the only forced break is its end: a mandatory break
      // is what ends a block, so there is never one inside.
      const bool forcedBreak = breakIndex == wordCount;

      nextActive.clear();
      newNodes.clear();
      float bestForcedDemerits = kInfDemerits;
      bool bestForcedOverfull = true;
      Node bestForced;

      for (const int32_t nodeIndex : active) {
        const Node& node = arena[nodeIndex];
        const FlatInterval* lineInterval =
            liveMeasure ? nullptr : intervalSequence.intervalAt(node.interval);
        if (!liveMeasure && !lineInterval) {
          // No geometry left for this path's next line: it ends here.
          considerEnd(nodeIndex);
          continue;
        }
        const uint32_t lineStart = node.breakAt;
        const float measure =
            (liveMeasure ? *liveMeasure : lineInterval->interval.length) *
            balanceFraction;
        float natural = lineNatural(lineStart, breakIndex);
        float stretch = lineStretch(lineStart, breakIndex);
        float shrink = lineShrink(lineStart, breakIndex);
        if (tabAware) {
          // Tab corrections: the natural width resolves through the stops,
          // and glue at or before the line's last tab cannot move the
          // line's end (the following stop swallows it), so elasticity
          // counts only the gaps past that tab.
          const std::span<const uint32_t> tabs =
              tabGapsInLine(lineStart, breakIndex);
          if (!tabs.empty()) {
            natural = tabResolvedNatural(lineStart, breakIndex, tabs);
            stretch = prefixStretch[atWord(breakIndex - 1)] -
                      prefixStretch[atWord(tabs.back() + 1)];
            shrink = prefixShrink[atWord(breakIndex - 1)] -
                     prefixShrink[atWord(tabs.back() + 1)];
          }
        }
        stretch += useEmergencyStretch ? measure : 0.0f;

        float ratio;
        // The final arm repeats this one; the order of the tests is the point.
        // NOLINTNEXTLINE(bugprone-branch-clone)
        if (forcedBreak && !balance && natural <= measure) {
          // Paragraph-final (and hard-break-final) lines end wherever they
          // end — TeX's \parfillskip absorbs the slack for free. Without
          // this, a stretch-free underfull last line scores kMaxBadness and
          // an *overfull* candidate with a cheaper history can beat it,
          // leaking text past the measure.
          ratio = 0;
        } else if (natural < measure) {
          ratio = stretch > 0 ? (measure - natural) / stretch
                              : (natural > measure - 0.5f ? 0.0f : 1e9f);
        } else if (natural > measure) {
          const bool canShrink = forcedBreak ? lastLineJustify : justify;
          ratio =
              (canShrink && shrink > 0) ? (measure - natural) / shrink : -1e9f;
        } else {
          ratio = 0;
        }

        const bool overfull = ratio < -1.0f;
        const float clampedRatio =
            std::min(std::abs(ratio), 500.0f);  // pre-clamp: ratio³
        const float badness = std::min(
            100.0f * clampedRatio * clampedRatio * clampedRatio, kMaxBadness);
        const bool feasible = !overfull && badness <= tolerance;
        // THE HYPHENATION ZONE, asked of the line WITHOUT the break: a
        // ragged line whose last whole word already ends inside the band at
        // the measure is square enough for the eye, so a word broken to
        // reach further is a hyphen the page did not need. The break is
        // simply not a candidate — the whole-word break it competes with is
        // one the DP already holds — and it is not a lifeline either,
        // because a line that fits without it is never the least-bad
        // answer. A justified line shows its slack in the gaps rather than
        // at the edge, so the zone is a ragged-setting rule only.
        bool zoneRefusesBreak = false;
        if (zone > 0 && !justify && breakHyphenWidth > 0) {
          uint32_t whole = breakIndex - 1;
          while (whole > lineStart && words[whole - 1].hyphenBreak) --whole;
          zoneRefusesBreak = whole > lineStart &&
                             measure - lineNatural(lineStart, whole) <= zone;
        }

        float demerits =
            node.demerits + (kLinePenalty + badness) * (kLinePenalty + badness);
        if (breakHyphenWidth > 0) demerits += hyphenPenalty * hyphenPenalty;

        if (zoneRefusesBreak) {
          // Neither a node nor a lifeline.
        } else if (feasible) {
          Node candidate{breakIndex, node.interval + 1, demerits, nodeIndex};
          newNodes.emplace_back(candidate.interval, -1);
          arena.push_back(candidate);
          newNodes.back().second = static_cast<int32_t>(arena.size() - 1);
        } else {
          // Lifeline: the least-bad infeasible candidate, uniformly
          // penalized so any feasible path always beats it. A loose line
          // beats an overfull one regardless of demerits: loose looks bad,
          // but overfull leaks past the measure — only accept one when
          // nothing else exists (a single box wider than the line).
          const float penalized = demerits + 1e12f;
          const bool better = overfull == bestForcedOverfull
                                  ? penalized < bestForcedDemerits
                                  : !overfull;
          if (better) {
            bestForcedDemerits = penalized;
            bestForcedOverfull = overfull;
            bestForced =
                Node{breakIndex, node.interval + 1, penalized, nodeIndex};
          }
        }

        // A node whose line is already over-shrunk only gets worse; retire.
        if (!overfull && !forcedBreak) nextActive.push_back(nodeIndex);
      }

      if (forcedBreak) {
        // Every line must end here: only nodes at this breakpoint survive.
        nextActive.clear();
      }

      if (uniform && !newNodes.empty()) {
        // Every interval has the same width, so paths that reached this
        // breakpoint on different line numbers face identical futures
        // (TeX's model — all lines share one measure): keep the single
        // best, preferring fewer lines consumed on ties (more geometry
        // left for a bounded flow). Without this merge the active list
        // grows with the paragraph and huge fully-placed paragraphs turn
        // super-linear.
        int32_t best = newNodes[0].second;
        uint32_t bestInterval = newNodes[0].first;
        for (size_t candidateIndex = 1; candidateIndex < newNodes.size();
             ++candidateIndex) {
          const int32_t candidateNode = newNodes[candidateIndex].second;
          if (arena[candidateNode].demerits < arena[best].demerits ||
              (arena[candidateNode].demerits == arena[best].demerits &&
               newNodes[candidateIndex].first < bestInterval)) {
            best = candidateNode;
            bestInterval = newNodes[candidateIndex].first;
          }
        }
        nextActive.push_back(best);
      } else {
        // Variable geometry: a path on a different interval index faces a
        // genuinely different future, so keep the best node per interval.
        std::sort(newNodes.begin(), newNodes.end());
        for (size_t candidateIndex = 0; candidateIndex < newNodes.size();
             ++candidateIndex) {
          if (candidateIndex + 1 < newNodes.size() &&
              newNodes[candidateIndex].first ==
                  newNodes[candidateIndex + 1].first) {
            // same interval: keep the lower-demerits one
            const int32_t firstCandidateNode = newNodes[candidateIndex].second;
            const int32_t secondCandidateNode =
                newNodes[candidateIndex + 1].second;
            if (arena[firstCandidateNode].demerits <
                arena[secondCandidateNode].demerits)
              std::swap(newNodes[candidateIndex], newNodes[candidateIndex + 1]);
            continue;
          }
          nextActive.push_back(newNodes[candidateIndex].second);
        }
      }

      if (nextActive.empty() && bestForcedDemerits < kInfDemerits) {
        // No feasible breaks anywhere: force the least-bad one.
        if (bestForcedOverfull) forcedOverfull = true;
        arena.push_back(bestForced);
        nextActive.push_back(static_cast<int32_t>(arena.size() - 1));
      }
      // With no forced candidate either, every path hit the end of the
      // geometry: the loop exits on the empty active list and the partial
      // result below stands.

      if (nextActive.size() > kMaxActiveNodes) {
        std::nth_element(nextActive.begin(),
                         nextActive.begin() + (long)kMaxActiveNodes,
                         nextActive.end(), [&](int32_t left, int32_t right) {
                           return arena[left].demerits < arena[right].demerits;
                         });
        nextActive.resize(kMaxActiveNodes);
      }
      active.swap(nextActive);
    }

    int32_t best = -1;
    for (int32_t nodeIndex : active)
      if (arena[nodeIndex].breakAt == wordCount &&
          (best < 0 || arena[nodeIndex].demerits < arena[best].demerits))
        best = nodeIndex;
    if (best < 0 && bestEnd >= 0) {
      // Geometry ran out before the text did: place what fits.
      best = bestEnd;
      firstUnplacedWord = arena[bestEnd].breakAt;
    }
    return best;
  };

  // A balanced block's own break list, when the search below found one.
  BreakList balancedBreaks;
  bool forcedOverfull = false;
  uint32_t firstUnplacedWord = ~0u;
  int32_t best = runPass(false, forcedOverfull, firstUnplacedWord);
  // A block the composer could not finish inside its budget is left where
  // it stands: nothing is placed, and the caller fills it greedily for
  // this frame.
  if (outOfBudget) return;
  if (best < 0 || forcedOverfull) {
    bool stillOverfull = false;
    uint32_t retryFirstUnplacedWord = ~0u;
    const int32_t retry = runPass(true, stillOverfull, retryFirstUnplacedWord);
    if (outOfBudget) return;
    if (retry >= 0) {
      best = retry;
      firstUnplacedWord = retryFirstUnplacedWord;
    }
  }

  // ── Balancing ─────────────────────────────────────────────────────────
  //
  // A BALANCED BLOCK IS THE SAME BLOCK SET IN THE NARROWEST MEASURE THAT
  // STILL TAKES THE SAME NUMBER OF LINES. Withdrawing the last line's
  // freedom to absorb the slack is not enough on its own — the breaker
  // still fills each line as full as its demerits allow and leaves the
  // remainder wherever it falls — so the measure itself is narrowed until
  // one more step would cost a line, and the block is broken against that.
  // The lines then have nowhere to be long, which is what an even rag is.
  //
  // The search is a bisection over that narrowing, and it is bounded: a
  // fixed number of steps, each one full pass over a block that asked to be
  // balanced. The narrowing is a FRACTION of each interval's own length, so
  // a block an exclusion cut into unequal lines gives up the same
  // proportion of every one of them rather than the same number of pixels;
  // over a uniform measure — the ordinary case, and the only one a live
  // block ever has — the two are the same search. ONE APPROXIMATION
  // REMAINS: the bisection stops after a fixed number of steps rather than
  // at the exact fraction where the line count turns over, so the last
  // sliver of slack survives.
  if (balance && best >= 0 && firstUnplacedWord == ~0u) {
    const auto lineCountOf = [&](int32_t node) {
      int lines = 0;
      for (int32_t index = node; index > 0; index = arena[index].previousNode)
        ++lines;
      return lines;
    };
    const int target = lineCountOf(best);
    // A block with one line has nothing to even out, and one with no
    // measure at all has nothing to narrow.
    float anyMeasure = liveMeasure.value_or(0.0f);
    for (const FlatInterval& flat : intervalSequence.flattened())
      anyMeasure = std::max(anyMeasure, flat.interval.length);
    if (target > 1 && anyMeasure > 0) {
      // The breaks the search will keep, remembered outside the arena the
      // next pass clears.
      BreakList keptBreaks;
      const auto rememberBreaks = [&](int32_t node) {
        keptBreaks.clear();
        for (int32_t index = node; index > 0; index = arena[index].previousNode)
          keptBreaks.emplace_back(arena[index].breakAt,
                                  arena[arena[index].previousNode].interval);
        std::reverse(keptBreaks.begin(), keptBreaks.end());
      };
      constexpr int kBisectionSteps = 8;
      float tooNarrow = 0;
      float wideEnough = 1.0f;
      for (int step = 0; step < kBisectionSteps; ++step) {
        balanceFraction = (tooNarrow + wideEnough) * 0.5f;
        bool narrowedOverfull = false;
        uint32_t narrowedUnplaced = ~0u;
        const int32_t narrowed =
            runPass(false, narrowedOverfull, narrowedUnplaced);
        // A narrowing that costs a line, forces a word past the measure or
        // leaves text behind is one step too far.
        if (narrowed >= 0 && !narrowedOverfull && narrowedUnplaced == ~0u &&
            lineCountOf(narrowed) == target) {
          wideEnough = balanceFraction;
          rememberBreaks(narrowed);
        } else {
          tooNarrow = balanceFraction;
        }
      }
      balanceFraction = 1.0f;
      if (!keptBreaks.empty()) {
        // Placement below walks the arena from `best`; the balanced answer
        // is a break list of its own, so it is handed over directly.
        arena.clear();
        arena.push_back({base, static_cast<uint32_t>(firstInterval), 0, -1});
        best = 0;
        balancedBreaks = std::move(keptBreaks);
      }
    }
  }
  if (best < 0) {
    overflowWord = base;
    return;
  }
  if (firstUnplacedWord != ~0u) overflowWord = firstUnplacedWord;

  // The chain, oldest first: each entry is where a line ends and which
  // interval the line before it occupied.
  BreakList breaks = balancedBreaks;
  if (breaks.empty()) {
    for (int32_t nodeIndex = best; nodeIndex > 0;
         nodeIndex = arena[nodeIndex].previousNode)
      breaks.emplace_back(arena[nodeIndex].breakAt,
                          arena[arena[nodeIndex].previousNode].interval);
    std::reverse(breaks.begin(), breaks.end());
  }

  // ── Placement ─────────────────────────────────────────────────────────
  // The break decisions are kept for the next frame before they are spent:
  // a moving text asks the same question of the same words at the same
  // measure again and again, and this is the answer to it.
  if (liveMeasure && firstUnplacedWord == ~0u)
    breakStore().store(BreakKey{paragraph.identity(), paragraph.wordRevision(),
                                base, wordCount, quantisedMeasure(*liveMeasure),
                                breakSetting(block)},
                       breaks);
  placeBreaks(fontContext, paragraph, intervalSequence, block, breaks, result,
              lastIntervalUsed, overflowWord);
}

// One line per entry of `breaks`, in the interval each names — the half of
// composing a block that is not deciding where its lines end, and the whole
// of what a block whose breaks were already decided has left to do.
void placeBreaks(FontContext& fontContext, Paragraph& paragraph,
                 IntervalSequence& intervalSequence, const Block& block,
                 const BreakList& breaks, ParagraphLayout& result,
                 size_t& lastIntervalUsed, uint32_t& overflowWord) {
  const ParagraphLayoutOptions& options = *block.options;
  const uint32_t wordCount = block.endWord;
  // Placing is the one thing that needs the glyphs, so a block placed from
  // break decisions made earlier pulls the shaping it needs and no more.
  if (!breaks.empty())
    paragraph.ensureShapedTo(fontContext, breaks.back().first);
  const std::vector<Word>& words = paragraph.words();
  uint32_t firstWordIndex = block.firstWord;
  int consecutiveHyphens = 0;
  for (size_t lineIndex = 0; lineIndex < breaks.size(); ++lineIndex) {
    const uint32_t lastWordIndex = breaks[lineIndex].first;
    const size_t intervalIndex = breaks[lineIndex].second;
    const FlatInterval* flatInterval =
        intervalSequence.intervalAt(intervalIndex);
    if (!flatInterval) {
      // The frame ran out of lines before the block ran out of breaks,
      // which is the ordinary end of a frame in a chain.
      overflowWord = std::min(overflowWord, firstWordIndex);
      break;
    }
    const bool lastLine = lineIndex + 1 == breaks.size();
    bool hyphenated = hyphenTakenAt(words, lastWordIndex, lastLine, options);
    if (hyphenated) {
      if (!options.hyphenation.lastWordOfBlock &&
          lastWordIndex + 1 >= wordCount)
        hyphenated = false;
      else if (options.hyphenation.consecutiveLimit > 0 &&
               consecutiveHyphens >= options.hyphenation.consecutiveLimit)
        hyphenated = false;
    }
    consecutiveHyphens = hyphenated ? consecutiveHyphens + 1 : 0;
    FlatInterval placed = *flatInterval;
    if (lastLine && block.style.indent.lastLine != 0 &&
        !placed.interval.contour.valid()) {
      const float indent = block.style.indent.lastLine;
      placed.interval.origin +=
          SkVector{placed.interval.direction.x() * indent,
                   placed.interval.direction.y() * indent};
      placed.interval.length = std::max(0.0f, placed.interval.length - indent);
    }
    placeWords(fontContext, paragraph, firstWordIndex, lastWordIndex, placed,
               options.alignment, lastLine, hyphenated, options, result,
               block.mojikumiAfter);
    lastIntervalUsed = intervalIndex;
    firstWordIndex = lastWordIndex;
  }
}

}  // namespace detail
}  // namespace sigil::weave
