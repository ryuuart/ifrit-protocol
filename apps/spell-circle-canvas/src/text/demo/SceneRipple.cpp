// Scene F(b) — a pool of text, at scale: drops land on a 1000-word
// paragraph and the expanding rings push each letter outward off its line as
// the front passes. The paragraph is re-laid-out every frame, and every
// ~150 frames a word is edited mid-ripple (cache-hot) to prove layout stays
// live.
#include "DemoScenes.h"
#include "DemoSupport.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <cstdio>
#include <random>
#include <string>

using namespace textflow;

void sceneRipple(FontContext &fontContext, int frames,
                 const std::filesystem::path &outputDirectory) {
  Paragraph paragraph = makeBigParagraph(1000, 13.0f);

  BlockFlow flow(SkRect::MakeXYWH(60, 50, 1080, 860));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 21.0f;

  {
    ParagraphLayout probe =
        layoutParagraph(fontContext, paragraph, flow, options);
    size_t glyphCount = 0;
    forEachPlacedGlyph(probe, paragraph, [&](auto...) { glyphCount++; });
    std::printf("  ripple: %zu letters from %zu words\n", glyphCount,
                paragraph.words().size());
  }

  struct Drop {
    SkPoint center;
    int birthFrameNumber;
  };
  std::vector<Drop> drops;
  std::mt19937 randomEngine(31);
  const float ringSpeed = 6.0f;

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1200, 960));
  SkCanvas *canvas = surface->getCanvas();
  GlyphRSXformBatches glyphBatches;
  TimingStats layoutTime, waveTime, drawTime;
  const char8_t *swaps[] = {u8"letters", u8"glyphs ", u8"symbols", u8"strokes"};

  for (int frame = 0; frame < frames; ++frame) {
    if (frame % 55 == 0)
      drops.push_back({{140.0f + static_cast<float>(randomEngine() % 920),
                        140.0f + static_cast<float>(randomEngine() % 680)},
                       frame});
    std::erase_if(drops, [&](const Drop &drop) {
      return frame - drop.birthFrameNumber > 280;
    });

    // Live edit mid-ripple: same-length swap, everything else cache-hot.
    if (frame > 0 && frame % 150 == 0) {
      const size_t textOffset = paragraph.text().find(u"letters");
      if (textOffset != std::u16string::npos)
        paragraph.replaceText(static_cast<uint32_t>(textOffset),
                              static_cast<uint32_t>(textOffset + 7),
                              swaps[(frame / 150) % 4]);
    }

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    const auto layoutEndTime = Clock::now();

    glyphBatches.clear();
    forEachPlacedGlyph(
        layout, paragraph,
        [&](const ShapedWord *shapedWord, SkGlyphID glyph, float glyphAdvance,
            SkColor color, SkPoint restingOrigin) {
          SkVector offset = {0, 0};
          float tilt = 0;
          for (const Drop &drop : drops) {
            const SkVector radialVector = restingOrigin - drop.center;
            const float distance = radialVector.length() + 1.0f;
            const float ringRadius =
                ringSpeed * static_cast<float>(frame - drop.birthFrameNumber);
            const float normalizedRingDistance =
                (distance - ringRadius) / 46.0f;
            if (normalizedRingDistance > 3 || normalizedRingDistance < -3)
              continue; // outside the ring front
            const float amplitude =
                42.0f *
                std::exp(-static_cast<float>(frame - drop.birthFrameNumber) /
                         150.0f);
            const float pulse = amplitude * std::exp(-normalizedRingDistance *
                                                     normalizedRingDistance);
            offset += radialVector * (pulse / distance);
            tilt +=
                pulse * 0.012f * (normalizedRingDistance < 0 ? -1.0f : 1.0f);
          }
          float cosine;
          float sine;
          quantizeAngle(tilt, cosine, sine);
          const SkPoint drawingOrigin = restingOrigin + offset;
          const float halfAdvance = glyphAdvance * 0.5f;
          GlyphRSXformBatches::Batch &batch =
              glyphBatches.batchForStyle(shapedWord, color);
          batch.glyphs.push_back(glyph);
          batch.transforms.push_back(
              {cosine, sine,
               drawingOrigin.fX - cosine * halfAdvance + halfAdvance,
               drawingOrigin.fY - sine * halfAdvance});
        });
    const auto waveEndTime = Clock::now();

    canvas->clear(kPaper);
    glyphBatches.draw(canvas);
    const auto drawEndTime = Clock::now();

    layoutTime.add(toMicroseconds(layoutEndTime - layoutStartTime));
    waveTime.add(toMicroseconds(waveEndTime - layoutEndTime));
    drawTime.add(toMicroseconds(drawEndTime - waveEndTime));

    if (frame == 85 || frame == 200 || frame == frames - 1) {
      char name[64];
      std::snprintf(name, sizeof name, "ripple_f%03d.png", frame);
      writePng(surface.get(), outputDirectory / name);
    }
  }
  layoutTime.report("ripple relayout (1000 words)");
  waveTime.report("ripple wave math (per letter)");
  drawTime.report("ripple draw (CPU raster)");
}
