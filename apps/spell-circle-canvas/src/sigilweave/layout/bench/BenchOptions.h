#pragma once
/** @file
 * What every layout benchmark in this directory reports and sets: the
 * per-word rate counter, and the two option sets an arm is measured under
 * — the optimizing breaker, and the optimizing breaker running live.
 */

#include <benchmark/benchmark.h>
#include <sigilweave/layout/ParagraphLayout.h>

namespace sigil::weave::bench {

inline void countWords(benchmark::State& state, int64_t words) {
  state.counters["words/s"] = benchmark::Counter(
      (double)words, benchmark::Counter::kIsIterationInvariantRate);
}

inline ParagraphLayoutOptions knuthPlass() {
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  return options;
}

/** The optimizing breaker composing a moving text, which is what the live
 *  half of the engine is judged on. */
inline ParagraphLayoutOptions liveComposer() {
  ParagraphLayoutOptions options = knuthPlass();
  options.live = true;
  return options;
}

}  // namespace sigil::weave::bench
