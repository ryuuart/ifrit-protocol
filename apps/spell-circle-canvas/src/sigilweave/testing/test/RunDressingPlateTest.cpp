/** @file
 * The dressing a run carries, one row each: metric decorations with an
 * ink-skipping underline, decoration spans and highlight bands, bands
 * shaded apart from their glyphs, locale-aware text-transform, word
 * spacing, variable-font axes, tab stops, and a line clamp with an
 * ellipsis.
 */

#include <gtest/gtest.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkShader.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilweave/kit/Features.h>
#include <sigilweave/layout/Flow.h>
#include <sigilweave/testing/Passage.h>
#include <sigilweave/testing/Reading.h>

#include <utility>

#include "support/Plates.h"

using namespace sigil::weave;
using namespace sigil::weave::test;
namespace weave = sigil::weave;

namespace {

/// A text-paint preset shaded by SigilMaterial's Skia backend.
sk_sp<SkShader> shade(const sigil::material::Material& material) {
  return sigil::material::skia::shader(material, {});
}

/// The caption above one row of the panel.
void drawRowLabel(FontContext& fontContext, SkCanvas* canvas,
                  const char8_t* label, float top) {
  kit::drawLabel(canvas, fontContext, label, {40, top},
                 {.color = kAccent, .width = 900, .height = 18});
}

}  // namespace

