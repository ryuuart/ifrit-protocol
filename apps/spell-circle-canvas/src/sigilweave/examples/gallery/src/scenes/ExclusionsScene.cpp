// Scene: exclusions & SkPath shapes.
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcore/cache/Rebuild.h>
#include <sigilmeasure/time/Stopwatch.h>

#include <cmath>

#include "SceneRegistry.h"
#include "SceneSupport.h"

using namespace sigil::weave;

namespace gallery {

namespace {

QString exclusionsDefaultText() {
  return QStringLiteral(
      "Typography is the craft of arranging type, and glyphs flow around "
      "obstacles the way water flows around stones. 日本語のテキストも同じ"
      "流れに乗って進み、한국어 단어들도 자연스럽게 흐르고, 中文字符同样围"
      "绕形状排布。Latin and CJK mix freely because every word is shaped "
      "once, cached, and repositioned with pure arithmetic. The orbiting "
      "circle is a classic exclusion; the pulsing spiky ring and the donut "
      "are arbitrary SkPaths — the ring is a brand-new path every frame as "
      "its points breathe in and out, and both holes stay open to text. "
      "When a shape slides across a line the line splits into fragments "
      "and each fragment justifies itself independently; when it moves on, "
      "the fragments knit back together as if nothing happened. No word is "
      "ever reshaped for any of this, frame after frame after frame. "
      "Everything you read here can be rewritten live from the panel on "
      "the left: retype the body, swap the family, drag the size — the "
      "shape cache absorbs each keystroke and the flow simply re-places "
      "the words. 形が動くたびに行は裂け、また元通りに繋がる。글자는 한 번"
      "만 성형되고 계속 재사용됩니다. The donut's hole is part of the same "
      "even-odd path, so with enough text the lines pour straight through "
      "its centre while avoiding the ring around it.");
}

class ExclusionsScene final : public Scene {
 public:
  FrameStats render(SkCanvas* canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParameters& parameters,
                    FontContext& fontContext) override {
    if (!m_serif) m_serif = defaultSerif(fontContext);
    m_body.ensure(parameters, exclusionsDefaultText(), m_serif);

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    const float fontSize = parameters.fontSize;
    ExclusionFlow flow(
        SkRect::MakeXYWH(28, 24, canvasWidth - 56, canvasHeight - 48));

    const float circleRadius = std::min(canvasWidth, canvasHeight) * 0.13f;
    const SkPoint circleCenter = {
        canvasWidth * 0.32f +
            canvasWidth * 0.2f *
                std::sin(static_cast<float>(elapsedSeconds) * 0.9f),
        canvasHeight * 0.38f +
            canvasHeight * 0.24f *
                std::sin(static_cast<float>(elapsedSeconds) * 0.53f)};
    flow.exclusions().push_back(
        {silhouette::circle(SkRect::MakeXYWH(
             circleCenter.x() - circleRadius, circleCenter.y() - circleRadius,
             2 * circleRadius, 2 * circleRadius)),
         fontSize * 0.5f});

    const SkPath& donutPath = m_donut.ensure({size}, [&] {
      SkPathBuilder donut;
      const float donutRadius = std::min(canvasWidth, canvasHeight) * 0.16f;
      donut.addCircle(canvasWidth * 0.72f, canvasHeight * 0.68f, donutRadius);
      donut.addCircle(canvasWidth * 0.72f, canvasHeight * 0.68f,
                      donutRadius * 0.5f);
      donut.setFillType(SkPathFillType::kEvenOdd);
      return donut.detach();
    });
    // Morphing exclusion: a brand-new spiky path every frame, points
    // pulsing in and out — the flow re-flattens and relayouts live.
    const SkPath spiky =
        spikyRingPath(static_cast<float>(elapsedSeconds),
                      std::min(canvasWidth, canvasHeight) * 0.19f);
    const SkPoint starOffset{
        canvasWidth * 0.6f +
            canvasWidth * 0.18f *
                std::cos(static_cast<float>(elapsedSeconds) * 0.4f),
        canvasHeight * 0.3f +
            canvasHeight * 0.1f *
                std::sin(static_cast<float>(elapsedSeconds) * 0.7f)};
    flow.exclusions().push_back(
        {silhouette::path(spiky), fontSize * 0.4f, starOffset});
    // The donut drifts too: an exclusion's offset is rigid motion, so its
    // silhouette answers from what it already measured and every frame is
    // still a full live relayout around it.
    const SkPoint donutOffset{
        canvasWidth * 0.05f *
            std::sin(static_cast<float>(elapsedSeconds) * 0.6f),
        canvasHeight * 0.06f *
            std::cos(static_cast<float>(elapsedSeconds) * 0.45f)};
    flow.exclusions().push_back(
        {silhouette::path(donutPath), fontSize * 0.4f, donutOffset});
    flow.setMinimumIntervalWidth(fontSize * 3);

    ParagraphLayoutOptions options;
    options.alignment = parameters.alignment;
    options.lineBreakStrategy = parameters.lineBreakStrategy;
    options.lineMetrics.height = fontSize * 1.7f;
    options.knuthPlass.minimumIntervalWidth = fontSize * 3;
    options.overflow.ellipsis =
        u"…";  // paste a novel: the tail is marked, not shaped

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, m_body.paragraph, flow, options);
    const auto layoutEndTime = Clock::now();

    canvas->clear(kPaper);
    SkPaint ghost;
    ghost.setAntiAlias(true);
    ghost.setColor(kShape);
    canvas->drawCircle(circleCenter.x(), circleCenter.y(), circleRadius, ghost);
    canvas->save();
    canvas->translate(starOffset.x(), starOffset.y());
    canvas->drawPath(spiky, ghost);
    canvas->restore();
    canvas->save();
    canvas->translate(donutOffset.x(), donutOffset.y());
    canvas->drawPath(donutPath, ghost);
    canvas->restore();
    layout.drawBatched(canvas, m_body.paragraph);

    return {toMicroseconds(layoutEndTime - layoutStartTime),
            static_cast<int>(layout.runs.size()), 0};
  }

 private:
  BodyCache m_body;
  sk_sp<SkTypeface> m_serif;
  sigil::core::CachedValue<SkPath, SkISize> m_donut;
};

SceneDescriptor makeExclusionsDescriptor() {
  SceneDescriptor descriptor;
  descriptor.name = QStringLiteral("Exclusions & shapes");
  descriptor.defaultText = exclusionsDefaultText();
  descriptor.displayOrder = 10;
  descriptor.make = [] { return std::make_unique<ExclusionsScene>(); };
  return descriptor;
}

}  // namespace

REGISTER_GALLERY_SCENE(makeExclusionsDescriptor)

}  // namespace gallery
