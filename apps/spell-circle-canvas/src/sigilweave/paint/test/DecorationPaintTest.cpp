/** @file
 * Decorations as drawn: a restyle draws without reshaping, spanning bands
 * cover the gaps between words while per-word bands break at them,
 * highlights sit beneath the glyphs, and a shaded band draws independently
 * of the glyph paint.
 */

#include <gtest/gtest.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>

#include <algorithm>
#include <iomanip>
#include <vector>

#include "support/Fonts.h"
#include "support/Layouts.h"
#include "support/Paragraphs.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(DecorationTest, RestyleDrawsWithoutReshaping) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"decorate me");
  BlockFlow flow(SkRect::MakeWH(400, 60));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  fontContext.resetStats();
  PaintStyle decorated(SK_ColorBLACK);
  decorated.addDecoration({}).addDecoration(
      {.kind = Decoration::Kind::kStrikethrough, .color = SK_ColorRED});
  paragraph.setPaint(0, 8, decorated);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 60));
  surface->getCanvas()->clear(SK_ColorWHITE);
  layout.draw(surface->getCanvas(), paragraph);
  layout.drawBatched(surface->getCanvas(), paragraph);
  EXPECT_EQ(fontContext.stats().shapeCalls, 0u)
      << "decorations are paint-side: no reshape, no relayout";

  // The red strikethrough must have put red ink on the surface. The band
  // may be 1px tall on a fractional baseline offset, so anti-aliasing can
  // blend every pixel — accept dominantly-red rather than exact SK_ColorRED.
  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  bool sawRed = false;
  for (int y = 0; y < pixmap.height() && !sawRed; ++y)
    for (int x = 0; x < pixmap.width() && !sawRed; ++x) {
      const SkColor color = pixmap.getColor(x, y);
      sawRed = SkColorGetR(color) > 200 && SkColorGetG(color) < 128 &&
               SkColorGetB(color) < 128;
    }
  EXPECT_TRUE(sawRed);
}

TEST(DecorationTest, UnderlineSpansAcrossWordGaps) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"mono nano", 32.0f);
  BlockFlow flow(SkRect::MakeWH(400, 80));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  // Two words → (at least) two runs on one line with a glue gap between.
  std::vector<const PositionedRun*> wordRuns;
  for (const PositionedRun& run : layout.runs)
    if (run.shaped) wordRuns.push_back(&run);
  ASSERT_GE(wordRuns.size(), 2u);
  const float gapStart = wordRuns[0]->origin.x() + wordRuns[0]->shaped->advance;
  const float gapEnd = wordRuns[1]->origin.x();
  ASSERT_GT(gapEnd, gapStart) << "expected inter-word glue";

  PaintStyle underlined(SK_ColorBLACK);
  Decoration underline;
  underline.thickness = 3.0f;
  underline.offset = 6.0f;  // clear of any glyph ink
  underline.skipInk = false;
  underlined.addDecoration(underline);
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()),
                     underlined);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 80));
  surface->getCanvas()->clear(SK_ColorWHITE);
  layout.draw(surface->getCanvas(), paragraph);

  // The decorated range is one continuous band: the middle of the glue gap
  // must be inked, not white (pre-spanning behavior drew per-word bands
  // that skipped the space).
  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  const int probeX = static_cast<int>((gapStart + gapEnd) * 0.5f);
  const int probeY = static_cast<int>(wordRuns[0]->origin.y() + 6.0f + 1.5f);
  ASSERT_LT(probeX, pixmap.width());
  ASSERT_LT(probeY, pixmap.height());
  const SkColor gapColor = pixmap.getColor(probeX, probeY);
  EXPECT_LT(SkColorGetR(gapColor), 100u)
      << "underline must cover the word gap (got " << std::hex << gapColor
      << ")";

  // Same probe with skip-ink on: gaps still covered (no ink there).
  PaintStyle skipInked(SK_ColorBLACK);
  Decoration inkAware;
  inkAware.thickness = 3.0f;
  inkAware.offset = 6.0f;
  skipInked.addDecoration(inkAware);
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()),
                     skipInked);
  surface->getCanvas()->clear(SK_ColorWHITE);
  layout.draw(surface->getCanvas(), paragraph);
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  EXPECT_LT(SkColorGetR(pixmap.getColor(probeX, probeY)), 100u)
      << "skip-ink must only break at glyph ink, never at word gaps";
}

