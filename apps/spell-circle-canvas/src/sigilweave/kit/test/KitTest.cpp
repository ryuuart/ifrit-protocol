/** @file
 * SigilWeaveKit: rebuild guards, layout memoization, glyph bucketing, and
 * the tables the layout asks for. These tests pin the invalidation
 * semantics the kit exists to make explicit — which key changes fire a
 * rebuild, and which must not — and what the stock tables actually hold.
 */

#include <gtest/gtest.h>
#include <sigilweave/kit/SigilWeaveKit.h>

#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "support/KitSupport.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

std::vector<uint32_t> breakPoints(const kit::PatternHyphenator& hyphenator,
                                  std::u16string_view word,
                                  std::string_view language) {
  std::vector<uint32_t> points;
  hyphenator.breakPoints(word, language, points);
  return points;
}

TEST(LayoutGuard, RelayoutsOnEditAndDeclaredKeysOnly) {
  Paragraph paragraph = makeParagraph(u8"guarded layout text");
  kit::LayoutGuard<SkISize, TextAlignment> guard;
  ParagraphLayout layout;
  int relayouts = 0;
  auto relayout = [&] {
    ++relayouts;
    BlockFlow flow(SkRect::MakeXYWH(0, 0, 200, 200));
    layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
  };

  const SkISize size{400, 300};
  EXPECT_TRUE(guard.ensure(paragraph, {size, TextAlignment::kStart}, relayout));
  // Steady frames: same content, same keys — the layout must be reused.
  EXPECT_FALSE(
      guard.ensure(paragraph, {size, TextAlignment::kStart}, relayout));
  EXPECT_FALSE(
      guard.ensure(paragraph, {size, TextAlignment::kStart}, relayout));
  EXPECT_EQ(relayouts, 1);

  // A declared key change fires exactly one relayout.
  EXPECT_TRUE(
      guard.ensure(paragraph, {size, TextAlignment::kJustify}, relayout));
  EXPECT_EQ(relayouts, 2);

  // A live edit bumps the revision/dirties shaping; the guard must notice
  // without the caller declaring anything.
  paragraph.appendText(u8" and more", basicStyle());
  EXPECT_TRUE(
      guard.ensure(paragraph, {size, TextAlignment::kJustify}, relayout));
  EXPECT_FALSE(
      guard.ensure(paragraph, {size, TextAlignment::kJustify}, relayout));
  EXPECT_EQ(relayouts, 3);
  EXPECT_FALSE(layout.runs.empty());
}

TEST(LayoutGuard, PaintOnlyRestyleDoesNotRelayout) {
  Paragraph paragraph = makeParagraph(u8"repaint me freely");
  kit::LayoutGuard<SkISize> guard;
  ParagraphLayout layout;
  int relayouts = 0;
  auto relayout = [&] {
    ++relayouts;
    BlockFlow flow(SkRect::MakeXYWH(0, 0, 300, 100));
    layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
  };

  const SkISize size{300, 100};
  // The first paint introduces new span boundaries, so the first ensure
  // (which also covers first use) lays out once.
  paragraph.setPaint(0, 7, PaintStyle(SK_ColorRED));
  EXPECT_TRUE(guard.ensure(paragraph, {size}, relayout));
  // The hue-cycling-marker idiom: repainting the *same* range every frame
  // leaves span boundaries untouched and must re-hit the cached layout.
  paragraph.setPaint(0, 7, PaintStyle(SK_ColorBLUE));
  EXPECT_FALSE(guard.ensure(paragraph, {size}, relayout));
  EXPECT_EQ(relayouts, 1);
}

TEST(GlyphBuckets, GlyphsGroupByKeyAndAnEmptyBucketIssuesNoDraw) {
  struct Shade {
    int level = 0;
    int fade = 0;
    bool operator==(const Shade&) const = default;
  };
  kit::GlyphBuckets<Shade> buckets;
  buckets.add({1, 0}, 10, {0, 0});
  buckets.add({1, 0}, 11, {1, 0});
  buckets.add({2, 3}, 12, {2, 0});
  ASSERT_EQ(buckets.buckets.size(), 2u);
  EXPECT_EQ(buckets.buckets[0].glyphs.size(), 2u);
  EXPECT_EQ(buckets.buckets[0].placements.size(), 2u);

  int visited = 0;
  EXPECT_EQ(buckets.drawEach([&](const auto&) { ++visited; }), 3);
  EXPECT_EQ(visited, 2);

  // clear() keeps the buckets (and their allocations) but empties them, so
  // the next frame's drawEach visits nothing.
  buckets.clear();
  ASSERT_EQ(buckets.buckets.size(), 2u);
  EXPECT_EQ(buckets.drawEach([&](const auto&) { ADD_FAILURE(); }), 0);
}

