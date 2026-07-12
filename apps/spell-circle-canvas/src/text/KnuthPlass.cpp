#include "LayoutInternal.h"
#include "textflow/Layout.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace textflow {
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
  int32_t prev = -1; // arena index of the predecessor node
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
Layout knuthPlassLayout(FontContext &ctx, Paragraph &paragraph,
                        IntervalSequence &seq, const LayoutOptions &opt) {
  Layout out;
  const std::vector<Word> &words = paragraph.words();
  const uint32_t n = static_cast<uint32_t>(words.size());
  if (n == 0)
    return out;
  if (!seq.get(0)) {
    out.firstUnplacedWord = 0;
    return out;
  }

  // Prefix sums: content width, and glue width/stretch/shrink per gap
  // (gap i sits after word i; the last word's "gap" is never on a line).
  // Extended on demand up to the furthest boundary the DP visits.
  static thread_local std::vector<float> prefWidth, prefGlue, prefStretch,
      prefShrink;
  prefWidth.assign(1, 0);
  prefGlue.assign(1, 0);
  prefStretch.assign(1, 0);
  prefShrink.assign(1, 0);
  auto ensurePref = [&](uint32_t upto) {
    for (uint32_t i = static_cast<uint32_t>(prefWidth.size()) - 1; i < upto;
         ++i) {
      prefWidth.push_back(prefWidth[i] + words[i].width);
      float glue = 0, stretch = 0, shrink = 0;
      if (words[i].spaceWidth > 0) {
        glue = words[i].spaceWidth;
        stretch = glue * opt.glueStretch;
        shrink = glue * opt.glueShrink;
      } else if (opt.ideographicJustify && i + 1 < n &&
                 (words[i].ideographic || words[i + 1].ideographic)) {
        const float em = words[i].segments.empty()
                             ? 16.0f
                             : words[i].segments[0].shaped->size;
        stretch = em * 0.25f;
        shrink = em * 0.03f;
      }
      prefGlue.push_back(prefGlue[i] + glue);
      prefStretch.push_back(prefStretch[i] + stretch);
      prefShrink.push_back(prefShrink[i] + shrink);
    }
  };

  // Extra width when the line ends on a discretionary (soft-hyphen) break.
  auto hyphenWidthAt = [&](uint32_t b) -> float {
    return hyphenTakenAt(words, b, b == n, opt)
               ? words[b - 1].hyphenGlyph->advance
               : 0.0f;
  };

  // Natural width and elasticity of a line holding words [a, b).
  auto lineNatural = [&](uint32_t a, uint32_t b) {
    return (prefWidth[b] - prefWidth[a]) + (prefGlue[b - 1] - prefGlue[a]) +
           hyphenWidthAt(b);
  };
  auto lineStretch = [&](uint32_t a, uint32_t b) {
    return prefStretch[b - 1] - prefStretch[a];
  };
  auto lineShrink = [&](uint32_t a, uint32_t b) {
    return prefShrink[b - 1] - prefShrink[a];
  };

  // Shrink is only real when placement will actually render the line
  // justified: ragged lines (and demoted last lines) render at natural
  // width, so a "feasible shrunk" break there would leak past the measure.
  const bool justify = opt.alignment == Alignment::kJustify;
  const bool lastLineJustify =
      justify && (opt.justifyLastLine ||
                  opt.lastLineAlignment == Alignment::kJustify);

  std::vector<Node> arena;
  std::vector<int32_t> active;
  std::vector<int32_t> nextActive;
  std::vector<std::pair<uint32_t, int32_t>> newNodes; // (interval, arena idx)

  // One full DP pass. Returns the best terminal arena index (-1 if nothing
  // could be placed), reports whether the lifeline ever had to force an
  // overfull line, and — when the geometry ran out before the text did —
  // the first word that no longer fit.
  auto runPass = [&](bool emergency, bool &forcedOverfull,
                     uint32_t &unplaced) -> int32_t {
    forcedOverfull = false;
    unplaced = ~0u;
    arena.clear();
    arena.push_back({0, 0, 0, -1});
    active = {0};

    // The path that placed the most text before running out of geometry —
    // the fallback result when no path reaches the end of the paragraph.
    int32_t bestEnd = -1;
    auto considerEnd = [&](int32_t idx) {
      if (bestEnd < 0 || arena[idx].breakAt > arena[bestEnd].breakAt ||
          (arena[idx].breakAt == arena[bestEnd].breakAt &&
           arena[idx].demerits < arena[bestEnd].demerits))
        bestEnd = idx;
    };

    for (uint32_t j = 1; j <= n && !active.empty(); ++j) {
      paragraph.ensureShapedTo(ctx, j); // lazy: shape at the DP frontier
      ensurePref(j);
      const bool forced = (j == n) || words[j - 1].mandatoryBreakAfter;

      nextActive.clear();
      newNodes.clear();
      float bestForcedDemerits = kInfDemerits;
      bool bestForcedOverfull = true;
      Node bestForced;

      for (size_t ai = 0; ai < active.size(); ++ai) {
        const int32_t nodeIdx = active[ai];
        const Node &node = arena[nodeIdx];
        const FlatInterval *lineIv = seq.get(node.interval);
        if (!lineIv) {
          // No geometry left for this path's next line: it ends here.
          considerEnd(nodeIdx);
          continue;
        }
        const uint32_t a = node.breakAt;
        const float W = lineIv->iv.length;
        const float natural = lineNatural(a, j);
        const float stretch = lineStretch(a, j) + (emergency ? W : 0.0f);
        const float shrink = lineShrink(a, j);

        float ratio;
        if (forced && natural <= W) {
          // Paragraph-final (and hard-break-final) lines end wherever they
          // end — TeX's \parfillskip absorbs the slack for free. Without
          // this, a stretch-free underfull last line scores kMaxBadness and
          // an *overfull* candidate with a cheaper history can beat it,
          // leaking text past the measure.
          ratio = 0;
        } else if (natural < W) {
          ratio = stretch > 0 ? (W - natural) / stretch
                              : (natural > W - 0.5f ? 0.0f : 1e9f);
        } else if (natural > W) {
          const bool canShrink = forced ? lastLineJustify : justify;
          ratio = (canShrink && shrink > 0) ? (W - natural) / shrink : -1e9f;
        } else {
          ratio = 0;
        }

        const bool overfull = ratio < -1.0f;
        const float r = std::min(std::abs(ratio), 500.0f); // pre-clamp: r³
        const float badness = std::min(100.0f * r * r * r, kMaxBadness);
        const bool feasible = !overfull && badness <= opt.kpTolerance;

        float demerits = node.demerits +
                         (kLinePenalty + badness) * (kLinePenalty + badness);
        if (hyphenWidthAt(j) > 0)
          demerits += opt.hyphenPenalty * opt.hyphenPenalty;

        if (feasible) {
          Node candidate{j, node.interval + 1, demerits, nodeIdx};
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
            bestForced = Node{j, node.interval + 1, penalized, nodeIdx};
          }
        }

        // A node whose line is already over-shrunk only gets worse; retire.
        if (!overfull && !forced)
          nextActive.push_back(nodeIdx);
      }

      if (forced) {
        // Every line must end here: only nodes at j survive.
        nextActive.clear();
      }

      // Keep the best new node per interval index (they subsume the rest).
      std::sort(newNodes.begin(), newNodes.end());
      for (size_t x = 0; x < newNodes.size(); ++x) {
        if (x + 1 < newNodes.size() &&
            newNodes[x].first == newNodes[x + 1].first) {
          // same interval: keep the lower-demerits one
          const int32_t a1 = newNodes[x].second, a2 = newNodes[x + 1].second;
          if (arena[a1].demerits < arena[a2].demerits)
            std::swap(newNodes[x], newNodes[x + 1]);
          continue;
        }
        nextActive.push_back(newNodes[x].second);
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
    for (int32_t idx : active)
      if (arena[idx].breakAt == n &&
          (best < 0 || arena[idx].demerits < arena[best].demerits))
        best = idx;
    if (best < 0 && bestEnd >= 0) {
      // Geometry ran out before the text did: place what fits.
      best = bestEnd;
      unplaced = arena[bestEnd].breakAt;
    }
    return best;
  };

  bool forcedOverfull = false;
  uint32_t unplaced = ~0u;
  int32_t best = runPass(false, forcedOverfull, unplaced);
  if (best < 0 || forcedOverfull) {
    bool stillOverfull = false;
    uint32_t unplaced2 = ~0u;
    const int32_t retry = runPass(true, stillOverfull, unplaced2);
    if (retry >= 0) {
      best = retry;
      unplaced = unplaced2;
    }
  }
  if (best < 0) {
    out.firstUnplacedWord = 0;
    return out;
  }
  if (unplaced != ~0u)
    out.firstUnplacedWord = unplaced;

  std::vector<uint32_t> breaks; // ascending word indices, ending with n
  for (int32_t idx = best; idx > 0; idx = arena[idx].prev)
    breaks.push_back(arena[idx].breakAt);
  std::reverse(breaks.begin(), breaks.end());

  // ── Placement ─────────────────────────────────────────────────────────
  uint32_t first = 0;
  int lastLineUsed = -1;
  for (size_t t = 0; t < breaks.size(); ++t) {
    const uint32_t last = breaks[t];
    const FlatInterval *fiv = seq.get(t);
    if (!fiv) {
      out.firstUnplacedWord = std::min(out.firstUnplacedWord, first);
      break;
    }
    const bool lastLine =
        (t + 1 == breaks.size()) || words[last - 1].mandatoryBreakAfter;
    placeWords(words, first, last, *fiv, opt.alignment, lastLine,
               hyphenTakenAt(words, last, lastLine, opt), opt, out);
    lastLineUsed = std::max(lastLineUsed, fiv->line);
    first = last;
  }
  out.lineCount = lastLineUsed + 1;
  return out;
}

} // namespace detail
} // namespace textflow
