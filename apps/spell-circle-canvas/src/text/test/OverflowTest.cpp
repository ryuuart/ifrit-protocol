/** @file
 * Overflow behavior: reporting, ellipsis, and the
 * geometry-bounded cost guarantees for overfull paragraphs.
 */

#include "TestSupport.h"

#include <gtest/gtest.h>

#include <charconv>
#include <chrono>
#include <string>
using namespace textflow;
using namespace textflow::test;

TEST(Overflow, ReportsFirstUnplacedWord) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"far more text than could ever fit inside such a tiny little box");
  BlockFlow flow(SkRect::MakeWH(120, 40));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  EXPECT_TRUE(layout.overflowed());
  EXPECT_GT(layout.firstUnplacedWord, 0u);
}

// ── Overflowing paragraphs must cost what fits, not what exists ───────────

TEST(Overflow, HugeRelayoutIsBoundedByGeometry) {
  FontContext &fontContext = sharedContext();
  static constexpr const char8_t *kWordPool[] = {
      u8"letters", u8"flow",    u8"around",  u8"boxes", u8"while",
      u8"the",     u8"breaker", u8"stops",   u8"at",    u8"geometry",
      u8"instead", u8"of",      u8"walking", u8"every", u8"word"};
  Paragraph paragraph;
  paragraph.appendText(makePooledText(kWordPool, 30000, 11), basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 320)); // room for ~1% of the text

  for (LineBreakStrategy breakStrategy :
       {LineBreakStrategy::kGreedy, LineBreakStrategy::kKnuthPlass}) {
    ParagraphLayoutOptions options;
    options.lineBreakStrategy = breakStrategy;
    options.alignment = TextAlignment::kJustify;
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options); // warm shapes
    EXPECT_TRUE(layout.overflowed());
    EXPECT_GT(layout.runs.size(), 50u);
    EXPECT_LT(layout.firstUnplacedWord, 600u);

    // Warm relayout must not scale with the ~29,700 words that never fit
    // (verified: a 3k-word paragraph relayouts in the same time as this
    // 30k one). The bounds sit far above the geometry-bounded cost but
    // far below an O(total words) regression, which is ~50× slower here.
    const auto startTime = std::chrono::steady_clock::now();
    constexpr int kIterationCount = 20;
    for (int iteration = 0; iteration < kIterationCount; ++iteration)
      layout = layoutParagraph(fontContext, paragraph, flow, options);
    const double averageMicroseconds =
        std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - startTime)
            .count() /
        kIterationCount;
#ifdef NDEBUG
    const double maximumMicroseconds = 2000.0;
#else
    const double maximumMicroseconds =
        20000.0; // Debug: same work, ~20× the overhead
#endif
    EXPECT_LT(averageMicroseconds, maximumMicroseconds)
        << (breakStrategy == LineBreakStrategy::kGreedy ? "greedy"
                                                        : "knuth-plass")
        << " relayout scales with unplaced text";
  }
}

TEST(Overflow, EllipsisMarksOverflow) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"far more text than a two line box can ever hope to hold so the "
      "marker has to step in and admit that the rest is missing");
  BlockFlow flow(SkRect::MakeWH(260, 44)); // ~2 lines
  ParagraphLayoutOptions options;
  options.overflow.ellipsis = u"…";

  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_TRUE(layout.overflowed());
  ASSERT_TRUE(layout.ellipsized);
  ASSERT_FALSE(layout.runs.empty());
  const PositionedRun &marker = layout.runs.back();
  ASSERT_TRUE(marker.shaped);
  // The marker sits at the end of the final line, inside the measure.
  EXPECT_EQ(marker.lineIndex, layout.runs[layout.runs.size() - 2].lineIndex);
  EXPECT_LE(marker.origin.x() + marker.shaped->advance, 260.0f + 0.75f);
  EXPECT_GT(marker.origin.x(), 0.0f);
  // Truncated words count as unplaced.
  for (const PositionedRun &run : layout.runs)
    if (&run != &marker)
      EXPECT_LT(run.wordIndex, layout.firstUnplacedWord);
}

