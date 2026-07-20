// Scene: overflow & ellipsis — CSS text-overflow semantics.
#include "SceneRegistry.h"
#include "SceneSupport.h"

#include <sigilweaveqt/SigilWeaveQt.h>

#include <include/core/SkPaint.h>

#include <cmath>

using namespace sigil::weave;

namespace gallery {

namespace {

QString overflowDefaultText() {
  return QStringLiteral(
      "The box below breathes between comfortably containing this "
      "paragraph and cutting it well short. The left pane simply stops "
      "wherever the last word happens to fit; the right pane trims its "
      "final line until a shaped ellipsis lands cleanly at the edge — "
      "CSS text-overflow semantics, except the marker is a real glyph run "
      "instead of a string glued onto the end. Nothing here reshapes: the "
      "same cached words are just placed into a shorter or taller measure "
      "as the box resizes, frame after frame.");
}

class OverflowScene final : public Scene {
public:
  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams &params,
                    FontContext &fontContext) override {
    if (!m_serif)
      m_serif = defaultSerif(fontContext);
    m_body.ensure(params, overflowDefaultText(), m_serif);

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    const float fontSize = params.fontSize;
    const float breathe =
        0.5f + 0.5f * static_cast<float>(std::sin(elapsedSeconds * 0.4));
    const float maximumBoxHeight =
        std::max(canvasHeight - 140.0f, fontSize * 3.0f);
    const float boxHeight =
        fontSize * 2.4f +
        std::max(0.0f, maximumBoxHeight - fontSize * 2.4f) * breathe;
    const float paneWidth = canvasWidth * 0.4f;

    canvas->clear(kPaper);
    drawCaption(canvas, fontContext, u8"clipped — no ellipsis",
                {canvasWidth * 0.06f, 18});
    drawCaption(canvas, fontContext, u8"options.overflow.ellipsis = \"…\"",
                {canvasWidth * 0.54f, 18});

    double layoutMicroseconds = 0;
    int runCount = 0;
    QString status[2];
    for (int pass = 0; pass < 2; ++pass) {
      const float left = pass == 0 ? canvasWidth * 0.06f : canvasWidth * 0.54f;
      const SkRect box = SkRect::MakeXYWH(left, 48, paneWidth, boxHeight);

      BlockFlow flow(box);
      ParagraphLayoutOptions options;
      options.alignment = params.alignment;
      options.lineBreakStrategy = params.lineBreakStrategy;
      options.lineMetrics.height = fontSize * 1.5f;
      if (pass == 1)
        options.overflow.ellipsis = u"…";
      // 0 = unlimited; anything else exercises OverflowOptions::maxLines
      // (the CSS line-clamp path) on top of the breathing geometry.
      options.overflow.maxLines =
          params.intValue(QStringLiteral("maxLines"), 0);

      const auto layoutStartTime = Clock::now();
      ParagraphLayout layout =
          layoutParagraph(fontContext, m_body.paragraph, flow, options);
      layoutMicroseconds += toMicroseconds(Clock::now() - layoutStartTime);
      runCount += static_cast<int>(layout.runs.size());

      SkPaint border;
      border.setAntiAlias(true);
      border.setStyle(SkPaint::kStroke_Style);
      border.setStrokeWidth(1.2f);
      border.setColor(layout.overflowed() ? kAccent : 0x3323252B);
      canvas->drawRect(box, border);
      layout.drawBatched(canvas, m_body.paragraph);

      if (layout.overflowed()) {
        const int unplacedWordCount = static_cast<int>(
            m_body.paragraph.words().size() - layout.firstUnplacedWord);
        const QString word = unplacedWordCount == 1 ? QStringLiteral("word")
                                                    : QStringLiteral("words");
        status[pass] = QStringLiteral("overflowed — %1 %2 cut%3")
                           .arg(unplacedWordCount)
                           .arg(word)
                           .arg(pass == 1 && layout.ellipsized
                                    ? QStringLiteral(", ellipsis drawn")
                                    : QString());
      } else {
        status[pass] = QStringLiteral("fits — nothing cut");
      }
    }

    drawCaption(canvas, fontContext, sigil::weave::qt::toU16(status[0]),
                {canvasWidth * 0.06f, 48 + boxHeight + 22}, paneWidth);
    drawCaption(canvas, fontContext, sigil::weave::qt::toU16(status[1]),
                {canvasWidth * 0.54f, 48 + boxHeight + 22}, paneWidth);

    return {layoutMicroseconds, runCount, 0};
  }

private:
  BodyCache m_body;
  sk_sp<SkTypeface> m_serif;
};

SceneDescriptor makeOverflowDescriptor() {
  SceneDescriptor descriptor;
  descriptor.name = QStringLiteral("Overflow & ellipsis");
  descriptor.defaultText = overflowDefaultText();
  descriptor.displayOrder = 130;
  descriptor.parameters = {
      {QStringLiteral("maxLines"), QStringLiteral("Max lines (0 = off)"),
       SceneParameter::Type::kInt, 0, 0, 12},
  };
  descriptor.make = [] { return std::make_unique<OverflowScene>(); };
  return descriptor;
}

} // namespace

REGISTER_GALLERY_SCENE(makeOverflowDescriptor())

} // namespace gallery