TEST(DecorationTest, PerWordSpanBreaksAtGaps) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"mono nano", 32.0f);
  BlockFlow flow(SkRect::MakeWH(400, 80));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  std::vector<const PositionedRun*> wordRuns;
  for (const PositionedRun& run : layout.runs)
    if (run.shaped) wordRuns.push_back(&run);
  ASSERT_GE(wordRuns.size(), 2u);
  const float gapStart = wordRuns[0]->origin.x() + wordRuns[0]->shaped->advance;
  const float gapEnd = wordRuns[1]->origin.x();
  ASSERT_GT(gapEnd, gapStart);

  PaintStyle underlined(SK_ColorBLACK);
  Decoration perWord;
  perWord.span = Decoration::Span::kPerWord;
  perWord.thickness = 3.0f;
  perWord.offset = 6.0f;
  perWord.skipInk = false;
  underlined.addDecoration(perWord);
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()),
                     underlined);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 80));
  surface->getCanvas()->clear(SK_ColorWHITE);
  layout.draw(surface->getCanvas(), paragraph);

  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  const int gapX = static_cast<int>((gapStart + gapEnd) * 0.5f);
  const int bandY = static_cast<int>(wordRuns[0]->origin.y() + 6.0f + 1.5f);
  // The gap stays bare…
  EXPECT_GT(SkColorGetR(pixmap.getColor(gapX, bandY)), 200u)
      << "kPerWord must not underline the word gap";
  // …while both words still carry their own bands.
  const int firstWordX = static_cast<int>(wordRuns[0]->origin.x() +
                                          wordRuns[0]->shaped->advance * 0.5f);
  EXPECT_LT(SkColorGetR(pixmap.getColor(firstWordX, bandY)), 100u);
}

TEST(DecorationTest, HighlightSpansGapsBeneathGlyphs) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"mono nano", 32.0f);
  BlockFlow flow(SkRect::MakeWH(400, 80));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  std::vector<const PositionedRun*> wordRuns;
  for (const PositionedRun& run : layout.runs)
    if (run.shaped) wordRuns.push_back(&run);
  ASSERT_GE(wordRuns.size(), 2u);
  const float gapStart = wordRuns[0]->origin.x() + wordRuns[0]->shaped->advance;
  const float gapEnd = wordRuns[1]->origin.x();
  ASSERT_GT(gapEnd, gapStart);

  PaintStyle marked(SK_ColorBLACK);
  Decoration highlight;
  highlight.kind = Decoration::Kind::kHighlight;
  highlight.color = 0x80FFE066;  // translucent marker yellow
  marked.addDecoration(highlight);
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()), marked);

  for (const bool batched : {false, true}) {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 80));
    surface->getCanvas()->clear(SK_ColorWHITE);
    if (batched)
      layout.drawBatched(surface->getCanvas(), paragraph);
    else
      layout.draw(surface->getCanvas(), paragraph);

    SkPixmap pixmap;
    ASSERT_TRUE(surface->peekPixels(&pixmap));
    // Mid-gap, mid-x-height: the marker stroke must cover the space
    // between words (tinted, not white).
    const int gapX = static_cast<int>((gapStart + gapEnd) * 0.5f);
    const int xHeightY = static_cast<int>(wordRuns[0]->origin.y() - 8.0f);
    const SkColor gapColor = pixmap.getColor(gapX, xHeightY);
    EXPECT_NE(gapColor, SK_ColorWHITE) << (batched ? "batched" : "immediate")
                                       << ": highlight must cover the word gap";
    EXPECT_GT(SkColorGetB(gapColor), 100u)
        << "gap should be a tint, not glyph ink";

    // The glyphs draw over the highlight: dark ink must survive somewhere
    // inside the first word's extent at x-height.
    bool sawInk = false;
    const int wordStartX = static_cast<int>(wordRuns[0]->origin.x());
    const int wordEndX = static_cast<int>(gapStart);
    for (int x = wordStartX; x < wordEndX && !sawInk; ++x)
      for (int y = xHeightY - 6; y <= xHeightY + 6 && !sawInk; ++y)
        sawInk = SkColorGetR(pixmap.getColor(x, y)) < 80;
    EXPECT_TRUE(sawInk) << "glyphs must draw above the highlight";
  }
}

