/** @file
 * Large-paragraph stress: runtime-shader rendering, warm
 * relayout linearity, paint-restyle cost bounds, and the
 * multi-script confetti scene.
 */

#include "TestSupport.h"

#include <gtest/gtest.h>

#include <sigilweave/PaintShaders.h>

#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>

#include <chrono>
#include <random>
#include <string>
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(Stress, RuntimeShadersRenderEntire2000WordParagraph) {
  constexpr int kWordCount = 2000;
  const char8_t *words[] = {u8"letters", u8"water", u8"stars", u8"flow",
                            u8"cached",  u8"paint", u8"文字",  u8"波紋",
                            u8"글자",    u8"星光"};
  std::u8string text;
  for (int wordIndex = 0; wordIndex < kWordCount; ++wordIndex) {
    text += words[(wordIndex * 7) % std::size(words)];
    text += ' ';
  }

  const SkRect bounds = SkRect::MakeXYWH(10, 10, 1180, 880);
  sk_sp<SkShader> water = PaintShaders::water(bounds, 1.25f);
  sk_sp<SkShader> mesh = PaintShaders::meshGradient(bounds, 1.25f);
  sk_sp<SkShader> stars = PaintShaders::sparkle(bounds, 1.25f);
  ASSERT_NE(water, nullptr);
  ASSERT_NE(mesh, nullptr);
  ASSERT_NE(stars, nullptr);

  TextStyle textStyle = basicStyle(8.0f);
  textStyle.paint = PaintStyle(SK_ColorWHITE);
  textStyle.paint
      .addUnderlay(PaintLayer::glow(0x772A77FF, 1.8f))
      .addUnderlay(PaintLayer::outline(0xFF061229, 0.7f));
  textStyle.paint.foreground.setShader(std::move(mesh));
  SkPaint starOverlay;
  starOverlay.setAntiAlias(true);
  starOverlay.setShader(std::move(stars));
  starOverlay.setBlendMode(SkBlendMode::kScreen);
  textStyle.paint.addOverlay(PaintLayer(std::move(starOverlay)));

  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText(text, textStyle);
  BlockFlow flow(bounds);
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 10.0f;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_FALSE(layout.overflowed()) << "the stress paragraph must be whole";

  size_t glyphCount = 0;
  for (const PositionedRun &run : layout.runs)
    if (run.shaped)
      glyphCount += run.shaped->glyphs.size();
  EXPECT_GT(glyphCount, 7000u);

  constexpr SkColor kBackground = 0xFF050A18;
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1200, 900));
  surface->getCanvas()->clear(kBackground);
  layout.drawBatched(surface->getCanvas(), paragraph);
  SkPixmap pixels;
  ASSERT_TRUE(surface->peekPixels(&pixels));
  bool sawInk = false;
  for (int y = 0; y < pixels.height() && !sawInk; y += 3)
    for (int x = 0; x < pixels.width() && !sawInk; x += 3)
      sawInk = pixels.getColor(x, y) != kBackground;
  EXPECT_TRUE(sawInk);
}

TEST(Stress, KnuthPlassFullyPlacedIsLinear) {
  // A huge paragraph that fits *entirely* (10k words on screen). On a
  // uniform-width flow the breaker merges same-breakpoint paths (TeX's
  // one-measure model), so the active list stays bounded by the line width
  // and warm relayout stays linear — without the merge this case was ~20×
  // slower and grew super-linearly.
  FontContext &fontContext = sharedContext();
  static constexpr const char8_t *kWordPool[] = {
      u8"letters", u8"falling", u8"gently", u8"against", u8"words",
      u8"beacon",  u8"steady",  u8"rhythm", u8"turing",  u8"flow",
      u8"lattice", u8"shapes",  u8"glyphs", u8"marker",  u8"cache"};
  Paragraph paragraph;
  paragraph.appendText(makePooledText(kWordPool, 10000, 11), basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 40000)); // tall: everything fits
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options); // warm shapes
  ASSERT_FALSE(layout.overflowed());

  const auto startTime = std::chrono::steady_clock::now();
  constexpr int kIterationCount = 5;
  for (int iteration = 0; iteration < kIterationCount; ++iteration)
    layout = layoutParagraph(fontContext, paragraph, flow, options);
  const double averageMicroseconds =
      std::chrono::duration<double, std::micro>(
          std::chrono::steady_clock::now() - startTime)
          .count() /
      kIterationCount;
#ifdef NDEBUG
  const double maximumMicroseconds = 8000.0; // measured ~1.9ms
#else
  const double maximumMicroseconds =
      80000.0; // Debug: same work, ~20× the overhead
