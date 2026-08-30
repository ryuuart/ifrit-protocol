// weave_layout_bench — layoutParagraph per word over a warm shape cache,
// so every arm is placement and line breaking with no shaping under it:
// greedy and Knuth-Plass swept over paragraph length, and each kind of
// per-frame update (one-word edit, paint restyle, size restyle, moving
// exclusions, whole-text replacement) against the same warm relayout, so
// what the update itself adds is the difference between two arms. Run a
// Release build; Debug numbers say nothing.

#include <benchmark/benchmark.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <numbers>
#include <random>
#include <string>

#include "support/Corpus.h"

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
  paragraph.appendText(makeText(words, /*mixed=*/true), style16());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(fontContext(), paragraph, flow);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
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
  paragraph.appendText(makeText(words, /*mixed=*/false), style16());
  BlockFlow flow(SkRect::MakeWH(420, 40000));
  const ParagraphLayoutOptions options = knuthPlass();
  layoutParagraph(fontContext(), paragraph, flow, options);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(fontContext(), paragraph, flow, options);
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
  paragraph.appendText(text, style16());
  BlockFlow flow(SkRect::MakeWH(180, 40000));
  const ParagraphLayoutOptions options = knuthPlass();
  layoutParagraph(fontContext(), paragraph, flow, options);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(fontContext(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 300);
}
BENCHMARK(BM_Layout_KnuthPlass_Hyphenated_300w)->Unit(benchmark::kMicrosecond);

// Three typefaces (serif/sans/mono) alternating span by span, plus CJK
// fallback — whether font mixing taxes the warm path.
void BM_Layout_Greedy_MultiFont_500w(benchmark::State& state) {
  SkFontMgr* fontManager = fontContext().fontManager();
  TextStyle styles[3] = {style16(), style16(), style16()};
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
  layoutParagraph(fontContext(), paragraph, flow);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
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
  paragraph.appendText(makeText(500, /*mixed=*/true), style16());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(fontContext(), paragraph, flow);
  // Same-length alternatives so the text stays put across iterations.
  const char8_t* alternatives[] = {u8"changed", u8"updated", u8"swapped",
                                   u8"resized"};
  paragraph.replaceText(0, 3, alternatives[0]);
  int alternativeIndex = 0;
  for ([[maybe_unused]] auto iteration : state) {
    paragraph.replaceText(0, 7, alternatives[(alternativeIndex++) % 4]);
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_EditOneWord_500w)->Unit(benchmark::kMicrosecond);

// The same edit under Knuth-Plass.
void BM_Update_EditOneWord_KnuthPlass_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/false), style16());
  BlockFlow flow(SkRect::MakeWH(420, 40000));
  const ParagraphLayoutOptions options = knuthPlass();
  layoutParagraph(fontContext(), paragraph, flow, options);
  const char8_t* alternatives[] = {u8"changed", u8"updated", u8"swapped",
                                   u8"resized"};
  paragraph.replaceText(0, 3, alternatives[0]);
  int alternativeIndex = 0;
  for ([[maybe_unused]] auto iteration : state) {
    paragraph.replaceText(0, 7, alternatives[(alternativeIndex++) % 4]);
    ParagraphLayout layout =
        layoutParagraph(fontContext(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_EditOneWord_KnuthPlass_500w)->Unit(benchmark::kMicrosecond);

// Paint-only restyle (a colour flash on a word) — must not reshape.
void BM_Update_PaintRestyle_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), style16());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(fontContext(), paragraph, flow);
  SkColor colors[] = {SK_ColorRED, SK_ColorBLUE, SK_ColorGREEN};
  int colorIndex = 0;
  for ([[maybe_unused]] auto iteration : state) {
    paragraph.setPaint(40, 60, PaintStyle{colors[colorIndex++ % 3]});
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_PaintRestyle_500w)->Unit(benchmark::kMicrosecond);

