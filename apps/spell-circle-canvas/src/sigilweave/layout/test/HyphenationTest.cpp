/** @file
 * Hyphenation as the layout reads it: the soft hyphen an author wrote into
 * the text, and the pattern table a language brings to it. Where the breaks
 * reach the word list, when the hyphen glyph is drawn, what turning
 * hyphenation off does to the word list, the zone that leaves an
 * already-square line ragged, and the minimum word length settled in the
 * analysis.
 */

#include <gtest/gtest.h>
#include <include/core/SkTextBlob.h>
#include <sigilweave/kit/Hyphenation.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "support/LayoutSupport.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

/// The English pattern table every table question here is asked of.
const kit::PatternHyphenator& englishPatterns() {
  static const kit::PatternHyphenator hyphenator(
      "en", kit::englishHyphenationPatterns());
  return hyphenator;
}

/// A word carrying one soft hyphen, and a measure too narrow for the whole
/// of it: the only thing that can decide the line count is whether the
/// hyphen is a break opportunity. At 16 px in the instrument face a letter
/// is 9.6 px and the hyphen 6.4, so the whole word is 144 px, its first
/// half with the hyphen 54.4 and its second half 96: the measure holds
/// either half and not the word.
constexpr std::u8string_view kOneSoftHyphen = u8"extra­ordinarily";
constexpr float kHalfWordMeasure = 110.0f;

/// Both breakers weigh a discretionary break, so every claim about one is
/// a claim about both.
class SoftHyphenBreaker : public BrokenBothWays {};

}  // namespace

// ── The soft hyphen an author wrote ──────────────────────────────────────

TEST(SoftHyphen, ABrokenWordDrawsItsHyphenGlyph) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"an extra­ordinarily narrow measure");
  BlockFlow flow(SkRect::MakeWH(kHalfWordMeasure, 300));
  ParagraphLayout layout = layoutParagraph(fonts, paragraph, flow);

  const Word* hyphenWord = nullptr;
  for (const Word& word : paragraph.words())
    if (word.hyphenBreak) hyphenWord = &word;
  ASSERT_NE(hyphenWord, nullptr);
  ASSERT_TRUE(hyphenWord->hyphenGlyph);

  const SkTextBlob* hyphenBlob = wordBlob(*hyphenWord->hyphenGlyph).get();
  bool found = false;
  for (const PositionedRun& run : layout.runs)
    found |= run.blob.get() == hyphenBlob;
  EXPECT_TRUE(found);
}

TEST(SoftHyphen, AnUnbrokenWordNeverDrawsItsHyphenGlyph) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(kOneSoftHyphen);
  BlockFlow flow(SkRect::MakeWH(500, 100));  // plenty of room: no break
  ParagraphLayout layout = layoutParagraph(fonts, paragraph, flow);

  ASSERT_EQ(paragraph.words().size(), 2u);  // the two halves
  const Word& first = paragraph.words()[0];
  EXPECT_TRUE(first.hyphenBreak);
  EXPECT_FLOAT_EQ(first.spaceWidth, 0.0f) << "halves join with zero gap";
  const SkTextBlob* hyphenBlob = wordBlob(*first.hyphenGlyph).get();
  for (const PositionedRun& run : layout.runs)
    EXPECT_NE(run.blob.get(), hyphenBlob) << "hyphen rendered without break";
  EXPECT_EQ(layout.lineCount, 1) << "both halves sit on one line";
}

TEST_P(SoftHyphenBreaker, ADiscretionaryBreakIsTakenToFitTheMeasure) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(
      u8"the as­ton­ish­ing­ly in­com­pre­hen"
      u8"­si­ble hy­phen­ation ma­chin­ery works");
  BlockFlow flow(SkRect::MakeWH(120, 600));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = breaker();
  options.alignment = TextAlignment::kJustify;
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  EXPECT_FALSE(layout.overflowed());
  EXPECT_GT(layout.lineCount, 2);
  size_t hyphenBreakCount = 0;
  for (const Word& word : paragraph.words())
    hyphenBreakCount += word.hyphenBreak;
  EXPECT_GT(hyphenBreakCount, 0u);
}

TEST_P(SoftHyphenBreaker, TurningHyphenationOffFusesTheHalvesAndRemovesTheBreak) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(kOneSoftHyphen);
  BlockFlow flow(SkRect::MakeWH(kHalfWordMeasure, 300));
  ParagraphLayoutOptions hyphenating;
  hyphenating.lineBreakStrategy = breaker();
  hyphenating.hyphenation.enabled = true;

  const ParagraphLayout broken =
      layoutParagraph(fonts, paragraph, flow, hyphenating);
  ASSERT_EQ(paragraph.words().size(), 2u);
  ASSERT_TRUE(paragraph.words()[0].hyphenBreak);
  EXPECT_EQ(broken.lineCount, 2);
  const SkTextBlob* hyphenBlob =
      wordBlob(*paragraph.words()[0].hyphenGlyph).get();
  bool hyphenOnFirstLine = false;
  for (const PositionedRun& run : broken.runs)
    hyphenOnFirstLine |= run.blob.get() == hyphenBlob && run.lineIndex == 0;
  EXPECT_TRUE(hyphenOnFirstLine);

  // The halves fuse into one word with no discretionary break, so the line
  // overflows the measure rather than splitting — which is the whole point.
  ParagraphLayoutOptions whole = hyphenating;
  whole.hyphenation.enabled = false;
  const ParagraphLayout unbroken =
      layoutParagraph(fonts, paragraph, flow, whole);
  ASSERT_EQ(paragraph.words().size(), 1u);
  EXPECT_FALSE(paragraph.words()[0].hyphenBreak);
  EXPECT_EQ(unbroken.lineCount, 1);
  ASSERT_FALSE(unbroken.runs.empty());
  float extent = 0;
  for (const PositionedRun& run : unbroken.runs)
    extent = std::max(extent, runEnd(paragraph, run));
  EXPECT_GT(extent, kHalfWordMeasure);

  // Turning it back on restores the break, so the decision is not sticky.
  const ParagraphLayout again =
      layoutParagraph(fonts, paragraph, flow, hyphenating);
  EXPECT_EQ(paragraph.words().size(), 2u);
  EXPECT_EQ(again.lineCount, 2);
}

