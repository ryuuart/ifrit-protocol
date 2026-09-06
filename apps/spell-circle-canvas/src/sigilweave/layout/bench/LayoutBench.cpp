/** @file
 * Warm relayout by paragraph length, both breakers, and the paragraph
 * controls that cost a layout something: every arm is placement and line
 * breaking over a warm shape cache, with no shaping under it. Run a
 * Release build; Debug numbers say nothing.
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

// ── Warm relayout by length, both breakers ───────────────────────────────

void BM_Layout_Greedy(benchmark::State& state) {
  const int words = (int)state.range(0);
  Paragraph paragraph;
  paragraph.appendText(makeText(words, /*mixed=*/true), basicStyle());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, words);
  state.SetComplexityN(words);
}
BENCHMARK(BM_Layout_Greedy)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Arg(10000)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_Layout_KnuthPlass(benchmark::State& state) {
  const int words = (int)state.range(0);
  Paragraph paragraph;
  paragraph.appendText(makeText(words, /*mixed=*/false), basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 40000));
  const ParagraphLayoutOptions options = knuthPlass();
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, words);
  state.SetComplexityN(words);
}
BENCHMARK(BM_Layout_KnuthPlass)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Arg(10000)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

// The same warm relayout of a paragraph the geometry cannot hold: thirty
// thousand words in a block with room for about one percent of them. What
// this arm is read against is the fully-placed sweep above — an overflowed
// frame must cost what fits, so its time belongs beside the small end of
// that sweep and not beside its own word count.
void BM_Layout_Overflowed_30000w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(30000, /*mixed=*/false), basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 320));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = state.range(0) == 0
                                  ? LineBreakStrategy::kGreedy
                                  : LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 30000);
}
BENCHMARK(BM_Layout_Overflowed_30000w)
    ->Arg(0)
    ->Arg(1)
    ->Unit(benchmark::kMicrosecond);

// The same warm relayout set DOWN COLUMNS. A column breaks between
// characters rather than at spaces, so the greedy breaker is asked for a
// decision at nearly every glyph — which is what this arm measures against
// the space-separated one above.
void BM_Layout_Vertical_Columns(benchmark::State& state) {
  const int words = (int)state.range(0);
  Paragraph paragraph;
  paragraph.appendText(makeColumnText(words), basicStyle());
  paragraph.setWritingMode(WritingMode::kVerticalRL);
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 26;  // column pitch
  VerticalBlockFlow flow(SkRect::MakeWH(20000, 600));
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, words);
  state.SetComplexityN(words);
}
BENCHMARK(BM_Layout_Vertical_Columns)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

// Knuth-Plass over text dense with soft hyphens (every word carries
// discretionary break points) on a narrow measure.
void BM_Layout_KnuthPlass_Hyphenated_300w(benchmark::State& state) {
  std::mt19937
      randomEngine(  // NOLINT(bugprone-random-generator-seed): a fixed corpus
          7);
  const auto& latin = latinWords();
  std::u8string text;
  for (int wordIndex = 0; wordIndex < 300; ++wordIndex) {
    const std::u8string& word = latin[randomEngine() % latin.size()];
    if (word.size() > 4) {
      text.append(word, 0, word.size() / 2);
      text += u8"\u00ad";
      text.append(word, word.size() / 2, std::u8string::npos);
    } else {
      text += word;
    }
    text += ' ';
  }
  Paragraph paragraph;
  paragraph.appendText(text, basicStyle());
  BlockFlow flow(SkRect::MakeWH(180, 40000));
  const ParagraphLayoutOptions options = knuthPlass();
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 300);
}
BENCHMARK(BM_Layout_KnuthPlass_Hyphenated_300w)->Unit(benchmark::kMicrosecond);

