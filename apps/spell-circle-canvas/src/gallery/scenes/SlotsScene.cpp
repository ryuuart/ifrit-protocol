// Scene: inline placeholders — pills and figures woven into the flow.
#include "SceneRegistry.h"
#include "SceneSupport.h"

#include <include/core/SkFontMetrics.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>

using namespace textflow;

namespace gallery {

namespace {

class SlotsScene final : public Scene {
public:
  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams &params,
                    FontContext &fontContext) override {
    if (!m_serif) {
      m_serif = defaultSerif(fontContext);
      m_sansTypeface = fontContext.fontManager()->matchFamilyStyle(
          "Noto Sans", SkFontStyle());
      if (!m_sansTypeface)
        m_sansTypeface = fontContext.defaultTypeface();
    }
    const float fontSize = params.fontSize;
    m_built.ensure({fontSize}, [&] { build(fontContext, fontSize); });

    // Pulse the first pill — resizing an inline object relayouts the whole
    // paragraph live, and reshapes exactly zero words.
    m_paragraph.setPlaceholder(
        0, {m_pillWidths[0] *
                (1.0f + 0.4f * (0.5f + 0.5f * static_cast<float>(std::sin(
                                                  elapsedSeconds * 1.7)))),
            fontSize * 1.35f, fontSize * 0.3f});

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    BlockFlow flow(SkRect::MakeXYWH(canvasWidth * 0.1f, 44, canvasWidth * 0.8f,
                                    canvasHeight - 90));
    ParagraphLayoutOptions options;
    options.alignment = params.alignment;
    options.lineBreakStrategy = params.lineBreakStrategy;
    options.lineMetrics.height = fontSize * 2.1f;

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, m_paragraph, flow, options);
    const auto layoutEndTime = Clock::now();

    canvas->clear(kPaper);
    layout.drawBatched(canvas, m_paragraph);

    // Draw whatever belongs in each slot: pills for the first four, a
    // little inline "figure" for the last.
    const SkFont pillFont = makeFont(m_sansTypeface, fontSize * 0.68f);
    SkFontMetrics pillMetrics;
    pillFont.getMetrics(&pillMetrics);
    for (const ParagraphLayout::PlacedPlaceholder &placed :
         layout.placeholderRects(m_paragraph)) {
      if (placed.index < static_cast<int>(m_pillTexts.size())) {
        SkPaint backgroundPaint;
        backgroundPaint.setAntiAlias(true);
        backgroundPaint.setColor(placed.index == 0 ? kAccent : kBlue);
        canvas->drawRoundRect(placed.rect, placed.rect.height() * 0.5f,
                              placed.rect.height() * 0.5f, backgroundPaint);
        Paragraph &label = m_pillLabelParagraphs.paragraphFor(
            m_pillTexts[static_cast<size_t>(placed.index)], m_sansTypeface,
            fontSize * 0.68f);
        const float textWidth = label.naturalWidth(fontContext);
        PaintStyle white(SK_ColorWHITE);
        layoutSingleLine(
            fontContext, label,
            {placed.rect.centerX() - textWidth * 0.5f,
             placed.rect.centerY() -
                 (pillMetrics.fAscent + pillMetrics.fDescent) * 0.5f})
            .draw(canvas, label, &white);
      } else {
        SkPaint fill;
        fill.setAntiAlias(true);
        fill.setColor(kShape);
        canvas->drawRoundRect(placed.rect, 6, 6, fill);
        SkPaint line;
        line.setAntiAlias(true);
        line.setStyle(SkPaint::kStroke_Style);
        line.setStrokeWidth(2);
        line.setColor(kAccent);
        // A tiny sparkline so the "figure" reads as content.
        SkPathBuilder spark;
        for (int pointIndex = 0; pointIndex <= 16; ++pointIndex) {
          const float pointX =
              placed.rect.left() + placed.rect.width() * pointIndex / 16.0f;
          const float pointY = placed.rect.centerY() -
                               placed.rect.height() * 0.3f *
                                   std::sin(pointIndex * 0.7f +
                                            static_cast<float>(elapsedSeconds));
          if (pointIndex == 0)
            spark.moveTo(pointX, pointY);
          else
            spark.lineTo(pointX, pointY);
        }
        canvas->drawPath(spark.detach(), line);
      }
    }

    drawCaption(canvas, fontContext,
                u8"appendPlaceholder() weaves fixed-size slots into the flow; "
                "placeholderRects() reports where they landed",
                {canvasWidth * 0.1f, canvasHeight - 30}, canvasWidth * 0.8f);
    return {toMicroseconds(layoutEndTime - layoutStartTime),
            static_cast<int>(layout.runs.size()), 0};
  }

private:
  /// Rebuilds the paragraph and measures every inline pill label.
  void build(FontContext &fontContext, float fontSize) {
    m_pillTexts = {u8"LOW RISK", u8"42 ms", u8"β-channel", u8"cache-hot"};
    m_pillWidths.clear();
    for (const char8_t *text : m_pillTexts) {
      Paragraph &label = m_pillLabelParagraphs.paragraphFor(
          text, m_sansTypeface, fontSize * 0.68f);
      m_pillWidths.push_back(label.naturalWidth(fontContext) + fontSize * 1.1f);
    }

    m_paragraph.clear();
    const TextStyle body = makeStyle(fontSize, kInk, "", m_serif);
    const Placeholder pill{0, fontSize * 1.35f, fontSize * 0.3f};
    auto addPill = [&](size_t pillIndex) {
      Placeholder placeholder = pill;
      placeholder.width = m_pillWidths[pillIndex];
      m_paragraph.appendPlaceholder(placeholder, body);
    };
    m_paragraph.appendText(u8"Status pills ride the baseline like words — ",
                           body);
    addPill(0);
    m_paragraph.appendText(
        u8" — and the flow simply makes room: it wraps them, justifies around "
        "them, and re-places them when they resize. Round-trip latency ",
        body);
    addPill(1);
    m_paragraph.appendText(u8" stays within budget, the experiment on the ",
                           body);
    addPill(2);
    m_paragraph.appendText(u8" keeps converging, and every relayout here is ",
                           body);
    addPill(3);
    m_paragraph.appendText(
        u8". Even a small figure can sit inline without leaving the "
        "paragraph ",
        body);
    // Sized to the line: lines don't grow per-box yet (see README
    // limitations), so a slot taller than the leading would overlap the
    // previous line.
    m_paragraph.appendPlaceholder(
        {fontSize * 7.0f, fontSize * 1.8f, fontSize * 0.3f}, body);
    m_paragraph.appendText(
        u8" because the breaker treats every slot as an unbreakable word — "
        "the object-replacement character anchors it in the text, and the "
        "layout reports the rect where it landed.",
        body);
  }

  Paragraph m_paragraph;
  SingleLineParagraphCache m_pillLabelParagraphs;
  std::vector<const char8_t *> m_pillTexts;
  std::vector<float> m_pillWidths;
  sk_sp<SkTypeface> m_serif;
  sk_sp<SkTypeface> m_sansTypeface;
  kit::RebuildGuard<float> m_built;
};

SceneDescriptor makeSlotsDescriptor() {
  SceneDescriptor descriptor;
  descriptor.name = QStringLiteral("Inline slots & pills");
  descriptor.textEditable = false;
  descriptor.displayOrder = 120;
  descriptor.make = [] { return std::make_unique<SlotsScene>(); };
  return descriptor;
}

} // namespace

REGISTER_GALLERY_SCENE(makeSlotsDescriptor())

} // namespace gallery
