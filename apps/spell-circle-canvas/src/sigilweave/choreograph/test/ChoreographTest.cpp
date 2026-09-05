/** @file
 * The walk over a finished layout: the identity forEachPlacedGlyph hands
 * an effect for every glyph it visits, and the interval and pen a glyph on
 * a contour reports so a caller can re-place it.
 */

#include <gtest/gtest.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "support/ChoreographSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

std::vector<PlacedGlyph> collect(const ParagraphLayout& layout,
                                 const Paragraph& paragraph) {
  std::vector<PlacedGlyph> placed;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    placed.push_back(glyph);
  });
  return placed;
}

}  // namespace

// ── The identity every effect selects and staggers on ────────────────────

/// Three sentences over two style spans, wrapped in a 240×400 block: the
/// setting every claim about a placed glyph's identity is read in, because
/// it carries more than one line, more than one span and more than one
/// sentence at once.
class Choreography : public ::testing::Test {
 protected:
  void SetUp() override {
    m_layout = compose();
    ASSERT_GT(m_layout.lineCount, 1) << "the fixture must wrap";
  }

  /// A fresh placement of the same paragraph in the same block.
  ParagraphLayout compose() {
    return layoutParagraph(sigil::test::fonts(), m_paragraph, m_flow);
  }

  /// Every glyph of the standing layout, in the order the walk hands them
  /// out.
  std::vector<PlacedGlyph> walk() { return collect(m_layout, m_paragraph); }

  Paragraph m_paragraph = mixedStyleParagraph();
  BlockFlow m_flow{SkRect::MakeWH(240, 400)};
  ParagraphLayout m_layout;
};

TEST_F(Choreography, TheOrdinalCountsEveryGlyphTheWalkHandsOut) {
  const std::vector<PlacedGlyph> placed = walk();
  ASSERT_FALSE(placed.empty());

  uint32_t expectedOrdinal = 0;
  for (const PlacedGlyph& glyph : placed) {
    EXPECT_EQ(glyph.ordinal, expectedOrdinal++);
    ASSERT_NE(glyph.shaped, nullptr);
    EXPECT_LT(glyph.glyphIndex, glyph.shaped->glyphs.size());
    EXPECT_EQ(glyph.glyph, glyph.shaped->glyphs[glyph.glyphIndex]);
    EXPECT_EQ(glyph.advance, glyph.shaped->advances[glyph.glyphIndex]);
  }
}

TEST_F(Choreography, EveryGlyphNamesTheWordAndTheSpanItsTextIndexFallsIn) {
  const std::vector<StyleSpan>& spans = m_paragraph.spans();
  const std::vector<Word>& words = m_paragraph.words();
  for (const PlacedGlyph& glyph : walk()) {
    ASSERT_NE(glyph.paint, nullptr);
    // The word that produced the glyph contains the text position it maps
    // back to, and the span that styles it covers the same position.
    ASSERT_LT(glyph.wordIndex, words.size());
    EXPECT_GE(glyph.textIndex, words[glyph.wordIndex].textBegin);
    EXPECT_LT(glyph.textIndex, words[glyph.wordIndex].textEnd);
    ASSERT_LT(glyph.styleIndex, spans.size());
    EXPECT_GE(glyph.textIndex, spans[glyph.styleIndex].start);
    EXPECT_LT(glyph.textIndex, spans[glyph.styleIndex].end);
    EXPECT_EQ(glyph.paint, &spans[glyph.styleIndex].style.paint);
  }
}

TEST_F(Choreography, TheWalkVisitsLinesInFlowOrderAndNeverReturnsToOne) {
  int previousLine = -1;
  for (const PlacedGlyph& glyph : walk()) {
    EXPECT_GE(glyph.lineIndex, previousLine);
    previousLine = glyph.lineIndex;
    EXPECT_LT(glyph.lineIndex, m_layout.lineCount);
  }
}

TEST_F(Choreography, EveryGlyphOfALineSitsOnThatLinesBaseline) {
  // Every glyph of one line shares that line's baseline, and later lines sit
  // further down the page.
  float lineBaseline = 0;
  int currentLine = -1;
  float previousBaseline = 0;
  for (const PlacedGlyph& glyph : walk()) {
    if (glyph.lineIndex != currentLine) {
      if (currentLine >= 0) EXPECT_GT(glyph.rest.y(), previousBaseline);
      previousBaseline = lineBaseline = glyph.rest.y();
      currentLine = glyph.lineIndex;
    }
    EXPECT_FLOAT_EQ(glyph.rest.y(), lineBaseline);
  }
}

