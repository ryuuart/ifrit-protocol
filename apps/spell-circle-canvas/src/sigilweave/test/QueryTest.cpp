/** @file
 * The optional Query layer: substring/regex search, word
 * ranges, edit-following MarkerSets, and batch paint application.
 */

#include "TestSupport.h"

#include <gtest/gtest.h>

#include <array>
#include <vector>
using namespace sigil::weave;
using namespace sigil::weave::test;

// ── Query layer (Query.h — optional, built on the Paragraph edit log) ────

TEST(Query, FindAllAndRegex) {
  Paragraph paragraph = makeParagraph(u8"the cat sat on the mat, the end");
  const std::vector<CharRange> occurrences =
      findAllOccurrences(paragraph, u8"the");
  ASSERT_EQ(occurrences.size(), 3u);
  EXPECT_EQ(occurrences[0], (CharRange{0, 3}));
  EXPECT_EQ(occurrences[2], (CharRange{24, 27}));

  const auto matches = findRegexMatches(paragraph, u8"[cms]at");
  ASSERT_TRUE(matches.has_value());
  ASSERT_EQ(matches->size(), 3u);
  EXPECT_EQ((*matches)[0], (CharRange{4, 7}));   // cat
  EXPECT_EQ((*matches)[2], (CharRange{19, 22})); // mat

  EXPECT_FALSE(findRegexMatches(paragraph, u8"[unclosed").has_value());
}

TEST(Query, WordRangesMatchSegmentation) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"one two three");
  const std::vector<CharRange> words = wordRanges(paragraph, fontContext);
  ASSERT_EQ(words.size(), 3u);
  EXPECT_EQ(words[0], (CharRange{0, 3}));
  EXPECT_EQ(words[1], (CharRange{4, 7}));
  EXPECT_EQ(words[2], (CharRange{8, 13}));
}

TEST(Query, MarkerSetFollowsEdits) {
  Paragraph paragraph = makeParagraph(u8"alpha beta gamma delta");
  MarkerSet marks(paragraph);
  marks.setRanges("greek",
                  findAllOccurrences(paragraph, u8"gamma")); // [11, 16)

  // Insert before the marker: it shifts.
  paragraph.replaceText(0, 0, u8">>> ");
  ASSERT_TRUE(marks.synchronize(paragraph));
  ASSERT_EQ(marks.rangesFor("greek")->size(), 1u);
  EXPECT_EQ(marks.rangesFor("greek")->front(), (CharRange{15, 20}));

  // Replace text overlapping the marker: it absorbs the replacement.
  paragraph.replaceText(17, 22,
                        u8"MMA plus"); // ">>> alpha beta gaMMA plus delta"
  ASSERT_TRUE(marks.synchronize(paragraph));
  EXPECT_EQ(marks.rangesFor("greek")->front(), (CharRange{15, 25}));

  // Delete the whole marked range: the marker collapses and is dropped.
  paragraph.replaceText(15, 25, u8"");
  ASSERT_TRUE(marks.synchronize(paragraph));
  EXPECT_TRUE(marks.rangesFor("greek")->empty());
}

TEST(Query, MarkerSetStylesAcrossEdits) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"keep the flame lit forever");
  MarkerSet marks(paragraph);
  marks.setRanges("flame", findAllOccurrences(paragraph, u8"flame"));

  paragraph.replaceText(0, 4, u8"guard"); // shifts the marker by +1
  PaintStyle red(0xFFFF0000);
  marks.applyPaint(paragraph, "flame", red);
  paragraph.ensureShaped(fontContext);

  // The span carrying red must cover exactly "flame" in the new text.
  const std::u16string &text = paragraph.text();
  const size_t flameOffset = text.find(u"flame");
  ASSERT_NE(flameOffset, std::u16string::npos);
  bool found = false;
  for (const StyleSpan &span : paragraph.spans())
    if (span.style.paint.foreground.getColor() == 0xFFFF0000) {
      EXPECT_EQ(span.start, static_cast<uint32_t>(flameOffset));
      EXPECT_EQ(span.end, static_cast<uint32_t>(flameOffset + 5));
      found = true;
    }
  EXPECT_TRUE(found);
}

