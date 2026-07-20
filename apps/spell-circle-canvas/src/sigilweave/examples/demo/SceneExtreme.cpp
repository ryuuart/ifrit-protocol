// Scene D — extreme geometries: zigzag lines, a wandering scribble path,
// high-frequency bumpy baselines, and rotated single-letter confetti.
#include "DemoScenes.h"
#include "DemoSupport.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <concepts>
#include <cstdio>
#include <random>
#include <string>

using namespace sigil::weave;

void sceneExtreme(FontContext &fontContext,
                  const std::filesystem::path &outputDirectory) {
  std::printf("Scene D — extreme geometries (zigzag, scribble, bumps, "
              "confetti)\n");

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1000, 860));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);

  auto measureColdAndWarm =
      [&]<std::invocable Operation>(const char *label, Operation &&operation) {
        const auto coldStartTime = Clock::now();
        operation();
        const auto coldEndTime = Clock::now();
        operation(); // Warm pass.
        const auto warmEndTime = Clock::now();
        std::printf("  %-10s cold %8.1f us, warm %7.1f us\n", label,
                    toMicroseconds(coldEndTime - coldStartTime),
                    toMicroseconds(warmEndTime - coldEndTime));
      };

  // Zigzag: each *line* is a chain of alternating up/down segments; words
  // march through the chain segment by segment.
  {
    Paragraph paragraph;
    paragraph.appendText(
        u8"zigzag lines carry words up and down and up and down while the "
        "layout treats every slanted segment as just another interval — "
        "ジグザグの線の上でも単語は流れ続ける — 지그재그 선 위에서도",
        style(16));
    LineSetFlow flow;
    for (int lineIndex = 0; lineIndex < 3; ++lineIndex) {
      std::vector<LineInterval> segments;
      float segmentStartX = 30;
      float segmentStartY = 70.0f + static_cast<float>(lineIndex) * 90.0f;
      for (int segmentIndex = 0; segmentIndex < 9; ++segmentIndex) {
        const float slope = (segmentIndex % 2 == 0) ? 0.45f : -0.45f;
        const float inverseLength = 1.0f / std::sqrt(1 + slope * slope);
        segments.push_back({LineInterval{{segmentStartX, segmentStartY},
                                         {inverseLength, slope * inverseLength},
                                         52}});
        segmentStartX += 52 * inverseLength;
        segmentStartY += 52 * slope * inverseLength;
      }
      flow.lines().push_back(std::move(segments));
    }
    ParagraphLayout layout;
    measureColdAndWarm("zigzag", [&] {
      layout = layoutParagraph(fontContext, paragraph, flow);
    });
    layout.draw(canvas, paragraph);
  }

  // Scribble: a wandering quadratic path.
  {
    SkPathBuilder pathBuilder;
    pathBuilder.moveTo(60, 420);
    std::mt19937 randomEngine(11);
    float previousX = 60;
    float previousY = 420;
    for (int curveIndex = 0; curveIndex < 9; ++curveIndex) {
      const float controlX =
          previousX + 40 + static_cast<float>(randomEngine() % 90);
      const float controlY = 340 + static_cast<float>(randomEngine() % 220);
      previousX += 80 + static_cast<float>(randomEngine() % 60);
      previousY = 360 + static_cast<float>(randomEngine() % 200);
      pathBuilder.quadTo(controlX, controlY, previousX, previousY);
    }
    Paragraph paragraph;
    paragraph.appendText(
        u8"a scribbled stroke is still a line to this engine, the pen just "
        "wanders wherever the curve goes — 落書きの線でも文字は流れる",
        style(15, kBlue));
    PathFlow flow(pathBuilder.detach());
    ParagraphLayout layout;
    measureColdAndWarm("scribble", [&] {
      layout = layoutParagraph(fontContext, paragraph, flow);
    });
    layout.draw(canvas, paragraph);
  }

  // Extremely bumpy: high-frequency sine paths.
  {
    for (int line = 0; line < 2; ++line) {
      SkPathBuilder pathBuilder;
      const float baseY = 640.0f + static_cast<float>(line) * 80.0f;
      const float amplitude = 14.0f + 10.0f * static_cast<float>(line);
      pathBuilder.moveTo(30, baseY);
      for (int sampleX = 30; sampleX <= 960; sampleX += 6)
        pathBuilder.lineTo(static_cast<float>(sampleX),
                           baseY + amplitude *
                                       std::sin(static_cast<float>(sampleX) *
                                                (0.055f - 0.015f * line)));
      Paragraph paragraph;
      paragraph.appendText(
          line == 0
              ? u8"extremely bumpy baselines shake every glyph yet the words "
                "keep their spacing along the arc length of the wave"
              : u8"波打つベースラインの上でも字形は接線に沿って進む — 파도치는 "
                "기준선 위에서도 글자는 계속 흐른다 — 波浪基线上的文字",
          style(15, line == 0 ? kInk : kAccent));
      PathFlow flow(pathBuilder.detach());
      ParagraphLayout layout;
      measureColdAndWarm(line == 0 ? "bumpy" : "bumpy2", [&] {
        layout = layoutParagraph(fontContext, paragraph, flow);
      });
      layout.draw(canvas, paragraph);
    }
  }

  writePng(surface.get(), outputDirectory / "extreme.png");

  // Confetti: single letters littered across the whole canvas — every
  // "word" is one letter, every line interval a tiny randomly rotated
  // stroke barely long enough for it.
  {
    sk_sp<SkSurface> confetti =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1000, 620));
    confetti->getCanvas()->clear(kPaper);

    std::u8string letters;
    const char8_t *cjk[] = {u8"文", u8"字", u8"術", u8"式", u8"光", u8"影",
                            u8"한", u8"글", u8"빛", u8"円", u8"陣", u8"魔"};
    std::mt19937 randomEngine(5);
    for (int letterIndex = 0; letterIndex < 120; ++letterIndex) {
      if (letterIndex % 3 == 2)
        letters += cjk[randomEngine() % 12];
      else
        letters += static_cast<char8_t>('a' + randomEngine() % 26);
      letters += ' ';
    }
    Paragraph paragraph;
    paragraph.appendText(letters, style(26));
    // Color a few random spans so the confetti is multicolored.
    const uint32_t textLength = static_cast<uint32_t>(paragraph.text().size());
    for (uint32_t textOffset = 0; textOffset + 8 < textLength; textOffset += 8)
      paragraph.setPaint(textOffset, textOffset + 4,
                         PaintStyle{(textOffset / 8) % 3 == 0   ? kAccent
                                    : (textOffset / 8) % 3 == 1 ? kBlue
                                                                : kInk});

    LineSetFlow flow;
    for (int letterIndex = 0; letterIndex < 120; ++letterIndex) {
      const float positionX = 20.0f + static_cast<float>(randomEngine() % 940);
      const float positionY = 30.0f + static_cast<float>(randomEngine() % 560);
      const float angle =
          static_cast<float>(randomEngine() % 628) * 0.01f; // 0..2π
      flow.lines().push_back({LineInterval{
          {positionX, positionY}, {std::cos(angle), std::sin(angle)}, 34}});
    }
    ParagraphLayout layout;
    measureColdAndWarm("confetti", [&] {
      layout = layoutParagraph(fontContext, paragraph, flow);
    });
    layout.draw(confetti->getCanvas(), paragraph);
    writePng(confetti.get(), outputDirectory / "confetti.png");
  }
  std::printf("\n");
}
