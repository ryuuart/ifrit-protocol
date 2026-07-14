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

const std::vector<std::u8string> &latinWords() {
  static const std::vector<std::u8string> words = {
      u8"the",     u8"quick",   u8"brown",  u8"fox",     u8"jumps",
      u8"over",    u8"lazy",    u8"dogs",   u8"while",   u8"seventy",
      u8"wizards", u8"conjure", u8"spell",  u8"circles", u8"of",
      u8"light",   u8"and",     u8"shadow", u8"beneath", u8"ancient",
      u8"stars",   u8"text",    u8"layout", u8"engines", u8"measure",
      u8"shape",   u8"place",   u8"glyphs", u8"with",    u8"care"};
  return words;
}

const std::vector<std::u8string> &cjkWords() {
  static const std::vector<std::u8string> words = {
      u8"文字",   u8"レイアウト", u8"エンジン", u8"高速",   u8"描画", u8"字形",
      u8"配置",   u8"計算",       u8"한글",     u8"텍스트", u8"배치", u8"엔진",
      u8"빠르게", u8"그리기",     u8"漢字",     u8"排版",   u8"引擎", u8"快速",
      u8"绘制",   u8"字体",       u8"測定",     u8"測量",   u8"캐시"};
  return words;
}

std::u8string makeText(int wordCount, bool mixed, uint32_t seed = 7) {
  std::mt19937 randomEngine(seed);
  const auto &latin = latinWords();
  const auto &cjk = cjkWords();
  std::u8string text;
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
  const std::u8string text = makeText(100, /*mixed=*/false);
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
  const std::u8string text = makeText(100, /*mixed=*/true);
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
  const std::u8string text = makeText(100, /*mixed=*/true);
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
  const char8_t *alternatives[] = {u8"changed", u8"updated", u8"swapped",
                                   u8"resized"};
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
    std::u8string word = (randomEngine() % 3 == 0)
                             ? cjk[randomEngine() % cjk.size()]
                             : latin[randomEngine() % latin.size()];
    paragraph.appendText(word + u8" ", styles[wordIndex % 3]);
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
  std::u8string variants[4];
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
    std::u8string next;
    for (int wordIndex = 0; wordIndex < 500; ++wordIndex) {
      const int wordLength = 3 + static_cast<int>(randomEngine() % 8);
      for (int characterIndex = 0; characterIndex < wordLength;
           ++characterIndex)
        next += static_cast<char8_t>('a' + randomEngine() % 26);
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
  const char8_t *alternatives[] = {u8"changed", u8"updated", u8"swapped",
                                   u8"resized"};
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
  std::u8string text;
  for (int wordIndex = 0; wordIndex < 300; ++wordIndex) {
    const std::u8string &word = latin[randomEngine() % latin.size()];
    if (word.size() > 4) { // Inject a soft hyphen mid-word.
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

static void BM_DrawBatched_Raster_300w(benchmark::State &state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(300, /*mixed=*/true), style16());
  BlockFlow flow(SkRect::MakeWH(700, 3000));
  ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(720, 1400));
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    surface->getCanvas()->clear(SK_ColorWHITE);
    layout.drawBatched(surface->getCanvas(), paragraph);
    benchmark::DoNotOptimize(surface.get());
  }
}
BENCHMARK(BM_DrawBatched_Raster_300w)->Unit(benchmark::kMicrosecond);

// Four ordered glyph passes: blurred shadow, blurred glow, outline, fill.
// This intentionally exposes the cost users opt into: draw submission scales
// with pass count, while blur-mask work is backend/font-size dependent.
static void BM_DrawBatched_Raster_300w_4PassEffects(benchmark::State &state) {
  TextStyle layered = style16();
  layered.paint.addUnderlay(PaintLayer::dropShadow(0x66000000, {2, 2}, 2.0f))
      .addUnderlay(PaintLayer::glow(0x440000FF, 3.0f))
      .addUnderlay(PaintLayer::outline(SK_ColorBLACK, 1.5f));
  Paragraph paragraph;
  paragraph.appendText(makeText(300, /*mixed=*/true), layered);
  BlockFlow flow(SkRect::MakeWH(700, 3000));
  ParagraphLayout layout = layoutParagraph(fontContext(), paragraph, flow);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(720, 1400));
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    surface->getCanvas()->clear(SK_ColorWHITE);
    layout.drawBatched(surface->getCanvas(), paragraph);
    benchmark::DoNotOptimize(surface.get());
  }
}
BENCHMARK(BM_DrawBatched_Raster_300w_4PassEffects)
    ->Unit(benchmark::kMicrosecond);