TEST(WeavePlates, RunDressingDrawsItsBaseline) {
  FontContext& fontContext = sigil::test::fonts();
  const weave::testing::Plate plate({980, 900}, kPaper);
  SkCanvas* canvas = plate.canvas();

  float rowTop = 30;

  // ── Decorations: metric-driven bands, ink-skipping underline ──────────
  drawRowLabel(
      fontContext, canvas,
      u8"decorations — underline (skip-ink) / strikethrough / overline",
      rowTop);
  {
    Paragraph paragraph;
    TextStyle underlined = plateStyle(26, kInk);
    underlined.paint.addDecoration({});  // metric underline, skipInk default
    paragraph.appendText(u8"typography just judged ", underlined);
    TextStyle struck = plateStyle(26, kInk);
    struck.paint.addDecoration(
        {.kind = Decoration::Kind::kStrikethrough, .color = kAccent});
    paragraph.appendText(u8"corrected ", struck);
    TextStyle overlined = plateStyle(26, kBlue);
    overlined.paint.addDecoration({.kind = Decoration::Kind::kOverline});
    paragraph.appendText(u8"annotated", overlined);
    BlockFlow flow(SkRect::MakeXYWH(40, rowTop + 22, 900, 44));
    layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
  }
  rowTop += 92;

  // ── Decoration spans: range vs per-word, and highlight bands ──────────
  drawRowLabel(fontContext, canvas,
               u8"decoration spans — range (default) / kPerWord / kHighlight "
               u8"behind the words",
               rowTop);
  {
    Paragraph paragraph;
    TextStyle range = plateStyle(24, kInk);
    range.paint.addDecoration({.skipInk = false});  // one continuous line
    paragraph.appendText(u8"spans the range ", range);
    TextStyle perWord = plateStyle(24, kInk);
    perWord.paint.addDecoration(
        {.span = Decoration::Span::kPerWord, .skipInk = false});
    paragraph.appendText(u8"breaks per word ", perWord);
    TextStyle marked = plateStyle(24, kInk);
    marked.paint.addDecoration(
        {.kind = Decoration::Kind::kHighlight, .color = 0x66FFD54A});
    paragraph.appendText(u8"marker over words and gaps", marked);
    BlockFlow flow(SkRect::MakeXYWH(40, rowTop + 22, 900, 44));
    layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
  }
  rowTop += 92;

  // ── Decoration fills: full-paint bands, shaded independently ──────────
  drawRowLabel(
      fontContext, canvas,
      u8"decoration fills — Decoration::paint shades the band, not the "
      u8"glyphs",
      rowTop);
  {
    const SkRect bandBounds = SkRect::MakeXYWH(40, rowTop + 22, 900, 44);
    Paragraph paragraph;

    // A text-paint preset behind plain ink: only the marker is shaded.
    TextStyle meshMarked = plateStyle(26, kInk);
    Decoration meshHighlight;
    meshHighlight.kind = Decoration::Kind::kHighlight;
    SkPaint meshPaint;
    meshPaint.setAntiAlias(true);
    meshPaint.setAlphaf(0.55f);  // keep the ink readable through the band
    meshPaint.setShader(
        shade(sigil::material::kit::meshGradient(bandBounds, 1.5f)));
    meshHighlight.paint = meshPaint;
    meshMarked.paint.addDecoration(meshHighlight);
    paragraph.appendText(u8"a mesh-gradient marker ", meshMarked);

    // A gradient underline under equally plain ink.
    TextStyle gradientRuled = plateStyle(26, kInk);
    Decoration gradientUnderline;
    gradientUnderline.thickness = 4.0f;
    gradientUnderline.skipInk = false;
    SkPaint underlinePaint;
    underlinePaint.setAntiAlias(true);
    const SkPoint gradientPoints[2] = {{bandBounds.left(), 0},
                                       {bandBounds.right(), 0}};
    const SkColor4f gradientColors[2] = {SkColor4f::FromColor(kAccent),
                                         SkColor4f::FromColor(kBlue)};
    underlinePaint.setShader(SkShaders::LinearGradient(
        gradientPoints,
        SkGradient(SkGradient::Colors({gradientColors, 2}, SkTileMode::kClamp),
                   SkGradient::Interpolation())));
    gradientUnderline.paint = underlinePaint;
    gradientRuled.paint.addDecoration(gradientUnderline);
    paragraph.appendText(u8"and a gradient rule under plain glyphs",
                         gradientRuled);

    BlockFlow flow(bandBounds);
    layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
  }
  rowTop += 92;

  // ── Text transform: shaping-side case mapping, locale-aware ───────────
  drawRowLabel(fontContext, canvas,
               u8"text-transform — uppercase (full ß→SS mapping) / capitalize",
               rowTop);
  {
    Paragraph paragraph;
    TextStyle upper = plateStyle(24, kInk);
    upper.shaping.textTransform = TextTransform::kUppercase;
    paragraph.appendText(u8"die straße wird groß — ", upper);
    TextStyle capitalized = plateStyle(24, kBlue);
    capitalized.shaping.textTransform = TextTransform::kCapitalize;
    paragraph.appendText(u8"every word starts big", capitalized);
    BlockFlow flow(SkRect::MakeXYWH(40, rowTop + 22, 900, 40));
    layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
  }
  rowTop += 88;

  // ── Word spacing: pure glue, cache untouched ──────────────────────────
  drawRowLabel(fontContext, canvas,
               u8"word-spacing — 0px vs 18px on identical shaped words",
               rowTop);
  for (int pass = 0; pass < 2; ++pass) {
    TextStyle spaced = plateStyle(20, pass == 0 ? kInk : kAccent);
    spaced.shaping.wordSpacing = pass == 0 ? 0.0f : 18.0f;
    Paragraph paragraph;
    paragraph.appendText(u8"the same words drift further apart", spaced);
    BlockFlow flow(SkRect::MakeXYWH(
        40, rowTop + 22 + static_cast<float>(pass) * 30, 900, 28));
    layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
  }
  rowTop += 118;

  // ── Variable axes: ShapingStyle::variations, memoized clones ──────────
  drawRowLabel(fontContext, canvas,
               u8"variations — {\"wght\"} sweep on one base typeface", rowTop);
  {
    sk_sp<SkTypeface> variableTypeface =
        fontContext.fontManager()->matchFamilyStyle("Noto Sans",
                                                    SkFontStyle::Normal());
    float columnLeft = 40;
    for (const float weight : {300.0f, 500.0f, 700.0f, 900.0f}) {
      TextStyle weighted = plateStyle(26, kInk);
      weighted.shaping.typeface = variableTypeface;
      weighted.shaping.variations = {{"wght", weight}};
      Paragraph paragraph;
      paragraph.appendText(u8"Weight", weighted);
      layoutSingleLine(fontContext, paragraph, {columnLeft, rowTop + 48})
          .draw(canvas, paragraph);
      columnLeft += 130;
    }
  }
  rowTop += 92;

  // ── Tab stops: explicit columns, greedy breaker ───────────────────────
  drawRowLabel(fontContext, canvas,
               u8"tab stops — positions {180, 420, 640} align three columns",
               rowTop);
  {
    ParagraphLayoutOptions options;
    options.tabStops.stops = {{180}, {420}, {640}};
    const char8_t* tabRows[] = {u8"ledger\t128.50\tconfirmed\tA",
                                u8"ink\t7.25\tpending\tB",
                                u8"paper\t1024.00\tarchived\tC"};
    float tabRowTop = rowTop + 22;
    for (const char8_t* rowText : tabRows) {
      // Tabular figures keep the numeric column rigid.
      TextStyle tabularStyle = plateStyle(18, kInk);
      tabularStyle.shaping.fontFeatures = {features::tabularNumbers};
      Paragraph tabbed;
      tabbed.appendText(rowText, tabularStyle);
      BlockFlow flow(SkRect::MakeXYWH(40, tabRowTop, 900, 26));
      layoutParagraph(fontContext, tabbed, flow, options).draw(canvas, tabbed);
      tabRowTop += 28;
    }
  }
  rowTop += 128;

  // ── Line clamp: maxLines + ellipsis on any geometry ───────────────────
  drawRowLabel(fontContext, canvas,
               u8"line clamp — overflow.maxLines = 2 with ellipsis", rowTop);
  {
    Paragraph paragraph;
    paragraph.appendText(
        u8"a paragraph that would happily run for many more lines than the "
        u8"clamp allows is cut after exactly two, with a shaped ellipsis "
        u8"marker landing on the second line no matter how much text "
        u8"follows it in the source document",
        plateStyle(18, kInk));
    ParagraphLayoutOptions options;
    options.overflow.maxLines = 2;
    options.overflow.ellipsis = u"…";
    BlockFlow flow(SkRect::MakeXYWH(40, rowTop + 22, 560, 400));
    const weave::testing::Passage clamped = weave::testing::lay(
        fontContext, std::move(paragraph), flow, std::move(options));
    const weave::testing::Reading reading = weave::testing::read(clamped);
    EXPECT_EQ(reading.lineCount, 2);
    EXPECT_TRUE(reading.ellipsized);
    plate.draw(clamped);
  }

  expectPlate(plate, "decorations");
}
