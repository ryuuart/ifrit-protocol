/** @file
 * weave_fonts_bench — the shaper per word: a word through HarfBuzz against
 * the same word served from the shape cache. Run a Release build; Debug
 * numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilweave/fonts/Shaper.h>
#include <sigilweave/unicode/Unicode.h>

#include <string>
#include <vector>

#include "support/Corpus.h"

using namespace sigil::weave;
using namespace sigil::weave::bench;

namespace {

constexpr ScriptTag tag(unsigned char a, unsigned char b, unsigned char c,
                        unsigned char d) {
  return (ScriptTag(a) << 24u) | (ScriptTag(b) << 16u) | (ScriptTag(c) << 8u) |
         ScriptTag(d);
}

struct Corpus {
  std::vector<std::u16string> words;
  ScriptTag script = tag('L', 'a', 't', 'n');
};

Corpus corpus(bool cjk) {
  Corpus out;
  for (const std::u8string& word : cjk ? cjkWords() : latinWords())
    out.words.push_back(unicode::toUtf16(word));
  if (cjk) out.script = tag('H', 'a', 'n', 'i');
  return out;
}

void countWords(benchmark::State& state, int64_t words) {
  state.counters["words/s"] = benchmark::Counter(
      (double)words, benchmark::Counter::kIsIterationInvariantRate);
}

/** Every word of the corpus through hb_shape: the cache is purged
 *  before each pass, outside the timed region. */
void BM_ShapeWord_Cold(benchmark::State& state) {
  const Corpus words = corpus(state.range(0) != 0);
  state.SetLabel(state.range(0) ? "cjk" : "latin");
  const ShapingStyle style = basicStyle().shaping;
  const sk_sp<SkTypeface> typeface = sigil::test::fonts().defaultTypeface();
  for ([[maybe_unused]] auto iteration : state) {
    state.PauseTiming();
    sigil::test::fonts().purgeShapeCache();
    state.ResumeTiming();
    for (const std::u16string& word : words.words) {
      ShapedWordRef shaped = shapeWord(sigil::test::fonts(), style, typeface, word,
                                       words.script, false);
      benchmark::DoNotOptimize(shaped.get());
    }
  }
  countWords(state, (int64_t)words.words.size());
}
BENCHMARK(BM_ShapeWord_Cold)->Arg(0)->Arg(1)->Unit(benchmark::kMicrosecond);

/** The same words again: every one a cache hit, so this is the key
 *  construction and lookup a warm frame pays per word. */
void BM_ShapeWord_Warm(benchmark::State& state) {
  const Corpus words = corpus(state.range(0) != 0);
  state.SetLabel(state.range(0) ? "cjk" : "latin");
  const ShapingStyle style = basicStyle().shaping;
  const sk_sp<SkTypeface> typeface = sigil::test::fonts().defaultTypeface();
  for (const std::u16string& word : words.words)
    (void)shapeWord(sigil::test::fonts(), style, typeface, word, words.script,
                    false);
  for ([[maybe_unused]] auto iteration : state) {
    for (const std::u16string& word : words.words) {
      ShapedWordRef shaped = shapeWord(sigil::test::fonts(), style, typeface, word,
                                       words.script, false);
      benchmark::DoNotOptimize(shaped.get());
    }
  }
  countWords(state, (int64_t)words.words.size());
}
BENCHMARK(BM_ShapeWord_Warm)->Arg(0)->Arg(1)->Unit(benchmark::kMicrosecond);

}  // namespace
