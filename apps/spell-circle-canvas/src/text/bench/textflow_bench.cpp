// TextFlow benchmarks — run in Release:
//   cmake --build build --config Release --target textflow_bench
//   ./build/bin/Release/textflow_bench
//
// The headline numbers this suite exists to confirm:
//   - warm relayout (moving geometry, no text change) is sub-millisecond
//     for real paragraph sizes, and
//   - a one-word edit or restyle costs barely more than a warm relayout
//     (the shape cache absorbs everything else).

#include <textflow/TextFlow.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <include/ports/SkFontMgr_mac_ct.h>

#include <benchmark/benchmark.h>

#include <cmath>
#include <numbers>
#include <random>
#include <string>
#include <vector>

using namespace textflow;

namespace {

FontContext &fontContext() {
  static auto *instance = new FontContext(SkFontMgr_New_CoreText(nullptr));
  return *instance;
}

const std::vector<std::string> &latinWords() {
  static const std::vector<std::string> words = {
      "the",     "quick",   "brown", "fox",     "jumps",   "over",
      "lazy",    "dogs",    "while", "seventy", "wizards", "conjure",
      "spell",   "circles", "of",    "light",   "and",     "shadow",
      "beneath", "ancient", "stars", "text",    "layout",  "engines",
      "measure", "shape",   "place", "glyphs",  "with",    "care"};
  return words;
}

const std::vector<std::string> &cjkWords() {
  static const std::vector<std::string> words = {
      "文字",   "レイアウト", "エンジン", "高速",   "描画", "字形",
      "配置",   "計算",       "한글",     "텍스트", "배치", "엔진",
      "빠르게", "그리기",     "漢字",     "排版",   "引擎", "快速",
      "绘制",   "字体",       "測定",     "測量",   "캐시"};
  return words;
}

std::string makeText(int wordCount, bool mixed, uint32_t seed = 7) {
  std::mt19937 randomEngine(seed);
  const auto &latin = latinWords();
  const auto &cjk = cjkWords();
  std::string text;
  for (int wordIndex = 0; wordIndex < wordCount; ++wordIndex) {
    if (mixed && (randomEngine() % 3 == 0))
      text += cjk[randomEngine() % cjk.size()];
    else
      text += latin[randomEngine() % latin.size()];
    text += ' ';
  }
  return text;
}

TextStyle style16() {
  TextStyle style;
  style.shaping.fontSize = 16.0f;
  return style;
}

} // namespace

// ── Shaping: cold vs cache-hot ────────────────────────────────────────────

static void BM_Shape_Cold_Latin100(benchmark::State &state) {
  const std::string text = makeText(100, /*mixed=*/false);
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    fontContext().purgeShapeCache();
    Paragraph paragraph;
    paragraph.appendText(text, style16());
    paragraph.ensureShaped(fontContext());
    benchmark::DoNotOptimize(paragraph.words().data());
  }
}
BENCHMARK(BM_Shape_Cold_Latin100)->Unit(benchmark::kMicrosecond);

static void BM_Shape_Cold_Mixed100(benchmark::State &state) {
  const std::string text = makeText(100, /*mixed=*/true);
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    fontContext().purgeShapeCache();
    Paragraph paragraph;
    paragraph.appendText(text, style16());
    paragraph.ensureShaped(fontContext());
    benchmark::DoNotOptimize(paragraph.words().data());
  }
}
BENCHMARK(BM_Shape_Cold_Mixed100)->Unit(benchmark::kMicrosecond);

static void BM_Shape_Warm_Mixed100(benchmark::State &state) {
  const std::string text = makeText(100, /*mixed=*/true);
  {
    Paragraph warmup;
    warmup.appendText(text, style16());
    warmup.ensureShaped(fontContext());
  }
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    Paragraph
        paragraph; // fresh paragraph, warm cache: full analysis, no hb_shape
    paragraph.appendText(text, style16());
    paragraph.ensureShaped(fontContext());
    benchmark::DoNotOptimize(paragraph.words().data());
  }
}
BENCHMARK(BM_Shape_Warm_Mixed100)->Unit(benchmark::kMicrosecond);