TEST_F(Choreography, EnumerationOrderSurvivesRelayout) {
  const std::vector<PlacedGlyph> first = collect(compose(), m_paragraph);
  const std::vector<PlacedGlyph> second = collect(compose(), m_paragraph);

  // Per-glyph particle state is keyed by position in this walk, so an
  // unedited paragraph must enumerate identically every frame.
  ASSERT_EQ(first.size(), second.size());
  for (size_t index = 0; index < first.size(); ++index) {
    EXPECT_EQ(first[index].glyph, second[index].glyph);
    EXPECT_EQ(first[index].textIndex, second[index].textIndex);
    EXPECT_EQ(first[index].wordIndex, second[index].wordIndex);
    EXPECT_EQ(first[index].sentenceIndex, second[index].sentenceIndex);
    EXPECT_EQ(first[index].rest, second[index].rest);
  }
}

TEST_F(Choreography, EveryGlyphReportsTheColourOfTheSpanThatCoversIt) {
  // The accent span is the one that is red, and exactly the glyphs inside
  // its range report it.
  const uint32_t accentStart = offsetOf(m_paragraph, u"Some");
  ASSERT_NE(accentStart, ~0u);
  int redGlyphs = 0;
  for (const PlacedGlyph& glyph : walk())
    if (glyph.color == SK_ColorRED) {
      ++redGlyphs;
      EXPECT_GE(glyph.textIndex, accentStart);
    }
  EXPECT_GT(redGlyphs, 0);

  // A paint declared after the placement is resolved on the next walk of
  // the SAME layout: new colour, new passes, nothing re-placed.
  PaintStyle blue(SK_ColorBLUE);
  blue.addUnderlay(PaintLayer::outline(SK_ColorBLACK, 2.0f));
  m_paragraph.setPaint(0, 7, blue);
  int blueGlyphs = 0;
  for (const PlacedGlyph& glyph : walk())
    if (glyph.color == SK_ColorBLUE) {
      ++blueGlyphs;
      EXPECT_EQ(glyph.paint->underlays.size(), 1u);
    }
  EXPECT_GT(blueGlyphs, 0);
}

TEST(PlacedGlyph, ClustersStayInsideTheirWordAcrossACombiningMark) {
  // Decomposed: "cafe" plus COMBINING ACUTE ACCENT — five code units
  // that shape to four or five glyphs, depending on whether the face
  // composes them.
  BlockFlow flow(SkRect::MakeWH(400, 100));
  auto [paragraph, layout] = laidOut(u8"cafe\u0301 noir", 24.0f, flow);

  const std::vector<Word>& words = paragraph.words();
  ASSERT_GE(words.size(), 1u);
  const uint32_t accentedEnd = words.front().textEnd;
  const uint32_t accentedLength = accentedEnd - words.front().textBegin;
  ASSERT_EQ(accentedLength, 5u);

  uint32_t previousCluster = 0;
  uint32_t previousTextIndex = 0;
  bool first = true;
  int accentedGlyphs = 0;
  for (const PlacedGlyph& glyph : collect(layout, paragraph)) {
    if (glyph.wordIndex != 0) continue;
    ++accentedGlyphs;
    EXPECT_LT(glyph.cluster, accentedLength);
    EXPECT_LT(glyph.textIndex, accentedEnd);
    if (first) {
      EXPECT_EQ(glyph.cluster, 0u) << "the first glyph starts the word";
      EXPECT_EQ(glyph.textIndex, words.front().textBegin);
    } else {
      // A base and its mark share one cluster; clusters never run backwards
      // in a left-to-right run.
      EXPECT_GE(glyph.cluster, previousCluster);
      EXPECT_GE(glyph.textIndex, previousTextIndex);
    }
    previousCluster = glyph.cluster;
    previousTextIndex = glyph.textIndex;
    first = false;
  }
  // Five code units ("cafe" + the mark) shape to at most five glyphs, and
  // the accent never lands past the word.
  EXPECT_GE(accentedGlyphs, 4);
  EXPECT_LE(accentedGlyphs, 5);
}

// ── Sentences ────────────────────────────────────────────────────────────

TEST_F(Choreography, SentenceIndexNamesTheSentenceTheGlyphIsIn) {
  const std::span<const uint32_t> starts = m_paragraph.sentenceStarts();
  ASSERT_EQ(starts.size(), 3u);
  int perSentence[3] = {0, 0, 0};
  uint32_t previousSentence = 0;
  for (const PlacedGlyph& glyph : walk()) {
    ASSERT_LT(glyph.sentenceIndex, starts.size());
    EXPECT_GE(glyph.textIndex, starts[glyph.sentenceIndex]);
    if (glyph.sentenceIndex + 1 < starts.size())
      EXPECT_LT(glyph.textIndex, starts[glyph.sentenceIndex + 1]);
    // Logical order: an effect staggering by sentence sees them in order.
    EXPECT_GE(glyph.sentenceIndex, previousSentence);
    previousSentence = glyph.sentenceIndex;
    ++perSentence[glyph.sentenceIndex];
  }
  EXPECT_GT(perSentence[0], 0);
  EXPECT_GT(perSentence[1], 0);
  EXPECT_GT(perSentence[2], 0);
}

