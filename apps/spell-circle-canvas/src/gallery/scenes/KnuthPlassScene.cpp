// Scene: Knuth-Plass vs greedy, hyphenation, last-line modes.
#include "SceneRegistry.h"
#include "SceneSupport.h"

#include <include/core/SkPaint.h>

#include <array>
#include <cmath>

using namespace textflow;

namespace gallery {

namespace {

// Soft hyphens (U+00AD) give both breakers discretionary break points.
QString knuthPlassDefaultText() {
  return QString::fromUtf8(
      "The para­graph breaker con­sid­ers every way to "
      "break this text into lines and picks the one with the least "
      "bad­ness, ex­act­ly like TeX. Greedy breaking "
      "com­mits line by line and leaves rag­ged, "
      "in­con­sis­tent spac­ing be­hind; "
      "op­ti­mal breaking spreads the slack across the whole "
      "para­graph in­stead, tak­ing dis­cre­tion"
      "­ary hy­phens when they pay for them­selves. The "
      "measure breathes to pose a fresh prob­lem every frame.");
}

class KnuthPlassScene final : public Scene {
public:
  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams &params,
                    FontContext &fontContext) override {
    if (!m_serif)
      m_serif = defaultSerif(fontContext);
    const bool rebuilt = m_body.ensure(params, knuthPlassDefaultText(), m_serif);

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    const float fontSize = params.fontSize;
    // Whole-pixel measure: the breathing sine moves well under a pixel per
    // frame, so most frames pose the *same* problem as the last — quantize
    // and reuse the cached layouts instead of re-breaking 2× per frame.
    // Sub-pixel measure changes are invisible anyway.
    const float measure = std::round(
        canvasWidth * 0.36f *
        (1.0f + 0.10f * std::sin(static_cast<float>(elapsedSeconds) * 0.5f)));

    double layoutMicroseconds = 0;
    if (rebuilt || m_body.paragraph.needsShaping() ||
        measure != m_lastMeasure || size != m_lastSize ||
        params.alignment != m_lastAlignment) {
      m_lastMeasure = measure;
      m_lastSize = size;
      m_lastAlignment = params.alignment;

      ParagraphLayoutOptions options;
      options.alignment = params.alignment;
      options.lineMetrics.height = fontSize * 1.6f;
      options.hyphenation.enabled = true;
      options.knuthPlass.minimumIntervalWidth = fontSize * 3;
      options.overflow.ellipsis = u"…";

      const auto layoutStartTime = Clock::now();
      for (int pass = 0; pass < 2; ++pass) {
        options.lineBreakStrategy = pass == 0 ? LineBreakStrategy::kGreedy
                                              : LineBreakStrategy::kKnuthPlass;
        const float left =
            pass == 0 ? canvasWidth * 0.07f : canvasWidth * 0.55f;
        BlockFlow flow(SkRect::MakeXYWH(left, 48, measure, canvasHeight - 80));
        m_layouts[pass] =
            layoutParagraph(fontContext, m_body.paragraph, flow, options);
      }
      layoutMicroseconds = toMicroseconds(Clock::now() - layoutStartTime);
    }

    canvas->clear(kPaper);
    drawCaption(canvas, fontContext, u8"greedy", {canvasWidth * 0.07f, 18});
    drawCaption(canvas, fontContext, u8"knuth-plass (optimal, TeX badness)",
                {canvasWidth * 0.55f, 18});

    int runCount = 0;
    for (int pass = 0; pass < 2; ++pass) {
      const float left = pass == 0 ? canvasWidth * 0.07f : canvasWidth * 0.55f;
      runCount += static_cast<int>(m_layouts[pass].runs.size());
      SkPaint rule;
      rule.setAntiAlias(true);
      rule.setStyle(SkPaint::kStroke_Style);
      rule.setStrokeWidth(1);
      rule.setColor(0x3323252B);
      canvas->drawLine(left + measure, 44, left + measure, canvasHeight - 44,
                       rule);
      m_layouts[pass].drawBatched(canvas, m_body.paragraph);
    }
    return {layoutMicroseconds, runCount, 0};
  }

private:
  BodyCache m_body;
  sk_sp<SkTypeface> m_serif;
  std::array<ParagraphLayout, 2> m_layouts;
  float m_lastMeasure = -1;
  SkISize m_lastSize = {0, 0};
  TextAlignment m_lastAlignment = TextAlignment::kStart;
};

SceneDescriptor makeKnuthPlassDescriptor() {
  SceneDescriptor descriptor;
  descriptor.name = QStringLiteral("Knuth–Plass & hyphens");
  descriptor.defaultText = knuthPlassDefaultText();
  descriptor.displayOrder = 20;
  descriptor.make = [] { return std::make_unique<KnuthPlassScene>(); };
  return descriptor;
}

} // namespace

REGISTER_GALLERY_SCENE(makeKnuthPlassDescriptor())

} // namespace gallery