// ── Full layout, cold and warm ────────────────────────────────────────────

static void BM_Layout_Warm(benchmark::State &state) {
  const int words = static_cast<int>(state.range(0));
  Paragraph paragraph;
  paragraph.appendText(makeText(words, /*mixed=*/true), style16());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(fontContext(), paragraph, flow); // warm everything
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
}
BENCHMARK(BM_Layout_Warm)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Arg(10000)
    ->Unit(benchmark::kMicrosecond);

// ── The acceptance scenarios ──────────────────────────────────────────────

// One word of a ~500-word mixed paragraph changes per frame (rich-text
// update): everything else must come out of the shape cache.
static void BM_Update_EditOneWord_500w(benchmark::State &state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), style16());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(fontContext(), paragraph, flow);
  // Same-length alternatives so the text stays put across iterations.
  const char *alternatives[] = {"changed", "updated", "swapped", "resized"};
  paragraph.replaceText(0, 3, alternatives[0]);
  int alternativeIndex = 0;
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    paragraph.replaceText(0, 7, alternatives[alternativeIndex++ & 3]);
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
}
BENCHMARK(BM_Update_EditOneWord_500w)->Unit(benchmark::kMicrosecond);

// Paint-only restyle (color flash on a word) — must not reshape anything.
static void BM_Update_PaintRestyle_500w(benchmark::State &state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), style16());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(fontContext(), paragraph, flow);
  SkColor colors[] = {SK_ColorRED, SK_ColorBLUE, SK_ColorGREEN};
  int colorIndex = 0;
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    paragraph.setPaint(40, 60, PaintStyle{colors[colorIndex++ % 3]});
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
}
BENCHMARK(BM_Update_PaintRestyle_500w)->Unit(benchmark::kMicrosecond);

// Shaping-relevant restyle (size bump on one word).
static void BM_Update_SizeRestyle_500w(benchmark::State &state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), style16());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(fontContext(), paragraph, flow);
  float sizes[] = {18.0f, 20.0f, 22.0f, 24.0f};
  int sizeIndex = 0;
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    TextStyle style = style16();
    style.shaping.fontSize = sizes[sizeIndex++ & 3];
    paragraph.setStyle(40, 60, style);
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
}
BENCHMARK(BM_Update_SizeRestyle_500w)->Unit(benchmark::kMicrosecond);