TEST(Overflow, NoEllipsisWhenTextFits) {
  FontContext &fontContext = sharedContext();
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
  FontContext &fontContext = sharedContext();
  std::u8string text;
  for (int wordIndex = 0; wordIndex < 30000; ++wordIndex) {
    text += u8"word";
    char number[16];
    const auto [end, error] =
        std::to_chars(std::begin(number), std::end(number), wordIndex);
    ASSERT_EQ(error, std::errc{});
    text.append(reinterpret_cast<const char8_t *>(number),
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
  // ~600 placed words (+ glue + slack); eager shaping would be ~30,000.
  EXPECT_LT(newShapeCallCount, 3000u) << "overflow text was shaped eagerly";
  EXPECT_LT(paragraph.shapedWordCount(), 3000u);

  // Full shaping still available on demand (measurement, queries, …).
  paragraph.ensureShaped(fontContext);
  EXPECT_EQ(paragraph.shapedWordCount(), paragraph.words().size());
  EXPECT_GT(paragraph.naturalWidth(fontContext),
            400.0f * 300.0f); // ~30k words wide
}

// ── Line clamp (OverflowOptions::maxLines) ───────────────────────────────

TEST(LineClamp, ClampsWithEllipsisOnLastLine) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"a paragraph long enough to fill five or six lines in this narrow "
      "measure keeps flowing and flowing until the clamp cuts it short");
  BlockFlow flow(SkRect::MakeWH(220, 1000)); // room for many lines
  ParagraphLayoutOptions options;
  options.overflow.maxLines = 2;
  options.overflow.ellipsis = u"…";

  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_TRUE(layout.overflowed());
  EXPECT_TRUE(layout.ellipsized);
  EXPECT_LE(layout.lineCount, 2);
  int maxLineIndex = 0;
  for (const PositionedRun &run : layout.runs)
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
  FontContext &fontContext = sharedContext();
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
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"text flows around the circle while the clamp limits how far down "
      "the exclusion geometry the paragraph is allowed to travel at all");
  ExclusionFlow flow(SkRect::MakeWH(300, 1000));
  flow.shapes().push_back(ExclusionFlow::Shape::fromCircle(
      SkRect::MakeXYWH(100, 20, 90, 90), 4));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  options.overflow.maxLines = 3;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_TRUE(layout.overflowed());
  for (const PositionedRun &run : layout.runs)
    EXPECT_LT(run.lineIndex, 3);
}

TEST(LineClamp, RespectsMandatoryBreaks) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"one\ntwo\nthree\nfour");
  BlockFlow flow(SkRect::MakeWH(400, 1000));
  ParagraphLayoutOptions options;
  options.overflow.maxLines = 2;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_TRUE(layout.overflowed());
  EXPECT_EQ(layout.lineCount, 2) << "clamp counts hard-broken lines too";
}

// ── Tab stops (ParagraphLayoutOptions::tabStops) ─────────────────────────

namespace {

/// x origin of the run for the word whose content is `needle`.
float runOriginFor(const Paragraph &paragraph, const ParagraphLayout &layout,
                   std::u16string_view needle) {
  const std::u16string &text = paragraph.text();
  for (const PositionedRun &run : layout.runs) {
    const Word &word = paragraph.words()[run.wordIndex];
    if (std::u16string_view(text).substr(word.textBegin,
                                         word.textEnd - word.textBegin) ==
        needle)
      return run.origin.x();
  }
  return -1.0f;
}

} // namespace

TEST(TabStops, ExplicitStopsAlignColumns) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"ab\tlongerhead\tx");
  BlockFlow flow(SkRect::MakeWH(600, 60));
  ParagraphLayoutOptions options;
  options.tabStops.positions = {120.0f, 300.0f};
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_EQ(layout.lineCount, 1);
  EXPECT_FLOAT_EQ(runOriginFor(paragraph, layout, u"longerhead"), 120.0f);
  EXPECT_FLOAT_EQ(runOriginFor(paragraph, layout, u"x"), 300.0f);
}

TEST(TabStops, RepeatingIntervalAfterExplicitStops) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"a\tb\tc\td");
  BlockFlow flow(SkRect::MakeWH(800, 60));
  ParagraphLayoutOptions options;
  options.tabStops.positions = {50.0f};
  options.tabStops.interval = 100.0f;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_EQ(layout.lineCount, 1);
  EXPECT_FLOAT_EQ(runOriginFor(paragraph, layout, u"b"), 50.0f);
  EXPECT_FLOAT_EQ(runOriginFor(paragraph, layout, u"c"), 150.0f);
  EXPECT_FLOAT_EQ(runOriginFor(paragraph, layout, u"d"), 250.0f);
}

TEST(TabStops, ContentPastStopAdvancesToNext) {
  FontContext &fontContext = sharedContext();
  // "wideenough" extends past the 40px stop, so the tab after it must jump
  // to the following stop instead of backing up.
  Paragraph paragraph = makeParagraph(u8"wideenoughcontent\tafter");
  BlockFlow flow(SkRect::MakeWH(800, 60));
  ParagraphLayoutOptions options;
  options.tabStops.positions = {40.0f, 400.0f};
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_FLOAT_EQ(runOriginFor(paragraph, layout, u"after"), 400.0f);
}

TEST(TabStops, WrapsWhenStopExceedsMeasure) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"head\ttail");
  BlockFlow flow(SkRect::MakeWH(200, 200));
  ParagraphLayoutOptions options;
  options.tabStops.positions = {180.0f}; // "tail" cannot fit after the stop
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_GT(layout.lineCount, 1) << "unfittable tabbed word wraps";
  EXPECT_FALSE(layout.overflowed());
}

TEST(TabStops, UnconfiguredTabsStillMeasureAsSpaces) {
  FontContext &fontContext = sharedContext();
  Paragraph tab = makeParagraph(u8"a\tb");
  Paragraph space = makeParagraph(u8"a b");
  BlockFlow tabFlow(SkRect::MakeWH(400, 60));
  BlockFlow spaceFlow(SkRect::MakeWH(400, 60));
  ParagraphLayout tabLayout = layoutParagraph(fontContext, tab, tabFlow);
  ParagraphLayout spaceLayout = layoutParagraph(fontContext, space, spaceFlow);
  EXPECT_FLOAT_EQ(runOriginFor(tab, tabLayout, u"b"),
                  runOriginFor(space, spaceLayout, u"b"));
}
