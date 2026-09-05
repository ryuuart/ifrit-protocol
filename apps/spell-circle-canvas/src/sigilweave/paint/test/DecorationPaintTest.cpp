/** @file
 * Decorations as drawn. Where a band's rectangles land is the decoration
 * feature's own claim; what reaches the canvas is this file's: a band
 * declared after the layout still inks, a highlight sits beneath the
 * glyphs, a shaded band draws independently of the glyph paint, and a
 * column's band runs beside the type on one side of it.
 */

#include <gtest/gtest.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "support/Faces.h"
#include "support/Layouts.h"
#include "support/Paragraphs.h"
#include "support/Pixels.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

/// The two-word setting every band is read across, with the glue gap
/// between the words measured off the shaped advances: a band's whole
/// question is what it does at a word boundary.
class DecorationInk : public ::testing::Test {
 protected:
  void SetUp() override {
    LaidOut setting = twoWordsOnOneLine();
    m_paragraph = std::move(setting.paragraph);
    m_layout = std::move(setting.layout);
    m_wordRuns = wordRuns(m_layout);
    ASSERT_GE(m_wordRuns.size(), 2u) << "the fixture must place two words";
    m_gapStart = m_wordRuns[0]->origin.x() + m_wordRuns[0]->shaped->advance;
    m_gapEnd = m_wordRuns[1]->origin.x();
    ASSERT_GT(m_gapEnd, m_gapStart) << "expected inter-word glue";
  }

  /// Paints the whole text with `style`, which is where a decoration is
  /// declared — the layout already stands and is never rebuilt.
  void paintEverything(const PaintStyle& style) {
    m_paragraph.setPaint(0, static_cast<uint32_t>(m_paragraph.text().size()),
                         style);
  }

  /// The layout drawn onto a white surface and read back. The pixmap
  /// borrows the surface this holds, so one probe is live at a time.
  SkPixmap probe(bool batched) {
    m_surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 80));
    m_surface->getCanvas()->clear(SK_ColorWHITE);
    if (batched)
      m_layout.drawBatched(m_surface->getCanvas(), m_paragraph);
    else
      m_layout.draw(m_surface->getCanvas(), m_paragraph);
    SkPixmap pixmap;
    EXPECT_TRUE(m_surface->peekPixels(&pixmap));
    return pixmap;
  }

  int gapX() const { return static_cast<int>((m_gapStart + m_gapEnd) * 0.5f); }
  int baselineY() const { return static_cast<int>(m_wordRuns[0]->origin.y()); }
  /// True when some pixel inside the first word's extent, within six
  /// pixels of `y`, satisfies `predicate`.
  template <typename Predicate>
  bool inkInFirstWord(const SkPixmap& pixmap, int y, Predicate&& predicate) {
    const int wordStartX = static_cast<int>(m_wordRuns[0]->origin.x());
    const int wordEndX = static_cast<int>(m_gapStart);
    for (int x = wordStartX; x < wordEndX; ++x)
      for (int row = y - 6; row <= y + 6; ++row)
        if (predicate(pixmap.getColor(x, row))) return true;
    return false;
  }

  Paragraph m_paragraph;
  ParagraphLayout m_layout;
  std::vector<const PositionedRun*> m_wordRuns;
  float m_gapStart = 0, m_gapEnd = 0;
  sk_sp<SkSurface> m_surface;
};

TEST_F(DecorationInk, ADecorationDeclaredAfterTheLayoutStillReachesTheCanvas) {
  // Decorations are paint-side: the band is asked for on a layout that was
  // placed before anyone mentioned it, and it must still ink.
  PaintStyle decorated(SK_ColorBLACK);
  decorated.addDecoration({}).addDecoration(
      {.kind = Decoration::Kind::kStrikethrough, .color = SK_ColorRED});
  paintEverything(decorated);

  // The band may be 1px tall on a fractional baseline offset, so
  // anti-aliasing can blend every pixel — accept dominantly-red rather
  // than exact SK_ColorRED.
  for (const bool batched : {false, true}) {
    const SkPixmap pixmap = probe(batched);
    EXPECT_TRUE(anyPixel(pixmap,
                         [](SkColor color) {
                           return SkColorGetR(color) > 200 &&
                                  SkColorGetG(color) < 128 &&
                                  SkColorGetB(color) < 128;
                         }))
        << (batched ? "batched" : "immediate") << ": no red band was drawn";
  }
}