// Shapes sweeping through a mixed-language paragraph, relayout every frame
// (acceptance test #1). Pure placement arithmetic — zero reshaping.
static void BM_Update_MovingExclusions_300w(benchmark::State &state) {
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
  for ([[maybe_unused]] auto benchmarkIteration : state) {
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
}
BENCHMARK(BM_Update_MovingExclusions_300w)->Unit(benchmark::kMicrosecond);

// Same sweep, but the obstacles are arbitrary SkPaths (a star and a donut
// with a live hole). Moving via pathOffset reuses the cached flattening, so
// the per-frame cost is scanline interval math, not path processing.
static void BM_Update_MovingPathExclusions_300w(benchmark::State &state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(300, /*mixed=*/true), style16());

  SkPathBuilder star;
  for (int pointIndex = 0; pointIndex < 5; ++pointIndex) {
    const float angle = -std::numbers::pi_v<float> / 2.0f +
                        static_cast<float>(pointIndex) * 4.0f *
                            std::numbers::pi_v<float> / 5.0f;
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
  for ([[maybe_unused]] auto benchmarkIteration : state) {
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
}
BENCHMARK(BM_Update_MovingPathExclusions_300w)->Unit(benchmark::kMicrosecond);

// ── Font/style mixing and whole-paragraph updates ─────────────────────────

// Three typefaces (serif/sans/mono) alternating span by span, plus CJK
// fallback — measures whether font mixing taxes the warm path.
static void BM_Layout_Warm_MultiFont_500w(benchmark::State &state) {
  SkFontMgr *fontManager = fontContext().fontManager();
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
  std::mt19937 randomEngine(7);
  const auto &latin = latinWords();
  const auto &cjk = cjkWords();
  for (int wordIndex = 0; wordIndex < 500; ++wordIndex) {
    std::string word = (randomEngine() % 3 == 0)
                           ? cjk[randomEngine() % cjk.size()]
                           : latin[randomEngine() % latin.size()];
    paragraph.appendText(word + " ", styles[wordIndex % 3]);
  }
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(fontContext(), paragraph, flow);
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
}
BENCHMARK(BM_Layout_Warm_MultiFont_500w)->Unit(benchmark::kMicrosecond);

// The entire paragraph text is replaced every frame, cycling four variants:
// after one cycle every word is cache-hot, so this is the steady-state cost
// of "swap the whole text each frame".
static void BM_Update_ReplaceWholeParagraph_500w(benchmark::State &state) {
  std::string variants[4];
  for (uint32_t variantIndex = 0; variantIndex < 4; ++variantIndex)
    variants[variantIndex] =
        makeText(500, /*mixed=*/true, /*seed=*/variantIndex + 1);
  Paragraph paragraph;
  paragraph.appendText(variants[0], style16());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  for (uint32_t variantIndex = 0; variantIndex < 4; ++variantIndex) {
    paragraph.replaceText(0, static_cast<uint32_t>(paragraph.text().size()),
                          variants[variantIndex]);
    layoutParagraph(fontContext(), paragraph, flow);
  }
  int variantIndex = 0;
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    paragraph.replaceText(0, static_cast<uint32_t>(paragraph.text().size()),
                          variants[++variantIndex & 3]);
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
}
BENCHMARK(BM_Update_ReplaceWholeParagraph_500w)->Unit(benchmark::kMicrosecond);

// Same, but the incoming text has never been seen: every word goes through
// HarfBuzz. The true worst case for a full-paragraph update.
static void BM_Update_ReplaceWholeParagraph_Cold_500w(benchmark::State &state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), style16());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(fontContext(), paragraph, flow);
  std::mt19937 randomEngine(1000);
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    state.PauseTiming();
    fontContext().purgeShapeCache(); // force truly-unseen text
    // Synthesize unique gibberish words so no shape can be reused, unlike
    // makeText's small vocabulary.
    std::string next;
    for (int wordIndex = 0; wordIndex < 500; ++wordIndex) {
      const int wordLength = 3 + static_cast<int>(randomEngine() % 8);
      for (int characterIndex = 0; characterIndex < wordLength;
           ++characterIndex)
        next += static_cast<char>('a' + randomEngine() % 26);
      next += ' ';
    }
    state.ResumeTiming();
    paragraph.replaceText(0, static_cast<uint32_t>(paragraph.text().size()),
                          next);
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
}
BENCHMARK(BM_Update_ReplaceWholeParagraph_Cold_500w)
    ->Unit(benchmark::kMicrosecond);

// One continuous paint span swept across a 500-word paragraph (crosses many
// lines): pure re-analysis + placement, zero reshaping.
static void BM_Update_SpanRestyleAcrossLines_500w(benchmark::State &state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), style16());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(fontContext(), paragraph, flow);
  const uint32_t textLength = static_cast<uint32_t>(paragraph.text().size());
  uint32_t rangeStart = 0;
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    rangeStart = (rangeStart + 97) % (textLength / 2);
    paragraph.setPaint(rangeStart, rangeStart + textLength / 3,
                       PaintStyle{SK_ColorRED});
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
}
BENCHMARK(BM_Update_SpanRestyleAcrossLines_500w)->Unit(benchmark::kMicrosecond);

// ── Knuth-Plass (acceptance test #2) ──────────────────────────────────────

