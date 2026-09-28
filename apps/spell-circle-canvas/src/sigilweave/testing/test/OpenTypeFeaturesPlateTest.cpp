/** @file
 * OpenType features made visible — ligature control, discretionary
 * ligatures, small caps, lining figures, fractions — each row the same
 * text through the same engine with a different feature list, which is
 * part of the shape cache's key.
 */

#include <gtest/gtest.h>
#include <include/core/SkFontMgr.h>
#include <sigilweave/layout/Flow.h>
#include <sigilweave/testing/Passage.h>

#include <utility>
#include <vector>

#include "sigilweave/advanced/Skia.h"
#include "support/Plates.h"

using namespace sigil::weave;
using namespace sigil::weave::test;
namespace weave = sigil::weave;

TEST(WeavePlates, OpenTypeFeaturesDrawTheirBaseline) {
  FontContext& fonts = sigil::test::fonts();
  const sk_sp<SkTypeface> hoefler =
      fonts.fontManager()->matchFamilyStyle("Hoefler Text", SkFontStyle());
  if (!hoefler) GTEST_SKIP() << "the panel is set in Hoefler Text";

  const weave::testing::Plate plate({980, 560}, kPaper);
  struct Row {
    const char8_t* label;
    const char8_t* text;
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
  for (const Row& row : rows) {
    kit::drawLabel(plate.canvas(), fonts, row.label, {40, rowTop},
                   {.color = SkColor4f::FromColor(kAccent), .width = 900, .height = 18});
    TextStyle body = plateStyle(30, kInk);
    body.shaping.typeface = hoefler;
    body.shaping.fontFeatures = row.fontFeatures;
    Paragraph paragraph;
    paragraph.appendText(row.text, body);
    BlockFlow flow(sigil::geometry::path::Rect::of({40, rowTop + 20}, {900, 48}));
    plate.draw(weave::testing::lay(fonts, std::move(paragraph), flow));
    rowTop += 88;
  }

  expectPlate(plate, "features");
}