static Paragraph makeDrawStressParagraph(bool effects) {
  TextStyle textStyle = style16();
  textStyle.shaping.fontSize = 8.0f;
  if (effects) {
    const SkRect bounds = SkRect::MakeXYWH(10, 10, 1180, 880);
    textStyle.paint
        .addUnderlay(PaintLayer::glow(0x772A77FF, 1.8f))
        .addUnderlay(PaintLayer::outline(0xFF061229, 0.7f));
    textStyle.paint.foreground.setShader(
        PaintShaders::meshGradient(bounds, 1.25f));
    SkPaint stars;
    stars.setAntiAlias(true);
    stars.setShader(PaintShaders::sparkle(bounds, 1.25f));
    stars.setBlendMode(SkBlendMode::kScreen);
    textStyle.paint.addOverlay(PaintLayer(std::move(stars)));
  }
  Paragraph paragraph;
  paragraph.appendText(makeText(2000, /*mixed=*/true), textStyle);
  return paragraph;
}

static void BM_DrawBatched_Raster_2000w(benchmark::State &state) {
  Paragraph paragraph = makeDrawStressParagraph(false);
  BlockFlow flow(SkRect::MakeXYWH(10, 10, 1180, 880));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 10.0f;
  ParagraphLayout layout =
      layoutParagraph(fontContext(), paragraph, flow, options);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1200, 900));
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    surface->getCanvas()->clear(0xFF050A18);
    layout.drawBatched(surface->getCanvas(), paragraph);
    benchmark::DoNotOptimize(surface.get());
  }
}
BENCHMARK(BM_DrawBatched_Raster_2000w)->Unit(benchmark::kMicrosecond);

static void BM_DrawBatched_Raster_2000w_4PassShaderEffects(
    benchmark::State &state) {
  Paragraph paragraph = makeDrawStressParagraph(true);
  BlockFlow flow(SkRect::MakeXYWH(10, 10, 1180, 880));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 10.0f;
  ParagraphLayout layout =
      layoutParagraph(fontContext(), paragraph, flow, options);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1200, 900));
  for ([[maybe_unused]] auto benchmarkIteration : state) {
    surface->getCanvas()->clear(0xFF050A18);
    layout.drawBatched(surface->getCanvas(), paragraph);
    benchmark::DoNotOptimize(surface.get());
  }
}
BENCHMARK(BM_DrawBatched_Raster_2000w_4PassShaderEffects)
    ->Unit(benchmark::kMicrosecond);

// 2000 multi-script tokens (Arabic, Devanagari, Hebrew, Thai, CJK, emoji,
// Greek, Cyrillic, Tamil, runes) scattered over 2000 rotated intervals —
// letter-confetti at paragraph scale. Warm: placement + per-glyph RSXform
// baking; every token still shape-cache resolved.
static void BM_Confetti_Babel_2000(benchmark::State &state) {
  const char8_t *tokens[] = {
      u8"حرف",  u8"كلمة", u8"अक्षर",  u8"शब्द",   u8"אות",   u8"מילה", u8"ตัวอักษร",
      u8"字",   u8"글",   u8"λόγος", u8"буква", u8"🎉",    u8"👍🏽", u8"文字",
      u8"ঢাকা", u8"கடல்",  u8"ᚱᚢᚾ",   u8"ainm",  u8"słowo", u8"λέξη"};
  std::mt19937 randomEngine(77);
  Paragraph paragraph;
  std::u8string text;
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