static void BM_KnuthPlass_Warm(benchmark::State &state) {
  const int words = static_cast<int>(state.range(0));
  Paragraph paragraph;
  paragraph.appendText(makeText(words, /*mixed=*/false), style16());
  BlockFlow flow(SkRect::MakeWH(420, 40000));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  layoutParagraph(fontContext(), paragraph, flow, options);
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    ParagraphLayout layout =
        layoutParagraph(fontContext(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
}
BENCHMARK(BM_KnuthPlass_Warm)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Arg(10000)
    ->Unit(benchmark::kMicrosecond);

static void BM_KnuthPlass_EditOneWord_500w(benchmark::State &state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/false), style16());
  BlockFlow flow(SkRect::MakeWH(420, 40000));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  layoutParagraph(fontContext(), paragraph, flow, options);
  const char *alternatives[] = {"changed", "updated", "swapped", "resized"};
  paragraph.replaceText(0, 3, alternatives[0]);
  int alternativeIndex = 0;
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    paragraph.replaceText(0, 7, alternatives[alternativeIndex++ & 3]);
    ParagraphLayout layout =
        layoutParagraph(fontContext(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
}
BENCHMARK(BM_KnuthPlass_EditOneWord_500w)->Unit(benchmark::kMicrosecond);

// Knuth-Plass over text dense with soft hyphens (every word carries
// discretionary break points) on a narrow measure.
static void BM_KnuthPlass_Hyphenated_300w(benchmark::State &state) {
  std::mt19937 randomEngine(7);
  const auto &latin = latinWords();
  std::string text;
  for (int wordIndex = 0; wordIndex < 300; ++wordIndex) {
    const std::string &word = latin[randomEngine() % latin.size()];
    if (word.size() > 4) { // Inject a soft hyphen mid-word.
      text.append(word, 0, word.size() / 2);
      text += "\xc2\xad"; // U+00AD
      text.append(word, word.size() / 2, std::string::npos);
    } else {
      text += word;
    }
    text += ' ';
  }
  Paragraph paragraph;
  paragraph.appendText(text, style16());
  BlockFlow flow(SkRect::MakeWH(180, 40000));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  layoutParagraph(fontContext(), paragraph, flow, options);
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    ParagraphLayout layout =
        layoutParagraph(fontContext(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
}
BENCHMARK(BM_KnuthPlass_Hyphenated_300w)->Unit(benchmark::kMicrosecond);

// ── Draw (CPU raster, for reference — GPU is the production path) ─────────

static void BM_Draw_Raster_300w(benchmark::State &state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(300, /*mixed=*/true), style16());
  BlockFlow flow(SkRect::MakeWH(700, 3000));
  ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(720, 1400));
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    surface->getCanvas()->clear(SK_ColorWHITE);
    layout.draw(surface->getCanvas(), paragraph);
    benchmark::DoNotOptimize(surface.get());
  }
}
BENCHMARK(BM_Draw_Raster_300w)->Unit(benchmark::kMicrosecond);

// 2000 multi-script tokens (Arabic, Devanagari, Hebrew, Thai, CJK, emoji,
// Greek, Cyrillic, Tamil, runes) scattered over 2000 rotated intervals —
// letter-confetti at paragraph scale. Warm: placement + per-glyph RSXform
// baking; every token still shape-cache resolved.
static void BM_Confetti_Babel_2000(benchmark::State &state) {
  const char *tokens[] = {"حرف",   "كلمة",   "अक्षर", "शब्द",   "אות",
                          "מילה",  "ตัวอักษร", "字",   "글",    "λόγος",
                          "буква", "🎉",     "👍🏽", "文字",  "ঢাকা",
                          "கடல்",   "ᚱᚢᚾ",    "ainm", "słowo", "λέξη"};
  std::mt19937 randomEngine(77);
  Paragraph paragraph;
  std::string text;
  for (int tokenIndex = 0; tokenIndex < 2000; ++tokenIndex) {
    text += tokens[randomEngine() % 20];
    text += ' ';
  }
  paragraph.appendText(text, style16());

  LineSetFlow flow;
  for (int tokenIndex = 0; tokenIndex < 2000; ++tokenIndex) {
    const float angle = static_cast<float>(randomEngine() % 628) * 0.01f;
    flow.lines().push_back(
        {LineInterval{{20.0f + static_cast<float>(randomEngine() % 1360),
                       20.0f + static_cast<float>(randomEngine() % 860)},
                      {std::cos(angle), std::sin(angle)},
                      60}});
  }
  layoutParagraph(fontContext(), paragraph, flow);
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
}
BENCHMARK(BM_Confetti_Babel_2000)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