TEST(SampleText, FillerIsDeterministicAndMultiSpan) {
  const Paragraph first = kit::mixedScriptFiller(240, 16.0f);
  const Paragraph second = kit::mixedScriptFiller(240, 16.0f);
  EXPECT_EQ(first.text(), second.text());
  EXPECT_GT(first.spans().size(), 1u);
}

// ── The tables ─────────────────────────────────────────────────────────

TEST(LineTables, TheStockProhibitionsAreTheFullWidthPunctuationOfTheGrid) {
  const KinsokuTable table = kit::kinsoku::japanese();
  for (const char16_t character :
       {u'、', u'。', u'）', u'」', u'』', u'！', u'？', u'ー', u'ぁ', u'ッ'})
    EXPECT_NE(table.notLineStart.find(character), std::u16string::npos)
        << "may not open a line";
  for (const char16_t character : {u'（', u'「', u'『', u'【'})
    EXPECT_NE(table.notLineEnd.find(character), std::u16string::npos)
        << "may not close a line";
  // A full-width cell is what the set is about: ASCII punctuation carries
  // the same line-break classes and is the segmentation's business.
  for (const char16_t character : {u',', u'.', u')', u'(', u'a'}) {
    EXPECT_EQ(table.notLineStart.find(character), std::u16string::npos);
    EXPECT_EQ(table.notLineEnd.find(character), std::u16string::npos);
  }
  EXPECT_EQ(kit::kinsoku::japanese(), table) << "one derivation, reused";
}

TEST(PatternHyphenator, ATableAnswersForTheLanguageItDeclaresAndNoOther) {
  const kit::PatternHyphenator german("de", "ü1be");
  EXPECT_EQ(breakPoints(german, u"über", "de-DE"), (std::vector<uint32_t>{1u}))
      << "ü-ber";
  EXPECT_TRUE(breakPoints(german, u"über", "en-US").empty())
      << "no answer beats a misspelling";
  EXPECT_TRUE(breakPoints(german, u"über", "").empty())
      << "a text that never said its language";
}

TEST(PatternHyphenator, LettersOutsideAsciiAreLettersLikeAnyOther) {
  // The same pattern reaches the word however the word is capitalised, and
  // a table written over one alphabet says nothing about another.
  const kit::PatternHyphenator german("de", "ü1be");
  const kit::PatternHyphenator greek("el", "λ1λη");
  EXPECT_EQ(breakPoints(german, u"Über", "de"), (std::vector<uint32_t>{1u}));
  EXPECT_EQ(breakPoints(greek, u"ελλην", "el"), (std::vector<uint32_t>{2u}));
}

TEST(PatternHyphenator, AWordThatIsNotAllLettersIsLeftWhole) {
  const kit::PatternHyphenator german("de", "ü1be 1be");
  EXPECT_TRUE(breakPoints(german, u"über2", "de").empty())
      << "a digit is not a letter";
  EXPECT_TRUE(breakPoints(german, u"über-alles", "de").empty())
      << "a word already carrying a hyphen";
}

TEST(PatternHyphenator, ExceptionSpellingsAndCommentsSurviveTheParse) {
  const kit::PatternHyphenator german("de",
                                      "% Über alles, a licence header\n"
                                      "ü1be\n"
                                      "exceptions\n"
                                      "über\n");
  EXPECT_TRUE(breakPoints(german, u"über", "de").empty())
      << "the exception spelling forbids the break";
  EXPECT_EQ(german.patternCount(), 1u) << "the comment held no pattern";
}

TEST(PatternHyphenator, PatternsOpenBreaksInsideWords) {
  static const kit::PatternHyphenator hyphenator(
      "en", kit::englishHyphenationPatterns());
  EXPECT_GT(hyphenator.patternCount(), 100u);
  const std::vector<uint32_t> points =
      breakPoints(hyphenator, u"hyphenation", "en-US");
  EXPECT_FALSE(points.empty());
  for (const uint32_t offset : points) {
    EXPECT_GT(offset, 0u);
    EXPECT_LT(offset, 11u);
  }
}

}  // namespace
