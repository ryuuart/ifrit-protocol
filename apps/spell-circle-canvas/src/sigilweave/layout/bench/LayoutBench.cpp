/** @file
 * weave_layout_bench — layoutParagraph per word over a warm shape cache,
 * so every arm is placement and line breaking with no shaping under it:
 * greedy and Knuth-Plass swept over paragraph length, and each kind of
 * per-frame update (one-word edit, paint restyle, size restyle, moving
 * exclusions in lines and in columns, whole-text replacement) against the
 * same warm relayout, so what the update itself adds is the difference
 * between two arms. Run a Release build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkPathBuilder.h>
#include <sigilweave/layout/Beside.h>

#include <cmath>
#include <numbers>
#include <random>
#include <string>
#include <vector>

#include "support/Corpus.h"
#include "support/Layouts.h"

using namespace sigil::weave;
using namespace sigil::weave::bench;

namespace {

void countWords(benchmark::State& state, int64_t words) {
  state.counters["words/s"] = benchmark::Counter(
      (double)words, benchmark::Counter::kIsIterationInvariantRate);
}

ParagraphLayoutOptions knuthPlass() {
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  return options;
}

// ── Warm relayout by length, both breakers ───────────────────────────────

void BM_Layout_Greedy(benchmark::State& state) {
  const int words = (int)state.range(0);
  Paragraph paragraph;
  paragraph.appendText(makeText(words, /*mixed=*/true), basicStyle());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
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
    ParagraphLayout layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Layout_Greedy_MultiFont_500w)->Unit(benchmark::kMicrosecond);

// ── Per-frame update scenarios ────────────────────────────────────────────

// One word of a 500-word mixed paragraph changes per frame: everything
// else must come out of the shape cache.
void BM_Update_EditOneWord_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), basicStyle());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow);
  // Same-length alternatives so the text stays put across iterations.
  const char8_t* alternatives[] = {u8"changed", u8"updated", u8"swapped",
                                   u8"resized"};
  paragraph.replaceText(0, 3, alternatives[0]);
  int alternativeIndex = 0;
  for ([[maybe_unused]] auto iteration : state) {
    paragraph.replaceText(0, 7, alternatives[(alternativeIndex++) % 4]);
    ParagraphLayout layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_EditOneWord_500w)->Unit(benchmark::kMicrosecond);

// The same edit under Knuth-Plass.
void BM_Update_EditOneWord_KnuthPlass_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/false), basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 40000));
  const ParagraphLayoutOptions options = knuthPlass();
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  const char8_t* alternatives[] = {u8"changed", u8"updated", u8"swapped",
                                   u8"resized"};
  paragraph.replaceText(0, 3, alternatives[0]);
  int alternativeIndex = 0;
  for ([[maybe_unused]] auto iteration : state) {
    paragraph.replaceText(0, 7, alternatives[(alternativeIndex++) % 4]);
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_EditOneWord_KnuthPlass_500w)->Unit(benchmark::kMicrosecond);

// Paint-only restyle (a colour flash on a word) — must not reshape.
void BM_Update_PaintRestyle_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), basicStyle());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow);
  SkColor colors[] = {SK_ColorRED, SK_ColorBLUE, SK_ColorGREEN};
  int colorIndex = 0;
  for ([[maybe_unused]] auto iteration : state) {
    paragraph.setPaint(40, 60, PaintStyle{colors[colorIndex++ % 3]});
    ParagraphLayout layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_PaintRestyle_500w)->Unit(benchmark::kMicrosecond);

// The same colour flash on a paragraph whose text is thirty times what the
// geometry holds, restyled inside the placed window. A paint edit runs no
// analysis over the text that never fits, so what this arm is read against
// is the fully-placed one above rather than its own word count.
void BM_Update_PaintRestyle_Overflowed_30000w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(30000, /*mixed=*/false), basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 320));
  ParagraphLayout warm = layoutParagraph(sigil::test::fonts(), paragraph, flow);
  const uint32_t placedEnd =
      paragraph.words()[warm.firstUnplacedWord].textBegin;
  uint32_t frame = 0;
  for ([[maybe_unused]] auto iteration : state) {
    PaintStyle hue(0xFF000000u | (frame++ * 1234567u));
    paragraph.setPaint(0, placedEnd, hue);
    ParagraphLayout layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 30000);
}
BENCHMARK(BM_Update_PaintRestyle_Overflowed_30000w)
    ->Unit(benchmark::kMicrosecond);

