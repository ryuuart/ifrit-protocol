/** @file
 * Typographic options: the four ways a justified paragraph's last line
 * can stand, a narrow Knuth-Plass column held together by soft hyphens,
 * paint-only effects (a drop shadow, a gradient, a glyph blur), and
 * several families and sizes in one flow.
 */

#include <gtest/gtest.h>
#include <include/core/SkBlurTypes.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>
#include <sigilweave/kit/PaintLayers.h>
#include <sigilweave/layout/Flow.h>
#include <sigilweave/testing/Passage.h>
#include <sigilweave/testing/Reading.h>

#include <utility>

#include "sigilweave/advanced/Skia.h"
#include "support/Plates.h"

using namespace sigil::weave;
using namespace sigil::weave::test;
namespace weave = sigil::weave;

TEST(WeavePlates, TypographicOptionsDrawTheirBaseline) {
  FontContext& fonts = sigil::test::fonts();
  const weave::testing::Plate plate({1060, 820}, kPaper);
  SkCanvas* canvas = plate.canvas();

  const char8_t* sample =
      u8"The last line of a justified paragraph reveals the typographer's "
      "intent more than any other line in the whole measure.";

  // Justified, with the last line at the start, centre, end, or full.
  const TextAlignment lastModes[] = {
      TextAlignment::kStart, TextAlignment::kCenter, TextAlignment::kEnd};
  const char8_t* labels[] = {u8"last: left", u8"last: center", u8"last: right",
                             u8"last: full"};
  for (int exampleIndex = 0; exampleIndex < 4; ++exampleIndex) {
    const float exampleX =
        30.0f + static_cast<float>(exampleIndex % 2) * 260.0f;
    const int exampleRow = exampleIndex / 2;
    const float exampleY = 40.0f + static_cast<float>(exampleRow) * 190.0f;
    kit::drawLabel(canvas, fonts, labels[exampleIndex],
                   {exampleX, exampleY - 24},
                   {.color = kAccent, .width = 220, .height = 20});

    Paragraph paragraph;
    paragraph.appendText(sample, plateStyle(14.5f));
    BlockFlow flow(sigil::geometry::path::Rect::of({exampleX, exampleY}, {220, 160}));
    ParagraphLayoutOptions options;
    options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
    options.alignment = TextAlignment::kJustify;
    if (exampleIndex < 3)
      options.justification.lastLineAlignment = lastModes[exampleIndex];
    else
      options.justification.justifyLastLine = true;
    plate.draw(weave::testing::lay(fonts, std::move(paragraph), flow,
                                   std::move(options)));
  }

  // A narrow Knuth-Plass column whose soft hyphens are its only
  // discretionary breaks.
  {
    kit::drawLabel(canvas, fonts, u8"KP + soft hyphens, 130px", {570, 16},
                   {.color = kAccent, .width = 220, .height = 20});

    Paragraph paragraph;
    paragraph.appendText(
        u8"In these as­ton­ish­ing­ly nar­row "
        "col­umns, dis­cre­tion­ary breaks keep "
        "jus­ti­fi­ca­tion from tear­ing the "
        "spac­ing apart, ex­act­ly as a book "
        "com­pos­i­tor would want.",
        plateStyle(14.5f));
    BlockFlow flow(sigil::geometry::path::Rect::of({570, 40}, {130, 400}));
    ParagraphLayoutOptions options;
    options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
    options.alignment = TextAlignment::kJustify;
    const weave::testing::Passage column = weave::testing::lay(
        fonts, std::move(paragraph), flow, std::move(options));
    const weave::testing::Reading reading = weave::testing::read(column);
    EXPECT_EQ(reading.hyphenationPoints.size(), 20u)
        << "every soft hyphen typed is a point the breakers may take";
    plate.draw(column);
  }

  // Effects that only paint: a drop shadow, a gradient shader, a blur.
  {
    Paragraph paragraph;
    TextStyle title = plateStyle(40, SK_ColorWHITE);
    title.paint.addUnderlay(kit::dropShadow(0x99000000, {3, 4}, 3.0f));
    paragraph.appendText(u8"Shadowed ", title);

    TextStyle gradient = plateStyle(40);
    const SkPoint gradientPoints[2] = {{730, 40}, {1030, 240}};
    const SkColor4f colors[2] = {SkColor4f::FromColor(kAccent),
                                 SkColor4f::FromColor(kBlue)};
    gradient.paint.foreground.setShader(SkShaders::LinearGradient(
        gradientPoints,
        SkGradient(SkGradient::Colors({colors, 2}, SkTileMode::kClamp),
                   SkGradient::Interpolation())));
    gradient.paint.addUnderlay(kit::dropShadow(0x44000000, {2, 2}, 2.0f));
    paragraph.appendText(u8"gradient ", gradient);

    TextStyle blurred = plateStyle(40, kInk);
    blurred.paint.foreground.setMaskFilter(
        SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 2.4f));
    paragraph.appendText(u8"blur", blurred);

    BlockFlow flow(sigil::geometry::path::Rect::of({730, 40}, {310, 400}));
    plate.draw(weave::testing::lay(fonts, std::move(paragraph), flow));
  }

  // Several families and sizes in one flow: serif, sans, mono, and CJK by
  // fallback.
  {
    SkFontMgr* fontManager = fonts.fontManager();
    TextStyle serif = plateStyle(20, kInk);
    serif.shaping.typeface =
        fontManager->matchFamilyStyle("Noto Serif", SkFontStyle());
    TextStyle sans = plateStyle(17, kBlue);
    sans.shaping.typeface =
        fontManager->matchFamilyStyle("Noto Sans", SkFontStyle());
    TextStyle mono = plateStyle(14, kAccent);
    mono.shaping.typeface =
        fontManager->matchFamilyStyle("Menlo", SkFontStyle());

    Paragraph paragraph;
    paragraph.appendText(u8"Serif voices carry the body, ", serif);
    paragraph.appendText(u8"a grotesque interjects, ", sans);
    paragraph.appendText(u8"code whispers in mono, ", mono);
    paragraph.appendText(u8"そして日本語がフォールバックで加わり、", sans);
    paragraph.appendText(u8"모든 서체가 한 단락 안에서 섞인다 ", serif);
    paragraph.appendText(u8"— one paragraph, many fonts, one shape cache.",
                         serif);

    BlockFlow flow(sigil::geometry::path::Rect::of({30, 470}, {660, 320}));
    ParagraphLayoutOptions options;
    options.alignment = TextAlignment::kJustify;
    options.lineMetrics.height = 34;
    plate.draw(weave::testing::lay(fonts, std::move(paragraph), flow,
                                   std::move(options)));
  }

  expectPlate(plate, "typography");
}
