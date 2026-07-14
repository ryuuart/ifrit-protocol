// Scene E — typographic options: last-line justification modes, hyphenated
// narrow columns, paint effects (shadow/gradient/blur), and mixed fonts.
#include "DemoScenes.h"
#include "DemoSupport.h"

#include <include/core/SkBlurTypes.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>

#include <cstdio>

using namespace textflow;

void sceneTypography(FontContext &fontContext,
                     const std::filesystem::path &outputDirectory) {
  std::printf("Scene E — typographic options (last-line modes, hyphenation, "
              "effects, mixed fonts)\n");

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1060, 820));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);

  const char8_t *sample =
      u8"The last line of a justified paragraph reveals the typographer's "
      "intent more than any other line in the whole measure.";

  // Justify with last line start / center / end / full (InDesign-style).
  const TextAlignment lastModes[] = {
      TextAlignment::kStart, TextAlignment::kCenter, TextAlignment::kEnd};
  const char8_t *labels[] = {u8"last: left", u8"last: center", u8"last: right",
                             u8"last: full"};
  for (int exampleIndex = 0; exampleIndex < 4; ++exampleIndex) {
    const float exampleX =
        30.0f + static_cast<float>(exampleIndex % 2) * 260.0f;
    const float exampleY =
        40.0f + static_cast<float>(exampleIndex / 2) * 190.0f;
    Paragraph caption;
    caption.appendText(labels[exampleIndex], style(12, kAccent));
    BlockFlow capFlow(SkRect::MakeXYWH(exampleX, exampleY - 24, 220, 20));
    layoutParagraph(fontContext, caption, capFlow).draw(canvas, caption);

    Paragraph paragraph;
    paragraph.appendText(sample, style(14.5f));
    BlockFlow flow(SkRect::MakeXYWH(exampleX, exampleY, 220, 160));
    ParagraphLayoutOptions options;
    options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
    options.alignment = TextAlignment::kJustify;
    if (exampleIndex < 3)
      options.justification.lastLineAlignment = lastModes[exampleIndex];
    else
      options.justification.justifyLastLine = true;
    layoutParagraph(fontContext, paragraph, flow, options)
        .draw(canvas, paragraph);
  }

  // Narrow hyphenated Knuth-Plass column (soft hyphens marked with ­).
  {
    Paragraph caption;
    caption.appendText(u8"KP + soft hyphens, 130px", style(12, kAccent));
    BlockFlow capFlow(SkRect::MakeXYWH(570, 16, 220, 20));
    layoutParagraph(fontContext, caption, capFlow).draw(canvas, caption);

    Paragraph paragraph;
    paragraph.appendText(u8"In these as­ton­ish­ing­ly nar­row "
                         "col­umns, dis­cre­tion­ary breaks keep "
                         "jus­ti­fi­ca­tion from tear­ing the "
                         "spac­ing apart, ex­act­ly as a book "
                         "com­pos­i­tor would want.",
                         style(14.5f));
    BlockFlow flow(SkRect::MakeXYWH(570, 40, 130, 400));
    ParagraphLayoutOptions options;
    options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
    options.alignment = TextAlignment::kJustify;
    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    const auto layoutEndTime = Clock::now();
    layout.draw(canvas, paragraph);
    std::printf("  hyphenated 130px KP column: %.1f us cold\n",
                toMicroseconds(layoutEndTime - layoutStartTime));
  }

  // Effects: drop shadow, gradient shader, glyph blur — all paint-only.
  {
    Paragraph paragraph;
    TextStyle title = style(40, SK_ColorWHITE);
    title.paint.shadows.push_back({0x99000000, {3, 4}, 3.0f});
    paragraph.appendText(u8"Shadowed ", title);

    TextStyle gradient = style(40);
    const SkPoint gradientPoints[2] = {{730, 40}, {1030, 240}};
    const SkColor4f colors[2] = {SkColor4f::FromColor(kAccent),
                                 SkColor4f::FromColor(kBlue)};
    gradient.paint.shader = SkShaders::LinearGradient(
        gradientPoints,
        SkGradient(SkGradient::Colors({colors, 2}, SkTileMode::kClamp),
                   SkGradient::Interpolation()));
    gradient.paint.shadows.push_back({0x44000000, {2, 2}, 2.0f});
    paragraph.appendText(u8"gradient ", gradient);

    TextStyle blurred = style(40, kInk);
    blurred.paint.maskFilter =
        SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 2.4f);
    paragraph.appendText(u8"blur", blurred);

    BlockFlow flow(SkRect::MakeXYWH(730, 40, 310, 400));
    layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
  }

  // Mixed families and sizes in one flow (serif / sans / mono / CJK).
  {
    // Noto Sans/Serif (user-installed variable fonts): richer OpenType
    // shaping coverage than the system defaults.
    SkFontMgr *fontManager = fontContext.fontManager();
    TextStyle serif = style(20, kInk);
    serif.shaping.typeface =
        fontManager->matchFamilyStyle("Noto Serif", SkFontStyle());
    TextStyle sans = style(17, kBlue);
    sans.shaping.typeface =
        fontManager->matchFamilyStyle("Noto Sans", SkFontStyle());
    TextStyle mono = style(14, kAccent);
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

    BlockFlow flow(SkRect::MakeXYWH(30, 470, 660, 320));
    ParagraphLayoutOptions options;
    options.alignment = TextAlignment::kJustify;
    options.lineMetrics.height = 34;
    const auto coldStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    const auto coldEndTime = Clock::now();
    ParagraphLayout warm =
        layoutParagraph(fontContext, paragraph, flow, options);
    const auto warmEndTime = Clock::now();
    layout.draw(canvas, paragraph);
    std::printf("  mixed-font paragraph: cold %.1f us, warm %.1f us\n",
                toMicroseconds(coldEndTime - coldStartTime),
                toMicroseconds(warmEndTime - coldEndTime));
  }

  writePng(surface.get(), outputDirectory / "typography.png");
  std::printf("\n");
}