// Shaping-relevant restyle (a size bump on one word).
void BM_Update_SizeRestyle_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), basicStyle());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow);
  float sizes[] = {18.0f, 20.0f, 22.0f, 24.0f};
  int sizeIndex = 0;
  for ([[maybe_unused]] auto iteration : state) {
    TextStyle style = basicStyle();
    style.shaping.fontSize = sizes[(sizeIndex++) % 4];
    paragraph.setStyle(40, 60, style);
    ParagraphLayout layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_SizeRestyle_500w)->Unit(benchmark::kMicrosecond);

// One continuous paint span swept across a 500-word paragraph (crosses
// many lines): re-analysis and placement, zero reshaping.
void BM_Update_SpanRestyleAcrossLines_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), basicStyle());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow);
  const uint32_t textLength = (uint32_t)paragraph.text().size();
  uint32_t rangeStart = 0;
  for ([[maybe_unused]] auto iteration : state) {
    rangeStart = (rangeStart + 97) % (textLength / 2);
    paragraph.setPaint(rangeStart, rangeStart + textLength / 3,
                       PaintStyle{SK_ColorRED});
    ParagraphLayout layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_SpanRestyleAcrossLines_500w)->Unit(benchmark::kMicrosecond);

// Shapes sweeping through a mixed-language paragraph, relayout every
// frame. The text never changes, so this is placement arithmetic.
void BM_Update_MovingExclusions_300w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(300, /*mixed=*/true), basicStyle());
  ExclusionFlow flow(SkRect::MakeWH(700, 3000));
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kCircle, SkRect::MakeXYWH(100, 100, 160, 160), 8});
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kRect, SkRect::MakeXYWH(400, 600, 180, 120), 8});
  layoutParagraph(sigil::test::fonts(), paragraph, flow);

  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  float animationTime = 0;
  for ([[maybe_unused]] auto iteration : state) {
    animationTime += 0.03f;
    flow.shapes()[0].bounds = SkRect::MakeXYWH(
        100 + 200 * std::sin(animationTime),
        100 + 900 * (0.5f + 0.5f * std::sin(animationTime * 0.7f)), 160, 160);
    flow.shapes()[1].bounds =
        SkRect::MakeXYWH(400 - 150 * std::cos(animationTime),
                         600 + 300 * std::sin(animationTime * 1.3f), 180, 120);
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 300);
}
BENCHMARK(BM_Update_MovingExclusions_300w)->Unit(benchmark::kMicrosecond);

// The same sweep with arbitrary SkPath obstacles (a star and a donut with
// a live hole). Moving via pathOffset reuses the cached flattening, so
// the per-frame cost is scanline interval math, not path processing.
void BM_Update_MovingPathExclusions_300w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(300, /*mixed=*/true), basicStyle());

  SkPathBuilder star;
  for (int pointIndex = 0; pointIndex < 5; ++pointIndex) {
    const float angle =
        -std::numbers::pi_v<float> / 2.0f +
        (float)pointIndex * 4.0f * std::numbers::pi_v<float> / 5.0f;
    const SkPoint point = {150 + 110 * std::cos(angle),
                           150 + 110 * std::sin(angle)};
    if (pointIndex == 0)
      star.moveTo(point);
    else
      star.lineTo(point);
  }
  star.close();
  SkPathBuilder donut;
  donut.addCircle(450, 700, 110);
  donut.addCircle(450, 700, 55);
  donut.setFillType(SkPathFillType::kEvenOdd);

  ExclusionFlow flow(SkRect::MakeWH(700, 3000));
  flow.shapes().push_back(ExclusionFlow::Shape::fromPath(star.detach(), 8));
  flow.shapes().push_back(ExclusionFlow::Shape::fromPath(donut.detach(), 8));
  layoutParagraph(sigil::test::fonts(), paragraph, flow);

  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  float animationTime = 0;
  for ([[maybe_unused]] auto iteration : state) {
    animationTime += 0.03f;
    flow.shapes()[0].pathOffset = {
        200 * std::sin(animationTime),
        900 * (0.5f + 0.5f * std::sin(animationTime * 0.7f))};
    flow.shapes()[1].pathOffset = {-150 * std::cos(animationTime),
                                   300 * std::sin(animationTime * 1.3f)};
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 300);
}
BENCHMARK(BM_Update_MovingPathExclusions_300w)->Unit(benchmark::kMicrosecond);

