/** @file
 * Every glyph of a page placed on a line of its own: the arm that says
 * what a layout costs when nothing about it can be shared.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkPathBuilder.h>
#include <sigilweave/layout/Beside.h>

#include <cmath>
#include <numbers>
#include <random>
#include <string>
#include <vector>

#include "BenchOptions.h"
#include "support/Corpus.h"
#include "support/Layouts.h"

using namespace sigil::weave;
using namespace sigil::weave::bench;

namespace {

void BM_Confetti_Babel_2000(benchmark::State& state) {
  const char8_t* tokens[] = {
      u8"حرف",  u8"كلمة", u8"अक्षर",  u8"शब्द",   u8"אות",   u8"מילה", u8"ตัวอักษร",
      u8"字",   u8"글",   u8"λόγος", u8"буква", u8"🎉",    u8"👍🏽", u8"文字",
      u8"ঢাকা", u8"கடல்",  u8"ᚱᚢᚾ",   u8"ainm",  u8"słowo", u8"λέξη"};
  std::mt19937
      randomEngine(  // NOLINT(bugprone-random-generator-seed): a fixed corpus
          77);
  Paragraph paragraph;
  std::u8string text;
  for (int tokenIndex = 0; tokenIndex < 2000; ++tokenIndex) {
    text += tokens[randomEngine() % 20];
    text += ' ';
  }
  paragraph.appendText(text, basicStyle());

  LineSetFlow flow;
  for (int tokenIndex = 0; tokenIndex < 2000; ++tokenIndex) {
    const float angle = (float)(randomEngine() % 628) * 0.01f;
    flow.lines().push_back(
        {LineInterval{{20.0f + (float)(randomEngine() % 1360),
                       20.0f + (float)(randomEngine() % 860)},
                      {std::cos(angle), std::sin(angle)},
                      60}});
  }
  layoutParagraph(sigil::test::fonts(), paragraph, flow);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 2000);
}
BENCHMARK(BM_Confetti_Babel_2000)->Unit(benchmark::kMicrosecond);

}  // namespace
