/** @file
 * Per-glyph choreography (Choreograph.h): the identity forEachPlacedGlyph
 * hands an effect, the sentence segmentation behind it, and the
 * paint-complete batched draw.
 */

#include <gtest/gtest.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>

#include <string>
#include <vector>

#include "TestSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

/// Three sentences over two style spans, long enough to wrap: the fixture
/// every index assertion below reads against.
Paragraph mixedStyleParagraph() {
  TextStyle base = basicStyle(18.0f);
  TextStyle accent = base;
  accent.paint.foreground.setColor(SK_ColorRED);
  ParagraphBuilder builder(base);
  builder.addText(u8"Letters leave their lines. ")
      .pushStyle(accent)
      .addText(u8"Some of them come back!")
      .popStyle()
      .addText(u8" The rest keep falling.");
  return builder.build();
}

std::vector<PlacedGlyph> collect(const ParagraphLayout& layout,
                                 const Paragraph& paragraph) {
  std::vector<PlacedGlyph> placed;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    placed.push_back(glyph);
  });
  return placed;
}

/// UTF-16 offset of `needle` in the paragraph's text.
uint32_t offsetOf(const Paragraph& paragraph, std::u16string_view needle) {
  const size_t position = paragraph.text().find(needle);
  return position == std::u16string::npos ? ~0u
                                          : static_cast<uint32_t>(position);
}

}  // namespace

// ── The identity every effect selects and staggers on ────────────────────

TEST(PlacedGlyph, IndicesAgreeWithTheParagraphAndTheLayout) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = mixedStyleParagraph();
  BlockFlow flow(SkRect::MakeWH(240, 400));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  ASSERT_GT(layout.lineCount, 1) << "the fixture must wrap";

  const std::vector<PlacedGlyph> placed = collect(layout, paragraph);
  ASSERT_FALSE(placed.empty());

  const std::vector<StyleSpan>& spans = paragraph.spans();
  const std::vector<Word>& words = paragraph.words();
  uint32_t expectedOrdinal = 0;
  int previousLine = -1;
  for (const PlacedGlyph& glyph : placed) {
    EXPECT_EQ(glyph.ordinal, expectedOrdinal++);
    ASSERT_NE(glyph.shaped, nullptr);
    ASSERT_NE(glyph.paint, nullptr);
    EXPECT_LT(glyph.glyphIndex, glyph.shaped->glyphs.size());
    EXPECT_EQ(glyph.glyph, glyph.shaped->glyphs[glyph.glyphIndex]);
    EXPECT_EQ(glyph.advance, glyph.shaped->advances[glyph.glyphIndex]);

    // The word that produced the glyph contains the text position it maps
    // back to, and the span that styles it covers the same position.
    ASSERT_LT(glyph.wordIndex, words.size());
    EXPECT_GE(glyph.textIndex, words[glyph.wordIndex].textBegin);
    EXPECT_LT(glyph.textIndex, words[glyph.wordIndex].textEnd);
    ASSERT_LT(glyph.styleIndex, spans.size());
    EXPECT_GE(glyph.textIndex, spans[glyph.styleIndex].start);
    EXPECT_LT(glyph.textIndex, spans[glyph.styleIndex].end);
    EXPECT_EQ(glyph.color,
              spans[glyph.styleIndex].style.paint.foreground.getColor());
    EXPECT_EQ(glyph.paint, &spans[glyph.styleIndex].style.paint);

    // Lines are visited in flow order and never revisited.
    EXPECT_GE(glyph.lineIndex, previousLine);
    previousLine = glyph.lineIndex;
    EXPECT_LT(glyph.lineIndex, layout.lineCount);
  }

  // The accent span is the one that is red, and exactly the glyphs inside
  // its range report it.
  const uint32_t accentStart = offsetOf(paragraph, u"Some");
  ASSERT_NE(accentStart, ~0u);
  int redGlyphs = 0;
  for (const PlacedGlyph& glyph : placed)
    if (glyph.color == SK_ColorRED) {
      ++redGlyphs;
      EXPECT_GE(glyph.textIndex, accentStart);
    }
  EXPECT_GT(redGlyphs, 0);
}

TEST(PlacedGlyph, LineIndexMatchesTheBaselineTheGlyphSitsOn) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = mixedStyleParagraph();
  BlockFlow flow(SkRect::MakeWH(240, 400));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  // Every glyph of one line shares that line's baseline, and later lines sit
  // further down the page.
  float lineBaseline = 0;
  int currentLine = -1;
  float previousBaseline = 0;
  for (const PlacedGlyph& glyph : collect(layout, paragraph)) {
    if (glyph.lineIndex != currentLine) {
      if (currentLine >= 0) EXPECT_GT(glyph.rest.y(), previousBaseline);
      previousBaseline = lineBaseline = glyph.rest.y();
      currentLine = glyph.lineIndex;
    }
    EXPECT_FLOAT_EQ(glyph.rest.y(), lineBaseline);
  }
}