#endif
  EXPECT_LT(averageMicroseconds, maximumMicroseconds)
      << "KP active list grows with the paragraph";
}

TEST(Stress, PaintOnlyRestyleIsGeometryBounded) {
  // The marker workflow: repaint ranges every frame (hue cycling), relayout.
  // Paint edits must not re-run ICU analysis over the whole text or rebuild
  // spans once per range — cost stays bounded by the geometry, like the
  // relayout itself (see Overflow.HugeRelayoutIsBoundedByGeometry).
  FontContext &fontContext = sharedContext();
  static constexpr const char8_t *kWordPool[] = {
      u8"letters", u8"falling", u8"gently", u8"against", u8"words",
      u8"Beacon",  u8"steady",  u8"rhythm", u8"Turing",  u8"flow",
      u8"Lattice", u8"shapes",  u8"glyphs", u8"Марка",   u8"cache"};
  Paragraph paragraph;
  paragraph.appendText(makePooledText(kWordPool, 30000, 7), basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 320)); // room for ~1% of the text
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow); // warm analysis + shapes
  ASSERT_TRUE(layout.overflowed());

  // Scoped query over the placed window only.
  const uint32_t placedEnd =
      paragraph.words()[layout.firstUnplacedWord].textBegin;
  const std::vector<CharRange> marks =
      findRegexMatches(paragraph, u8"\\b\\p{Lu}\\p{Ll}+", {0, placedEnd})
          .value_or(std::vector<CharRange>{});
  ASSERT_GT(marks.size(), 10u);

  const auto startTime = std::chrono::steady_clock::now();
  constexpr int kIterationCount = 20;
  for (int iteration = 0; iteration < kIterationCount; ++iteration) {
    PaintStyle hue(0xFF000000 | static_cast<uint32_t>(iteration * 1234567));
    paragraph.setPaint(marks, hue);
    layout = layoutParagraph(fontContext, paragraph, flow);
  }
  const double averageMicroseconds =
      std::chrono::duration<double, std::micro>(
          std::chrono::steady_clock::now() - startTime)
          .count() /
      kIterationCount;
#ifdef NDEBUG
  const double maximumMicroseconds = 3000.0;
#else
  const double maximumMicroseconds =
      30000.0; // Debug: same work, ~20× the overhead
#endif
  EXPECT_LT(averageMicroseconds, maximumMicroseconds)
      << "paint restyle scales with unplaced text";
}
// ── 2000-token multi-script confetti stress ───────────────────────────────

TEST(Stress, BabelConfetti2000) {
  FontContext &fontContext = sharedContext();
  const char8_t *tokens[] = {
      u8"حرف",  u8"كلمة", u8"अक्षर",  u8"शब्द",   u8"אות",   u8"מילה", u8"ตัวอักษร",
      u8"字",   u8"글",   u8"λόγος", u8"буква", u8"🎉",    u8"👍🏽", u8"文字",
      u8"ঢাকা", u8"கடல்",  u8"ᚱᚢᚾ",   u8"ainm",  u8"słowo", u8"λέξη"};
  std::mt19937 randomEngine(77);
  Paragraph paragraph;
  TextStyle style = basicStyle(18.0f);
  std::u8string text;
  for (int tokenIndex = 0; tokenIndex < 2000; ++tokenIndex) {
    text += tokens[randomEngine() % 20];
    text += ' ';
  }
  paragraph.appendText(text, style);

  LineSetFlow flow;
  for (int intervalIndex = 0; intervalIndex < 2000; ++intervalIndex) {
    const float angle = static_cast<float>(randomEngine() % 628) * 0.01f;
    flow.lines().push_back(
        {LineInterval{{20.0f + static_cast<float>(randomEngine() % 1360),
                       20.0f + static_cast<float>(randomEngine() % 860)},
                      {std::cos(angle), std::sin(angle)},
                      60}});
  }
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  EXPECT_GT(layout.runs.size(), 1500u);
  EXPECT_GT(paragraph.words().size(), 1900u);

  // Nothing may leak a .notdef for scripts macOS covers (all of these).
  size_t unresolvedGlyphCount = 0;
  size_t totalGlyphCount = 0;
  for (const PositionedRun &run : layout.runs)
    for (uint16_t glyph : run.shaped->glyphs) {
      totalGlyphCount++;
      unresolvedGlyphCount += glyph == 0;
    }
  EXPECT_EQ(unresolvedGlyphCount, 0u)
      << unresolvedGlyphCount << " of " << totalGlyphCount
      << " glyphs unresolved";
}