// Shaping-relevant restyle (a size bump on one word).
void BM_Update_SizeRestyle_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), style16());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(fontContext(), paragraph, flow);
  float sizes[] = {18.0f, 20.0f, 22.0f, 24.0f};
  int sizeIndex = 0;
  for ([[maybe_unused]] auto iteration : state) {
    TextStyle style = style16();
    style.shaping.fontSize = sizes[(sizeIndex++) % 4];
    paragraph.setStyle(40, 60, style);
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_SizeRestyle_500w)->Unit(benchmark::kMicrosecond);

// One continuous paint span swept across a 500-word paragraph (crosses
// many lines): re-analysis and placement, zero reshaping.
void BM_Update_SpanRestyleAcrossLines_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), style16());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(fontContext(), paragraph, flow);
  const uint32_t textLength = (uint32_t)paragraph.text().size();
  uint32_t rangeStart = 0;
  for ([[maybe_unused]] auto iteration : state) {
    rangeStart = (rangeStart + 97) % (textLength / 2);
    paragraph.setPaint(rangeStart, rangeStart + textLength / 3,
                       PaintStyle{SK_ColorRED});
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_SpanRestyleAcrossLines_500w)->Unit(benchmark::kMicrosecond);

// Shapes sweeping through a mixed-language paragraph, relayout every
// frame. The text never changes, so this is placement arithmetic.
void BM_Update_MovingExclusions_300w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(300, /*mixed=*/true), style16());
  ExclusionFlow flow(SkRect::MakeWH(700, 3000));
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kCircle, SkRect::MakeXYWH(100, 100, 160, 160), 8});
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kRect, SkRect::MakeXYWH(400, 600, 180, 120), 8});
  layoutParagraph(fontContext(), paragraph, flow);

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
        layoutParagraph(fontContext(), paragraph, flow, options);
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
  paragraph.appendText(makeText(300, /*mixed=*/true), style16());

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
  layoutParagraph(fontContext(), paragraph, flow);

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
        layoutParagraph(fontContext(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 300);
}
BENCHMARK(BM_Update_MovingPathExclusions_300w)->Unit(benchmark::kMicrosecond);

// The entire paragraph text is replaced every frame, cycling four
// variants: after one cycle every word is cache-hot, so this is the
// steady-state cost of "swap the whole text each frame".
void BM_Update_ReplaceWholeParagraph_500w(benchmark::State& state) {
  std::u8string variants[4];
  for (uint32_t variantIndex = 0; variantIndex < 4; ++variantIndex)
    variants[variantIndex] =
        makeText(500, /*mixed=*/true, /*seed=*/variantIndex + 1);
  Paragraph paragraph;
  paragraph.appendText(variants[0], style16());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  for (const std::u8string& variant : variants) {
    paragraph.replaceText(0, (uint32_t)paragraph.text().size(), variant);
    layoutParagraph(fontContext(), paragraph, flow);
  }
  int variantIndex = 0;
  for ([[maybe_unused]] auto iteration : state) {
    paragraph.replaceText(0, (uint32_t)paragraph.text().size(),
                          variants[(++variantIndex) % 4]);
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_ReplaceWholeParagraph_500w)->Unit(benchmark::kMicrosecond);

// The same, but the incoming text has never been seen: every word goes
// through HarfBuzz. The worst case for a full-paragraph update.
void BM_Update_ReplaceWholeParagraph_Cold_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), style16());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(fontContext(), paragraph, flow);
  std::mt19937
      randomEngine(  // NOLINT(bugprone-random-generator-seed): a fixed corpus
          1000);
  for ([[maybe_unused]] auto iteration : state) {
    state.PauseTiming();
    fontContext().purgeShapeCache();
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
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
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
  paragraph.appendText(text, style16());

  LineSetFlow flow;
  for (int tokenIndex = 0; tokenIndex < 2000; ++tokenIndex) {
    const float angle = (float)(randomEngine() % 628) * 0.01f;
    flow.lines().push_back(
        {LineInterval{{20.0f + (float)(randomEngine() % 1360),
                       20.0f + (float)(randomEngine() % 860)},
                      {std::cos(angle), std::sin(angle)},
                      60}});
  }
  layoutParagraph(fontContext(), paragraph, flow);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 2000);
}
BENCHMARK(BM_Confetti_Babel_2000)->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();
