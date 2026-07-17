// Scene: infinite loop marquee on a closed figure-eight.
#include "SceneRegistry.h"
#include "SceneSupport.h"

#include <include/core/SkContourMeasure.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <numbers>

using namespace textflow;

namespace gallery {

namespace {

QString loopDefaultText() {
  return QStringLiteral("and the words go round 終わらない文字の環 끝나지 "
                        "않는 글의 고리 文字環繞不息 round and round again "
                        "— ")
      .repeated(4);
}

class LoopScene final : public Scene {
public:
  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams &params,
                    FontContext &fontContext) override {
    if (!m_serif)
      m_serif = defaultSerif(fontContext);
    m_body.ensure(params, loopDefaultText(), m_serif);

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    if (m_size != size) {
      m_size = size;
      SkPathBuilder pathBuilder;
      const SkPoint center = {canvasWidth * 0.5f, canvasHeight * 0.5f};
      const int steps = 400;
      for (int stepIndex = 0; stepIndex <= steps; ++stepIndex) {
        const float angle = static_cast<float>(stepIndex) / steps * 2.0f *
                            std::numbers::pi_v<float>;
        const SkPoint point = {
            center.x() + canvasWidth * 0.4f * std::sin(angle),
            center.y() +
                canvasHeight * 0.78f * std::sin(angle) * std::cos(angle)};
        if (stepIndex == 0)
          pathBuilder.moveTo(point);
        else
          pathBuilder.lineTo(point);
      }
      pathBuilder.close();
      m_eight = pathBuilder.detach();
      SkContourMeasureIter contourIterator(m_eight, false);
      m_contour = contourIterator.next();
    }
    if (!m_contour)
      return {};
    const float loopLength = m_contour->length();

    LineSetFlow flow;
    LineInterval interval;
    interval.contour = m_contour;
    interval.length = loopLength;
    interval.contourStart =
        std::fmod(static_cast<float>(elapsedSeconds) * 110.0f, loopLength);
    flow.lines().push_back({interval});

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, m_body.paragraph, flow);
    const auto layoutEndTime = Clock::now();

    canvas->clear(kPaper);
    SkPaint rail;
    rail.setAntiAlias(true);
    rail.setStyle(SkPaint::kStroke_Style);
    rail.setStrokeWidth(1.2f);
    rail.setColor(0x2A23252B);
    canvas->drawPath(m_eight, rail);
    layout.draw(canvas, m_body.paragraph);
    return {toMicroseconds(layoutEndTime - layoutStartTime),
            static_cast<int>(layout.runs.size()), 0};
  }

private:
  BodyCache m_body;
  sk_sp<SkTypeface> m_serif;
  SkPath m_eight;
  sk_sp<SkContourMeasure> m_contour;
  SkISize m_size = {0, 0};
};

SceneDescriptor makeLoopDescriptor() {
  SceneDescriptor descriptor;
  descriptor.name = QStringLiteral("Infinite loop");
  descriptor.defaultText = loopDefaultText();
  descriptor.displayOrder = 30;
  descriptor.make = [] { return std::make_unique<LoopScene>(); };
  return descriptor;
}

} // namespace

REGISTER_GALLERY_SCENE(makeLoopDescriptor())

} // namespace gallery
