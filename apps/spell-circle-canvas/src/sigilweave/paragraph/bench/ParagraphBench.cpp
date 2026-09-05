/** @file
 * weave_paragraph_bench — a whole paragraph analysed and shaped cold
 * against warm, which is what the shape cache absorbs. Run a Release
 * build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilweave/paragraph/Paragraph.h>

#include <string>

#include "support/Corpus.h"

using namespace sigil::weave;
using namespace sigil::weave::bench;

namespace {

void countWords(benchmark::State& state, int64_t words) {
  state.counters["words/s"] = benchmark::Counter(
      (double)words, benchmark::Counter::kIsIterationInvariantRate);
}

/** A fresh paragraph analysed and shaped with an empty cache: itemize,
 *  segment, fall back, hb_shape for every word. */
void BM_ShapeParagraph_Cold(benchmark::State& state) {
  const int words = (int)state.range(0);
  const std::u8string text = makeText(words, state.range(1) != 0);
  state.SetLabel(state.range(1) ? "mixed" : "latin");
  for ([[maybe_unused]] auto iteration : state) {
    state.PauseTiming();
    sigil::test::fonts().purgeShapeCache();
    state.ResumeTiming();
    Paragraph paragraph;
    paragraph.appendText(text, basicStyle());
    paragraph.ensureShaped(sigil::test::fonts());
    benchmark::DoNotOptimize(paragraph.words().data());
  }
  countWords(state, words);
}
BENCHMARK(BM_ShapeParagraph_Cold)
    ->ArgsProduct({{100, 1000}, {0, 1}})
    ->ArgNames({"words", "mixed"})
    ->Unit(benchmark::kMicrosecond);

/** A fresh paragraph over a warm cache: the full analysis with no
 *  hb_shape behind it. */
void BM_ShapeParagraph_Warm(benchmark::State& state) {
  const int words = (int)state.range(0);
  const std::u8string text = makeText(words, state.range(1) != 0);
  state.SetLabel(state.range(1) ? "mixed" : "latin");
  {
    Paragraph warmup;
    warmup.appendText(text, basicStyle());
    warmup.ensureShaped(sigil::test::fonts());
  }
  for ([[maybe_unused]] auto iteration : state) {
    Paragraph paragraph;
    paragraph.appendText(text, basicStyle());
    paragraph.ensureShaped(sigil::test::fonts());
    benchmark::DoNotOptimize(paragraph.words().data());
  }
  countWords(state, words);
}
BENCHMARK(BM_ShapeParagraph_Warm)
    ->ArgsProduct({{100, 1000}, {0, 1}})
    ->ArgNames({"words", "mixed"})
    ->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();
