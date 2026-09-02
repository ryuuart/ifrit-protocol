/** @file
 * Hyphenation as the layout reads it: where the pattern breaks reach the
 * word list, the zone that leaves an already-square line ragged, and the
 * minimum word length settled in the analysis.
 */

#include <gtest/gtest.h>
#include <sigilweave/kit/Hyphenation.h>

#include <string>
#include <vector>

#include "support/LayoutSupport.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

// ── Hyphenation ───────────────────────────────────────────────────────────

TEST(Hyphenation, PatternBreaksReachTheWordList) {
  FontContext& fonts = sharedContext();
  static const kit::PatternHyphenator hyphenator("en",
                                                 kit::patterns::english());
  TextStyle style = basicStyle(16.0f);
  style.shaping.languageTag = "en-US";
  Paragraph paragraph;
  paragraph.appendText(u8"hyphenation", style);
  BlockFlow flow(SkRect::MakeWH(40, 400));
  ParagraphLayoutOptions options;
  options.hyphenation.patterns = &hyphenator;
  layoutParagraph(fonts, paragraph, flow, options);
  int opportunities = 0;
  for (const Word& word : paragraph.words())
    if (word.hyphenBreak) ++opportunities;
  EXPECT_GT(opportunities, 0);
}

/// How many placed lines end in a hyphen glyph.
int hyphenatedLines(const ParagraphLayout& layout, const Paragraph& paragraph) {
  int count = 0;
  for (const PositionedRun& run : layout.runs)
    if (paragraph.words()[run.wordIndex].hyphenBreak &&
        (&run == &layout.runs.back() || (&run + 1)->lineIndex != run.lineIndex))
      ++count;
  return count;
}

TEST(Hyphenation, TheZoneLeavesALineThatIsAlreadySquareEnoughRagged) {
  FontContext& fonts = sharedContext();
  static const kit::PatternHyphenator hyphenator("en",
                                                 kit::patterns::english());
  TextStyle style = basicStyle(16.0f);
  style.shaping.languageTag = "en-US";
  const auto fillWith = [&](float zone, LineBreakStrategy strategy) {
    Paragraph paragraph;
    paragraph.appendText(
        u8"The typography of hyphenation and justification is a discipline "
        u8"of considerable subtlety and consequence.",
        style);
    BlockFlow flow(SkRect::MakeWH(110, 600));
    ParagraphLayoutOptions options;
    options.lineBreakStrategy = strategy;
    options.hyphenation.patterns = &hyphenator;
    options.hyphenation.penalty = 0;
    options.hyphenation.zone = zone;
    const ParagraphLayout layout =
        layoutParagraph(fonts, paragraph, flow, options);
    return hyphenatedLines(layout, paragraph);
  };
  for (const LineBreakStrategy strategy :
       {LineBreakStrategy::kGreedy, LineBreakStrategy::kKnuthPlass}) {
    EXPECT_GT(fillWith(0.0f, strategy), 1);
    // A zone as wide as the measure refuses every break a whole word could
    // have avoided: no ragged line ends further than the whole measure
    // from it. What survives is the word that is the line — there is
    // nothing else on it for the zone to measure.
    EXPECT_LT(fillWith(110.0f, strategy), fillWith(0.0f, strategy));
  }
}

TEST(Hyphenation, TheZoneIsARaggedSettingRuleAndAJustifiedLineIgnoresIt) {
  FontContext& fonts = sharedContext();
  static const kit::PatternHyphenator hyphenator("en",
                                                 kit::patterns::english());
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
  options.hyphenation.patterns = &hyphenator;
  options.hyphenation.zone = 180.0f;
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  EXPECT_GT(hyphenatedLines(layout, paragraph), 0);
}

TEST(Hyphenation, TheMinimumWordLengthIsSettledInTheAnalysis) {
  FontContext& fonts = sharedContext();
  static const kit::PatternHyphenator hyphenator("en",
                                                 kit::patterns::english());
  TextStyle style = basicStyle(16.0f);
  style.shaping.languageTag = "en-US";
  Paragraph paragraph;
  paragraph.appendText(u8"hyphenation", style);
  BlockFlow flow(SkRect::MakeWH(40, 400));
  ParagraphLayoutOptions options;
  options.hyphenation.patterns = &hyphenator;
  options.hyphenation.limits.minimumWordLength = 40;
  layoutParagraph(fonts, paragraph, flow, options);
  for (const Word& word : paragraph.words()) EXPECT_FALSE(word.hyphenBreak);
}
