/** @file
 * Large paragraphs: a fully-placed ten-thousand-word Knuth-Plass block, a
 * repaint scoped to the placed window of an overflowed one, a
 * two-thousand-word block at a small size, and two thousand multi-script
 * tokens scattered over as many rotated intervals.
 */

#include <gtest/gtest.h>
#include <sigilweave/query/Query.h>

#include <cmath>
#include <iterator>
#include <random>
#include <string>
#include <vector>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(Stress, KnuthPlassPlacesTenThousandWordsInAFlowTallEnoughForThem) {
  // The worst case for the Knuth-Plass active list: nothing overflows, so
  // every word is a breakpoint candidate. On a uniform-width flow the
  // breaker merges paths that reached the same breakpoint on different line
  // numbers, which is what keeps the active list bounded by the line width
  // instead of growing with the paragraph.
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
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_FALSE(layout.overflowed());
  EXPECT_GT(layout.lineCount, 100);
}

TEST(Stress, ARepaintScopedToThePlacedWindowLeavesAnOverflowWhereItWas) {
  // Thirty thousand words in a box with room for about one percent of
  // them, repainted over the window that was actually placed. A paint edit
  // carries no geometry, so the frame it lands on must come back with the
  // same runs and the same first unplaced word.
  FontContext& fontContext = sharedContext();
  static constexpr const char8_t* kWordPool[] = {
      u8"letters", u8"falling", u8"gently", u8"against", u8"words",
      u8"Beacon",  u8"steady",  u8"rhythm", u8"Turing",  u8"flow",
      u8"Lattice", u8"shapes",  u8"glyphs", u8"Марка",   u8"cache"};
  Paragraph paragraph;
  paragraph.appendText(makePooledText(kWordPool, 30000, 7), basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 320));  // room for ~1% of the text
  ParagraphLayout before = layoutParagraph(fontContext, paragraph, flow);
  ASSERT_TRUE(before.overflowed());

  // Scoped query over the placed window only.
  const uint32_t placedEnd =
      paragraph.words()[before.firstUnplacedWord].textBegin;
  const std::vector<CharRange> marks =
      findRegexMatches(paragraph, u8"\\b\\p{Lu}\\p{Ll}+", {0, placedEnd})
          .value_or(std::vector<CharRange>{});
  ASSERT_GT(marks.size(), 10u);

  paragraph.setPaint(marks, PaintStyle(0xFFCC0000));
  const ParagraphLayout after = layoutParagraph(fontContext, paragraph, flow);
  EXPECT_EQ(after.firstUnplacedWord, before.firstUnplacedWord);
  ASSERT_EQ(after.runs.size(), before.runs.size());
  for (size_t index = 0; index < after.runs.size(); ++index)
    EXPECT_EQ(after.runs[index].origin, before.runs[index].origin);
}

TEST(Stress, TwoThousandSmallWordsAreAllPlacedInABlockThatHoldsThem) {
  // A page of eight-point text on a ten-point rhythm: the whole paragraph
  // must be placed, and the glyphs it puts down must outnumber its words
  // several times over.
  FontContext& fontContext = sharedContext();
  const char8_t* words[] = {u8"letters", u8"water", u8"stars", u8"flow",
                            u8"cached",  u8"paint", u8"文字",  u8"波紋",
                            u8"글자",    u8"星光"};
  std::u8string text;
  for (int wordIndex = 0; wordIndex < 2000; ++wordIndex) {
    text += words[static_cast<size_t>(wordIndex * 7) % std::size(words)];
    text += ' ';
  }
  Paragraph paragraph;
  paragraph.appendText(text, basicStyle(8.0f));
  BlockFlow flow(SkRect::MakeXYWH(10, 10, 1180, 880));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 10.0f;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_FALSE(layout.overflowed());
  EXPECT_GT(glyphCount(layout), 7000);
}

// ── 2000-token multi-script confetti stress ───────────────────────────────

TEST(Stress, NoScriptOnThisSystemLeaksANotdefThroughTwoThousandTokens) {
  FontContext& fontContext = sharedContext();
  const char8_t* tokens[] = {
      u8"حرف",  u8"كلمة", u8"अक्षर",  u8"शब्द",   u8"אות",   u8"מילה", u8"ตัวอักษร",
      u8"字",   u8"글",   u8"λόγος", u8"буква", u8"🎉",    u8"👍🏽", u8"文字",
      u8"ঢাকা", u8"கடல்",  u8"ᚱᚢᚾ",   u8"ainm",  u8"słowo", u8"λέξη"};
  std::mt19937 randomEngine(77);  // NOLINT(bugprone-random-generator-seed): a
                                  // fixed seed keeps the test reproducible
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