// Three typefaces (serif/sans/mono) alternating span by span, plus CJK
// fallback — whether font mixing taxes the warm path.
void BM_Layout_Greedy_MultiFont_500w(benchmark::State& state) {
  SkFontMgr* fontManager = sigil::test::fonts().fontManager();
  TextStyle styles[3] = {basicStyle(), basicStyle(), basicStyle()};
  styles[0].shaping.typeface =
      fontManager->matchFamilyStyle("Georgia", SkFontStyle());
  styles[1].shaping.typeface =
      fontManager->matchFamilyStyle("Avenir Next", SkFontStyle());
  styles[2].shaping.typeface =
      fontManager->matchFamilyStyle("Menlo", SkFontStyle());
  styles[1].shaping.fontSize = 19.0f;
  styles[2].shaping.fontSize = 14.0f;

  Paragraph paragraph;
  std::mt19937
      randomEngine(  // NOLINT(bugprone-random-generator-seed): a fixed corpus
          7);
  const auto& latin = latinWords();
  const auto& cjk = cjkWords();
  for (int wordIndex = 0; wordIndex < 500; ++wordIndex) {
    std::u8string word = (randomEngine() % 3 == 0)
                             ? cjk[randomEngine() % cjk.size()]
                             : latin[randomEngine() % latin.size()];
    paragraph.appendText(word + u8" ", styles[wordIndex % 3]);
  }
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Layout_Greedy_MultiFont_500w)->Unit(benchmark::kMicrosecond);

// ── The paragraph controls ────────────────────────────────────────────────

// Twelve blocks under twelve paragraph styles — leading kinds, spacing,
// the four indents, a baseline grid — against the same text set flat, so
// what the block model costs is the difference between the two.
void BM_Layout_ParagraphStyles_600w(benchmark::State& state) {
  Paragraph paragraph;
  for (int blockIndex = 0; blockIndex < 12; ++blockIndex) {
    paragraph.appendText(
        makeText(50, /*mixed=*/false, 7u + (uint32_t)blockIndex), basicStyle());
    if (blockIndex + 1 < 12) paragraph.appendText(u8"\n", basicStyle());
  }
  ParagraphLayoutOptions options;
  for (int blockIndex = 0; blockIndex < 12; ++blockIndex) {
    ParagraphStyle style;
    style.leading = blockIndex % 3 == 0   ? Leading::multiple(1.4f)
                    : blockIndex % 3 == 1 ? Leading::grid(24.0f)
                                          : Leading::absolute(21.0f);
    style.spaceBefore = 6.0f;
    style.spaceAfter = 8.0f;
    style.indent.firstLine = blockIndex % 2 == 0 ? 18.0f : -12.0f;
    style.indent.start = 12.0f;
    style.indent.end = 6.0f;
    options.blocks.push_back(style);
  }
  BlockFlow flow(SkRect::MakeWH(420, 40000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 600);
}
BENCHMARK(BM_Layout_ParagraphStyles_600w)->Unit(benchmark::kMicrosecond);

// Justification with all three passes open — word gaps, letter spacing and
// glyph scaling — against the same text justified on gaps alone.
void BM_Layout_JustificationRanges_600w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(600, /*mixed=*/false), basicStyle());
  ParagraphLayoutOptions options = knuthPlass();
  options.justification.letterSpacingMinimum = -0.02f;
  options.justification.letterSpacingMaximum = 0.06f;
  options.justification.glyphScaleMinimum = 0.98f;
  options.justification.glyphScaleMaximum = 1.03f;
  options.justification.singleWord = JustificationOptions::SingleWord::kJustify;
  BlockFlow flow(SkRect::MakeWH(420, 40000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 600);
}
BENCHMARK(BM_Layout_JustificationRanges_600w)->Unit(benchmark::kMicrosecond);

// A reading reserved above every line: the band enters the strut before
// anything is broken, so what it costs a layout is one wider pitch and
// nothing chasing anything.
void BM_Layout_ReservedBand_600w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(600, /*mixed=*/false), basicStyle());
  ParagraphLayoutOptions options;
  TextStyle reading = basicStyle();
  reading.shaping.fontSize = 8.0f;
  options.reserved.before = bandBeside(sigil::test::fonts(), reading, 1.0f);
  BlockFlow flow(SkRect::MakeWH(420, 40000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 600);
}
BENCHMARK(BM_Layout_ReservedBand_600w)->Unit(benchmark::kMicrosecond);

}  // namespace
