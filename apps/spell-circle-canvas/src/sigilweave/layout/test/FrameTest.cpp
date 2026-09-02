/** @file
 * The frame: where its first baseline is seated, how leftover room is
 * distributed down it, and the keeps a block enforces at the boundary
 * where one frame hands over to the next.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "support/LayoutSupport.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

// ── The frame ─────────────────────────────────────────────────────────────

TEST(FrameSeating, FixedFirstBaselineMovesTheWholePassage) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(u8"one two three four five six seven");
  BlockFlow flow(SkRect::MakeWH(120, 600));
  ParagraphLayoutOptions options;
  options.frame.firstBaseline = FrameOptions::FirstBaseline::kFixed;
  options.frame.firstBaselineOffset = 60.0f;
  const std::vector<float> lines =
      baselines(layoutParagraph(fonts, paragraph, flow, options));
  ASSERT_GE(lines.size(), 2u);
  EXPECT_NEAR(lines[0], 60.0f, 0.01f);
  const Paragraph::Strut strut = paragraph.strutAt(fonts, 0);
  EXPECT_NEAR(lines[1] - lines[0], strut.height, 0.01f);
}

TEST(FrameSeating, CentredDistributionHalvesTheLeftoverRoom) {
  FontContext& fonts = sharedContext();
  Paragraph plain = makeParagraph(u8"one two three four");
  Paragraph centred = makeParagraph(u8"one two three four");
  BlockFlow flowA(SkRect::MakeWH(120, 400));
  BlockFlow flowB(SkRect::MakeWH(120, 400));
  const std::vector<float> bare =
      baselines(layoutParagraph(fonts, plain, flowA));
  ParagraphLayoutOptions options;
  options.frame.extent = 400.0f;
  options.frame.distribute = FrameOptions::Distribute::kCenter;
  const std::vector<float> moved =
      baselines(layoutParagraph(fonts, centred, flowB, options));
  ASSERT_EQ(bare.size(), moved.size());
  ASSERT_FALSE(bare.empty());
  const float shift = moved.front() - bare.front();
  EXPECT_GT(shift, 100.0f);
  for (size_t index = 0; index < bare.size(); ++index)
    EXPECT_NEAR(moved[index] - bare[index], shift, 0.01f);
}

// ── Keeps at the frame boundary ───────────────────────────────────────────

namespace {

/// Where the second block of twoBlocks() begins, in UTF-16 units.
constexpr uint32_t kSecondBlockText =
    sizeof("The first block runs on for several words so that it wraps.") - 1;

/// How many distinct lines the words at or past `textBegin` landed on.
int linesFrom(const ParagraphLayout& layout, const Paragraph& paragraph,
              uint32_t textBegin) {
  std::vector<int> lines;
  for (const PositionedRun& run : layout.runs) {
    if (paragraph.words()[run.wordIndex].textBegin < textBegin) continue;
    if (std::find(lines.begin(), lines.end(), run.lineIndex) == lines.end())
      lines.push_back(run.lineIndex);
  }
  return static_cast<int>(lines.size());
}

/// The first word of the block starting at `textBegin`.
uint32_t firstWordAt(const Paragraph& paragraph, uint32_t textBegin) {
  for (uint32_t index = 0; index < paragraph.words().size(); ++index)
    if (paragraph.words()[index].textBegin >= textBegin) return index;
  return 0;
}

/// A frame `lines` deep: the geometry holds the whole text and the clamp
/// says where the frame ends.
ParagraphLayoutOptions framedTo(int lines) {
  ParagraphLayoutOptions options;
  options.overflow.maxLines = lines;
  return options;
}

}  // namespace

TEST(Keeps, AnOrphanTooShortToStandMovesItsWholeBlockOver) {
  FontContext& fonts = sharedContext();
  Paragraph measured = twoBlocks();
  BlockFlow open(SkRect::MakeWH(150, 600));
  const ParagraphLayout whole = layoutParagraph(fonts, measured, open);
  const int firstBlockLines =
      whole.lineCount - linesFrom(whole, measured, kSecondBlockText);
  ASSERT_GT(firstBlockLines, 0);

  // A frame with room for one line of the second block: without a keep it
  // takes that line, with an orphan rule of two it takes none.
  Paragraph loose = twoBlocks();
  BlockFlow flowA(SkRect::MakeWH(150, 600));
  const ParagraphLayout unkept =
      layoutParagraph(fonts, loose, flowA, framedTo(firstBlockLines + 1));
  EXPECT_EQ(linesFrom(unkept, loose, kSecondBlockText), 1);

  Paragraph kept = twoBlocks();
  BlockFlow flowB(SkRect::MakeWH(150, 600));
  ParagraphLayoutOptions options = framedTo(firstBlockLines + 1);
  ParagraphStyle second;
  second.keep.orphanLines = 2;
  options.blocks = {ParagraphStyle{}, second};
  const ParagraphLayout layout = layoutParagraph(fonts, kept, flowB, options);
  EXPECT_EQ(linesFrom(layout, kept, kSecondBlockText), 0);
  EXPECT_EQ(layout.firstUnplacedWord, firstWordAt(kept, kSecondBlockText));
}

TEST(Keeps, BothBreakersEnforceTheSameKeepBecauseNoBreakIsReDecided) {
  FontContext& fonts = sharedContext();
  Paragraph measured = twoBlocks();
  BlockFlow open(SkRect::MakeWH(150, 600));
  const int firstBlockLines = layoutParagraph(fonts, measured, open).lineCount -
                              linesFrom(layoutParagraph(fonts, measured, open),
                                        measured, kSecondBlockText);

  ParagraphStyle second;
  second.keep.orphanLines = 2;
  const auto fillWith = [&](LineBreakStrategy strategy) {
    Paragraph paragraph = twoBlocks();
    BlockFlow flow(SkRect::MakeWH(150, 600));
    ParagraphLayoutOptions options = framedTo(firstBlockLines + 1);
    options.lineBreakStrategy = strategy;
    options.blocks = {ParagraphStyle{}, second};
    return layoutParagraph(fonts, paragraph, flow, options).firstUnplacedWord;
  };
  EXPECT_EQ(fillWith(LineBreakStrategy::kGreedy),
            fillWith(LineBreakStrategy::kKnuthPlass));
}

TEST(Keeps, WithNextTakesTheBlockThatEndedAtTheBoundaryWithIt) {
  FontContext& fonts = sharedContext();
  // Three blocks: a body, a heading that asks to keep with what follows,
  // and the body under it.
  const std::u8string text =
      u8"An opening block that runs on for enough words to wrap twice.\n"
      u8"A heading.\n"
      u8"The block the heading introduces, which also wraps.";
  constexpr uint32_t kHeadingText =
      sizeof("An opening block that runs on for enough words to wrap twice.") -
      1;
  constexpr uint32_t kThirdBlockText = kHeadingText + sizeof("A heading.") - 1;

  Paragraph measured = makeParagraph(text, 16.0f);
  BlockFlow open(SkRect::MakeWH(150, 600));
  const ParagraphLayout whole = layoutParagraph(fonts, measured, open);
  const int throughHeading =
      whole.lineCount - linesFrom(whole, measured, kThirdBlockText);
  ASSERT_GT(throughHeading, 1);

  const auto fillWith = [&](bool withNext) {
    Paragraph paragraph = makeParagraph(text, 16.0f);
    BlockFlow flow(SkRect::MakeWH(150, 600));
    ParagraphLayoutOptions options = framedTo(throughHeading);
    ParagraphStyle heading;
    heading.keep.withNext = withNext;
    options.blocks = {ParagraphStyle{}, heading};
    return layoutParagraph(fonts, paragraph, flow, options);
  };
  // Without the keep the heading sits alone at the foot of the frame.
  EXPECT_GT(linesFrom(fillWith(false), measured, kHeadingText), 0);
  // With it, the heading goes over to stand above its own block.
  const ParagraphLayout kept = fillWith(true);
  EXPECT_EQ(linesFrom(kept, measured, kHeadingText), 0);
  EXPECT_EQ(kept.firstUnplacedWord, firstWordAt(measured, kHeadingText));
  EXPECT_FALSE(kept.runs.empty());
}

TEST(Keeps, StartInNextFrameEndsTheFillBeforeTheBlock) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = twoBlocks();
  BlockFlow flow(SkRect::MakeWH(150, 600));
  ParagraphLayoutOptions options;
  ParagraphStyle second;
  second.keep.startInNextFrame = true;
  options.blocks = {ParagraphStyle{}, second};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  EXPECT_EQ(linesFrom(layout, paragraph, kSecondBlockText), 0);
  EXPECT_EQ(layout.firstUnplacedWord, firstWordAt(paragraph, kSecondBlockText));
}

TEST(Keeps, AKeepNeverEmptiesAFrame) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"A single block whose lines cannot go anywhere but here.", 16.0f);
  BlockFlow flow(SkRect::MakeWH(150, 600));
  ParagraphLayoutOptions options = framedTo(1);
  ParagraphStyle only;
  only.keep.orphanLines = 4;
  only.keep.allLinesTogether = true;
  options.blocks = {only};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  // Retracting the one line it placed would hand the next frame exactly
  // what emptied this one, so the fill stands.
  EXPECT_FALSE(layout.runs.empty());
  EXPECT_TRUE(layout.overflowed());
}

TEST(Keeps, AWidowIsCountedInTheLinesTheNextFrameWouldGet) {
  FontContext& fonts = sharedContext();
  Paragraph measured = makeParagraph(
      u8"One block long enough to fill several lines of a narrow frame and "
      u8"leave a short remainder behind it.",
      16.0f);
  BlockFlow open(SkRect::MakeWH(150, 600));
  const int total = layoutParagraph(fonts, measured, open).lineCount;
  ASSERT_GT(total, 3);

  // A frame one line short of the whole block carries one line over; a
  // widow rule of two pulls a second line back out of the frame.
  const auto placedLines = [&](int widowLines) {
    Paragraph paragraph = makeParagraph(
        u8"One block long enough to fill several lines of a narrow frame and "
        u8"leave a short remainder behind it.",
        16.0f);
    BlockFlow flow(SkRect::MakeWH(150, 600));
    ParagraphLayoutOptions options = framedTo(total - 1);
    ParagraphStyle style;
    style.keep.widowLines = widowLines;
    options.blocks = {style};
    return layoutParagraph(fonts, paragraph, flow, options).lineCount;
  };
  EXPECT_EQ(placedLines(0), total - 1);
  EXPECT_EQ(placedLines(2), total - 2);
}

TEST(Keeps, AWidowIsCountedAtTheMeasureTheNextFrameSetsIn) {
  FontContext& fonts = sharedContext();
  const std::u8string body =
      u8"One block long enough to fill several lines of a narrow frame and "
      u8"leave a short remainder behind it.";
  Paragraph measured = makeParagraph(body, 16.0f);
  BlockFlow open(SkRect::MakeWH(150, 600));
  const int total = layoutParagraph(fonts, measured, open).lineCount;
  ASSERT_GT(total, 3);

  // The remainder is what the NEXT frame will set, so how many lines it
  // takes is a fact about the next frame's measure. A chain that widens
  // hands the remainder fewer lines than this frame would have given it,
  // and a widow rule counting at this frame's measure over-counts and
  // retracts a line it did not need to.
  const auto placedLines = [&](float nextMeasure) {
    Paragraph paragraph = makeParagraph(body, 16.0f);
    BlockFlow flow(SkRect::MakeWH(150, 600));
    ParagraphLayoutOptions options = framedTo(total - 2);
    options.nextMeasure = nextMeasure;
    ParagraphStyle style;
    style.keep.widowLines = 2;
    options.blocks = {style};
    return layoutParagraph(fonts, paragraph, flow, options).lineCount;
  };
  // Two lines carry over at this frame's own measure, which the rule
  // already accepts, so nothing is retracted either way — and a next frame
  // wide enough to take those two lines as one leaves a single widow, so
  // the rule pulls a line back to join it.
  EXPECT_EQ(placedLines(0), total - 2);
  EXPECT_EQ(placedLines(600.0f), total - 3);
}