TEST(Query, MarkerSetReportsHistoryLoss) {
  Paragraph paragraph = makeParagraph(u8"word");
  MarkerSet marks(paragraph);
  marks.setRanges("w", findAllOccurrences(paragraph, u8"word"));
  // Blow past the bounded history (256 ops).
  for (int editIndex = 0; editIndex < 400; ++editIndex)
    paragraph.replaceText(0, 0, u8"x");
  EXPECT_FALSE(marks.synchronize(paragraph));
  EXPECT_TRUE(marks.rangesFor("w")->empty()); // cleared, caller must re-query
}

TEST(Query, ScopedSearchesStayInsideTheWindow) {
  //                            0123456789012345678901234567890
  Paragraph paragraph = makeParagraph(u8"the cat sat on the mat, the end");

  // Substring search: offsets are absolute, matches before the window
  // drop out ("the" at 0), matches ending exactly at the edge stay.
  const std::vector<CharRange> occurrences =
      findAllOccurrences(paragraph, u8"the", {4, 27});
  ASSERT_EQ(occurrences.size(), 2u);
  EXPECT_EQ(occurrences[0], (CharRange{15, 18}));
  EXPECT_EQ(occurrences[1], (CharRange{24, 27}));
  // A match straddling the edge is not a match: "the" at 24 ends at 27,
  // one unit past end=26.
  const std::vector<CharRange> clipped =
      findAllOccurrences(paragraph, u8"the", {4, 26});
  ASSERT_EQ(clipped.size(), 1u);
  EXPECT_EQ(clipped[0], (CharRange{15, 18}));

  // Regex: same substring semantics, offsets absolute.
  const auto matches = findRegexMatches(paragraph, u8"[cms]at", {8, 22});
  ASSERT_TRUE(matches.has_value());
  ASSERT_EQ(matches->size(), 2u); // sat, mat — cat starts at 4, outside
  EXPECT_EQ((*matches)[0], (CharRange{8, 11}));
  EXPECT_EQ((*matches)[1], (CharRange{19, 22}));

  // The window's edges are text boundaries: ^ and $ anchor to them.
  const auto anchored = findRegexMatches(paragraph, u8"^sat", {8, 22});
  ASSERT_TRUE(anchored.has_value());
  ASSERT_EQ(anchored->size(), 1u);
  EXPECT_EQ((*anchored)[0], (CharRange{8, 11}));

  // Degenerate scopes clamp instead of tripping.
  EXPECT_TRUE(findAllOccurrences(paragraph, u8"the", {40, 90}).empty());
  EXPECT_TRUE(findRegexMatches(paragraph, u8"the", {27, 9000})->empty());
  const auto full = findRegexMatches(paragraph, u8"the", {0, 9000});
  ASSERT_TRUE(full.has_value());
  EXPECT_EQ(full->size(), 3u);
}

TEST(Query, BatchPaintMatchesSequentialPaint) {
  const char8_t *text = u8"one two three four five six seven eight nine ten";
  Paragraph sequential = makeParagraph(text);
  Paragraph batched = makeParagraph(text);

  const std::vector<CharRange> words =
      findAllOccurrences(sequential, u8"e"); // scattered
  ASSERT_GT(words.size(), 3u);
  PaintStyle green(0xFF00AA00);
  for (const CharRange &wordRange : words)
    sequential.setPaint(wordRange.start, wordRange.end, green);
  batched.setPaint(words, green);

  ASSERT_EQ(sequential.spans().size(), batched.spans().size());
  for (size_t spanIndex = 0; spanIndex < batched.spans().size(); ++spanIndex) {
    EXPECT_EQ(sequential.spans()[spanIndex].start,
              batched.spans()[spanIndex].start);
    EXPECT_EQ(sequential.spans()[spanIndex].end,
              batched.spans()[spanIndex].end);
    EXPECT_EQ(sequential.spans()[spanIndex].style.paint.foreground.getColor(),
              batched.spans()[spanIndex].style.paint.foreground.getColor());
  }

  // Unsorted and overlapping input is sanitized, not trusted.
  Paragraph messy = makeParagraph(text);
  const std::array<CharRange, 4> ranges = {CharRange{8, 13}, CharRange{0, 3},
                                           CharRange{10, 17}, CharRange{3, 3}};
  messy.setPaint(ranges, green);
  uint32_t painted = 0;
  for (const StyleSpan &span : messy.spans())
    if (span.style.paint.foreground.getColor() == green.foreground.getColor())
      painted += span.end - span.start;
  EXPECT_EQ(painted, 3u + 9u); // [0,3) + merged [8,17)
}

