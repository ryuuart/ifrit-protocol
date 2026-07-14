// Scene C — freeform typography: text along a spiral path and along an
// arbitrary set of slanted lines (the Pretext-style demos).
#include "DemoScenes.h"
#include "DemoSupport.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <cstdio>
#include <numbers>

using namespace textflow;

void sceneFreeform(FontContext &fontContext,
                   const std::filesystem::path &outputDirectory) {
  std::printf("Scene C — freeform lines (spiral path + slanted line set)\n");

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(900, 480));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);

  // Spiral: three turns, as one SkPath, glyphs tangent to the curve.
  {
    SkPathBuilder spiralBuilder;
    const SkPoint center = {240, 240};
    const int steps = 220;
    for (int stepIndex = 0; stepIndex <= steps; ++stepIndex) {
      const float progress = static_cast<float>(stepIndex) / steps;
      const float angle = progress * 6.0f * std::numbers::pi_v<float>;
      const float radius = 40 + 150 * progress;
      const SkPoint point = {center.x() + radius * std::cos(angle),
                             center.y() + radius * std::sin(angle)};
      if (stepIndex == 0)
        spiralBuilder.moveTo(point);
      else
        spiralBuilder.lineTo(point);
    }
    const SkPath spiral = spiralBuilder.detach();
    Paragraph paragraph;
    paragraph.appendText(
        "text can follow any path — 螺旋に沿って文字が流れ、 나선을 따라 "
        "글자가 흐르고, 文字沿着螺旋流动 — and every glyph stays tangent to "
        "the curve while the words keep their cached shapes",
        style(17));
    PathFlow flow(spiral);
    const auto coldStartTime = Clock::now();
    ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
    const auto coldEndTime = Clock::now();
    ParagraphLayout warm =
        layoutParagraph(fontContext, paragraph, flow); // caches primed
    const auto warmEndTime = Clock::now();
    layout.draw(canvas, paragraph);
    std::printf("  spiral layout: cold %.1f us, warm %.1f us\n",
                toMicroseconds(coldEndTime - coldStartTime),
                toMicroseconds(warmEndTime - coldEndTime));
  }

  // Arbitrary slanted line set (Pretext-style freedom).
  {
    LineSetFlow flow;
    for (int lineIndex = 0; lineIndex < 6; ++lineIndex) {
      // Gentle alternating slopes; rise stays well under the line pitch so
      // neighboring lines never collide.
      const float baselineY = 80.0f + static_cast<float>(lineIndex) * 64.0f;
      const float slope = (lineIndex % 2 == 0) ? 0.07f : -0.055f;
      const float inverseLength = 1.0f / std::sqrt(1 + slope * slope);
      flow.lines().push_back({LineInterval{
          {500, baselineY}, {inverseLength, slope * inverseLength}, 350}});
    }
    Paragraph paragraph;
    paragraph.appendText("lines need not be horizontal nor parallel; ",
                         style(18, kInk));
    paragraph.appendText("각 행은 자유로운 방향을 갖고 ",
                         style(18, kBlue, "ko"));
    paragraph.appendText("每一行都有自己的方向 ", style(18, kAccent, "zh"));
    paragraph.appendText("and the paragraph simply flows through whatever pen "
                         "strokes you hand it.",
                         style(18, kInk));
    const auto coldStartTime = Clock::now();
    ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
    const auto coldEndTime = Clock::now();
    ParagraphLayout warm = layoutParagraph(fontContext, paragraph, flow);
    const auto warmEndTime = Clock::now();
    layout.draw(canvas, paragraph);
    std::printf("  line-set layout: cold %.1f us, warm %.1f us\n",
                toMicroseconds(coldEndTime - coldStartTime),
                toMicroseconds(warmEndTime - coldEndTime));
  }

  writePng(surface.get(), outputDirectory / "freeform.png");
  std::printf("\n");
}