TEST_F(DecorationInk, AHighlightCoversTheGapAndTheGlyphsDrawOverIt) {
  PaintStyle marked(SK_ColorBLACK);
  Decoration highlight;
  highlight.kind = Decoration::Kind::kHighlight;
  highlight.color = 0x80FFE066;  // translucent marker yellow
  marked.addDecoration(highlight);
  paintEverything(marked);

  for (const bool batched : {false, true}) {
    const SkPixmap pixmap = probe(batched);
    // Mid-gap, mid-x-height: the marker stroke must cover the space
    // between words (tinted, not white).
    const int xHeightY = baselineY() - 8;
    const SkColor gapColor = pixmap.getColor(gapX(), xHeightY);
    EXPECT_NE(gapColor, SK_ColorWHITE) << (batched ? "batched" : "immediate")
                                       << ": highlight must cover the word gap";
    EXPECT_GT(SkColorGetB(gapColor), 100u)
        << "gap should be a tint, not glyph ink";

    // The glyphs draw over the highlight: dark ink must survive somewhere
    // inside the first word's extent at x-height.
    EXPECT_TRUE(inkInFirstWord(pixmap, xHeightY, [](SkColor color) {
      return SkColorGetR(color) < 80;
    })) << "glyphs must draw above the highlight";
  }
}

TEST_F(DecorationInk, AShadedBandFillsTheGapWhileTheGlyphsKeepTheirOwnPaint) {
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
  paintEverything(marked);

  for (const bool batched : {false, true}) {
    const SkPixmap pixmap = probe(batched);
    // Mid-gap, mid-x-height sits inside the band and clear of glyph ink:
    // the shader must have filled it.
    const int xHeightY = baselineY() - 8;
    const SkColor gapColor = pixmap.getColor(gapX(), xHeightY);
    EXPECT_GT(SkColorGetG(gapColor), 200u)
        << (batched ? "batched" : "immediate")
        << ": band shader must fill the gap";
    EXPECT_LT(SkColorGetR(gapColor), 100u);

    // The glyph fill stays plain black above the shaded band — the two
    // pipelines resolve independently.
    EXPECT_TRUE(inkInFirstWord(pixmap, xHeightY, [](SkColor color) {
      return SkColorGetR(color) < 80 && SkColorGetG(color) < 80;
    })) << "glyphs must keep their own paint";
  }
}

TEST(ColumnDecorationInk, AColumnDrawsItsBandBesideTheType) {
  // 傍線: down a column the emphasis line runs BESIDE the characters on the
  // right, the length of the run — the same band the horizontal setting
  // draws under a line, turned with the type.
  FontContext& fontContext = sigil::test::fonts();
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

  const auto isBandRed = [](SkColor color) {
    return SkColorGetR(color) >= 200 && SkColorGetG(color) <= 128 &&
           SkColorGetB(color) <= 128;
  };
  ASSERT_GT(countPixels(pixmap, isBandRed), 0) << "a column drew no band";

  // All of the band's ink on ONE side of the column axis, spread down the
  // column rather than across it.
  int redLeftOfAxis = 0;
  int topMost = pixmap.height(), bottomMost = -1;
  float minX = 1e9f, maxX = -1e9f;
  for (int y = 0; y < pixmap.height(); ++y)
    for (int x = 0; x < pixmap.width(); ++x) {
      if (!isBandRed(pixmap.getColor(x, y))) continue;
      if ((float)x < first.origin.x()) ++redLeftOfAxis;
      topMost = std::min(topMost, y);
      bottomMost = std::max(bottomMost, y);
      minX = std::min(minX, (float)x);
      maxX = std::max(maxX, (float)x);
    }
  EXPECT_EQ(redLeftOfAxis, 0)
      << "the band crossed to the wrong side of the column";
  EXPECT_GT(bottomMost - topMost, 60)
      << "the band must run DOWN the column, not across it";
}