TEST(Query, PaintOnlyRestyleSkipsReanalysis) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"the quick brown fox jumps over the lazy dog again and again");
  BlockFlow flow(SkRect::MakeWH(300, 200));
  ParagraphLayout before = layoutParagraph(fontContext, paragraph, flow);
  const uint32_t shapedBefore = paragraph.shapedWordCount();

  // Repaint word-aligned ranges. (That this reshapes nothing is asserted
  // canonically by Shaper.PaintOnlyRestyleNeverReshapes; here the unique
  // claims are geometry identity and steady-state paint resolution.)
  const std::vector<CharRange> marks = findAllOccurrences(paragraph, u8"again");
  ASSERT_EQ(marks.size(), 2u);
  PaintStyle red(0xFFCC0000);
  paragraph.setPaint(marks, red);
  ParagraphLayout after = layoutParagraph(fontContext, paragraph, flow);

  // Same geometry in, same geometry out — only paint moved.
  EXPECT_EQ(paragraph.shapedWordCount(), shapedBefore);
  ASSERT_EQ(after.runs.size(), before.runs.size());
  for (size_t runIndex = 0; runIndex < after.runs.size(); ++runIndex) {
    EXPECT_EQ(after.runs[runIndex].origin.x(),
              before.runs[runIndex].origin.x());
    EXPECT_EQ(after.runs[runIndex].origin.y(),
              before.runs[runIndex].origin.y());
  }
  // And the marked words resolve to the new paint through their runs.
  int redRuns = 0;
  for (const PositionedRun &run : after.runs)
    if (paragraph.spans()[run.styleIndex].style.paint.foreground.getColor() ==
        red.foreground.getColor())
      redRuns++;
  EXPECT_EQ(redRuns, 2);

  // Steady state (hue cycling the same ranges): the span boundaries come
  // out identical, so nothing is even paint-dirty — the existing layout's
  // styleIndices already resolve to the new color, no relayout required.
  PaintStyle blue(0xFF0000CC);
  paragraph.setPaint(marks, blue);
  EXPECT_FALSE(paragraph.needsShaping());
  int blueRuns = 0;
  for (const PositionedRun &run : after.runs)
    if (paragraph.spans()[run.styleIndex].style.paint.foreground.getColor() ==
        blue.foreground.getColor())
      blueRuns++;
  EXPECT_EQ(blueRuns, 2);
}

TEST(Query, PaintBoundaryMidWordSplitsSegments) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"highlight");
  paragraph.ensureShaped(fontContext);
  ASSERT_EQ(paragraph.words().size(), 1u);
  ASSERT_EQ(paragraph.words()[0].segments.size(), 1u);
  const float whole = paragraph.words()[0].width;

  PaintStyle blue(0xFF0000CC);
  paragraph.setPaint(0, 4, blue); // "high" | "light"
  paragraph.ensureShaped(fontContext);

  const Word &word = paragraph.words()[0];
  ASSERT_EQ(word.segments.size(), 2u);
  EXPECT_EQ(paragraph.spans()[word.segments[0].styleIndex]
                .style.paint.foreground.getColor(),
            blue.foreground.getColor());
  EXPECT_NE(paragraph.spans()[word.segments[1].styleIndex]
                .style.paint.foreground.getColor(),
            blue.foreground.getColor());
  // Width comes back as the sum of the halves (kerning across the split
  // may drift a hair, nothing more).
  EXPECT_NEAR(word.width, whole, whole * 0.05f);
}