TEST(PlacedGlyph, EnumerationOrderSurvivesRelayout) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = mixedStyleParagraph();
  BlockFlow flow(SkRect::MakeWH(240, 400));
  const std::vector<PlacedGlyph> first =
      collect(layoutParagraph(fontContext, paragraph, flow), paragraph);
  const std::vector<PlacedGlyph> second =
      collect(layoutParagraph(fontContext, paragraph, flow), paragraph);

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

TEST(PlacedGlyph, PaintIsResolvedPerSpanAtWalkTime) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"recolor me", 18.0f);
  BlockFlow flow(SkRect::MakeWH(400, 100));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  fontContext.resetStats();
  PaintStyle blue(SK_ColorBLUE);
  blue.addUnderlay(PaintLayer::outline(SK_ColorBLACK, 2.0f));
  paragraph.setPaint(0, 7, blue);

  // The same layout object, walked again: no reshape, new paint.
  int blueGlyphs = 0;
  for (const PlacedGlyph& glyph : collect(layout, paragraph))
    if (glyph.color == SK_ColorBLUE) {
      ++blueGlyphs;
      EXPECT_EQ(glyph.paint->underlays.size(), 1u);
    }
  EXPECT_GT(blueGlyphs, 0);
  EXPECT_EQ(fontContext.stats().shapeCalls, 0u);
}

TEST(PlacedGlyph, ClustersStayInsideTheirWordAcrossACombiningMark) {
  FontContext& fontContext = sharedContext();
  // Decomposed: "cafe" plus COMBINING ACUTE ACCENT — five code units
  // that shape to four or five glyphs, depending on whether the face
  // composes them.
  Paragraph paragraph = makeParagraph(u8"cafe\u0301 noir", 24.0f);
  BlockFlow flow(SkRect::MakeWH(400, 100));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

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

TEST(SentenceSegmentation, StartsFollowTheTextAndSurvivePaintEdits) {
  Paragraph paragraph = mixedStyleParagraph();
  const std::span<const uint32_t> starts = paragraph.sentenceStarts();
  ASSERT_EQ(starts.size(), 3u);
  EXPECT_EQ(starts[0], 0u);
  EXPECT_EQ(starts[1], offsetOf(paragraph, u"Some"));
  EXPECT_EQ(starts[2], offsetOf(paragraph, u"The rest"));

  // Paint is not text: recoloring must not move a sentence boundary.
  paragraph.setPaint(0, 5, PaintStyle{SK_ColorGREEN});
  const std::span<const uint32_t> afterPaint = paragraph.sentenceStarts();
  ASSERT_EQ(afterPaint.size(), 3u);
  EXPECT_EQ(afterPaint[1], starts[1]);

  // An edit does move them.
  paragraph.replaceText(0, 0, u8"Wait. ");
  const std::span<const uint32_t> afterEdit = paragraph.sentenceStarts();
  ASSERT_EQ(afterEdit.size(), 4u);
  EXPECT_EQ(afterEdit[1], 6u);
}

TEST(SentenceSegmentation, EmptyTextHasNoSentences) {
  Paragraph paragraph;
  EXPECT_TRUE(paragraph.sentenceStarts().empty());
}

TEST(PlacedGlyph, SentenceIndexNamesTheSentenceTheGlyphIsIn) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = mixedStyleParagraph();
  BlockFlow flow(SkRect::MakeWH(240, 400));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  const std::span<const uint32_t> starts = paragraph.sentenceStarts();
  ASSERT_EQ(starts.size(), 3u);
  int perSentence[3] = {0, 0, 0};
  uint32_t previousSentence = 0;
  for (const PlacedGlyph& glyph : collect(layout, paragraph)) {
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

namespace {

/// A style whose foreground is a red-to-green gradient running from `left`
/// to `right` in canvas space, under a thick blue outline pass.
PaintStyle outlinedGradient(float left, float right) {
  PaintStyle style;
  const SkPoint gradientPoints[2] = {{left, 0}, {right, 0}};
  const SkColor4f gradientColors[2] = {SkColor4f::FromColor(SK_ColorRED),
                                       SkColor4f::FromColor(SK_ColorGREEN)};
  style.foreground.setAntiAlias(true);
  style.foreground.setShader(SkShaders::LinearGradient(
      gradientPoints,
      SkGradient(SkGradient::Colors({gradientColors, 2}, SkTileMode::kClamp),
                 SkGradient::Interpolation())));
  style.addUnderlay(PaintLayer::outline(SK_ColorBLUE, 6.0f));
  return style;
}

/// Accumulates every glyph of `layout` at its rest pose.
GlyphRSXformBatches batchAtRest(const ParagraphLayout& layout,
                                const Paragraph& paragraph,
                                float alphaScale = 1.0f) {
  GlyphRSXformBatches batches;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    batches.addGlyph(glyph, glyph.rest + SkVector{glyph.advance * 0.5f, 0},
                     1.0f, 0.0f, alphaScale);
  });
  return batches;
}

}  // namespace