// ── The paint-complete batched draw ──────────────────────────────────────

// ---------------------------------------------------------------------------
// TEXT ON A PATH — one placement function, read by the layout and by the
// caller that re-places its glyphs

namespace {
/// One line whose whole measure is one contour.
LineSetFlow ringFlow(const sigil::geometry::path::Contour& contour,
                     float length, float start = 0, float advanceScale = 1.0f) {
  LineInterval interval;
  interval.contour = contour;
  interval.length = length;
  interval.contourStart = start;
  interval.advanceScale = advanceScale;
  LineSetFlow flow;
  flow.lines().push_back({interval});
  return flow;
}
}  // namespace

TEST(PlacedGlyphOnAContour, ItReportsTheIntervalAndPenItWasPlacedAt) {
  // The pair a caller needs to re-place a curved run at draw time. The pen
  // is the glyph's ADVANCE CENTRE, in advance units, which is what
  // placeAt() anchors — feed one straight back to the other and the answer
  // must be the position the layout itself computed.
  auto [contour, length] = circleContour(200.0f);
  ASSERT_TRUE(contour.valid());
  Paragraph paragraph = ParagraphBuilder(basicStyle(20.0f))
                            .addText(u8"round and round we go")
                            .build();
  FontContext& context = sigil::test::fonts();
  LineSetFlow flow = ringFlow(contour, length);
  ParagraphLayout layout = layoutParagraph(context, paragraph, flow);

  ASSERT_EQ(layout.intervals.size(), 1u);
  int seen = 0;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    ASSERT_TRUE(glyph.transformed);
    ASSERT_EQ(glyph.intervalIndex, 0);
    ++seen;
    SkPoint centre;
    SkVector tangent;
    layout.intervals[0].placeAt(glyph.pen, 0.0f, layout.tangentRotationSteps,
                                &centre, &tangent);
    // The rest position is the glyph's ORIGIN; walking back from it by the
    // same half-advance the placement walked forward lands on the centre.
    const SkPoint fromRest{glyph.rest.x() + tangent.x() * glyph.advance * 0.5f,
                           glyph.rest.y() + tangent.y() * glyph.advance * 0.5f};
    EXPECT_NEAR(fromRest.x(), centre.x(), 0.75f);
    EXPECT_NEAR(fromRest.y(), centre.y(), 0.75f);
    EXPECT_NEAR(glyph.tangent.x(), tangent.x(), 1e-4f);
    EXPECT_NEAR(glyph.tangent.y(), tangent.y(), 1e-4f);
    // …and every one of them sits on the ring.
    EXPECT_NEAR(std::hypot(centre.x(), centre.y()), 200.0f, 1.0f);
  });
  EXPECT_GT(seen, 10);
}

TEST(PlacedGlyphOnAContour, ThePenIsTheAccumulatedAdvanceNotTheShapedPosition) {
  // The advance-centre contract, stated as a number. HarfBuzz's per-glyph
  // offsets sit ON TOP of the pen position, and the arc coordinate must be
  // taken from the pen — an accented glyph anchored by its shaped x drifts
  // off the curve by exactly its own offset.
  auto [contour, length] = circleContour(300.0f);
  ASSERT_TRUE(contour.valid());
  Paragraph paragraph =
      ParagraphBuilder(basicStyle(24.0f)).addText(u8"clockwise").build();
  FontContext& context = sigil::test::fonts();
  LineSetFlow flow = ringFlow(contour, length);
  ParagraphLayout layout = layoutParagraph(context, paragraph, flow);

  // Within a run the pen is the running sum of ADVANCES and nothing else:
  // each glyph's centre sits half its own advance past the previous
  // glyph's, whatever offsets the shaper applied on top.
  float penStart = 0;
  int checked = 0;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    if (glyph.glyphIndex == 0) {
      penStart = glyph.pen - glyph.advance * 0.5f;  // the run's entry point
    } else {
      EXPECT_NEAR(glyph.pen, penStart + glyph.advance * 0.5f, 1e-3f);
      ++checked;
    }
    penStart += glyph.advance;
  });
  EXPECT_GT(checked, 4) << "no glyphs were placed";
}
