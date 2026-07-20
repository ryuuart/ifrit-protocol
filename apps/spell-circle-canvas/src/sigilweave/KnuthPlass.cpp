#include "ParagraphLayoutInternal.h"
#include "sigilweave/ParagraphLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

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
  uint32_t breakAt = 0;  // line starts at word `breakAt`
  uint32_t interval = 0; // interval index the *next* line will occupy
  float demerits = 0;
  int32_t previousNode = -1; // arena index of the predecessor node
};

} // namespace

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
ParagraphLayout knuthPlassLayout(FontContext &fontContext, Paragraph &paragraph,
                                 IntervalSequence &intervalSequence,
                                 const ParagraphLayoutOptions &options) {
  ParagraphLayout result;
  const std::vector<Word> &words = paragraph.words();
  const uint32_t wordCount = static_cast<uint32_t>(words.size());
  if (wordCount == 0)
    return result;
  if (!intervalSequence.intervalAt(0)) {
    result.firstUnplacedWord = 0;
    return result;
  }

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
  auto ensurePrefixSums = [&](uint32_t endWordIndex) {
    for (uint32_t wordIndex = static_cast<uint32_t>(prefixWidth.size()) - 1;
         wordIndex < endWordIndex; ++wordIndex) {
      prefixWidth.push_back(prefixWidth[wordIndex] + words[wordIndex].width);
      float glue = 0;
      float stretch = 0;
      float shrink = 0;
      if (tabAware && words[wordIndex].tabAfter) {
        // Tab gaps are rigid (columns pin to stops) and positional; the
        // width they'll actually take is resolved in lineNatural.
        tabGapIndices.push_back(wordIndex);
      } else if (words[wordIndex].spaceWidth > 0) {
        glue = words[wordIndex].spaceWidth;
        stretch = glue * options.justification.spaceStretch;
        shrink = glue * options.justification.spaceShrink;
      } else if (options.justification.expandIdeographicGaps &&
                 wordIndex + 1 < wordCount &&
                 (words[wordIndex].ideographic ||
                  words[wordIndex + 1].ideographic)) {
        const float fontSize =
            words[wordIndex].segments.empty()
                ? 16.0f
                : words[wordIndex].segments[0].shaped->fontSize;
        stretch = fontSize * 0.25f;
        shrink = fontSize * 0.03f;
      }
      prefixGlue.push_back(prefixGlue[wordIndex] + glue);
      prefixStretch.push_back(prefixStretch[wordIndex] + stretch);
      prefixShrink.push_back(prefixShrink[wordIndex] + shrink);
    }
  };

  // Tab gaps interior to a candidate line [lineStart, lineEnd): gap indices
  // in [lineStart, lineEnd - 1) — the break-side gap is never on the line.
  auto tabGapsInLine = [&](uint32_t lineStart, uint32_t lineEnd) {
    const auto first = std::lower_bound(tabGapIndices.begin(),
                                        tabGapIndices.end(), lineStart);
    const auto last =
        std::lower_bound(first, tabGapIndices.end(), lineEnd - 1);
    return std::span<const uint32_t>(first, last);
  };

  // Extra width when the line ends on a discretionary (soft-hyphen) break.
  auto hyphenWidthAt = [&](uint32_t breakIndex) -> float {
    return hyphenTakenAt(words, breakIndex, breakIndex == wordCount, options)
               ? words[breakIndex - 1].hyphenGlyph->advance
               : 0.0f;
  };

  // Natural width and elasticity of a line holding a half-open word range.
  // These are the DP loop's hottest calls, so they stay slim enough to
  // inline; the tab corrections live in one flat, `tabAware`-guarded block
  // at their call site instead (nesting them here measurably de-inlined
  // the lot and cost ~10% on tab-free Knuth-Plass layouts).
  auto lineNatural = [&](uint32_t lineStart, uint32_t lineEnd) {
    return (prefixWidth[lineEnd] - prefixWidth[lineStart]) +
           (prefixGlue[lineEnd - 1] - prefixGlue[lineStart]) +
           hyphenWidthAt(lineEnd);
  };
  auto lineStretch = [&](uint32_t lineStart, uint32_t lineEnd) {
    return prefixStretch[lineEnd - 1] - prefixStretch[lineStart];
  };
  auto lineShrink = [&](uint32_t lineStart, uint32_t lineEnd) {
    return prefixShrink[lineEnd - 1] - prefixShrink[lineStart];
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
      pen += (prefixWidth[tabIndex + 1] - prefixWidth[segmentStart]) +
             (prefixGlue[tabIndex] - prefixGlue[segmentStart]);
      pen += glueAfter(words[tabIndex], pen, options);
      segmentStart = tabIndex + 1;
    }
    return pen + (prefixWidth[lineEnd] - prefixWidth[segmentStart]) +
           (prefixGlue[lineEnd - 1] - prefixGlue[segmentStart]) +
           hyphenWidthAt(lineEnd);
  };

  // Shrink is only real when placement will actually render the line
  // justified: ragged lines (and demoted last lines) render at natural
  // width, so a "feasible shrunk" break there would leak past the measure.
  const bool justify = options.alignment == TextAlignment::kJustify;
  const bool lastLineJustify =
      justify &&
      (options.justification.justifyLastLine ||
       options.justification.lastLineAlignment == TextAlignment::kJustify);

  std::vector<Node> arena;
  std::vector<int32_t> active;
  std::vector<int32_t> nextActive;
  // Pairs are (next interval index, node arena index).
  std::vector<std::pair<uint32_t, int32_t>> newNodes;
  const bool uniform = intervalSequence.uniform();

  // One full DP pass. Returns the best terminal arena index (-1 if nothing
  // could be placed), reports whether the lifeline ever had to force an
  // overfull line, and — when the geometry ran out before the text did —
  // the first word that no longer fit.
  auto runPass = [&](bool useEmergencyStretch, bool &forcedOverfull,
                     uint32_t &firstUnplacedWord) -> int32_t {
    forcedOverfull = false;
    firstUnplacedWord = ~0u;
    arena.clear();
    arena.push_back({0, 0, 0, -1});
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

    for (uint32_t breakIndex = 1; breakIndex <= wordCount && !active.empty();
         ++breakIndex) {
      // Shape only as the dynamic-programming frontier advances.
      paragraph.ensureShapedTo(fontContext, breakIndex);
      ensurePrefixSums(breakIndex);
      const bool forcedBreak =
          breakIndex == wordCount || words[breakIndex - 1].mandatoryBreakAfter;

      nextActive.clear();
      newNodes.clear();
      float bestForcedDemerits = kInfDemerits;
      bool bestForcedOverfull = true;
      Node bestForced;

      for (size_t activeIndex = 0; activeIndex < active.size(); ++activeIndex) {
        const int32_t nodeIndex = active[activeIndex];
        const Node &node = arena[nodeIndex];
        const FlatInterval *lineInterval =
            intervalSequence.intervalAt(node.interval);
        if (!lineInterval) {
          // No geometry left for this path's next line: it ends here.
          considerEnd(nodeIndex);
          continue;
        }
        const uint32_t lineStart = node.breakAt;
        const float measure = lineInterval->interval.length;
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
            stretch =
                prefixStretch[breakIndex - 1] - prefixStretch[tabs.back() + 1];
            shrink =
                prefixShrink[breakIndex - 1] - prefixShrink[tabs.back() + 1];
          }
        }
        stretch += useEmergencyStretch ? measure : 0.0f;

        float ratio;
        if (forcedBreak && natural <= measure) {
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
            std::min(std::abs(ratio), 500.0f); // pre-clamp: ratio³
        const float badness = std::min(
            100.0f * clampedRatio * clampedRatio * clampedRatio, kMaxBadness);
        const bool feasible =
            !overfull && badness <= options.knuthPlass.tolerance;

        float demerits =
            node.demerits + (kLinePenalty + badness) * (kLinePenalty + badness);
        if (hyphenWidthAt(breakIndex) > 0)
          demerits += options.hyphenation.penalty * options.hyphenation.penalty;

        if (feasible) {
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
        if (!overfull && !forcedBreak)
          nextActive.push_back(nodeIndex);
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
        if (bestForcedOverfull)
          forcedOverfull = true;
        arena.push_back(bestForced);
        nextActive.push_back(static_cast<int32_t>(arena.size() - 1));
      }
      // With no forced candidate either, every path hit the end of the
      // geometry: the loop exits on the empty active list and the partial
      // result below stands.

      active = nextActive;
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

  bool forcedOverfull = false;
  uint32_t firstUnplacedWord = ~0u;
  int32_t best = runPass(false, forcedOverfull, firstUnplacedWord);
  if (best < 0 || forcedOverfull) {
    bool stillOverfull = false;
    uint32_t retryFirstUnplacedWord = ~0u;
    const int32_t retry = runPass(true, stillOverfull, retryFirstUnplacedWord);
    if (retry >= 0) {
      best = retry;
      firstUnplacedWord = retryFirstUnplacedWord;
    }
  }
  if (best < 0) {
    result.firstUnplacedWord = 0;
    return result;
  }
  if (firstUnplacedWord != ~0u)
    result.firstUnplacedWord = firstUnplacedWord;

  std::vector<uint32_t> breaks; // ascending word indices, ending at wordCount
  for (int32_t nodeIndex = best; nodeIndex > 0;
       nodeIndex = arena[nodeIndex].previousNode)
    breaks.push_back(arena[nodeIndex].breakAt);
  std::reverse(breaks.begin(), breaks.end());

  // ── Placement ─────────────────────────────────────────────────────────
  uint32_t firstWordIndex = 0;
  int lastLineUsed = -1;
  for (size_t lineIndex = 0; lineIndex < breaks.size(); ++lineIndex) {
    const uint32_t lastWordIndex = breaks[lineIndex];
    const FlatInterval *flatInterval = intervalSequence.intervalAt(lineIndex);
    if (!flatInterval) {
      result.firstUnplacedWord =
          std::min(result.firstUnplacedWord, firstWordIndex);
      break;
    }
    const bool lastLine = lineIndex + 1 == breaks.size() ||
                          words[lastWordIndex - 1].mandatoryBreakAfter;
    placeWords(words, firstWordIndex, lastWordIndex, *flatInterval,
               options.alignment, lastLine,
               hyphenTakenAt(words, lastWordIndex, lastLine, options), options,
               result);
    lastLineUsed = std::max(lastLineUsed, flatInterval->sourceLineIndex);
    firstWordIndex = lastWordIndex;
  }
  result.lineCount = lastLineUsed + 1;
  return result;
}

} // namespace detail
} // namespace sigil::weave
