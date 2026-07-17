// Scene K — the typographic features added in the standalone-polish pass,
// one panel each: text decorations (metric underline with ink skipping,
// strikethrough, overline), locale-aware text-transform, word spacing,
// variable-font axes through ShapingStyle::variations, tab stops, and line
// clamp. Doubles as the visual-regression PNG for those features.
#include "DemoScenes.h"
#include "DemoSupport.h"

#include <textflow/Features.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkSurface.h>

#include <cstdio>

using namespace textflow;

namespace {

void drawLabel(FontContext &fontContext, SkCanvas *canvas,
               const char8_t *label, float top) {
  Paragraph caption;
  caption.appendText(label, style(12, kAccent));
  BlockFlow captionFlow(SkRect::MakeXYWH(40, top, 900, 18));
  layoutParagraph(fontContext, caption, captionFlow).draw(canvas, caption);
}

} // namespace

void sceneNewFeatures(FontContext &fontContext,
                      const std::filesystem::path &outputDirectory) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(980, 900));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);

  float rowTop = 30;

  // ── Decorations: metric-driven bands, ink-skipping underline ──────────
  drawLabel(fontContext, canvas,
            u8"decorations — underline (skip-ink) / strikethrough / overline",
            rowTop);
  {
    Paragraph paragraph;
    TextStyle underlined = style(26, kInk);
    underlined.paint.addDecoration({}); // metric underline, skipInk default
    paragraph.appendText(u8"typography just judged ", underlined);
    TextStyle struck = style(26, kInk);
    struck.paint.addDecoration(
        {.kind = Decoration::Kind::kStrikethrough, .color = kAccent});
    paragraph.appendText(u8"corrected ", struck);
    TextStyle overlined = style(26, kBlue);
    overlined.paint.addDecoration({.kind = Decoration::Kind::kOverline});
    paragraph.appendText(u8"annotated", overlined);
    BlockFlow flow(SkRect::MakeXYWH(40, rowTop + 22, 900, 44));
    layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
  }
  rowTop += 92;

  // ── Text transform: shaping-side case mapping, locale-aware ───────────
  drawLabel(fontContext, canvas,
            u8"text-transform — uppercase (full ß→SS mapping) / capitalize",
            rowTop);
  {
    Paragraph paragraph;
    TextStyle upper = style(24, kInk);
    upper.shaping.textTransform = TextTransform::kUppercase;
    paragraph.appendText(u8"die straße wird groß — ", upper);
    TextStyle capitalized = style(24, kBlue);
    capitalized.shaping.textTransform = TextTransform::kCapitalize;
    paragraph.appendText(u8"every word starts big", capitalized);
    BlockFlow flow(SkRect::MakeXYWH(40, rowTop + 22, 900, 40));
    layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
  }
  rowTop += 88;

  // ── Word spacing: pure glue, cache untouched ──────────────────────────
  drawLabel(fontContext, canvas,
            u8"word-spacing — 0px vs 18px on identical shaped words", rowTop);
  for (int pass = 0; pass < 2; ++pass) {
    TextStyle spaced = style(20, pass == 0 ? kInk : kAccent);
    spaced.shaping.wordSpacing = pass == 0 ? 0.0f : 18.0f;
    Paragraph paragraph;
    paragraph.appendText(u8"the same words drift further apart", spaced);
    BlockFlow flow(SkRect::MakeXYWH(
        40, rowTop + 22 + static_cast<float>(pass) * 30, 900, 28));
    layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
  }
  rowTop += 118;

  // ── Variable axes: ShapingStyle::variations, memoized clones ──────────
  drawLabel(fontContext, canvas,
            u8"variations — {\"wght\"} sweep on one base typeface", rowTop);
  {
    sk_sp<SkTypeface> variableTypeface =
        fontContext.fontManager()->matchFamilyStyle("Noto Sans",
                                                    SkFontStyle::Normal());
    float columnLeft = 40;
    for (const float weight : {300.0f, 500.0f, 700.0f, 900.0f}) {
      TextStyle weighted = style(26, kInk);
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
  drawLabel(fontContext, canvas,
            u8"tab stops — positions {180, 420, 640} align three columns",
            rowTop);
  {
    ParagraphLayoutOptions options;
    options.tabStops.positions = {180, 420, 640};
    const char8_t *tabRows[] = {u8"ledger\t128.50\tconfirmed\tA",
                                u8"ink\t7.25\tpending\tB",
                                u8"paper\t1024.00\tarchived\tC"};
    float tabRowTop = rowTop + 22;
    for (const char8_t *rowText : tabRows) {
      // Tabular figures keep the numeric column rigid.
      TextStyle tabularStyle = style(18, kInk);
      tabularStyle.shaping.fontFeatures = {Features::tabularNumbers};
      Paragraph tabbed;
      tabbed.appendText(rowText, tabularStyle);
      BlockFlow flow(SkRect::MakeXYWH(40, tabRowTop, 900, 26));
      layoutParagraph(fontContext, tabbed, flow, options).draw(canvas, tabbed);
      tabRowTop += 28;
    }
  }
  rowTop += 128;

  // ── Line clamp: maxLines + ellipsis on any geometry ───────────────────
  drawLabel(fontContext, canvas,
            u8"line clamp — overflow.maxLines = 2 with ellipsis", rowTop);
  {
    Paragraph paragraph;
    paragraph.appendText(
        u8"a paragraph that would happily run for many more lines than the "
        u8"clamp allows is cut after exactly two, with a shaped ellipsis "
        u8"marker landing on the second line no matter how much text "
        u8"follows it in the source document",
        style(18, kInk));
    ParagraphLayoutOptions options;
    options.overflow.maxLines = 2;
    options.overflow.ellipsis = u"…";
    BlockFlow flow(SkRect::MakeXYWH(40, rowTop + 22, 560, 400));
    layoutParagraph(fontContext, paragraph, flow, options)
        .draw(canvas, paragraph);
  }

  writePng(surface.get(), outputDirectory / "new_features.png");
  std::printf("Scene K — new-features panel written\n\n");
}