TEST(GlyphBatches, EveryPaintPassOfTheSpanDraws) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"HALO", 64.0f);
  BlockFlow flow(SkRect::MakeXYWH(10, 10, 380, 100));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  const std::vector<LineMetrics> lines = layout.lineMetrics(paragraph);
  ASSERT_EQ(lines.size(), 1u);
  // The ramp spans exactly the placed text, so both ends of it are on the
  // glyphs and a flat fill could not pass for the shader.
  paragraph.setPaint(0, 4, outlinedGradient(lines[0].left, lines[0].right));

  GlyphRSXformBatches batches = batchAtRest(layout, paragraph);
  // One font, two passes: the outline underlay and the gradient foreground.
  ASSERT_EQ(batches.batches.size(), 2u);
  EXPECT_EQ(batches.batches[0].paint.getStyle(), SkPaint::kStroke_Style)
      << "underlays draw before the foreground";
  EXPECT_NE(batches.batches[1].paint.getShader(), nullptr);
  EXPECT_EQ(batches.batches[0].glyphs.size(), batches.batches[1].glyphs.size());

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 120));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorWHITE);
  const int drawn = batches.draw(canvas);
  EXPECT_EQ(drawn, static_cast<int>(batches.batches[0].glyphs.size() * 2));

  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  bool sawOutline = false, sawGradientStart = false, sawGradientEnd = false;
  for (int y = 0; y < pixmap.height(); ++y)
    for (int x = 0; x < pixmap.width(); ++x) {
      const SkColor pixel = pixmap.getColor(x, y);
      const U8CPU red = SkColorGetR(pixel), green = SkColorGetG(pixel),
                  blue = SkColorGetB(pixel);
      if (blue > 200 && red < 60 && green < 60) sawOutline = true;
      if (red > 150 && green < 90 && blue < 60) sawGradientStart = true;
      if (green > 120 && red < 90 && blue < 60) sawGradientEnd = true;
    }
  EXPECT_TRUE(sawOutline) << "the underlay pass must reach the canvas";
  EXPECT_TRUE(sawGradientStart) << "the shader foreground must reach it too";
  EXPECT_TRUE(sawGradientEnd) << "and it must be the shader, not a flat fill";
}

TEST(GlyphBatches, BucketsSplitOnPassAndFontButNotOnGlyph) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"many many letters here", 24.0f);
  BlockFlow flow(SkRect::MakeWH(400, 200));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  // A plain single-pass style is one bucket, however many glyphs it draws.
  GlyphRSXformBatches flat = batchAtRest(layout, paragraph);
  ASSERT_EQ(flat.batches.size(), 1u);
  EXPECT_GT(flat.batches[0].glyphs.size(), 10u);

  // Adding an offset pass adds exactly one bucket, and the offset is the
  // bucket's, not the glyph's — the transforms stay identical.
  PaintStyle shadowed(SK_ColorBLACK);
  shadowed.addUnderlay(PaintLayer(SK_ColorRED, {4, 4}));
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()),
                     shadowed);
  GlyphRSXformBatches layered = batchAtRest(layout, paragraph);
  ASSERT_EQ(layered.batches.size(), 2u);
  EXPECT_EQ(layered.batches[0].offset, (SkVector{4, 4}));
  EXPECT_EQ(layered.batches[1].offset, (SkVector{0, 0}));
  ASSERT_EQ(layered.batches[0].transforms.size(),
            layered.batches[1].transforms.size());
  for (size_t index = 0; index < layered.batches[0].transforms.size(); ++index)
    EXPECT_EQ(layered.batches[0].transforms[index].fTx,
              layered.batches[1].transforms[index].fTx);
}

TEST(GlyphBatches, AlphaScaleFadesEveryPassAndDropsInvisibleOnes) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"fade", 32.0f);
  BlockFlow flow(SkRect::MakeWH(400, 100));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  PaintStyle style(SK_ColorBLACK);
  style.addUnderlay(PaintLayer(0x80FF0000, {2, 2}));
  paragraph.setPaint(0, 4, style);

  GlyphRSXformBatches half = batchAtRest(layout, paragraph, 0.5f);
  ASSERT_EQ(half.batches.size(), 2u);
  EXPECT_NEAR(half.batches[0].paint.getAlphaf(), 0.5f * 128.0f / 255.0f, 0.01f);
  EXPECT_NEAR(half.batches[1].paint.getAlphaf(), 0.5f, 0.01f);

  // A fully faded glyph mints no bucket at all.
  GlyphRSXformBatches gone = batchAtRest(layout, paragraph, 0.0f);
  EXPECT_TRUE(gone.batches.empty());
}

TEST(GlyphBatches, ClearKeepsBucketsButReleasesGlyphs) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"reuse", 20.0f);
  BlockFlow flow(SkRect::MakeWH(400, 100));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  GlyphRSXformBatches batches = batchAtRest(layout, paragraph);
  ASSERT_EQ(batches.batches.size(), 1u);
  batches.clear();
  ASSERT_EQ(batches.batches.size(), 1u) << "allocations are kept for reuse";
  EXPECT_TRUE(batches.batches[0].glyphs.empty());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 32));
  EXPECT_EQ(batches.draw(surface->getCanvas()), 0)
      << "an emptied batch issues no draw";
}
