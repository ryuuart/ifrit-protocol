/** @file
 * Overflow behaviour: which word a frame stopped at, the marker that
 * admits the rest is missing, the shaping an overflow never does, and the
 * line clamp over every geometry.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <charconv>
#include <string>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(Overflow, ReportsFirstUnplacedWord) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(
      u8"far more text than could ever fit inside such a tiny little box");
  BlockFlow flow(SkRect::MakeWH(120, 40));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  EXPECT_TRUE(layout.overflowed());
  EXPECT_GT(layout.firstUnplacedWord, 0u);
}

// ── Overflowing paragraphs fill what fits, not what exists ───────────────

namespace {

/// Filling a frame is a breaking decision, so both breakers answer for it.
class OverflowedFrame : public BrokenBothWays {};

}  // namespace

TEST_P(OverflowedFrame, AFrameStopsAtItsGeometryAndNotAtTheLastWord) {
  // Thirty thousand words in a box with room for about one percent of
  // them: the breaker must fill the box, report the overflow, and name a
  // first unplaced word near the geometry's own end rather than walking to
  // the end of the text.
  FontContext& fontContext = sigil::test::fonts();
  static constexpr const char8_t* kWordPool[] = {
      u8"letters", u8"flow",    u8"around",  u8"boxes", u8"while",
      u8"the",     u8"breaker", u8"stops",   u8"at",    u8"geometry",
      u8"instead", u8"of",      u8"walking", u8"every", u8"word"};
  Paragraph paragraph;
  paragraph.appendText(makePooledText(kWordPool, 30000, 11), basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 320));  // room for a small part of it

  ParagraphLayoutOptions options;
  options.lineBreakStrategy = breaker();
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_TRUE(layout.overflowed());
  EXPECT_GT(layout.runs.size(), 50u);
  EXPECT_LT(layout.firstUnplacedWord, 600u);
}

INSTANTIATE_TEST_SUITE_P(Breakers, OverflowedFrame, bothBreakers(),
                         breakerName);

TEST(Overflow, EllipsisMarksOverflow) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(
      u8"far more text than a two line box can ever hope to hold so the "
      "marker has to step in and admit that the rest is missing");
  BlockFlow flow(SkRect::MakeWH(260, 44));  // ~2 lines
  ParagraphLayoutOptions options;
  options.overflow.ellipsis = u"…";

  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_TRUE(layout.overflowed());
  ASSERT_TRUE(layout.ellipsized);
  ASSERT_FALSE(layout.runs.empty());
  const PositionedRun& marker = layout.runs.back();
  ASSERT_TRUE(marker.shaped);
  // The marker sits at the end of the final line, inside the measure.
  EXPECT_EQ(marker.lineIndex, layout.runs[layout.runs.size() - 2].lineIndex);
  EXPECT_LE(marker.origin.x() + marker.shaped->advance, 260.0f + 0.75f);
  EXPECT_GT(marker.origin.x(), 0.0f);
  // Truncated words count as unplaced.
  for (const PositionedRun& run : layout.runs)
    if (&run != &marker) EXPECT_LT(run.wordIndex, layout.firstUnplacedWord);
}

TEST(Overflow, NoEllipsisWhenTextFits) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"short and sweet");
  BlockFlow flow(SkRect::MakeWH(400, 200));
  ParagraphLayoutOptions options;
  options.overflow.ellipsis = u"…";

  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_FALSE(layout.overflowed());
  EXPECT_FALSE(layout.ellipsized);
}

TEST(Overflow, ShapesOnlyWhatFits) {
  // Lazy shaping: layout pulls HarfBuzz along its frontier, so the ~29k
  // words that never fit the box are itemized but never shaped. Every word
  // is unique so the content-addressed cache can't hide eager shaping.
  FontContext& fontContext = sigil::test::fonts();
  std::u8string text;
  for (int wordIndex = 0; wordIndex < 30000; ++wordIndex) {
    text += u8"word";
    char number[16];
    const auto [end, error] =
        std::to_chars(std::begin(number), std::end(number), wordIndex);
    ASSERT_EQ(error, std::errc{});
    text.append(reinterpret_cast<const char8_t*>(number),
                static_cast<size_t>(end - number));
    text += ' ';
  }
  Paragraph paragraph;
  paragraph.appendText(text, basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 320));

  const uint64_t callsBefore = fontContext.stats().shapeCalls;
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  const uint64_t newShapeCallCount =
      fontContext.stats().shapeCalls - callsBefore;

  EXPECT_TRUE(layout.overflowed());
  EXPECT_GT(layout.runs.size(), 50u);
  EXPECT_GT(paragraph.shapedWordCount(), layout.runs.size() / 2);
  // Only the frontier is shaped: eager shaping would be one call per word
  // of the whole thirty thousand.
  EXPECT_LT(newShapeCallCount, 3000u) << "overflow text was shaped eagerly";
  EXPECT_LT(paragraph.shapedWordCount(), 3000u);

  // Full shaping still available on demand (measurement, queries, …).
  paragraph.ensureShaped(fontContext);
  EXPECT_EQ(paragraph.shapedWordCount(), paragraph.words().size());
  EXPECT_GT(paragraph.naturalWidth(fontContext),
            400.0f * 300.0f);  // ~30k words wide
}

// ── Line clamp (OverflowOptions::maxLines) ───────────────────────────────

TEST(LineClamp, ClampsWithEllipsisOnLastLine) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(
      u8"a paragraph long enough to fill five or six lines in this narrow "
      "measure keeps flowing and flowing until the clamp cuts it short");
  BlockFlow flow(SkRect::MakeWH(220, 1000));  // room for many lines
  ParagraphLayoutOptions options;
  options.overflow.maxLines = 2;
  options.overflow.ellipsis = u"…";

  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_TRUE(layout.overflowed());
  EXPECT_TRUE(layout.ellipsized);
  EXPECT_LE(layout.lineCount, 2);
  int maxLineIndex = 0;
  for (const PositionedRun& run : layout.runs)
    maxLineIndex = std::max(maxLineIndex, run.lineIndex);
  EXPECT_LT(maxLineIndex, 2) << "no run may land past the clamp";

  // Without the clamp the same layout uses more lines.
  ParagraphLayoutOptions unclamped;
  ParagraphLayout full =
      layoutParagraph(fontContext, paragraph, flow, unclamped);
  EXPECT_GT(full.lineCount, 2);
  EXPECT_FALSE(full.overflowed());
}

TEST(LineClamp, TruncatesSilentlyWithoutEllipsis) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(
      u8"plenty of words that will not fit inside a single clamped line at "
      "all in this measure");
  BlockFlow flow(SkRect::MakeWH(200, 1000));
  ParagraphLayoutOptions options;
  options.overflow.maxLines = 1;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_TRUE(layout.overflowed());
  EXPECT_FALSE(layout.ellipsized);
  EXPECT_EQ(layout.lineCount, 1);
}

TEST(LineClamp, WorksUnderKnuthPlassAndExclusions) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(
      u8"text flows around the circle while the clamp limits how far down "
      "the exclusion geometry the paragraph is allowed to travel at all");
  ExclusionFlow flow(SkRect::MakeWH(300, 1000));
  flow.shapes().push_back(
      ExclusionFlow::Shape::fromCircle(SkRect::MakeXYWH(100, 20, 90, 90), 4));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  options.overflow.maxLines = 3;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_TRUE(layout.overflowed());
  for (const PositionedRun& run : layout.runs) EXPECT_LT(run.lineIndex, 3);
}

TEST(LineClamp, RespectsMandatoryBreaks) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"one\ntwo\nthree\nfour");
  BlockFlow flow(SkRect::MakeWH(400, 1000));
  ParagraphLayoutOptions options;
  options.overflow.maxLines = 2;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_TRUE(layout.overflowed());
  EXPECT_EQ(layout.lineCount, 2) << "clamp counts hard-broken lines too";
}