// The same sweep read DOWN COLUMNS: the identical star and donut, met by
// a column flow instead of a line flow. It is the attribution arm for the
// per-column band scan — the same scan a quarter turn later, over a
// corpus whose break opportunities sit between characters.
void BM_Update_MovingColumnExclusions_300w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeColumnText(300), basicStyle());
  paragraph.setWritingMode(WritingMode::kVerticalRL);

  SkPathBuilder star;
  for (int pointIndex = 0; pointIndex < 5; ++pointIndex) {
    const float angle =
        -std::numbers::pi_v<float> / 2.0f +
        (float)pointIndex * 4.0f * std::numbers::pi_v<float> / 5.0f;
    const SkPoint point = {150 + 110 * std::cos(angle),
                           150 + 110 * std::sin(angle)};
    if (pointIndex == 0)
      star.moveTo(point);
    else
      star.lineTo(point);
  }
  star.close();
  SkPathBuilder donut;
  donut.addCircle(700, 450, 110);
  donut.addCircle(700, 450, 55);
  donut.setFillType(SkPathFillType::kEvenOdd);

  ExclusionFlow flow(SkRect::MakeWH(3000, 700), FlowAxis::kColumns);
  flow.shapes().push_back(ExclusionFlow::Shape::fromPath(star.detach(), 8));
  flow.shapes().push_back(ExclusionFlow::Shape::fromPath(donut.detach(), 8));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 26;  // column pitch
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);

  float animationTime = 0;
  for ([[maybe_unused]] auto iteration : state) {
    animationTime += 0.03f;
    flow.shapes()[0].pathOffset = {
        900 * (0.5f + 0.5f * std::sin(animationTime * 0.7f)),
        200 * std::sin(animationTime)};
    flow.shapes()[1].pathOffset = {300 * std::sin(animationTime * 1.3f),
                                   -150 * std::cos(animationTime)};
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 300);
}
BENCHMARK(BM_Update_MovingColumnExclusions_300w)->Unit(benchmark::kMicrosecond);

// A clamped column with an overflow marker at its foot: the trim walks
// back up the column shaping nothing new, so what this measures against
// the unclamped column arm is the marker's own cost.
void BM_Layout_Vertical_ClampedEllipsis_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeColumnText(500), basicStyle());
  paragraph.setWritingMode(WritingMode::kVerticalRL);
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 26;  // column pitch
  options.overflow.maxLines = 8;
  options.overflow.ellipsis = u"\u2026";
  VerticalBlockFlow flow(SkRect::MakeWH(20000, 600));
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Layout_Vertical_ClampedEllipsis_500w)
    ->Unit(benchmark::kMicrosecond);

// The entire paragraph text is replaced every frame, cycling four
// variants: after one cycle every word is cache-hot, so this is the
// steady-state cost of "swap the whole text each frame".
void BM_Update_ReplaceWholeParagraph_500w(benchmark::State& state) {
  std::u8string variants[4];
  for (uint32_t variantIndex = 0; variantIndex < 4; ++variantIndex)
    variants[variantIndex] =
        makeText(500, /*mixed=*/true, /*seed=*/variantIndex + 1);
  Paragraph paragraph;
  paragraph.appendText(variants[0], basicStyle());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  for (const std::u8string& variant : variants) {
    paragraph.replaceText(0, (uint32_t)paragraph.text().size(), variant);
    layoutParagraph(sigil::test::fonts(), paragraph, flow);
  }
  int variantIndex = 0;
  for ([[maybe_unused]] auto iteration : state) {
    paragraph.replaceText(0, (uint32_t)paragraph.text().size(),
                          variants[(++variantIndex) % 4]);
    ParagraphLayout layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_ReplaceWholeParagraph_500w)->Unit(benchmark::kMicrosecond);

// The same, but the incoming text has never been seen: every word goes
// through HarfBuzz. The worst case for a full-paragraph update.
void BM_Update_ReplaceWholeParagraph_Cold_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), basicStyle());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow);
  std::mt19937
      randomEngine(  // NOLINT(bugprone-random-generator-seed): a fixed corpus
          1000);
  for ([[maybe_unused]] auto iteration : state) {
    state.PauseTiming();
    sigil::test::fonts().purgeShapeCache();
    // Unique gibberish words so no shape can be reused, unlike makeText's
    // small vocabulary.
    std::u8string next;
    for (int wordIndex = 0; wordIndex < 500; ++wordIndex) {
      const int wordLength = 3 + (int)(randomEngine() % 8);
      for (int characterIndex = 0; characterIndex < wordLength;
           ++characterIndex)
        next += (char8_t)('a' + randomEngine() % 26);
      next += ' ';
    }
    state.ResumeTiming();
    paragraph.replaceText(0, (uint32_t)paragraph.text().size(), next);
    ParagraphLayout layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_ReplaceWholeParagraph_Cold_500w)
    ->Unit(benchmark::kMicrosecond);

