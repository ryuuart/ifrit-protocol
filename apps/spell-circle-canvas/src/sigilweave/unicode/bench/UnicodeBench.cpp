/** @file
 * weave_unicode_bench — the Unicode leaf per code point: script
 * itemization, line-break opportunities and bidi resolution over text
 * that is all one script, a Latin and CJK mix, and a Latin and Arabic
 * mix that forces the bidi algorithm to reorder. Run a Release build;
 * Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilweave/unicode/Unicode.h>

#include <random>
#include <string>
#include <vector>

using namespace sigil::weave::unicode;

namespace {

enum Corpus { kLatin = 0, kLatinCjk = 1, kLatinArabic = 2 };

const char* corpusName(int corpus) {
  switch (corpus) {
    case kLatin:
      return "latin";
    case kLatinCjk:
      return "latin+cjk";
    default:
      return "latin+arabic";
  }
}

/** About `codePoints` code points of space-separated words drawn from
 *  the corpus, as the UTF-16 the leaf reads. */
std::u16string makeText(int corpus, int codePoints) {
  static const std::vector<std::u16string> latin = {
      u"the",  u"quick", u"brown", u"fox",     u"jumps",   u"over",
      u"lazy", u"dogs",  u"while", u"wizards", u"conjure", u"circles"};
  static const std::vector<std::u16string> cjk = {
      u"文字", u"レイアウト", u"高速", u"描画", u"한글", u"텍스트", u"排版"};
  static const std::vector<std::u16string> arabic = {
      u"حرف", u"كلمة", u"النص", u"سريع", u"تخطيط", u"محرك"};
  std::mt19937
      randomEngine(  // NOLINT(bugprone-random-generator-seed): a fixed corpus
          11);
  std::u16string text;
  int count = 0;
  while (count < codePoints) {
    const std::u16string* word = &latin[randomEngine() % latin.size()];
    if (corpus == kLatinCjk && randomEngine() % 3 == 0)
      word = &cjk[randomEngine() % cjk.size()];
    else if (corpus == kLatinArabic && randomEngine() % 3 == 0)
      word = &arabic[randomEngine() % arabic.size()];
    text += *word;
    text += u' ';
    count += (int)word->size() + 1;
  }
  return text;
}

void countCodePoints(benchmark::State& state, const std::u16string& text) {
  state.counters["cp/s"] = benchmark::Counter(
      (double)text.size(), benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN((int64_t)text.size());
}

void BM_Itemize(benchmark::State& state) {
  const std::u16string text =
      makeText((int)state.range(0), (int)state.range(1));
  state.SetLabel(corpusName((int)state.range(0)));
  std::vector<ScriptRun> runs;
  for ([[maybe_unused]] auto iteration : state) {
    itemize(text, runs);
    benchmark::DoNotOptimize(runs.data());
  }
  countCodePoints(state, text);
}
BENCHMARK(BM_Itemize)
    ->ArgsProduct({{kLatin, kLatinCjk, kLatinArabic}, {100, 1000, 10000}})
    ->ArgNames({"corpus", "cp"})
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_LineBreaks(benchmark::State& state) {
  const std::u16string text =
      makeText((int)state.range(0), (int)state.range(1));
  state.SetLabel(corpusName((int)state.range(0)));
  std::vector<LineBreak> breaks;
  for ([[maybe_unused]] auto iteration : state) {
    lineBreaks(text, breaks);
    benchmark::DoNotOptimize(breaks.data());
  }
  countCodePoints(state, text);
}
BENCHMARK(BM_LineBreaks)
    ->ArgsProduct({{kLatin, kLatinCjk, kLatinArabic}, {100, 1000, 10000}})
    ->ArgNames({"corpus", "cp"})
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_Bidi(benchmark::State& state) {
  const std::u16string text =
      makeText((int)state.range(0), (int)state.range(1));
  state.SetLabel(corpusName((int)state.range(0)));
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<BidiRun> runs = bidi(text);
    benchmark::DoNotOptimize(runs.data());
  }
  countCodePoints(state, text);
}
BENCHMARK(BM_Bidi)
    ->ArgsProduct({{kLatin, kLatinArabic}, {100, 1000, 10000}})
    ->ArgNames({"corpus", "cp"})
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_ToUtf16(benchmark::State& state) {
  const std::u16string wide = makeText(kLatinCjk, (int)state.range(0));
  const std::u8string text = toUtf8(wide);
  for ([[maybe_unused]] auto iteration : state) {
    std::u16string out = toUtf16(text);
    benchmark::DoNotOptimize(out.data());
  }
  countCodePoints(state, wide);
}
BENCHMARK(BM_ToUtf16)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

}  // namespace

BENCHMARK_MAIN();
