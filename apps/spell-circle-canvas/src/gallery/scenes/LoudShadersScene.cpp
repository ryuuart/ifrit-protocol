// Scene: extreme, "seen from across the room" SkSL shaders as glyph
// foregrounds, alongside the brighter, twinkling sparkle overlay.
#include "SceneRegistry.h"
#include "SceneSupport.h"

#include <textflow/PaintShaders.h>

#include <include/core/SkBlendMode.h>

#include <algorithm>
#include <array>
#include <string>

using namespace textflow;

namespace gallery {

namespace {

QString loudShadersDefaultText() { return QStringLiteral("SHADER"); }

class LoudShadersScene final : public Scene {
public:
  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams &params,
                    FontContext &fontContext) override {
    if (!m_serif)
      m_serif = defaultSerif(fontContext);

    const QString text =
        params.text.isEmpty() ? loudShadersDefaultText() : params.text;
    const sk_sp<SkTypeface> &typeface =
        params.typeface ? params.typeface : m_serif;
    const float fontSize = std::clamp(params.fontSize * 3.4f, 46.0f, 200.0f);
    const bool rebuild = text != m_text || typeface.get() != m_typeface ||
                         fontSize != m_fontSize || size != m_size;

    double layoutMicroseconds = 0;
    if (rebuild) {
      m_text = text;
      m_typeface = typeface.get();
      m_fontSize = fontSize;
      m_size = size;

      m_paints[0] = PaintStyle(SK_ColorWHITE); // starNest shader set per frame
      m_paints[1] = PaintStyle(SK_ColorWHITE); // clouds shader set per frame
      m_paints[2] = PaintStyle(SK_ColorWHITE); // tunnel shader set per frame

      // Screen-blended sparkle over a near-white fill barely moves the
      // result (screen saturates fast against an already-bright base) — a
      // darker fill is what lets each point actually read as brighter.
      m_paints[3] = PaintStyle(0xFF171C2E);
      SkPaint sparkleOverlay;
      sparkleOverlay.setAntiAlias(true);
      sparkleOverlay.setBlendMode(SkBlendMode::kScreen);
      m_paints[3].addOverlay(PaintLayer(std::move(sparkleOverlay)));

      const auto layoutStart = Clock::now();
      const float top = 60.0f;
      const float rowHeight =
          std::max(80.0f, (static_cast<float>(size.height()) - top - 30.0f) /
                              static_cast<float>(m_paragraphs.size()));
      for (size_t row = 0; row < m_paragraphs.size(); ++row) {
        Paragraph &paragraph = m_paragraphs[row];
        paragraph.clear();
        TextStyle textStyle = makeStyle(fontSize, SK_ColorWHITE, "", typeface);
        const std::u16string sample(
            reinterpret_cast<const char16_t *>(text.utf16()),
            static_cast<size_t>(text.size()));
        paragraph.appendText(sample, textStyle);
        m_textLengths[row] = static_cast<uint32_t>(sample.size());
        m_layouts[row] = layoutSingleLine(
            fontContext, paragraph,
            {std::max(220.0f, static_cast<float>(size.width()) * 0.32f),
             top + rowHeight * (static_cast<float>(row) + 0.68f)});
      }
      layoutMicroseconds = toMicroseconds(Clock::now() - layoutStart);
    }

    // Bound to the whole canvas so every row's shader shares one coordinate
    // space, the same pattern the other effect scenes use for their shaders.
    const float canvasWidth = static_cast<float>(size.width());
    const SkRect shaderBounds =
        SkRect::MakeWH(canvasWidth, static_cast<float>(size.height()));
    const float time = static_cast<float>(elapsedSeconds);
    m_paints[0].foreground.setShader(
        PaintShaders::starNest(shaderBounds, time * 0.005));
    m_paints[1].foreground.setShader(PaintShaders::clouds(shaderBounds, time));
    m_paints[2].foreground.setShader(PaintShaders::tunnel(shaderBounds, time));
    m_paints[3].overlays[0].paint.setShader(
        PaintShaders::sparkle(shaderBounds, time));

    for (size_t row = 0; row < m_paragraphs.size(); ++row)
      m_paragraphs[row].setPaint(0, m_textLengths[row], m_paints[row]);

    canvas->clear(0xFF05060B);
    int runCount = 0;
    for (size_t row = 0; row < m_paragraphs.size(); ++row) {
      m_layouts[row].drawBatched(canvas, m_paragraphs[row]);
      runCount += static_cast<int>(m_layouts[row].runs.size());
    }

    const std::array<std::u8string_view, 4> labels = {
        u8"starNest() — volumetric raymarched fractal dust",
        u8"clouds() — layered ridged/fbm value-noise sky",
        u8"tunnel() — raymarched kaleidoscope tunnel",
        u8"sparkle() — brighter, size-varied, twinkling overlay"};
    const float top = 60.0f;
    const float rowHeight =
        std::max(80.0f, (static_cast<float>(size.height()) - top - 30.0f) /
                            static_cast<float>(m_paragraphs.size()));
    for (size_t row = 0; row < labels.size(); ++row)
      drawCaption(canvas, fontContext, labels[row],
                  {24, top + rowHeight * (static_cast<float>(row) + 0.68f) -
                           fontSize * 0.55f},
                  std::max(220.0f, canvasWidth * 0.3f));

    drawCaption(canvas, fontContext,
                u8"Extreme SkSL presets meant to be seen from across the "
                u8"room — heavier raymarch/fbm loops, still cheap per glyph "
                u8"since only covered mask pixels evaluate them.",
                {24, 16}, canvasWidth - 48);
    return {layoutMicroseconds, runCount, 0};
  }

private:
  std::array<Paragraph, 4> m_paragraphs;
  std::array<ParagraphLayout, 4> m_layouts;
  std::array<PaintStyle, 4> m_paints;
  std::array<uint32_t, 4> m_textLengths{};
  QString m_text;
  SkTypeface *m_typeface = nullptr;
  sk_sp<SkTypeface> m_serif;
  float m_fontSize = 0;
  SkISize m_size = {0, 0};
};

SceneDescriptor makeLoudShadersDescriptor() {
  SceneDescriptor descriptor;
  descriptor.name = QStringLiteral("Loud shaders");
  descriptor.defaultText = loudShadersDefaultText();
  descriptor.displayOrder = 100;
  descriptor.make = [] { return std::make_unique<LoudShadersScene>(); };
  return descriptor;
}

} // namespace

REGISTER_GALLERY_SCENE(makeLoudShadersDescriptor())

} // namespace gallery
