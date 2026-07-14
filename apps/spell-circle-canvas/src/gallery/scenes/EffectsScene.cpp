// Scene: ordered SkPaint glyph layers, presets, and animated custom paints.
#include "SceneFactories.h"
#include "SceneSupport.h"

#include <textflow/PaintShaders.h>

#include <include/core/SkBlendMode.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

using namespace textflow;

namespace gallery {

namespace {

class EffectsScene final : public Scene {
public:
  QString name() const override { return QStringLiteral("Paint layers"); }

  QString defaultText() const override {
    return QStringLiteral("Layered glyphs");
  }

  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams &params,
                    FontContext &fontContext) override {
    if (!m_serif)
      m_serif = defaultSerif(fontContext);

    const QString text = params.text.isEmpty() ? defaultText() : params.text;
    const sk_sp<SkTypeface> &typeface =
        params.typeface ? params.typeface : m_serif;
    const float fontSize = std::clamp(params.fontSize * 2.5f, 30.0f, 68.0f);
    const bool rebuild = text != m_text || typeface.get() != m_typeface ||
                         fontSize != m_fontSize || size != m_size;

    double layoutMicroseconds = 0;
    if (rebuild) {
      m_text = text;
      m_typeface = typeface.get();
      m_fontSize = fontSize;
      m_size = size;
      buildPaints(fontSize);

      const auto layoutStart = Clock::now();
      const float top = 72.0f;
      const float rowHeight =
          std::max(72.0f, (static_cast<float>(size.height()) - top - 38.0f) /
                              static_cast<float>(m_paragraphs.size()));
      for (size_t row = 0; row < m_paragraphs.size(); ++row) {
        Paragraph &paragraph = m_paragraphs[row];
        paragraph.clear();
        TextStyle textStyle = makeStyle(fontSize, kInk, "", typeface);
        const std::u16string sample(
            reinterpret_cast<const char16_t *>(text.utf16()),
            static_cast<size_t>(text.size()));
        paragraph.appendText(sample, textStyle);
        m_textLengths[row] = static_cast<uint32_t>(sample.size());
        m_layouts[row] = layoutSingleLine(
            fontContext, paragraph,
            {std::max(230.0f, static_cast<float>(size.width()) * 0.28f),
             top + rowHeight * (static_cast<float>(row) + 0.67f)});
      }
      layoutMicroseconds = toMicroseconds(Clock::now() - layoutStart);
    }

    // Runtime effects compile once inside PaintShaders. These calls only make
    // new uniform blocks/shader instances; replacing them is paint-only and
    // the existing shapes and layouts remain valid.
    const float canvasWidth = static_cast<float>(size.width());
    const SkRect shaderBounds = SkRect::MakeWH(
        canvasWidth, static_cast<float>(size.height()));
    const float time = static_cast<float>(elapsedSeconds);
    m_paints[3].foreground.setShader(
        PaintShaders::water(shaderBounds, time));
    m_paints[4].foreground.setShader(
        PaintShaders::meshGradient(shaderBounds, time));
    m_paints[4].overlays[0].paint.setShader(
        PaintShaders::starField(shaderBounds, time));

    // A separate translucent gradient is composited above the stars as a
    // traveling sheen, demonstrating that runtime and stock shaders layer in
    // exactly the same PaintStyle stack.
    const float phase =
        static_cast<float>(std::fmod(elapsedSeconds * 0.22, 1.0));
    const SkPoint gradientPoints[2] = {{canvasWidth * (phase - 0.35f), 0},
                                       {canvasWidth * (phase + 0.35f), 0}};
    const SkColor4f gradientColors[3] = {
        SkColor4f::FromColor(0x001A66FF),
        SkColor4f::FromColor(0xAAFFFFFF),
        SkColor4f::FromColor(0x00FFFFFF)};
    sk_sp<SkShader> animatedGradient = SkShaders::LinearGradient(
        gradientPoints,
        SkGradient(SkGradient::Colors({gradientColors, 3}, SkTileMode::kMirror),
                   SkGradient::Interpolation()));
    m_paints[4].overlays[1].paint.setShader(std::move(animatedGradient));

    for (size_t row = 0; row < m_paragraphs.size(); ++row)
      m_paragraphs[row].setPaint(0, m_textLengths[row], m_paints[row]);

    canvas->clear(kPaper);
    int runCount = 0;
    for (size_t row = 0; row < m_paragraphs.size(); ++row) {
      m_layouts[row].drawBatched(canvas, m_paragraphs[row]);
      runCount += static_cast<int>(m_layouts[row].runs.size());
    }

    const std::array<std::u8string_view, 5> labels = {
        u8"1 draw: foreground SkPaint",
        u8"2 draws: dropShadow preset + foreground",
        u8"2 draws: glow preset + foreground",
        u8"2 draws: outline + animated water shader",
        u8"6 draws: mesh + stars + gradient composite"};
    const float top = 72.0f;
    const float rowHeight =
        std::max(72.0f, (static_cast<float>(size.height()) - top - 38.0f) /
                            static_cast<float>(m_paragraphs.size()));
    for (size_t row = 0; row < labels.size(); ++row)
      drawCaption(canvas, fontContext, labels[row],
                  {24, top + rowHeight * (static_cast<float>(row) + 0.67f) -
                           fontSize * 0.45f},
                  std::max(180.0f, canvasWidth * 0.23f));

    drawCaption(canvas, fontContext,
                u8"Underlays → foreground → overlays. Water, mesh, stars, and "
                u8"the gradient animate by replacing paint, never shaping.",
                {24, 18}, canvasWidth - 48);
    return {layoutMicroseconds, runCount, 0};
  }

private:
  void buildPaints(float fontSize) {
    m_paints[0] = PaintStyle(kInk);

    m_paints[1] = PaintStyle(kInk);
    m_paints[1].addUnderlay(PaintLayer::dropShadow(0x77000000, {4, 5}, 3.2f));

    m_paints[2] = PaintStyle(kBlue);
    m_paints[2].addUnderlay(PaintLayer::glow(0x8892C7FF, 6.0f));

    m_paints[3] = PaintStyle(SK_ColorWHITE); // water shader set per frame
    m_paints[3].addUnderlay(PaintLayer::outline(0xFF5A1E17, fontSize * 0.11f));

    m_paints[4] = PaintStyle(SK_ColorWHITE); // mesh shader set per frame
    m_paints[4]
        .addUnderlay(PaintLayer::dropShadow(0x77000000, {5, 6}, 4.0f))
        .addUnderlay(PaintLayer::glow(0x6692C7FF, 7.0f))
        .addUnderlay(PaintLayer::outline(kInk, fontSize * 0.14f));

    SkPaint stars;
    stars.setAntiAlias(true);
    stars.setBlendMode(SkBlendMode::kScreen);
    m_paints[4].addOverlay(PaintLayer(std::move(stars)));

    SkPaint sheen;
    sheen.setAntiAlias(true);
    sheen.setBlendMode(SkBlendMode::kScreen);
    m_paints[4].addOverlay(PaintLayer(std::move(sheen)));
  }

  std::array<Paragraph, 5> m_paragraphs;
  std::array<ParagraphLayout, 5> m_layouts;
  std::array<PaintStyle, 5> m_paints;
  std::array<uint32_t, 5> m_textLengths{};
  QString m_text;
  SkTypeface *m_typeface = nullptr;
  sk_sp<SkTypeface> m_serif;
  float m_fontSize = 0;
  SkISize m_size = {0, 0};
};

} // namespace

std::unique_ptr<Scene> makeEffectsScene() {
  return std::make_unique<EffectsScene>();
}

} // namespace gallery
