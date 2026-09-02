/** @file
 * SigilWeaveKit: rebuild guards, layout memoization, glyph bucketing, and
 * the tables the layout asks for. These tests pin the invalidation
 * semantics the kit exists to make explicit — which key changes fire a
 * rebuild, and which must not — and what the stock tables actually hold.
 */

#include <gtest/gtest.h>
#include <sigilweave/kit/SigilWeaveKit.h>

#include <string>
#include <tuple>
#include <vector>

#include "support/KitSupport.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

TEST(RebuildGuard, FiresOnFirstUseThenOnlyOnKeyChange) {
  sigil::weave::kit::RebuildGuard<std::string, float> guard;
  int builds = 0;
  auto build = [&] { ++builds; };

  EXPECT_TRUE(guard.ensure({"a", 1.0f}, build));
  EXPECT_FALSE(guard.ensure({"a", 1.0f}, build));
  EXPECT_TRUE(guard.ensure({"a", 2.0f}, build));
  EXPECT_TRUE(guard.ensure({"b", 2.0f}, build));
  EXPECT_FALSE(guard.ensure({"b", 2.0f}, build));
  EXPECT_EQ(builds, 3);

  guard.invalidate();
  EXPECT_TRUE(guard.ensure({"b", 2.0f}, build));
  EXPECT_EQ(builds, 4);
}

TEST(RebuildGuard, ThrowingBuildStaysInvalidAndRetries) {
  sigil::weave::kit::RebuildGuard<int> guard;
  EXPECT_THROW(guard.ensure({1}, [] { throw std::runtime_error("boom"); }),
               std::runtime_error);
  EXPECT_FALSE(guard.built());
  int builds = 0;
  EXPECT_TRUE(guard.ensure({1}, [&] { ++builds; }));
  EXPECT_EQ(builds, 1);
}

TEST(CachedValue, ReturnsCachedValueUntilKeyChanges) {
  sigil::weave::kit::CachedValue<int, int> cached;
  EXPECT_EQ(cached.ensure({10}, [] { return 100; }), 100);
  // Same key: the stale-looking callable must not run.
  EXPECT_EQ(cached.ensure({10}, [] { return 999; }), 100);
  EXPECT_EQ(cached.ensure({20}, [] { return 200; }), 200);
  EXPECT_EQ(cached.value(), 200);
}

TEST(CachedValue, KeylessEnsureBuildsOnce) {
  sigil::weave::kit::CachedValue<int> lazy;
  int builds = 0;
  auto build = [&] {
    ++builds;
    return 7;
  };
  EXPECT_EQ(lazy.ensure(build), 7);
  EXPECT_EQ(lazy.ensure(build), 7);
  EXPECT_EQ(builds, 1);
}

TEST(LayoutGuard, RelayoutsOnEditAndDeclaredKeysOnly) {
  Paragraph paragraph = makeParagraph(u8"guarded layout text");
  sigil::weave::kit::LayoutGuard<SkISize, TextAlignment> guard;
  ParagraphLayout layout;
  int relayouts = 0;
  auto relayout = [&] {
    ++relayouts;
    BlockFlow flow(SkRect::MakeXYWH(0, 0, 200, 200));
    layout = layoutParagraph(sharedContext(), paragraph, flow);
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
  sigil::weave::kit::LayoutGuard<SkISize> guard;
  ParagraphLayout layout;
  int relayouts = 0;
  auto relayout = [&] {
    ++relayouts;
    BlockFlow flow(SkRect::MakeXYWH(0, 0, 300, 100));
    layout = layoutParagraph(sharedContext(), paragraph, flow);
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

TEST(Quantize, SnapsToStepMultiples) {
  EXPECT_FLOAT_EQ(sigil::weave::kit::quantize(10.3f), 10.0f);
  EXPECT_FLOAT_EQ(sigil::weave::kit::quantize(10.6f), 11.0f);
  EXPECT_FLOAT_EQ(sigil::weave::kit::quantize(103.0f, 8.0f), 104.0f);
  EXPECT_FLOAT_EQ(sigil::weave::kit::quantize(-2.6f), -3.0f);
}

TEST(GlyphBuckets, GroupsByKeyAndSkipsEmptyOnDraw) {
  struct Shade {
    int level = 0;
    int fade = 0;
    bool operator==(const Shade&) const = default;
  };
  sigil::weave::kit::GlyphBuckets<Shade> buckets;
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
  const Paragraph first = sigil::weave::kit::mixedScriptFiller(240, 16.0f);
  const Paragraph second = sigil::weave::kit::mixedScriptFiller(240, 16.0f);
  EXPECT_EQ(first.text(), second.text());
  EXPECT_GT(first.spans().size(), 1u);
}

// ── The tables ─────────────────────────────────────────────────────────

TEST(LineTables, TheStockProhibitionsAreTheFullWidthPunctuationOfTheGrid) {
  const KinsokuTable table = kit::kinsoku::japanese();
  for (const char16_t character : {u'、', u'。', u'）', u'」', u'』',
                                   u'！', u'？', u'ー', u'ぁ', u'ッ'})
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

TEST(Hyphenation, ATableAnswersForTheLanguageItDeclaresAndNoOther) {
  const kit::PatternHyphenator german("de", "ü1be");
  std::vector<uint32_t> breaks;
  german.breakPoints(u"über", "de-DE", breaks);
  EXPECT_EQ(breaks, (std::vector<uint32_t>{1u})) << "ü-ber";
  breaks.clear();
  german.breakPoints(u"über", "en-US", breaks);
  EXPECT_TRUE(breaks.empty()) << "no answer beats a misspelling";
  breaks.clear();
  german.breakPoints(u"über", "", breaks);
  EXPECT_TRUE(breaks.empty()) << "a text that never said its language";
}

TEST(Hyphenation, LettersOutsideAsciiAreLettersLikeAnyOther) {
  // The same pattern reaches the word however the word is capitalised, and
  // a table written over one alphabet says nothing about another.
  const kit::PatternHyphenator german("de", "ü1be");
  std::vector<uint32_t> breaks;
  german.breakPoints(u"Über", "de", breaks);
  EXPECT_EQ(breaks, (std::vector<uint32_t>{1u}));
  breaks.clear();
  const kit::PatternHyphenator greek("el", "λ1λη");
  greek.breakPoints(u"ελλην", "el", breaks);
  EXPECT_EQ(breaks, (std::vector<uint32_t>{2u}));
}

TEST(Hyphenation, AWordThatIsNotAllLettersIsLeftWhole) {
  const kit::PatternHyphenator german("de", "ü1be 1be");
  std::vector<uint32_t> breaks;
  german.breakPoints(u"über2", "de", breaks);
  EXPECT_TRUE(breaks.empty()) << "a digit is not a letter";
  german.breakPoints(u"über-alles", "de", breaks);
  EXPECT_TRUE(breaks.empty()) << "a word already carrying a hyphen";
}

TEST(Hyphenation, ExceptionSpellingsAndCommentsSurviveTheParse) {
  const kit::PatternHyphenator german("de",
                                      "% Über alles, a licence header\n"
                                      "ü1be\n"
                                      "exceptions\n"
                                      "über\n");
  std::vector<uint32_t> breaks;
  german.breakPoints(u"über", "de", breaks);
  EXPECT_TRUE(breaks.empty()) << "the exception spelling forbids the break";
  EXPECT_EQ(german.patternCount(), 1u) << "the comment held no pattern";
}

}  // namespace
