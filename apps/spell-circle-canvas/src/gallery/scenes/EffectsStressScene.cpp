// Scene: a fully placed 2,000-word paragraph with four animated paint passes.
#include "SceneFactories.h"
#include "SceneSupport.h"

#include <textflow/PaintShaders.h>

#include <include/core/SkBlendMode.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

using namespace textflow;

namespace gallery {

namespace {

constexpr int kStressWordCount = 2000;

std::u8string makeStressText() {
  constexpr std::array<std::u8string_view, 24> words = {
      u8"letters", u8"flow",    u8"through", u8"water", u8"under",
      u8"stars",   u8"shaping", u8"stays",   u8"cached", u8"while",
      u8"paint",   u8"moves",   u8"across",  u8"every", u8"glyph",
      u8"文字",    u8"波紋",    u8"星光",     u8"글자",  u8"물결",
      u8"漣漪",    u8"字形",    u8"lumen",   u8"cascade"};
  std::u8string text;
  text.reserve(kStressWordCount * 7);
  for (int wordIndex = 0; wordIndex < kStressWordCount; ++wordIndex) {
    text += words[static_cast<size_t>((wordIndex * 17 + wordIndex / 11) %
                                      words.size())];
    text += ' ';
  }
  return text;
}

class EffectsStressScene final : public Scene {
public:
  QString name() const override {
    return QStringLiteral("2,000-word shader stress");
  }

  bool supportsTextEdit() const override { return false; }

  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams &params,
                    FontContext &fontContext) override {
    if (!m_serif)
      m_serif = defaultSerif(fontContext);
    const sk_sp<SkTypeface> &typeface =
        params.typeface ? params.typeface : m_serif;

    const SkRect textBounds = SkRect::MakeLTRB(
        22.0f, 58.0f, std::max(23.0f, static_cast<float>(size.width()) - 22.0f),
        std::max(59.0f, static_cast<float>(size.height()) - 18.0f));
    const float area = std::max(1.0f, textBounds.width() * textBounds.height());
    const float fitSize = std::sqrt(area / (kStressWordCount * 5.2f));
    const float stressFontSize =
        std::clamp(std::min(params.fontSize * 0.62f, fitSize), 4.5f, 11.0f);
    const bool rebuild = size != m_size || typeface.get() != m_typeface ||
                         stressFontSize != m_fontSize;

    double layoutMicroseconds = 0;
    if (rebuild) {
      m_size = size;
      m_typeface = typeface.get();
      m_fontSize = stressFontSize;
      m_paragraph.clear();
      TextStyle textStyle =
          makeStyle(stressFontSize, SK_ColorWHITE, "", typeface);
      m_paragraph.appendText(makeStressText(), textStyle);
      m_textLength = static_cast<uint32_t>(m_paragraph.text().size());

      m_effect = PaintStyle(SK_ColorWHITE);
      m_effect
          .addUnderlay(PaintLayer::glow(0x882A77FF,
                                        std::max(1.2f, stressFontSize * 0.28f)))
          .addUnderlay(PaintLayer::outline(0xFF061229,
                                           std::max(0.55f,
                                                    stressFontSize * 0.10f)));
      SkPaint stars;
      stars.setAntiAlias(true);
      stars.setBlendMode(SkBlendMode::kScreen);
      m_effect.addOverlay(PaintLayer(std::move(stars)));

      ParagraphLayoutOptions options;
      options.alignment = TextAlignment::kJustify;
      options.lineBreakStrategy = LineBreakStrategy::kGreedy;
      options.lineMetrics.height = stressFontSize * 1.22f;
      BlockFlow flow(textBounds);
      const auto layoutStart = Clock::now();
      m_layout = layoutParagraph(fontContext, m_paragraph, flow, options);
      layoutMicroseconds = toMicroseconds(Clock::now() - layoutStart);
      m_glyphCount = 0;
      for (const PositionedRun &run : m_layout.runs)
        if (run.shaped)
          m_glyphCount += static_cast<int>(run.shaped->glyphs.size());
    }

    const float time = static_cast<float>(elapsedSeconds);
    m_effect.foreground.setShader(
        PaintShaders::meshGradient(textBounds, time));
    m_effect.overlays[0].paint.setShader(
        PaintShaders::starField(textBounds, time));
    m_paragraph.setPaint(0, m_textLength, m_effect);

    canvas->clear(0xFF050A18);
    m_layout.drawBatched(canvas, m_paragraph);

    Paragraph caption;
    TextStyle captionStyle = makeStyle(12.0f, 0xFFD7E7FF);
    caption.appendText(
        m_layout.overflowed()
            ? u8"2,000 words · overflowed (resize taller)"
            : u8"2,000 words · all placed · 4 passes · mesh + stars · no relayout",
        captionStyle);
    layoutSingleLine(fontContext, caption, {22, 30}).draw(canvas, caption);

    return {layoutMicroseconds, static_cast<int>(m_layout.runs.size()),
            m_glyphCount};
  }

private:
  Paragraph m_paragraph;
  ParagraphLayout m_layout;
  PaintStyle m_effect;
  sk_sp<SkTypeface> m_serif;
  SkTypeface *m_typeface = nullptr;
  SkISize m_size = {0, 0};
  float m_fontSize = 0;
  uint32_t m_textLength = 0;
  int m_glyphCount = 0;
};

} // namespace

std::unique_ptr<Scene> makeEffectsStressScene() {
  return std::make_unique<EffectsStressScene>();
}

} // namespace gallery
