// Scene: ripple pool (click to drop).
#include "SceneRegistry.h"
#include "SceneSupport.h"

#include <include/core/SkPaint.h>

#include <algorithm>
#include <cmath>
#include <random>

using namespace textflow;

namespace gallery {

namespace {

class RippleScene final : public Scene {
public:
  void pointerPress(SkPoint position) override {
    m_pendingDrops.push_back(position);
  }

  FrameStats render(SkCanvas *canvas, SkISize size, double /*elapsedSeconds*/,
                    int frameNumber, const SceneParams & /*params*/,
                    FontContext &fontContext) override {
    if (m_paragraph.text().empty())
      m_paragraph = makeBigParagraph(700, 13.0f);

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    if (frameNumber % 90 == 0)
      m_drops.push_back(
          {{60.0f + static_cast<float>(
                        m_randomEngine() %
                        std::max(1, static_cast<int>(canvasWidth) - 120)),
            60.0f + static_cast<float>(
                        m_randomEngine() %
                        std::max(1, static_cast<int>(canvasHeight) - 120))},
           frameNumber});
    for (const SkPoint &pendingPosition : m_pendingDrops)
      m_drops.push_back({pendingPosition, frameNumber});
    m_pendingDrops.clear();
    std::erase_if(m_drops, [&](const Drop &drop) {
      return frameNumber - drop.birthFrameNumber > 280;
    });

    // Live edit mid-ripple: same-length swap, everything else cache-hot.
    static const char8_t *swaps[] = {u8"letters", u8"glyphs ", u8"symbols",
                                     u8"strokes"};
    if (frameNumber > 0 && frameNumber % 150 == 0) {
      const size_t textOffset = m_paragraph.text().find(u"letters");
      if (textOffset != std::u16string::npos)
        m_paragraph.replaceText(static_cast<uint32_t>(textOffset),
                                static_cast<uint32_t>(textOffset + 7),
                                swaps[(frameNumber / 150) % 4]);
    }

    BlockFlow flow(
        SkRect::MakeXYWH(36, 24, canvasWidth - 72, canvasHeight - 48));
    ParagraphLayoutOptions options;
    options.alignment = TextAlignment::kJustify;
    options.lineMetrics.height = 21;

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, m_paragraph, flow, options);
    const auto layoutEndTime = Clock::now();

    m_batches.clear();
    forEachPlacedGlyph(
        layout, m_paragraph,
        [&](const ShapedWord *shapedWord, SkGlyphID glyph, float glyphAdvance,
            SkColor color, SkPoint restingOrigin) {
          SkVector offset = {0, 0};
          float tilt = 0;
          for (const Drop &drop : m_drops) {
            const SkVector radialVector = restingOrigin - drop.center;
            const float distance = radialVector.length() + 1.0f;
            const float ringRadius =
                6.0f * static_cast<float>(frameNumber - drop.birthFrameNumber);
            const float normalizedRingDistance =
                (distance - ringRadius) / 46.0f;
            if (normalizedRingDistance > 3 || normalizedRingDistance < -3)
              continue;
            const float amplitude =
                42.0f * std::exp(-static_cast<float>(frameNumber -
                                                     drop.birthFrameNumber) /
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
              m_batches.batchForStyle(shapedWord, color);
          batch.glyphs.push_back(glyph);
          batch.transforms.push_back(
              {cosine, sine,
               drawingOrigin.fX - cosine * halfAdvance + halfAdvance,
               drawingOrigin.fY - sine * halfAdvance});
        });

    canvas->clear(kPaper);
    const int drawnGlyphCount = m_batches.draw(canvas);
    return {toMicroseconds(layoutEndTime - layoutStartTime),
            static_cast<int>(layout.runs.size()), drawnGlyphCount};
  }

private:
  struct Drop {
    SkPoint center;
    int birthFrameNumber;
  };
  Paragraph m_paragraph;
  std::vector<Drop> m_drops;
  std::vector<SkPoint> m_pendingDrops;
  GlyphRSXformBatches m_batches;
  std::mt19937 m_randomEngine{31};
};

SceneDescriptor makeRippleDescriptor() {
  SceneDescriptor descriptor;
  descriptor.name = QStringLiteral("Ripple pool — click me");
  descriptor.textEditable = false;
  descriptor.displayOrder = 50;
  descriptor.make = [] { return std::make_unique<RippleScene>(); };
  return descriptor;
}

} // namespace

REGISTER_GALLERY_SCENE(makeRippleDescriptor())

} // namespace gallery
