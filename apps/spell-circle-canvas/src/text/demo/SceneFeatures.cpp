// Scene G(b) — OpenType features made visible: ligature control,
// discretionary ligatures, small caps, lining vs oldstyle figures — each row
// is the same engine with a different FontFeature list (part of the
// shape-cache key).
#include "DemoScenes.h"
#include "DemoSupport.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkSurface.h>

#include <cstdio>
#include <vector>

using namespace textflow;

void sceneFeatures(FontContext &fontContext,
                   const std::filesystem::path &outputDirectory) {
  sk_sp<SkTypeface> hoeflerTypeface =
      fontContext.fontManager()->matchFamilyStyle("Hoefler Text",
                                                  SkFontStyle());
  if (!hoeflerTypeface) {
    std::printf("Scene G — features: Hoefler Text missing, skipped\n\n");
    return;
  }

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(980, 560));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);

  struct Row {
    const char8_t *label;
    const char8_t *text;
    std::vector<FontFeature> fontFeatures;
  };
  const Row rows[] = {
      {u8"default (liga on, oldstyle figures)",
       u8"The office staff filed 1234567890 affidavits.",
       {}},
      {u8"liga=0 clig=0 (ligatures off)",
       u8"The office staff filed 1234567890 affidavits.",
       {{"liga", 0}, {"clig", 0}}},
      {u8"dlig=1 (discretionary ct/st ligatures)",
       u8"The strict architect stood fast.",
       {{"dlig", 1}}},
      {u8"smcp=1 (small caps)",
       u8"The office staff filed affidavits.",
       {{"smcp", 1}}},
      {u8"lnum=1 (lining figures)",
       u8"Figures 1234567890 rise to the cap height.",
       {{"lnum", 1}}},
      {u8"frac=1 (fractions)", u8"Mix 1/2 cup with 3/4 spoon.", {{"frac", 1}}},
  };

  float rowTop = 30;
  for (const Row &row : rows) {
    textflowkit::drawLabel(canvas, fontContext, row.label, {40, rowTop},
                           {.color = kAccent, .width = 900, .height = 18});

    TextStyle body = style(30, kInk);
    body.shaping.typeface = hoeflerTypeface;
    body.shaping.fontFeatures = row.fontFeatures;
    Paragraph paragraph;
    paragraph.appendText(row.text, body);
    BlockFlow flow(SkRect::MakeXYWH(40, rowTop + 20, 900, 48));
    layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
    rowTop += 88;
  }

  writePng(surface.get(), outputDirectory / "features.png");
  std::printf("Scene G — OpenType features panel written\n\n");
}