INSTANTIATE_TEST_SUITE_P(Breakers, SoftHyphenBreaker, bothBreakers(),
                         breakerName);

// ── The pattern table a language brings ──────────────────────────────────

TEST(Hyphenation, PatternBreaksReachTheWordList) {
  FontContext& fonts = sigil::test::fonts();
  TextStyle style = basicStyle(16.0f);
  style.shaping.languageTag = "en-US";
  Paragraph paragraph;
  paragraph.appendText(u8"hyphenation", style);
  BlockFlow flow(SkRect::MakeWH(40, 400));
  ParagraphLayoutOptions options;
  options.hyphenation.patterns = &englishPatterns();
  layoutParagraph(fonts, paragraph, flow, options);
  int opportunities = 0;
  for (const Word& word : paragraph.words())
    if (word.hyphenBreak) ++opportunities;
  EXPECT_GT(opportunities, 0);
}

namespace {

/// How many placed lines end in a hyphen glyph.
int hyphenatedLines(const ParagraphLayout& layout, const Paragraph& paragraph) {
  int count = 0;
  for (const PositionedRun& run : layout.runs)
    if (paragraph.words()[run.wordIndex].hyphenBreak &&
        (&run == &layout.runs.back() || (&run + 1)->lineIndex != run.lineIndex))
      ++count;
  return count;
}

/// The zone is a property of the breaking, so both breakers answer for it.
class HyphenationZone : public BrokenBothWays {};

}  // namespace

TEST_P(HyphenationZone, AZoneAsWideAsTheMeasureLeavesTheRagAlone) {
  FontContext& fonts = sigil::test::fonts();
  TextStyle style = basicStyle(16.0f);
  style.shaping.languageTag = "en-US";
  const auto fillWith = [&](float zone) {
    Paragraph paragraph;
    paragraph.appendText(
        u8"The typography of hyphenation and justification is a discipline "
        u8"of considerable subtlety and consequence.",
        style);
    // Wide enough for the passage's longest word, "justification" at 13
    // letters of 9.6 px, so no break is forced on a word the measure
    // cannot hold and every hyphen is one the zone could refuse.
    constexpr float kMeasure = 150.0f;
    BlockFlow flow(SkRect::MakeWH(kMeasure, 600));
    ParagraphLayoutOptions options;
    options.lineBreakStrategy = breaker();
    options.hyphenation.patterns = &englishPatterns();
    options.hyphenation.penalty = 0;
    options.hyphenation.zone = zone;
    const ParagraphLayout layout =
        layoutParagraph(fonts, paragraph, flow, options);
    return hyphenatedLines(layout, paragraph);
  };
  EXPECT_GT(fillWith(0.0f), 1);
  // A zone as wide as the measure refuses every break a whole word could
  // have avoided: no ragged line ends further than the whole measure from
  // it. What survives is the word that is the line — there is nothing else
  // on it for the zone to measure.
  EXPECT_LT(fillWith(150.0f), fillWith(0.0f));
}

INSTANTIATE_TEST_SUITE_P(Breakers, HyphenationZone, bothBreakers(),
                         breakerName);

TEST(Hyphenation, TheZoneIsARaggedSettingRuleAndAJustifiedLineIgnoresIt) {
  FontContext& fonts = sigil::test::fonts();
  TextStyle style = basicStyle(16.0f);
  style.shaping.languageTag = "en-US";
  Paragraph paragraph;
  paragraph.appendText(
      u8"The typography of hyphenation and justification is a discipline "
      u8"of considerable subtlety and consequence.",
      style);
  BlockFlow flow(SkRect::MakeWH(180, 600));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.hyphenation.patterns = &englishPatterns();
  options.hyphenation.zone = 180.0f;
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  EXPECT_GT(hyphenatedLines(layout, paragraph), 0);
}

TEST(Hyphenation, TheMinimumWordLengthIsSettledInTheAnalysis) {
  FontContext& fonts = sigil::test::fonts();
  TextStyle style = basicStyle(16.0f);
  style.shaping.languageTag = "en-US";
  Paragraph paragraph;
  paragraph.appendText(u8"hyphenation", style);
  BlockFlow flow(SkRect::MakeWH(40, 400));
  ParagraphLayoutOptions options;
  options.hyphenation.patterns = &englishPatterns();
  options.hyphenation.limits.minimumWordLength = 40;
  layoutParagraph(fonts, paragraph, flow, options);
  for (const Word& word : paragraph.words()) EXPECT_FALSE(word.hyphenBreak);
}
