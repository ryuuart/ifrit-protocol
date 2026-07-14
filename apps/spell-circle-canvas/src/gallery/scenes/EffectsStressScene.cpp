// Scene: a fully placed 2,000-word paragraph with four animated paint passes.
#include "SceneFactories.h"
#include "SceneSupport.h"

#include <textflow/PaintShaders.h>
#include <textflowqt/TextFlowQt.h>

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
      u8"letters", u8"flow",  u8"through", u8"water", u8"under", u8"stars",
      u8"shaping", u8"stays", u8"cached",  u8"while", u8"paint", u8"moves",
      u8"across",  u8"every", u8"glyph",   u8"文字",  u8"波紋",  u8"星光",
      u8"글자",    u8"물결",  u8"漣漪",    u8"字形",  u8"lumen", u8"cascade"};
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
  bool supportsEffectToggles() const override { return true; }

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
    // The panel's font size drives this directly rather than auto-fitting to
    // the box, so the slider stays meaningful; large sizes simply overflow.
    const float stressFontSize =
        std::clamp(params.fontSize * 0.6f, 4.0f, 40.0f);
    // Reshaping/relayout only depends on text, typeface, and size; the paint
    // stack (toggles, glow spread/intensity) is far cheaper to rebuild, so it
    // gets its own dirty flag and never forces a relayout of 2,000 words.
    const bool layoutDirty = size != m_size || typeface.get() != m_typeface ||
                             stressFontSize != m_fontSize;
    const bool effectDirty = layoutDirty || params.effectGlow != m_effectGlow ||
                             params.effectOutline != m_effectOutline ||
                             params.effectStars != m_effectStars ||
                             params.glowSpread != m_glowSpread ||
                             params.glowIntensity != m_glowIntensity;

    double layoutMicroseconds = 0;
    if (layoutDirty) {
      m_size = size;
      m_typeface = typeface.get();
      m_fontSize = stressFontSize;
      m_paragraph.clear();
      TextStyle textStyle =
          makeStyle(stressFontSize, SK_ColorWHITE, "", typeface);
      m_paragraph.appendText(makeStressText(), textStyle);
      m_textLength = static_cast<uint32_t>(m_paragraph.text().size());

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

    if (effectDirty) {
      m_effectGlow = params.effectGlow;
      m_effectOutline = params.effectOutline;
      m_effectStars = params.effectStars;
      m_glowSpread = params.glowSpread;
      m_glowIntensity = params.glowIntensity;

      m_effect = PaintStyle(SK_ColorWHITE);
      if (m_effectGlow) {
        // Dilating the glyph shape before the blur flattens its falloff into
        // a wider plateau rather than just a softer edge, so even a couple
        // of pixels of spread reads as solid at this scene's small sizes and
        // bridges neighboring glyphs and lines. Scale the cap tightly with
        // the font so the slider stays gentle there and only opens up once
        // the text is large enough to take it.
        const float glowSpread = std::min(m_glowSpread, stressFontSize * 0.06f);
        m_effect.addUnderlay(
            PaintLayer::glow(0x882A77FF, std::max(1.2f, stressFontSize * 0.28f),
                             glowSpread, m_glowIntensity));
      }
      if (m_effectOutline)
        m_effect.addUnderlay(
            PaintLayer::outline(SkColors::kBlue.toSkColor(),
                                std::max(0.55f, stressFontSize * 0.03f)));
      if (m_effectStars) {
        SkPaint stars;
        stars.setAntiAlias(true);
        stars.setBlendMode(SkBlendMode::kScreen);
        m_effect.addOverlay(PaintLayer(std::move(stars)));
      }
    }

    const float time = static_cast<float>(elapsedSeconds);
    if (params.effectShader)
      m_effect.foreground.setShader(
          PaintShaders::meshGradient(textBounds, time));
    else
      m_effect.foreground.setShader(nullptr);
    if (m_effectStars && !m_effect.overlays.empty())
      m_effect.overlays[0].paint.setShader(
          PaintShaders::sparkle(textBounds, time));
    m_paragraph.setPaint(0, m_textLength, m_effect);

    canvas->clear(0xFF050A18);
    m_layout.drawBatched(canvas, m_paragraph);

    const int passCount = 1 + static_cast<int>(m_effectGlow) +
                          static_cast<int>(m_effectOutline) +
                          static_cast<int>(m_effectStars);
    const QString captionText =
        m_layout.overflowed()
            ? QStringLiteral("2,000 words · overflowed (lower the font size "
                             "to fit)")
            : QStringLiteral("2,000 words · all placed · %1 %2 · no relayout")
                  .arg(passCount)
                  .arg(passCount == 1 ? QStringLiteral("pass")
                                      : QStringLiteral("passes"));
    Paragraph caption;
    TextStyle captionStyle = makeStyle(12.0f, 0xFFD7E7FF);
    textflowqt::appendText(caption, captionText, captionStyle);
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
  bool m_effectGlow = true;
  bool m_effectOutline = true;
  bool m_effectStars = true;
  float m_glowSpread = 0;
  float m_glowIntensity = 1;
};

} // namespace

std::unique_ptr<Scene> makeEffectsStressScene() {
  return std::make_unique<EffectsStressScene>();
}

} // namespace gallery