TEST(DecorationTest, ShadedBandDrawsIndependentlyOfGlyphPaint) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"mono nano", 32.0f);
  BlockFlow flow(SkRect::MakeWH(400, 80));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  std::vector<const PositionedRun*> wordRuns;
  for (const PositionedRun& run : layout.runs)
    if (run.shaped) wordRuns.push_back(&run);
  ASSERT_GE(wordRuns.size(), 2u);
  const float gapStart = wordRuns[0]->origin.x() + wordRuns[0]->shaped->advance;
  const float gapEnd = wordRuns[1]->origin.x();
  ASSERT_GT(gapEnd, gapStart);

  // Glyphs keep a plain black fill; only the highlight band gets a shader.
  // A solid green color shader stands in for the animated presets: green
  // can only reach the surface through the band's paint override.
  PaintStyle marked(SK_ColorBLACK);
  Decoration highlight;
  highlight.kind = Decoration::Kind::kHighlight;
  SkPaint bandPaint;
  bandPaint.setAntiAlias(true);
  bandPaint.setShader(SkShaders::Color(SK_ColorGREEN));
  highlight.paint = bandPaint;
  marked.addDecoration(highlight);
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()), marked);

  for (const bool batched : {false, true}) {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 80));
    surface->getCanvas()->clear(SK_ColorWHITE);
    if (batched)
      layout.drawBatched(surface->getCanvas(), paragraph);
    else
      layout.draw(surface->getCanvas(), paragraph);

    SkPixmap pixmap;
    ASSERT_TRUE(surface->peekPixels(&pixmap));
    // Mid-gap, mid-x-height sits inside the band and clear of glyph ink:
    // the shader must have filled it.
    const int gapX = static_cast<int>((gapStart + gapEnd) * 0.5f);
    const int xHeightY = static_cast<int>(wordRuns[0]->origin.y() - 8.0f);
    const SkColor gapColor = pixmap.getColor(gapX, xHeightY);
    EXPECT_GT(SkColorGetG(gapColor), 200u)
        << (batched ? "batched" : "immediate")
        << ": band shader must fill the gap";
    EXPECT_LT(SkColorGetR(gapColor), 100u);

    // The glyph fill stays plain black above the shaded band — the two
    // pipelines resolve independently.
    bool sawInk = false;
    const int wordStartX = static_cast<int>(wordRuns[0]->origin.x());
    const int wordEndX = static_cast<int>(gapStart);
    for (int x = wordStartX; x < wordEndX && !sawInk; ++x)
      for (int y = xHeightY - 6; y <= xHeightY + 6 && !sawInk; ++y) {
        const SkColor color = pixmap.getColor(x, y);
        sawInk = SkColorGetR(color) < 80 && SkColorGetG(color) < 80;
      }
    EXPECT_TRUE(sawInk) << "glyphs must keep their own paint";
  }
}

TEST(DecorationTest, AColumnDrawsItsBandBesideTheType) {
  // 傍線: down a column the emphasis line runs BESIDE the characters on the
  // right, the length of the run — the same band the horizontal setting
  // draws under a line, turned with the type.
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"縦書きの傍線", 32.0f);
  paragraph.setWritingMode(WritingMode::kVerticalRL);
  VerticalBlockFlow flow(SkRect::MakeWH(120, 300));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 40;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_FALSE(layout.runs.empty());
  const PositionedRun& first = layout.runs.front();
  ASSERT_TRUE(first.shaped && first.shaped->vertical)
      << "the fixture did not set in columns";

  PaintStyle underlined(SK_ColorBLACK);
  Decoration sideline;
  sideline.thickness = 3.0f;
  sideline.color = SK_ColorRED;
  underlined.addDecoration(sideline);
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()),
                     underlined);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 300));
  surface->getCanvas()->clear(SK_ColorWHITE);
  layout.draw(surface->getCanvas(), paragraph);
  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));

  // Red ink, and all of it on ONE side of the column axis, spread down the
  // column rather than across it.
  int redPixels = 0, redLeftOfAxis = 0;
  int topMost = pixmap.height(), bottomMost = -1;
  float minX = 1e9f, maxX = -1e9f;
  for (int y = 0; y < pixmap.height(); ++y)
    for (int x = 0; x < pixmap.width(); ++x) {
      const SkColor color = pixmap.getColor(x, y);
      if (SkColorGetR(color) < 200 || SkColorGetG(color) > 128 ||
          SkColorGetB(color) > 128)
        continue;
      ++redPixels;
      if ((float)x < first.origin.x()) ++redLeftOfAxis;
      topMost = std::min(topMost, y);
      bottomMost = std::max(bottomMost, y);
      minX = std::min(minX, (float)x);
      maxX = std::max(maxX, (float)x);
    }
  ASSERT_GT(redPixels, 0) << "a column drew no band at all";
  EXPECT_EQ(redLeftOfAxis, 0)
      << "the band crossed to the wrong side of the column";
  EXPECT_GT(bottomMost - topMost, 60)
      << "the band must run DOWN the column, not across it";
  EXPECT_LT(maxX - minX, 6.0f) << "a 3px band must stay 3px wide";
}
