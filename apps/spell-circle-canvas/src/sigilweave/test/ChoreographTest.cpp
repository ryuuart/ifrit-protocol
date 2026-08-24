/** @file
 * Per-glyph choreography (Choreograph.h): the identity forEachPlacedGlyph
 * hands an effect, the sentence segmentation behind it, and the
 * paint-complete batched draw.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkPathBuilder.h>
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

TEST(GlyphBatches, UnderlaysDrawBeneathForegroundsAcrossFadeClasses) {
  // Distinct per-glyph fades mint distinct buckets. The draw must still put
  // EVERY underlay beneath EVERY foreground: a blurred halo reaches past its
  // own glyph, and a cascade mid-flight (each letter at its own fade) must
  // not lay a later letter's halo over an earlier letter's stroke.
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"OO", 64.0f);
  BlockFlow flow(SkRect::MakeWH(300, 120));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  SkPaint stroke;
  stroke.setAntiAlias(true);
  stroke.setStyle(SkPaint::kStroke_Style);
  stroke.setStrokeWidth(4.0f);
  stroke.setColor(0xFFDED8CC);
  SkPaint halo;
  halo.setAntiAlias(true);
  halo.setStyle(SkPaint::kStroke_Style);
  halo.setStrokeWidth(8.0f);
  halo.setColor(0xFF000000);
  const PaintLayer blurredHalo = PaintLayer::blurred(halo, 5.0f);

  PaintStyle hollow;
  hollow.foreground = stroke;
  hollow.underlays.push_back(blurredHalo);
  paragraph.setPaint(0, 2, hollow);

  // One fade class per glyph — the second differs just enough to be its own
  // bucket pair while staying visually opaque.
  const auto batchFaded = [&](const PaintStyle* override) {
    GlyphRSXformBatches batches;
    uint32_t index = 0;
    forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
      const float alpha = index++ == 0 ? 0.995f : 1.0f;
      batches.addGlyph(glyph.shaped, override ? *override : *glyph.paint,
                       glyph.glyph, glyph.advance * 0.5f,
                       glyph.rest + SkVector{glyph.advance * 0.5f, 0}, 1.0f,
                       0.0f, alpha);
    });
    return batches;
  };

  const SkImageInfo info = SkImageInfo::MakeN32Premul(300, 120);
  sk_sp<SkSurface> actualSurface = SkSurfaces::Raster(info);
  actualSurface->getCanvas()->clear(SK_ColorWHITE);
  batchFaded(nullptr).draw(actualSurface->getCanvas());

  // Ground truth: the same glyphs as two single-pass styles, every halo
  // drawn before any stroke.
  PaintStyle haloOnly;
  haloOnly.foreground = blurredHalo.paint;
  PaintStyle strokeOnly;
  strokeOnly.foreground = stroke;
  sk_sp<SkSurface> expectedSurface = SkSurfaces::Raster(info);
  expectedSurface->getCanvas()->clear(SK_ColorWHITE);
  batchFaded(&haloOnly).draw(expectedSurface->getCanvas());
  batchFaded(&strokeOnly).draw(expectedSurface->getCanvas());

  SkPixmap actual, expected;
  ASSERT_TRUE(actualSurface->peekPixels(&actual));
  ASSERT_TRUE(expectedSurface->peekPixels(&expected));
  int worst = 0, worstX = -1, worstY = -1;
  for (int y = 0; y < info.height(); ++y)
    for (int x = 0; x < info.width(); ++x) {
      const SkColor a = actual.getColor(x, y);
      const SkColor b = expected.getColor(x, y);
      const int diff =
          std::max({std::abs((int)SkColorGetR(a) - (int)SkColorGetR(b)),
                    std::abs((int)SkColorGetG(a) - (int)SkColorGetG(b)),
                    std::abs((int)SkColorGetB(a) - (int)SkColorGetB(b))});
      if (diff > worst) {
        worst = diff;
        worstX = x;
        worstY = y;
      }
    }
  // The two fade classes differ by 1/255 at most, so anything past a couple
  // of counts is a compositing-order divergence, not the fade.
  EXPECT_LE(worst, 4) << "batched draw diverges from underlays-then-"
                         "foregrounds at ("
                      << worstX << ", " << worstY << ")";
}

TEST(GlyphBatches, TintMultipliesAFlatPassAndModulatesAShaderOne) {
  // The colour multiplier has to reach EVERY pass, and the two kinds of
  // pass take it differently: a flat pass multiplies the colour it already
  // carries, a shader pass cannot (its colour is decided downstream) and
  // takes an equivalent modulating filter instead. Either way the glyph
  // keeps the pass — a tinted letter is not a re-styled letter.
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"HALO", 64.0f);
  BlockFlow flow(SkRect::MakeXYWH(10, 10, 380, 100));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  const std::vector<LineMetrics> lines = layout.lineMetrics(paragraph);
  ASSERT_EQ(lines.size(), 1u);
  paragraph.setPaint(0, 4, outlinedGradient(lines[0].left, lines[0].right));

  // Green only: the blue outline underlay must go black, and the red end of
  // the gradient foreground must go black too, while its green end holds.
  GlyphDress dress;
  dress.colorMul = {0, 1, 0, 1};
  GlyphRSXformBatches batches;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    GlyphDress placed = dress;
    placed.center = glyph.rest + SkVector{glyph.advance * 0.5f, 0};
    batches.addGlyph(glyph, placed);
  });
  ASSERT_EQ(batches.batches.size(), 2u) << "the tint dropped a pass";
  EXPECT_EQ(batches.batches[0].paint.getStyle(), SkPaint::kStroke_Style);
  EXPECT_EQ(batches.batches[0].paint.getColor4f().fB, 0.0f)
      << "a flat pass takes the tint in its own colour";
  EXPECT_NE(batches.batches[1].paint.getColorFilter(), nullptr)
      << "a shader pass takes the tint as a modulating filter";

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 120));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorBLACK);
  batches.draw(canvas);
  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  bool sawGreen = false;
  for (int y = 0; y < pixmap.height(); ++y)
    for (int x = 0; x < pixmap.width(); ++x) {
      const SkColor pixel = pixmap.getColor(x, y);
      EXPECT_LT(SkColorGetB(pixel), 40u)
          << "blue survived a tint that multiplies it by zero, at " << x << ","
          << y;
      EXPECT_LT(SkColorGetR(pixel), 40u) << "red survived it too";
      if (SkColorGetG(pixel) > 120) sawGreen = true;
    }
  EXPECT_TRUE(sawGreen) << "the tint painted nothing at all";
}

TEST(GlyphBatches, OneTintIsOneBucketHoweverManyGlyphsWearIt) {
  // A bucket's key is a whole SkPaint and SkPaint compares its colour
  // filter by POINTER, so the modulating filter has to be memoized: a fresh
  // one per glyph would mint a bucket per glyph and undo the batching that
  // is the entire point of this file.
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"many many letters here", 24.0f);
  BlockFlow flow(SkRect::MakeWH(400, 200));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  PaintStyle shaded;
  const SkPoint ends[2] = {{0, 0}, {400, 0}};
  const SkColor4f ramp[2] = {SkColor4f::FromColor(SK_ColorRED),
                             SkColor4f::FromColor(SK_ColorGREEN)};
  shaded.foreground.setShader(SkShaders::LinearGradient(
      ends, SkGradient(SkGradient::Colors({ramp, 2}, SkTileMode::kClamp),
                       SkGradient::Interpolation())));
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()), shaded);

  GlyphRSXformBatches batches;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    GlyphDress dress;
    dress.center = glyph.rest + SkVector{glyph.advance * 0.5f, 0};
    dress.colorMul = {0.5f, 0.75f, 1.0f, 1.0f};
    batches.addGlyph(glyph, dress);
  });
  EXPECT_EQ(batches.batches.size(), 1u)
      << "one tint over one style must stay one bucket";
  EXPECT_GT(batches.batches[0].glyphs.size(), 10u);
}

TEST(GlyphBatches, ADrivenFaceIsItsOwnBucket) {
  // A glyph drawn through a varied clone cannot share a bucket with one
  // drawn through the base face: the face is what the draw call carries.
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"AB", 32.0f);
  BlockFlow flow(SkRect::MakeWH(400, 100));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  const SkTypeface* shapedFace = nullptr;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    if (!shapedFace && glyph.shaped) shapedFace = glyph.shaped->typeface.get();
  });
  ASSERT_NE(shapedFace, nullptr);
  // A second face stands in for a varied clone — the bucket key cares that
  // the typeface differs, not how it was made.
  sk_sp<SkTypeface> other;
  for (const char* family :
       {"Courier New", "Times New Roman", "Georgia", "Menlo", "Monaco"}) {
    other = fontContext.fontManager()->matchFamilyStyle(family, SkFontStyle());
    if (other && other.get() != shapedFace) break;
    other = nullptr;
  }
  if (!other) GTEST_SKIP() << "no second face on this system";
  bool first = true;
  GlyphRSXformBatches batches;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    GlyphDress dress;
    dress.center = glyph.rest + SkVector{glyph.advance * 0.5f, 0};
    if (!first) dress.face = other;
    first = false;
    batches.addGlyph(glyph, dress);
  });
  ASSERT_EQ(batches.batches.size(), 2u);
  EXPECT_NE(batches.batches[0].typeface.get(),
            batches.batches[1].typeface.get());
}

TEST(GlyphBatches, AMatrixGlyphRidesItsOwnLaneInsideItsBucket) {
  // A shear cannot be an RSXform, so that glyph draws under its own matrix
  // — in the SAME bucket, so it keeps its pass order and its paint, and
  // without disturbing the shared transform array its neighbours ride.
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"HH", 48.0f);
  BlockFlow flow(SkRect::MakeXYWH(10, 10, 380, 80));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  SkMatrix sheared;
  GlyphRSXformBatches batches;
  bool first = true;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    GlyphDress dress;
    const SkPoint center = glyph.rest + SkVector{glyph.advance * 0.5f, 0};
    if (first) {
      dress.center = center;
    } else {
      sheared = SkMatrix::Translate(center.x(), center.y());
      sheared.preConcat(SkMatrix::MakeAll(1, -0.5f, 0, 0, 1, 0, 0, 0, 1));
      sheared.preTranslate(-glyph.advance * 0.5f, 0);
      dress.matrix = &sheared;
    }
    first = false;
    batches.addGlyph(glyph, dress);
  });
  ASSERT_EQ(batches.batches.size(), 1u) << "the matrix lane split the bucket";
  EXPECT_EQ(batches.batches[0].glyphs.size(), 1u);
  EXPECT_EQ(batches.batches[0].matrixGlyphs.size(), 1u);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 100));
  surface->getCanvas()->clear(SK_ColorWHITE);
  EXPECT_EQ(batches.draw(surface->getCanvas()), 2)
      << "both lanes must reach the canvas";
  batches.clear();
  EXPECT_TRUE(batches.batches[0].matrixGlyphs.empty())
      << "clear() left the matrix lane behind";
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

// ---------------------------------------------------------------------------
// TEXT ON A PATH — one placement function, read by the layout and by the
// caller that re-places its glyphs

namespace {
/// A closed circle as one contour measure, plus its length.
std::pair<sk_sp<SkContourMeasure>, float> circleContour(float radius) {
  SkPathBuilder builder;
  builder.addCircle(0, 0, radius);
  const SkPath path = builder.detach();
  SkContourMeasureIter iterator(path, false);
  sk_sp<SkContourMeasure> contour = iterator.next();
  return {contour, contour ? contour->length() : 0.0f};
}

/// One line whose whole measure is one contour.
LineSetFlow ringFlow(const sk_sp<SkContourMeasure>& contour, float length,
                     float start = 0, float advanceScale = 1.0f) {
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

TEST(PathText, APlacedGlyphReportsTheIntervalAndPenItWasPlacedAt) {
  // The pair a caller needs to re-place a curved run at draw time. The pen
  // is the glyph's ADVANCE CENTRE, in advance units, which is what
  // placeAt() anchors — feed one straight back to the other and the answer
  // must be the position the layout itself computed.
  auto [contour, length] = circleContour(200.0f);
  ASSERT_TRUE(contour);
  Paragraph paragraph = ParagraphBuilder(basicStyle(20.0f))
                            .addText(u8"round and round we go")
                            .build();
  FontContext& context = sharedContext();
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

TEST(PathText, ThePenIsTheAccumulatedAdvanceNotTheShapedPosition) {
  // The advance-centre contract, stated as a number. HarfBuzz's per-glyph
  // offsets sit ON TOP of the pen position, and the arc coordinate must be
  // taken from the pen — an accented glyph anchored by its shaped x drifts
  // off the curve by exactly its own offset.
  auto [contour, length] = circleContour(300.0f);
  ASSERT_TRUE(contour);
  Paragraph paragraph =
      ParagraphBuilder(basicStyle(24.0f)).addText(u8"clockwise").build();
  FontContext& context = sharedContext();
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

TEST(PathText, APhaseWalksTheRunRoundAClosedContourWithoutRelayout) {
  // The marquee, at the level the geometry provides it: one layout, and a
  // phase that re-places every glyph. Crossing the seam is the case that
  // must not fall off — the run walks through fraction 1 and out the other
  // side.
  auto [contour, length] = circleContour(150.0f);
  ASSERT_TRUE(contour);
  LineInterval interval;
  interval.contour = contour;
  interval.length = length;

  SkPoint atZero, nearSeam, pastSeam;
  SkVector tangent;
  EXPECT_TRUE(interval.placeAt(0.0f, 0.0f, 0, &atZero, &tangent));
  EXPECT_TRUE(interval.placeAt(0.0f, length * 0.98f, 0, &nearSeam, &tangent));
  EXPECT_TRUE(interval.placeAt(0.0f, length * 1.02f, 0, &pastSeam, &tangent));
  // Every one of them is ON the circle…
  for (const SkPoint& p : {atZero, nearSeam, pastSeam})
    EXPECT_NEAR(std::hypot(p.x(), p.y()), 150.0f, 0.5f);
  // …and a phase past the seam is a short step from the phase before it,
  // not a jump back to the start.
  EXPECT_LT(SkPoint::Distance(nearSeam, pastSeam), length * 0.1f);
  EXPECT_GT(SkPoint::Distance(atZero, nearSeam), 1.0f);
}

TEST(PathText, AGeometricallyClosedContourWrapsWhenItSaysSo) {
  // A 359.9-degree arc is a common spelling of a ring and is NOT flagged
  // closed. Without the opt-in it clamps; with it, it wraps.
  SkPathBuilder arcBuilder;
  arcBuilder.addArc(SkRect::MakeLTRB(-100, -100, 100, 100), 0, 359.9f);
  const SkPath arc = arcBuilder.detach();
  SkContourMeasureIter iterator(arc, false);
  sk_sp<SkContourMeasure> contour = iterator.next();
  ASSERT_TRUE(contour);
  ASSERT_FALSE(contour->isClosed());

  LineInterval clamping;
  clamping.contour = contour;
  clamping.length = contour->length();
  LineInterval wrapping = clamping;
  wrapping.wrapContour = true;

  SkPoint clamped, wrapped;
  SkVector tangent;
  EXPECT_FALSE(clamping.placeAt(-40.0f, 0.0f, 0, &clamped, &tangent))
      << "a pen before the start must report that it was clamped";
  EXPECT_TRUE(wrapping.placeAt(-40.0f, 0.0f, 0, &wrapped, &tangent));
  EXPECT_GT(SkPoint::Distance(clamped, wrapped), 10.0f);
  EXPECT_NEAR(std::hypot(wrapped.x(), wrapped.y()), 100.0f, 1.0f);
}

TEST(PathText, ANegativeAdvanceScaleWalksTheContourBackwards) {
  // How a run reads right way up along the lower half of a ring: the whole
  // run turns round once, rather than each letter turning over.
  auto [contour, length] = circleContour(120.0f);
  ASSERT_TRUE(contour);
  LineInterval forward;
  forward.contour = contour;
  forward.length = length;
  LineInterval backward = forward;
  backward.advanceScale = -1.0f;
  backward.contourStart = 100.0f;

  SkPoint forwardPoint, backwardPoint;
  SkVector forwardTangent, backwardTangent;
  forward.placeAt(100.0f, 0.0f, 0, &forwardPoint, &forwardTangent);
  backward.placeAt(0.0f, 0.0f, 0, &backwardPoint, &backwardTangent);
  // Same point on the contour…
  EXPECT_NEAR(forwardPoint.x(), backwardPoint.x(), 0.01f);
  EXPECT_NEAR(forwardPoint.y(), backwardPoint.y(), 0.01f);
  // …faced the other way.
  EXPECT_NEAR(backwardTangent.x(), -forwardTangent.x(), 1e-4f);
  EXPECT_NEAR(backwardTangent.y(), -forwardTangent.y(), 1e-4f);
  // And the pen still travels forward through the text: a later pen sits
  // further BACK along the contour.
  SkPoint later;
  SkVector ignored;
  backward.placeAt(30.0f, 0.0f, 0, &later, &ignored);
  SkPoint earlierForward;
  forward.placeAt(70.0f, 0.0f, 0, &earlierForward, &ignored);
  EXPECT_NEAR(later.x(), earlierForward.x(), 0.01f);
  EXPECT_NEAR(later.y(), earlierForward.y(), 0.01f);
}

TEST(GlyphBatches, ACentreOffsetMovesThePivotOffTheAdvanceAxis) {
  // The RSXform convention backs a glyph out from its pose centre by half
  // its advance ALONG ITS OWN X. A vertical column's advance is not on x,
  // so the dress carries the back-out instead — and it turns with the
  // glyph, exactly as the default one does.
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"H", 40.0f);
  BlockFlow flow(SkRect::MakeWH(200, 60));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  const SkPoint centre{100, 30};
  const SkVector offset{0, 12};
  const auto placedAt = [&](float cosine, float sine, const SkVector* off) {
    GlyphRSXformBatches batches;
    forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
      GlyphDress dress;
      dress.center = centre;
      dress.cosine = cosine;
      dress.sine = sine;
      dress.centreOffset = off;
      batches.addGlyph(glyph, dress);
    });
    return batches.batches.at(0).transforms.at(0);
  };

  const SkRSXform upright = placedAt(1, 0, &offset);
  EXPECT_FLOAT_EQ(upright.fTx, centre.x());
  EXPECT_FLOAT_EQ(upright.fTy, centre.y() - offset.y())
      << "an unrotated glyph backs out along the offset itself";

  // A quarter turn takes (0, 12) to (-12, 0).
  const SkRSXform turned = placedAt(0, 1, &offset);
  EXPECT_NEAR(turned.fTx, centre.x() + offset.y(), 1e-4f);
  EXPECT_NEAR(turned.fTy, centre.y(), 1e-4f);

  // Null keeps the horizontal convention.
  const SkRSXform plain = placedAt(1, 0, nullptr);
  EXPECT_LT(plain.fTx, centre.x()) << "backed out by half its advance";
  EXPECT_FLOAT_EQ(plain.fTy, centre.y());
}

TEST(GlyphBatches, SubpixelDecidesWhetherAFractionOfAPixelMovesAnything) {
  // The declaration a moving run makes, and its whole observable meaning.
  // Slide one letter through a pixel and a half and count the DISTINCT
  // frames: whole-pixel origins re-use one rasterization until the origin
  // crosses a boundary, so the count is the number of crossings, while the
  // subpixel grid answers a fresh one nearly every step. The hop a turning
  // ring shows is the first of those, one letter at a time as each origin
  // reaches its own boundary.
  //
  // Counted over a sweep rather than compared between two placements, so
  // neither verdict can be an accident of where inside a pixel the run
  // happened to start.
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"H", 44.0f);
  BlockFlow flow(SkRect::MakeXYWH(10, 10, 380, 100));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  PaintStyle white;
  white.foreground.setColor(SK_ColorWHITE);
  paragraph.setPaint(0, 1, white);

  auto render = [&](bool subpixel, float nudge) {
    GlyphRSXformBatches batches;
    batches.subpixel = subpixel;
    forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
      batches.addGlyph(glyph,
                       glyph.rest + SkVector{glyph.advance * 0.5f + nudge, 0});
    });
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 100));
    surface->getCanvas()->clear(SK_ColorBLACK);
    batches.draw(surface->getCanvas());
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(200, 100));
    EXPECT_TRUE(surface->readPixels(bitmap.pixmap(), 0, 0));
    return bitmap;
  };
  auto distinctAcrossOnePixel = [&](bool subpixel) {
    constexpr int kSteps = 8;
    std::vector<SkBitmap> frames;
    frames.reserve(kSteps);
    for (int i = 0; i < kSteps; ++i)
      frames.push_back(render(subpixel, 1.5f * (float)i / (float)(kSteps - 1)));
    auto same = [](const SkBitmap& a, const SkBitmap& b) {
      for (int y = 0; y < 100; ++y)
        for (int x = 0; x < 200; ++x)
          if (a.getColor(x, y) != b.getColor(x, y)) return false;
      return true;
    };
    int distinct = 0;
    for (size_t i = 0; i < frames.size(); ++i) {
      bool seen = false;
      for (size_t j = 0; j < i; ++j) seen |= same(frames[i], frames[j]);
      distinct += !seen;
    }
    return distinct;
  };

  EXPECT_LE(distinctAcrossOnePixel(false), 3)
      << "whole-pixel origins moved on a fraction of a pixel";
  EXPECT_GE(distinctAcrossOnePixel(true), 5)
      << "the subpixel grid ignored fractions of a pixel";
}
