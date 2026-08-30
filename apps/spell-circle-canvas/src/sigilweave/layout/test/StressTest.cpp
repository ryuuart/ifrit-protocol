/** @file
 * Large-paragraph stress: warm relayout linearity, paint-restyle cost
 * bounds, and the multi-script confetti scene.
 */

#include <gtest/gtest.h>
#include <sigilweave/query/Query.h>

#include <chrono>
#include <random>
#include <string>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(Stress, KnuthPlassFullyPlacedIsLinear) {
  // A huge paragraph that fits *entirely* (10k words on screen), which is
  // the worst case for the Knuth-Plass active list: nothing overflows, so
  // every word is a breakpoint candidate. On a uniform-width flow the
  // breaker merges paths that reached the same breakpoint on different line
  // numbers (TeX's one-measure model), which is what keeps the active list
  // bounded by the line width instead of growing with the paragraph. The
  // time bound below fails if that merge stops happening.
  FontContext& fontContext = sharedContext();
  static constexpr const char8_t* kWordPool[] = {
      u8"letters", u8"falling", u8"gently", u8"against", u8"words",
      u8"beacon",  u8"steady",  u8"rhythm", u8"turing",  u8"flow",
      u8"lattice", u8"shapes",  u8"glyphs", u8"marker",  u8"cache"};
  Paragraph paragraph;
  paragraph.appendText(makePooledText(kWordPool, 10000, 11), basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 40000));  // tall: everything fits
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);  // warm shapes
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
  // A loose ceiling: it is here to catch a super-linear active list, not to
  // police small regressions. Debug builds do the same work with unelided
  // container and iterator overhead, so they get their own bound.
#ifdef NDEBUG
  const double maximumMicroseconds = 8000.0;
#else
  const double maximumMicroseconds = 80000.0;
#endif
  EXPECT_LT(averageMicroseconds, maximumMicroseconds)
      << "KP active list grows with the paragraph";
}

TEST(Stress, PaintOnlyRestyleIsGeometryBounded) {
  // Repaint a set of ranges every frame (hue cycling) and relayout. A paint
  // edit must not re-run ICU analysis over the whole text, and the batch
  // form must not rebuild the span list once per range, so the per-frame
  // cost stays bounded by what the geometry can hold rather than by the
  // paragraph — the same property Overflow.HugeRelayoutIsBoundedByGeometry
  // checks for relayout. Almost all of this text never gets placed.
  FontContext& fontContext = sharedContext();
  static constexpr const char8_t* kWordPool[] = {
      u8"letters", u8"falling", u8"gently", u8"against", u8"words",
      u8"Beacon",  u8"steady",  u8"rhythm", u8"Turing",  u8"flow",
      u8"Lattice", u8"shapes",  u8"glyphs", u8"Марка",   u8"cache"};
  Paragraph paragraph;
  paragraph.appendText(makePooledText(kWordPool, 30000, 7), basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 320));  // room for ~1% of the text
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow);  // warm analysis + shapes
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
  // Loose ceiling again: what it must catch is cost scaling with the 30k
  // words of unplaced text. Debug builds get their own bound for the same
  // reason as above.
#ifdef NDEBUG
  const double maximumMicroseconds = 3000.0;
#else
  const double maximumMicroseconds = 30000.0;
#endif
  EXPECT_LT(averageMicroseconds, maximumMicroseconds)
      << "paint restyle scales with unplaced text";
}
// ── 2000-token multi-script confetti stress ───────────────────────────────

TEST(Stress, BabelConfetti2000) {
  FontContext& fontContext = sharedContext();
  const char8_t* tokens[] = {
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
  for (const PositionedRun& run : layout.runs)
    for (uint16_t glyph : run.shaped->glyphs) {
      totalGlyphCount++;
      unresolvedGlyphCount += glyph == 0;
    }
  EXPECT_EQ(unresolvedGlyphCount, 0u)
      << unresolvedGlyphCount << " of " << totalGlyphCount
      << " glyphs unresolved";
}
