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
// per-line (per-interval) widths from the flow geometry. Robustness twist:
// when no feasible break survives at some boundary, the least-bad candidate
// is force-accepted so the breaker always terminates with a full layout.
Layout knuthPlassLayout(const std::vector<Word> &words, IntervalSequence &seq,
                        const LayoutOptions &opt) {
  Layout out;
  const uint32_t n = static_cast<uint32_t>(words.size());
  if (n == 0)
    return out;
  if (!seq.get(0)) {
    out.firstUnplacedWord = 0;
    return out;
  }

  // Prefix sums: content width, and glue width/stretch/shrink per gap
  // (gap i sits after word i; the last word's "gap" is never on a line).
  std::vector<float> prefWidth(n + 1, 0), prefGlue(n + 1, 0),
      prefStretch(n + 1, 0), prefShrink(n + 1, 0);
  for (uint32_t i = 0; i < n; ++i) {
    prefWidth[i + 1] = prefWidth[i] + words[i].width;
    float glue = 0, stretch = 0, shrink = 0;
    if (words[i].spaceWidth > 0) {
      glue = words[i].spaceWidth;
      stretch = glue * opt.glueStretch;
      shrink = glue * opt.glueShrink;
    } else if (opt.ideographicJustify && i + 1 < n &&
               (words[i].ideographic || words[i + 1].ideographic)) {
      const float em =
          words[i].segments.empty() ? 16.0f : words[i].segments[0].shaped->size;
      stretch = em * 0.25f;
      shrink = em * 0.03f;
    }
    prefGlue[i + 1] = prefGlue[i] + glue;
    prefStretch[i + 1] = prefStretch[i] + stretch;
    prefShrink[i + 1] = prefShrink[i] + shrink;
  }

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

  float lastWidth = seq.get(0)->iv.length;
  auto intervalWidth = [&](uint32_t k) {
    if (const FlatInterval *fiv = seq.get(k)) {
      lastWidth = fiv->iv.length;
      return fiv->iv.length;
    }
    return lastWidth; // geometry exhausted: assume it keeps the last width
  };

  std::vector<Node> arena;
  arena.push_back({0, 0, 0, -1});
  std::vector<int32_t> active = {0};
  std::vector<int32_t> nextActive;

  for (uint32_t j = 1; j <= n; ++j) {
    const bool forced = (j == n) || words[j - 1].mandatoryBreakAfter;

    nextActive.clear();
    float bestForcedDemerits = kInfDemerits;
    Node bestForced;
    int32_t bestNewPerInterval = -1; // dedupe: best new node (any interval)

    std::vector<std::pair<uint32_t, int32_t>> newNodes; // (interval, arena idx)

    for (size_t ai = 0; ai < active.size(); ++ai) {
      const int32_t nodeIdx = active[ai];
      const Node &node = arena[nodeIdx];
      const uint32_t a = node.breakAt;
      const float W = intervalWidth(node.interval);
      const float natural = lineNatural(a, j);
      const float stretch = lineStretch(a, j);
      const float shrink = lineShrink(a, j);

      float ratio;
      if (natural < W)
        ratio = stretch > 0 ? (W - natural) / stretch
                            : (natural > W - 0.5f ? 0.0f : 1e9f);
      else if (natural > W)
        ratio = shrink > 0 ? (W - natural) / shrink : -1e9f;
      else
        ratio = 0;

      const bool overfull = ratio < -1.0f;
      const float r = std::min(std::abs(ratio), 500.0f); // pre-clamp: r³ fits
      const float badness = std::min(100.0f * r * r * r, kMaxBadness);
      const bool feasible = !overfull && badness <= opt.kpTolerance;

      float demerits =
          node.demerits + (kLinePenalty + badness) * (kLinePenalty + badness);
      if (hyphenWidthAt(j) > 0)
        demerits += opt.hyphenPenalty * opt.hyphenPenalty;

      if (feasible || forced) {
        Node candidate{j, node.interval + 1, demerits, nodeIdx};
        if (feasible) {
          newNodes.emplace_back(candidate.interval, -1);
          arena.push_back(candidate);
          newNodes.back().second = static_cast<int32_t>(arena.size() - 1);
        } else if (demerits < bestForcedDemerits) {
          bestForcedDemerits = demerits;
          bestForced = candidate;
        }
      } else if (demerits < bestForcedDemerits) {
        // Track the least-bad infeasible candidate as a lifeline.
        bestForcedDemerits = demerits + 1e12f;
        bestForced = Node{j, node.interval + 1, demerits + 1e12f, nodeIdx};
      }

      // A node whose line is already over-shrunk only gets worse; retire it.
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
      if (x + 1 < newNodes.size() && newNodes[x].first == newNodes[x + 1].first) {
        // same interval: keep the lower-demerits one
        const int32_t a1 = newNodes[x].second, a2 = newNodes[x + 1].second;
        if (arena[a1].demerits < arena[a2].demerits)
          std::swap(newNodes[x], newNodes[x + 1]);
        continue;
      }
      nextActive.push_back(newNodes[x].second);
    }

    if (nextActive.empty()) {
      // No feasible breaks anywhere: force the least-bad one.
      if (bestForcedDemerits >= kInfDemerits) {
        // Not even a forced candidate (empty active list — cannot happen
        // while j advances one at a time, but stay safe).
        out.firstUnplacedWord = j - 1;
        break;
      }
      arena.push_back(bestForced);
      nextActive.push_back(static_cast<int32_t>(arena.size() - 1));
    }

    active = nextActive;
    (void)bestNewPerInterval;
  }

  // Best terminal node.
  int32_t best = -1;
  for (int32_t idx : active)
    if (arena[idx].breakAt == n &&
        (best < 0 || arena[idx].demerits < arena[best].demerits))
      best = idx;
  if (best < 0) {
    if (out.firstUnplacedWord == ~0u)
      out.firstUnplacedWord = 0;
    return out;
  }

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
