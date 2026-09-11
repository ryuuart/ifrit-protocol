/** @file
 * Configured paragraph reuse, complete style identity, and the lifetime of
 * text published with a layout independently of its context's retention.
 */

#include <gtest/gtest.h>
#include <sigilweave/layout/TextContext.h>

#include <array>
#include <functional>
#include <optional>
#include <string>

#include "support/LayoutSupport.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(TextContext, BothEncodingsReuseOneAnalyzedParagraph) {
  TextContext context(sigil::test::fonts());
  const auto style = basicStyle(18);
  const float width = context.naturalWidth(u8"signal β", style);
  const auto shapeCalls = context.fonts().stats().shapeCalls;
  EXPECT_FLOAT_EQ(context.naturalWidth(u"signal β", style), width);
  EXPECT_EQ(context.stats().paragraphBuilds, 1u);
  EXPECT_EQ(context.stats().paragraphCacheHits, 1u);
  EXPECT_EQ(context.stats().paragraphEntries, 1u);
  EXPECT_EQ(context.fonts().stats().shapeCalls, shapeCalls);
}

TEST(TextContext, ZeroRetentionRebuildsWithoutChangingTheResult) {
  const auto style = basicStyle(18);
  TextContext cached(sigil::test::fonts());
  TextContext uncached(sigil::test::fonts(), {.paragraphCacheEntries = 0});
  const auto expected = cached.singleLine(u8"signal", style, {10, 30});
  for (int i = 0; i < 3; ++i) {
    const auto actual = uncached.singleLine(u8"signal", style, {10, 30});
    ASSERT_EQ(actual.layout().runs.size(), expected.layout().runs.size());
    EXPECT_EQ(actual.layout().glyphOutline(), expected.layout().glyphOutline());
    EXPECT_EQ(actual.paragraph().text(), expected.paragraph().text());
  }
  EXPECT_EQ(uncached.stats().paragraphBuilds, 3u);
  EXPECT_EQ(uncached.stats().paragraphEntries, 0u);
  EXPECT_EQ(uncached.stats().paragraphCacheHits, 0u);
}

TEST(TextContext, TheEntryLimitRetainsTheMostRecentlyRequestedText) {
  TextContext context(sigil::test::fonts(), {.paragraphCacheEntries = 2});
  const auto style = basicStyle(18);
  for (auto text : {u8"one", u8"two", u8"one", u8"three", u8"one"})
    (void)context.naturalWidth(text, style);
  EXPECT_EQ(context.stats().paragraphBuilds, 3u);
  EXPECT_EQ(context.stats().paragraphEntries, 2u);
  (void)context.naturalWidth(u8"two", style);
  EXPECT_EQ(context.stats().paragraphBuilds, 4u);
  EXPECT_EQ(context.stats().paragraphEntries, 2u);
}

TEST(TextContext, NearbySizesKeepTheirExactMetricsInEitherRequestOrder) {
  const auto small = basicStyle(16);
  const auto large = basicStyle(16.03125f);
  for (bool largeFirst : {false, true}) {
    TextContext context(sigil::test::fonts());
    (void)context.naturalWidth(u8"measurement", largeFirst ? large : small);
    const float smallWidth = context.naturalWidth(u8"measurement", small);
    const float largeWidth = context.naturalWidth(u8"measurement", large);
    EXPECT_GT(largeWidth, smallWidth);
    EXPECT_NEAR(largeWidth / smallWidth,
                large.shaping.fontSize / small.shaping.fontSize, 0.00001f);
    EXPECT_EQ(context.stats().paragraphEntries, 2u);
  }
}

TEST(TextContext, EveryShapingControlParticipatesInParagraphIdentity) {
  TextContext context(sigil::test::fonts());
  const auto base = basicStyle(18);
  const std::array<std::function<void(ShapingStyle&)>, 13> changes = {
      [](auto& s) { s.typeface = nullptr; },
      [](auto& s) { s.fontSize += 1; },
      [](auto& s) { s.letterSpacing = 1; },
      [](auto& s) { s.scaleX = 0.8f; },
      [](auto& s) { s.wordSpacing = 1; },
      [](auto& s) { s.languageTag = "tr"; },
      [](auto& s) { s.fontFeatures = {{"liga", 0}}; },
      [](auto& s) { s.variations = {{"wght", 600}}; },
      [](auto& s) { s.textTransform = TextTransform::kUppercase; },
      [](auto& s) { s.verticalForm = VerticalForm::kUpright; },
      [](auto& s) { s.aliased = true; },
      [](auto& s) { s.opticalKerning = true; },
      [](auto& s) { s.languageTag = "ja"; }};
  (void)context.naturalWidth(u8"office ink", base);
  size_t expected = 1;
  for (const auto& change : changes) {
    auto style = base;
    change(style.shaping);
    (void)context.naturalWidth(u8"office ink", style);
    EXPECT_EQ(context.stats().paragraphEntries, ++expected);
  }
  (void)context.naturalWidth(u8"office ink", base);
  EXPECT_EQ(context.stats().paragraphBuilds, expected);
}

TEST(TextContext, PaintChangesReuseAnalysisWhenNoResultIsHeld) {
  TextContext context(sigil::test::fonts());
  auto style = basicStyle(18);
  uint64_t identity = 0;
  {
    const auto first = context.singleLine(u8"caption", style, {0, 20});
    identity = first.paragraph().identity();
  }
  const auto shapeCalls = context.fonts().stats().shapeCalls;
  style.paint.foreground.setColor(SK_ColorRED);
  const auto red = context.singleLine(u8"caption", style, {30, 40});
  EXPECT_EQ(red.paragraph().identity(), identity);
  ASSERT_FALSE(red.paragraph().spans().empty());
  EXPECT_EQ(red.paragraph().spans().front().style.paint.foreground.getColor(),
            SK_ColorRED);
  EXPECT_EQ(context.stats().paragraphBuilds, 1u);
  EXPECT_EQ(context.fonts().stats().shapeCalls, shapeCalls);
}