// 2000 multi-script tokens scattered over 2000 rotated intervals —
// letter-confetti at paragraph scale. Warm: placement plus per-glyph
// RSXform baking; every token still shape-cache resolved.
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
    ParagraphLayout layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 2000);
}
BENCHMARK(BM_Confetti_Babel_2000)->Unit(benchmark::kMicrosecond);

// ── The live composer ─────────────────────────────────────────────────────
//
// These are the arms the realtime half of the engine is judged on: a story
// of the size a page holds, composed under an input that MOVES. The
// measure animating and the content churning are the two ways text moves,
// and both are answered by the same composer with the same precompute
// behind it.

ParagraphLayoutOptions liveComposer() {
  ParagraphLayoutOptions options = knuthPlass();
  options.live = true;
  return options;
}

// A 600-word story at a measure that moves a pixel per frame: every frame
// is a measure the break store has not seen, so every frame decides its
// breaks. This is the composer's worst honest case.
void BM_Live_Composer_AnimatingMeasure_600w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(600, /*mixed=*/false), basicStyle());
  const ParagraphLayoutOptions options = liveComposer();
  float measure = 380.0f;
  BlockFlow warm(SkRect::MakeWH(measure, 40000));
  layoutParagraph(sigil::test::fonts(), paragraph, warm, options);
  int frame = 0;
  for ([[maybe_unused]] auto iteration : state) {
    BlockFlow flow(SkRect::MakeWH(measure + (float)(frame++ % 120), 40000));
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 600);
}
BENCHMARK(BM_Live_Composer_AnimatingMeasure_600w)
    ->Unit(benchmark::kMicrosecond);

// The same story at a measure that returns to widths already seen: the
// break store answers and the frame costs its fill alone.
void BM_Live_Composer_SeenMeasure_600w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(600, /*mixed=*/false), basicStyle());
  const ParagraphLayoutOptions options = liveComposer();
  BlockFlow flow(SkRect::MakeWH(380, 40000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 600);
}
BENCHMARK(BM_Live_Composer_SeenMeasure_600w)->Unit(benchmark::kMicrosecond);

// A 600-word story with one word in twenty replaced every frame — a
// scramble, a decode, a counter, a feed. The shape cache answers for the
// words that did not change and the composer decides the whole story's
// breaks again, because a word that moved moves every break after it.
void BM_Live_Composer_ContentChurn_600w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(600, /*mixed=*/false), basicStyle());
  const ParagraphLayoutOptions options = liveComposer();
  BlockFlow flow(SkRect::MakeWH(380, 40000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  // Same-length replacements, so the churn moves glyphs and not offsets.
  const char8_t* alternatives[] = {u8"aaaa", u8"bbbb", u8"cccc", u8"dddd"};
  std::mt19937
      randomEngine(  // NOLINT(bugprone-random-generator-seed): a fixed corpus
          19);
  std::vector<uint32_t> starts;
  for (const Word& word : paragraph.words())
    if (word.textEnd - word.textBegin == 4) starts.push_back(word.textBegin);
  int frame = 0;
  for ([[maybe_unused]] auto iteration : state) {
    const size_t churn = starts.size() / 20;  // one word in twenty
    for (size_t index = 0; index < churn && !starts.empty(); ++index)
      paragraph.replaceText(starts[(size_t)(randomEngine() % starts.size())], 4,
                            alternatives[(frame + (int)index) % 4]);
    ++frame;
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 600);
}
BENCHMARK(BM_Live_Composer_ContentChurn_600w)->Unit(benchmark::kMicrosecond);

// A 600-word story refilled through a chain of six frames every frame —
// the threaded case, where each fill resumes at the word the one before it
// reported and the whole story is shaped once.
void BM_Live_Story_Refill_600w_SixFrames(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(600, /*mixed=*/false), basicStyle());
  const ParagraphLayoutOptions options = liveComposer();
  for ([[maybe_unused]] auto iteration : state) {
    uint32_t cursor = 0;
    for (int frameIndex = 0; frameIndex < 6; ++frameIndex) {
      BlockFlow flow(SkRect::MakeWH(380, 700));
      ParagraphLayout layout =
          layoutParagraph(sigil::test::fonts(), paragraph, flow, options, cursor);
      benchmark::DoNotOptimize(layout.runs.data());
      if (!layout.overflowed()) break;
      cursor = layout.firstUnplacedWord;
    }
  }
  countWords(state, 600);
}
BENCHMARK(BM_Live_Story_Refill_600w_SixFrames)->Unit(benchmark::kMicrosecond);

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

BENCHMARK_MAIN();
