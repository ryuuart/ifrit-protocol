/** @file
 * The three passes a justified line is fitted in — the word gaps, then
 * letter spacing between the glyphs, then a horizontal scale on them — and
 * the two rules about which lines are justified at all. Every claim is
 * read off where the lines and the words actually landed, because a pass
 * that spent nothing is a pass that moved nothing.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

/// A passage long enough, and awkward enough, that a narrow measure leaves
/// the word gaps unable to fill every line on their own — which is what
/// gives the later passes anything to do.
constexpr std::u8string_view kTightPassage =
    u8"Justification spends interword gaps before letterspacing, and "
    u8"reaches for horizontal glyph-scaling last of all.";

/// The measure the passage is set in, at 12 px in the instrument face —
/// a letter is 7.2 px, a space 3.6, a mark 4.8. Greedy breaking fills it
/// as "Justification spends" (140.4), "interword gaps before" (144),
/// "letterspacing, and reaches" (177.6), "for horizontal" (97.2) and
/// "glyph-scaling last of all." (171.6): five lines, every one holding a
/// gap for justification to spend, and the last short of the measure.
constexpr float kMeasure = 180.0f;

/// The passage set justified under `spec`, in `measure`.
LaidOut justified(const JustificationOptions& spec, std::u8string_view body,
                  float measure,
                  LineBreakStrategy strategy = LineBreakStrategy::kGreedy) {
  Paragraph paragraph = makeParagraph(body, 12.0f);
  BlockFlow flow(SkRect::MakeWH(measure, 400));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineBreakStrategy = strategy;
  options.justification = spec;
  ParagraphLayout layout =
      layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  return {std::move(paragraph), std::move(layout)};
}

/// Where each line of a justified setting ended. Read off the line
/// metrics rather than off the shaped advances, because two of the three
/// passes spend their fit INSIDE the runs — a pen reading would be short
/// by exactly what the letter and glyph passes took.
std::vector<float> edgesUnder(
    const JustificationOptions& spec, std::u8string_view body, float measure,
    LineBreakStrategy strategy = LineBreakStrategy::kGreedy) {
  const LaidOut set = justified(spec, body, measure, strategy);
  std::vector<float> edges;
  for (const LineMetrics& line : set.layout.lineMetrics(set.paragraph))
    edges.push_back(line.right);
  return edges;
}

/// Where each placed run of a justified setting started. A pass changes
/// the INTERIOR of a line — how much of the fit stands in the gaps and how
/// much between the letters — so this is what a claim about one reads.
std::vector<float> runStartsUnder(const JustificationOptions& spec,
                                  std::u8string_view body, float measure) {
  const LaidOut set = justified(spec, body, measure);
  std::vector<float> starts;
  for (const PositionedRun* run : wordRuns(set.layout))
    starts.push_back(run->origin.x());
  return starts;
}

/// True when `spec` places the runs anywhere the stock settings did not.
bool movesTheRuns(const JustificationOptions& spec) {
  const std::vector<float> stock = runStartsUnder({}, kTightPassage, kMeasure);
  const std::vector<float> under =
      runStartsUnder(spec, kTightPassage, kMeasure);
  if (under.size() != stock.size()) return true;
  for (size_t index = 0; index < under.size(); ++index)
    if (std::abs(under[index] - stock[index]) > 0.5f) return true;
  return false;
}

}  // namespace

TEST(Justification, ShrinkNeverCollapsesASpacePastItsLimit) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(
      u8"several reasonably long words keep justification honest here", 18.0f);
  paragraph.ensureShaped(fontContext);
  // A measure a hair narrower than a natural line forces shrink.
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  BlockFlow flow(SkRect::MakeWH(200, 400));
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  ASSERT_GT(layout.lineCount, 1);
  for (size_t runIndex = 0; runIndex + 1 < layout.runs.size(); ++runIndex) {
    const PositionedRun& firstRun = layout.runs[runIndex];
    const PositionedRun& secondRun = layout.runs[runIndex + 1];
    if (firstRun.lineIndex != secondRun.lineIndex) continue;
    const float gapWidth = secondRun.origin.x() - runEnd(paragraph, firstRun);
    const float naturalSpaceWidth =
        paragraph.words()[firstRun.wordIndex].spaceWidth;
    if (naturalSpaceWidth <= 0) continue;
    // Shrink is clamped at JustificationOptions::spaceShrink, a fraction of
    // the natural space width, which defaults to one third.
    EXPECT_GT(gapWidth, naturalSpaceWidth * (1.0f - 0.34f) - 0.25f)
        << "space collapsed past the shrink limit on line "
        << firstRun.lineIndex;
  }
}

TEST(Justification, TheGapsAloneFillEveryLineButTheLast) {
  const std::vector<float> stock = edgesUnder({}, kTightPassage, kMeasure);
  ASSERT_GE(stock.size(), 4u);
  for (size_t index = 0; index + 1 < stock.size(); ++index)
    EXPECT_NEAR(stock[index], kMeasure, 0.75f) << "line " << index;
  EXPECT_LT(stock.back(), kMeasure - 2.0f)
      << "the last line was justified unasked";
}

TEST(Justification, OpeningTheLetterPassSetsThePassageDifferently) {
  JustificationOptions letters;
  letters.letterSpacing = 0.05f;
  letters.letterSpacingMaximum = 0.1f;
  EXPECT_TRUE(movesTheRuns(letters));
}

TEST(Justification, AGlyphScalePinnedBothSidesSetsThePassageDifferently) {
  // Room above a desired value is room the fit spends: a scale of 0.92 with
  // the stock maximum of 1 is a scale of 1 on every line that needed
  // widening, which is the stock setting and not a different one. A scale
  // meant to hold says so with its limits.
  JustificationOptions loose;
  loose.glyphScale = 0.92f;
  loose.glyphScaleMinimum = 0.92f;
  JustificationOptions pinned = loose;
  pinned.glyphScaleMaximum = 0.92f;
  EXPECT_TRUE(movesTheRuns(pinned));
}

TEST(Justification, TheWidthAGapIsAimedAtIsWhatABreakIsWeighedAgainst) {
  // The gap pass's desired width is not a placement decision — a justified
  // line fills its measure whatever the gaps were aimed at. It is what the
  // optimizing breaker weighs a break against, so a passage aimed at twice
  // the shaped space is BROKEN differently.
  JustificationOptions wider;
  wider.wordSpacing = 2.0f;
  const std::vector<float> stock =
      edgesUnder({}, kTightPassage, kMeasure, LineBreakStrategy::kKnuthPlass);
  const std::vector<float> aimed = edgesUnder(wider, kTightPassage, kMeasure,
                                              LineBreakStrategy::kKnuthPlass);
  bool differs = aimed.size() != stock.size();
  for (size_t index = 0; !differs && index < aimed.size(); ++index)
    differs = std::abs(aimed[index] - stock[index]) > 0.5f;
  EXPECT_TRUE(differs);
}

TEST(Justification, TheLetterPassTakesWhatTheGapsMayNotStretchTo) {
  // The gaps open only to their own stretch limit where a later pass can
  // spend what they may not: with the letter pass opened, the gaps stay
  // near the width they were aimed at and the letters take the rest, so
  // the line still reaches the measure with tighter word spacing than the
  // gaps alone would have left.
  JustificationOptions tightGaps;
  tightGaps.spaceStretch = 0.02f;
  tightGaps.letterSpacingMaximum = 0.3f;
  const std::vector<float> edges =
      edgesUnder(tightGaps, kTightPassage, kMeasure);
  ASSERT_GE(edges.size(), 2u);
  EXPECT_NEAR(edges.front(), kMeasure, 1.0f) << "the line did not fill";
  const std::vector<float> tight =
      runStartsUnder(tightGaps, kTightPassage, kMeasure);
  const std::vector<float> stock = runStartsUnder({}, kTightPassage, kMeasure);
  ASSERT_GE(tight.size(), 2u);
  ASSERT_GE(stock.size(), 2u);
  EXPECT_LT(tight[1], stock[1] - 1.0f)
      << "the gaps opened as wide as they always did";
}

TEST(Justification, GapsStayUnboundedWhenNoLaterPassCanSpendWhatTheyDrop) {
  // A bound on the gaps with both later passes shut would open a hole at
  // the right margin that nothing in the line is allowed to close. So a
  // stretch limit alone, and a rule about lone-word lines alone, leave
  // every ordinary line set exactly as the gaps alone set it.
  const std::vector<float> stock = edgesUnder({}, kTightPassage, kMeasure);
  ASSERT_GE(stock.size(), 2u);
  JustificationOptions tightGaps;
  tightGaps.spaceStretch = 0.02f;
  JustificationOptions lone;
  lone.singleWord = JustificationOptions::SingleWord::kJustify;
  for (const JustificationOptions& spec : {tightGaps, lone}) {
    const std::vector<float> edges = edgesUnder(spec, kTightPassage, kMeasure);
    ASSERT_EQ(edges.size(), stock.size());
    for (size_t index = 0; index < edges.size(); ++index)
      EXPECT_NEAR(edges[index], stock[index], 0.5f);
  }
}

TEST(Justification, ALineOfOneWordStretchesOnlyWhenAskedTo) {
  // A line holding ONE word has no gaps to spend at all: kAlign leaves it
  // at the block's alignment, kJustify stretches it across the measure
  // with letter spacing alone.
  JustificationOptions aligned;
  aligned.justifyLastLine = true;
  JustificationOptions stretched = aligned;
  stretched.singleWord = JustificationOptions::SingleWord::kJustify;

  // The measure is derived from the word's own shaped width rather than
  // named, so the word is on one line whatever face this machine hands us
  // and the stretch target is a number the case computed.
  constexpr std::u8string_view kWord = u8"Antidisestablishmentarianism";
  Paragraph alone = makeParagraph(kWord, 12.0f);
  const float measure = alone.naturalWidth(sigil::test::fonts()) + 40.0f;

  const std::vector<float> left = edgesUnder(aligned, kWord, measure);
  const std::vector<float> spread = edgesUnder(stretched, kWord, measure);
  ASSERT_EQ(left.size(), 1u) << "the word did not fit on one line";
  ASSERT_EQ(spread.size(), 1u);
  EXPECT_LT(left.front(), measure - 20.0f);
  EXPECT_NEAR(spread.front(), measure, 1.0f);
}

TEST(Justification, TheLastLineJustifiesOnlyWhenAskedTo) {
  const std::vector<float> ragged = edgesUnder({}, kTightPassage, kMeasure);
  JustificationOptions all;
  all.justifyLastLine = true;
  const std::vector<float> full = edgesUnder(all, kTightPassage, kMeasure);
  ASSERT_EQ(ragged.size(), full.size());
  EXPECT_LT(ragged.back(), kMeasure - 2.0f);
  EXPECT_NEAR(full.back(), kMeasure, 1.0f);
}

TEST(Justification, TheLastLineTakesItsOwnAlignmentAndNotTheParagraphs) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(
      u8"a justified paragraph whose final line is pushed to the right edge "
      "instead of hanging on the left like usual short last lines do");
  BlockFlow flow(SkRect::MakeWH(260, 600));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.justification.lastLineAlignment = TextAlignment::kEnd;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_GT(layout.lineCount, 1);

  // The last line's last word ends at the right edge, and its first run
  // does not start at the left one.
  float lastLineEnd = 0, lastLineStart = 1e9f;
  for (const PositionedRun& run : layout.runs) {
    if (run.lineIndex != layout.lineCount - 1) continue;
    lastLineEnd = std::max(lastLineEnd, runEnd(paragraph, run));
    lastLineStart = std::min(lastLineStart, run.origin.x());
  }
  EXPECT_NEAR(lastLineEnd, 260.0f, 1.0f);
  EXPECT_GT(lastLineStart, 5.0f);
}

namespace {

/// The spec with only its method changed.
JustificationOptions under(JustificationMethod method,
                           JustificationOptions spec = {}) {
  spec.method = method;
  return spec;
}

/// Where a run ends as the line set it, its fit included.
float fittedEnd(const PositionedRun& run) {
  return run.origin.x() + run.advance;
}

/// Every line of a setting, as the runs it holds.
std::vector<std::vector<const PositionedRun*>> linesOf(const LaidOut& set) {
  std::vector<std::vector<const PositionedRun*>> lines(
      static_cast<size_t>(std::max(set.layout.lineCount, 0)));
  for (const PositionedRun* run : wordRuns(set.layout))
    if (run->lineIndex >= 0 && run->lineIndex < set.layout.lineCount)
      lines[static_cast<size_t>(run->lineIndex)].push_back(run);
  return lines;
}

}  // namespace

TEST(JustificationMethod, InterCharacterOpensEveryClusterByOneAmount) {
  // Every justified line still reaches the measure, and it does so with
  // spacing after every cluster: each run of a line but the last carries
  // one cluster spacing, the same for every run of that line.
  const JustificationOptions spec = under(JustificationMethod::kInterCharacter);
  const std::vector<float> edges = edgesUnder(spec, kTightPassage, kMeasure);
  ASSERT_GE(edges.size(), 4u);
  for (size_t index = 0; index + 1 < edges.size(); ++index)
    EXPECT_NEAR(edges[index], kMeasure, 0.75f) << "line " << index;
  EXPECT_LT(edges.back(), kMeasure - 2.0f) << "the last line was justified";

  const LaidOut set = justified(spec, kTightPassage, kMeasure);
  const auto lines = linesOf(set);
  for (size_t line = 0; line + 1 < lines.size(); ++line) {
    ASSERT_FALSE(lines[line].empty());
    const float spacing = lines[line].front()->fit.clusterSpacing;
    EXPECT_GT(spacing, 0.0f) << "line " << line;
    EXPECT_FLOAT_EQ(lines[line].front()->fit.letterSpacing, 0.0f);
    for (const PositionedRun* run : lines[line])
      EXPECT_FLOAT_EQ(run->fit.clusterSpacing, spacing) << "line " << line;
  }
  for (const PositionedRun* run : lines.back())
    EXPECT_TRUE(run->fit.plain()) << "the last line was respaced";
}

TEST(JustificationMethod, InterCharacterSpacingStopsAtItsCap) {
  // Two words in a wide measure: each of the five opportunities — the
  // four clusters and the one separator — would open far past the cap,
  // so the clusters stop at it and the separator takes the rest; the
  // line still reaches the measure.
  JustificationOptions spec = under(JustificationMethod::kInterCharacter);
  spec.justifyLastLine = true;
  spec.maxInterCharacterExpansion = 0.25f;
  const LaidOut set = justified(spec, u8"ab cd", 160.0f);
  ASSERT_EQ(set.layout.lineCount, 1);
  const auto runs = wordRuns(set.layout);
  ASSERT_EQ(runs.size(), 2u);
  EXPECT_FLOAT_EQ(runs.front()->fit.clusterSpacing, 0.25f * 12.0f);
  EXPECT_NEAR(fittedEnd(*runs.back()), 160.0f, 0.75f);

  // One word has no separator to hand the rest to, and stays short.
  const LaidOut alone = justified(spec, u8"abcd", 160.0f);
  const auto word = wordRuns(alone.layout);
  ASSERT_EQ(word.size(), 1u);
  EXPECT_FLOAT_EQ(word.front()->fit.clusterSpacing, 0.25f * 12.0f);
  EXPECT_LT(fittedEnd(*word.front()), 160.0f - 20.0f);
}

TEST(JustificationMethod, InterWordLeavesTheLetterPassOut) {
  // The letter pass opened, and then a method that names the separators
  // alone: the passage is set exactly as the gaps alone set it.
  JustificationOptions letters;
  letters.letterSpacing = 0.05f;
  letters.letterSpacingMaximum = 0.1f;
  ASSERT_TRUE(movesTheRuns(letters));
  EXPECT_FALSE(movesTheRuns(under(JustificationMethod::kInterWord, letters)));
  const std::vector<float> edges = edgesUnder(
      under(JustificationMethod::kInterWord), kTightPassage, kMeasure);
  ASSERT_GE(edges.size(), 4u);
  for (size_t index = 0; index + 1 < edges.size(); ++index)
    EXPECT_NEAR(edges[index], kMeasure, 0.75f) << "line " << index;
}

TEST(JustificationMethod, NoneSetsAJustifiedLineAtItsStart) {
  // Justified with no method is a passage set from its start: the same
  // lines, every one ending where its words end.
  const LaidOut none =
      justified(under(JustificationMethod::kNone), kTightPassage, kMeasure);
  Paragraph paragraph = makeParagraph(kTightPassage, 12.0f);
  BlockFlow flow(SkRect::MakeWH(kMeasure, 400));
  ParagraphLayoutOptions start;
  ParagraphLayout ragged =
      layoutParagraph(sigil::test::fonts(), paragraph, flow, start);
  const auto set = none.layout.lineMetrics(none.paragraph);
  const auto plain = ragged.lineMetrics(paragraph);
  ASSERT_EQ(set.size(), plain.size());
  for (size_t index = 0; index < set.size(); ++index) {
    EXPECT_FLOAT_EQ(set[index].left, plain[index].left) << "line " << index;
    EXPECT_FLOAT_EQ(set[index].right, plain[index].right) << "line " << index;
  }
}