TEST(TextContext, RetainedResultsKeepTheirPaintWordsAndPlacement) {
  TextContext context(sigil::test::fonts());
  auto style = basicStyle(18);
  BlockFlow wide(SkRect::MakeWH(800, 400));
  const auto first = context.layout(u8"one two three four five", style, wide);
  const auto outline = first.layout().glyphOutline();
  const auto words = first.paragraph().words().size();
  const auto paint = first.paragraph().spans().front().style.paint;
  style.paint.foreground.setColor(SK_ColorRED);
  BlockFlow narrow(SkRect::MakeWH(55, 400));
  const auto next = context.layout(u8"one two three four five", style, narrow);
  EXPECT_GT(next.layout().lineCount, first.layout().lineCount);
  EXPECT_NE(&next.paragraph(), &first.paragraph());
  EXPECT_NE(next.paragraph().identity(), first.paragraph().identity());
  EXPECT_EQ(first.layout().glyphOutline(), outline);
  EXPECT_EQ(first.paragraph().words().size(), words);
  EXPECT_EQ(first.paragraph().spans().front().style.paint, paint);
}

TEST(TextContext, ResultsSurviveEvictionPurgeAndContextDestruction) {
  std::optional<TextLayout> held;
  SkPath outline;
  {
    TextContext context(sigil::test::fonts(), {.paragraphCacheEntries = 1});
    const auto style = basicStyle(18);
    held.emplace(context.singleLine(u8"held", style, {10, 30}));
    outline = held->layout().glyphOutline();
    (void)context.naturalWidth(u8"replacement", style);
    EXPECT_EQ(held->layout().glyphOutline(), outline);
    context.purgeParagraphs();
    EXPECT_EQ(context.stats().paragraphEntries, 0u);
    EXPECT_EQ(held->paragraph().text(), u"held");
  }
  const auto copy = *held;
  held.reset();
  EXPECT_EQ(copy.paragraph().text(), u"held");
  EXPECT_FALSE(copy.layout().runs.empty());
  EXPECT_EQ(copy.layout().glyphOutline(), outline);
}

TEST(TextContext, MeasurementDoesNotInheritAPreviousLayoutsSegmentation) {
  TextContext context(sigil::test::fonts());
  const auto style = basicStyle(18);
  constexpr auto text = u8"hy\u00adphen\u00adation";
  const float expected = context.naturalWidth(text, style);
  {
    BlockFlow flow(SkRect::MakeWH(80, 400));
    ParagraphLayoutOptions options;
    options.hyphenation.enabled = false;
    (void)context.layout(text, style, flow, options);
  }
  EXPECT_FLOAT_EQ(context.naturalWidth(text, style), expected);
  const auto line = context.singleLine(text, style, {0, 20});
  EXPECT_FALSE(line.layout().overflowed());
}

TEST(TextContext, ReusedTextStillQueriesAMutatingFlow) {
  class MutableFlow final : public FlowGeometry {
   public:
    int queries = 0;
    float width = 500;
    bool lineIntervals(const LineRequest& request,
                       std::vector<LineInterval>& intervals) override {
      ++queries;
      if (request.index > 12) return false;
      intervals.push_back({{0, 20.0f + request.index * 24}, {1, 0}, width});
      return true;
    }
  } flow;
  TextContext context(sigil::test::fonts());
  const auto style = basicStyle(18);
  int firstLines = 0;
  {
    const auto result =
        context.layout(u8"one two three four five", style, flow);
    firstLines = result.layout().lineCount;
  }
  flow.width = 55;
  flow.queries = 0;
  const auto result = context.layout(u8"one two three four five", style, flow);
  EXPECT_GT(flow.queries, 0);
  EXPECT_GT(result.layout().lineCount, firstLines);
  EXPECT_EQ(context.stats().paragraphBuilds, 1u);
}

TEST(TextContext, ResettingCountersDoesNotPurgeAndContextsDoNotShareEntries) {
  const auto style = basicStyle(18);
  TextContext first(sigil::test::fonts());
  TextContext second(sigil::test::fonts());
  (void)first.naturalWidth(u8"caption", style);
  first.resetStats();
  EXPECT_EQ(first.stats().paragraphEntries, 1u);
  EXPECT_EQ(first.stats().paragraphBuilds, 0u);
  (void)first.naturalWidth(u8"caption", style);
  (void)second.naturalWidth(u8"caption", style);
  EXPECT_EQ(first.stats().paragraphBuilds, 0u);
  EXPECT_EQ(first.stats().paragraphCacheHits, 1u);
  EXPECT_EQ(second.stats().paragraphBuilds, 1u);
  first.purgeParagraphs();
  EXPECT_EQ(second.stats().paragraphEntries, 1u);
}

TEST(TextContext, EmptyTextAndOwnedFontsNeedNoExternalParagraphStorage) {
  TextContext context(sigil::test::fonts().fontManager()
                          ? sk_ref_sp(sigil::test::fonts().fontManager())
                          : nullptr,
                      {}, basicStyle(18).shaping.typeface);
  TextStyle style;
  EXPECT_FLOAT_EQ(context.naturalWidth(std::u8string_view{}, style), 0);
  const auto empty = context.singleLine(std::u16string_view{}, style, {0, 20});
  EXPECT_TRUE(empty.layout().runs.empty());
  EXPECT_TRUE(empty.paragraph().text().empty());
  EXPECT_EQ(context.stats().paragraphEntries, 1u);
  EXPECT_GT(context.naturalWidth(u8"caption", style), 0);
}
