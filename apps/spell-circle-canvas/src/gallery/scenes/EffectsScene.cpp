// Scene: ordered SkPaint glyph layers, presets, and animated custom paints.
#include "SceneRegistry.h"
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

// One clause per row style above: setPaint() applies a different PaintStyle
// to each clause's own [start, end) range within a single Paragraph, so the
// five presets cover five word-range spans of one running paragraph rather
// than each needing its own paragraph.
constexpr std::array<std::u8string_view, 5> kParagraphClauses = {
    u8"A plain foreground SkPaint is the default: one draw per glyph, "
    u8"nothing else. ",
    u8"This clause adds a dropShadow preset underlay, so every glyph also "
    u8"gets a soft offset copy drawn behind it. ",
    u8"Here a glow preset blurs a zero-offset copy into a halo that sits "
    u8"behind each letter. ",
    u8"This span pairs an outline underlay with a water shader that "
    u8"repaints the foreground's color every frame. ",
    u8"The final span layers dropShadow, glow, and outline beneath a "
    u8"mesh-gradient foreground, then stars and a traveling sheen on top "
    u8"— six draws, one paragraph, five separate setPaint ranges."};

QString effectsDefaultText() { return QStringLiteral("Layered glyphs"); }

class EffectsScene final : public Scene {
public:
  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams &params,
                    FontContext &fontContext) override {
    if (!m_serif)
      m_serif = defaultSerif(fontContext);

    const QString text =
        params.text.isEmpty() ? effectsDefaultText() : params.text;
    const sk_sp<SkTypeface> &typeface =
        params.typeface ? params.typeface : m_serif;
    const float fontSize = std::clamp(params.fontSize * 2.5f, 30.0f, 68.0f);
    const float paragraphFontSize =
        std::clamp(params.fontSize * 0.85f, 14.0f, 22.0f);

    const float canvasWidth = static_cast<float>(size.width());
    const float canvasHeight = static_cast<float>(size.height());
    const float top = 72.0f;
    // The paragraph block claims a fixed band at the bottom of the canvas;
    // the display-title rows share whatever height remains above it.
    const float paragraphAreaHeight =
        std::clamp(canvasHeight * 0.30f, 120.0f, 210.0f);
    const float paragraphCaptionGap = 26.0f;
    const float rowsBottom =
        canvasHeight - 38.0f - paragraphCaptionGap - paragraphAreaHeight;
    const float rowHeight = std::max(
        60.0f, (rowsBottom - top) / static_cast<float>(m_paragraphs.size()));
    const SkRect paragraphBounds =
        SkRect::MakeLTRB(24.0f, canvasHeight - paragraphAreaHeight,
                         canvasWidth - 24.0f, canvasHeight - 20.0f);

    const bool rebuild = text != m_text || typeface.get() != m_typeface ||
                         fontSize != m_fontSize ||
                         paragraphFontSize != m_paragraphFontSize ||
                         size != m_size;

    double layoutMicroseconds = 0;
    if (rebuild) {
      m_text = text;
      m_typeface = typeface.get();
      m_fontSize = fontSize;
      m_paragraphFontSize = paragraphFontSize;
      m_size = size;
      buildPaints(fontSize);
      buildParagraphPaints(paragraphFontSize);

      const auto layoutStart = Clock::now();
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
            {std::max(230.0f, canvasWidth * 0.28f),
             top + rowHeight * (static_cast<float>(row) + 0.67f)});
      }

      m_paragraphBlock.clear();
      TextStyle paragraphStyle =
          makeStyle(paragraphFontSize, kInk, "", typeface);
      uint32_t clauseStart = 0;
      for (size_t row = 0; row < kParagraphClauses.size(); ++row) {
        m_paragraphBlock.appendText(kParagraphClauses[row], paragraphStyle);
        const uint32_t clauseEnd =
            static_cast<uint32_t>(m_paragraphBlock.text().size());
        m_paragraphSpans[row] = {clauseStart, clauseEnd};
        clauseStart = clauseEnd;
      }
      // Split the paint spans before the first layout, not after: a run's
      // style index is fixed relative to the span list at layout time, and
      // splitting one span into five afterward (below, every frame) only
      // updates already-indexed spans in place — it doesn't retarget runs
      // that were shaped against the single pre-split span.
      for (size_t row = 0; row < m_paragraphSpans.size(); ++row)
        m_paragraphBlock.setPaint(m_paragraphSpans[row].start,
                                  m_paragraphSpans[row].end,
                                  m_paragraphPaints[row]);
      ParagraphLayoutOptions paragraphOptions;
      paragraphOptions.alignment = TextAlignment::kJustify;
      paragraphOptions.lineMetrics.height = paragraphFontSize * 1.35f;
      BlockFlow paragraphFlow(paragraphBounds);
      m_paragraphBlockLayout = layoutParagraph(fontContext, m_paragraphBlock,
                                               paragraphFlow, paragraphOptions);

      layoutMicroseconds = toMicroseconds(Clock::now() - layoutStart);
    }

    // Runtime effects compile once inside PaintShaders. These calls only make
    // new uniform blocks/shader instances; replacing them is paint-only and
    // the existing shapes and layouts remain valid.
    const SkRect shaderBounds = SkRect::MakeWH(canvasWidth, canvasHeight);
    const float time = static_cast<float>(elapsedSeconds);
    m_paints[3].foreground.setShader(PaintShaders::water(shaderBounds, time));
    m_paints[4].foreground.setShader(
        PaintShaders::meshGradient(shaderBounds, time));
    m_paints[4].overlays[0].paint.setShader(
        PaintShaders::sparkle(shaderBounds, time));
    m_paragraphPaints[3].foreground.setShader(
        PaintShaders::water(paragraphBounds, time));
    m_paragraphPaints[4].foreground.setShader(
        PaintShaders::meshGradient(paragraphBounds, time));
    m_paragraphPaints[4].overlays[0].paint.setShader(
        PaintShaders::sparkle(paragraphBounds, time));

    // A separate translucent gradient is composited above the stars as a
    // traveling sheen, demonstrating that runtime and stock shaders layer in
    // exactly the same PaintStyle stack. The same shader instance is shared
    // by the display row and the paragraph below it.
    const float phase =
        static_cast<float>(std::fmod(elapsedSeconds * 0.22, 1.0));
    const SkPoint gradientPoints[2] = {{canvasWidth * (phase - 0.35f), 0},
                                       {canvasWidth * (phase + 0.35f), 0}};
    const SkColor4f gradientColors[3] = {SkColor4f::FromColor(0x001A66FF),
                                         SkColor4f::FromColor(0x104885),
                                         SkColor4f::FromColor(0x00FFFFFF)};
    sk_sp<SkShader> animatedGradient = SkShaders::LinearGradient(
        gradientPoints,
        SkGradient(SkGradient::Colors({gradientColors, 3}, SkTileMode::kMirror),
                   SkGradient::Interpolation()));
    m_paints[4].overlays[1].paint.setShader(animatedGradient);
    m_paragraphPaints[4].overlays[1].paint.setShader(std::move(animatedGradient));

    for (size_t row = 0; row < m_paragraphs.size(); ++row)
      m_paragraphs[row].setPaint(0, m_textLengths[row], m_paints[row]);
    for (size_t row = 0; row < m_paragraphSpans.size(); ++row)
      m_paragraphBlock.setPaint(m_paragraphSpans[row].start,
                                m_paragraphSpans[row].end,
                                m_paragraphPaints[row]);

    canvas->clear(kPaper);
    int runCount = 0;
    for (size_t row = 0; row < m_paragraphs.size(); ++row) {
      m_layouts[row].drawBatched(canvas, m_paragraphs[row]);
      runCount += static_cast<int>(m_layouts[row].runs.size());
    }
    m_paragraphBlockLayout.drawBatched(canvas, m_paragraphBlock);
    runCount += static_cast<int>(m_paragraphBlockLayout.runs.size());

    const std::array<std::u8string_view, 5> labels = {
        u8"1 draw: foreground SkPaint",
        u8"2 draws: dropShadow preset + foreground",
        u8"2 draws: glow preset + foreground",
        u8"2 draws: outline + animated water shader",
        u8"6 draws: mesh + stars + gradient composite"};
    for (size_t row = 0; row < labels.size(); ++row)
      drawCaption(canvas, fontContext, labels[row],
                  {24, top + rowHeight * (static_cast<float>(row) + 0.67f) -
                           fontSize * 0.45f},
                  std::max(180.0f, canvasWidth * 0.23f));
    drawCaption(canvas, fontContext,
                u8"Each preset above, covering its own span of one running "
                u8"paragraph (5 setPaint ranges, 1 Paragraph):",
                {24, canvasHeight - paragraphAreaHeight - 20.0f},
                canvasWidth - 48);

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
    m_paints[2].addUnderlay(
        PaintLayer::glow(0x8892C7FF, 6.0f, /*spread=*/2.0f, /*intensity=*/1.6f));

    m_paints[3] = PaintStyle(SK_ColorWHITE); // water shader set per frame
    m_paints[3].addUnderlay(PaintLayer::outline(0xFF5A1E17, fontSize * 0.11f));

    m_paints[4] = PaintStyle(SK_ColorWHITE); // mesh shader set per frame
    m_paints[4]
        .addUnderlay(PaintLayer::dropShadow(0x77000000, {5, 6}, 4.0f))
        .addUnderlay(PaintLayer::glow(0x6692C7FF, 7.0f, /*spread=*/1.5f,
                                     /*intensity=*/1.5f))
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

  // Rebuilds the same five presets as buildPaints(), scaled for the much
  // smaller paragraph body size rather than the display-title size — the
  // underlay offsets/blur/stroke widths above would read as oversized blobs
  // at body scale, so each preset gets its own size-appropriate copy.
  void buildParagraphPaints(float paragraphFontSize) {
    m_paragraphPaints[0] = PaintStyle(kInk);

    m_paragraphPaints[1] = PaintStyle(kInk);
    m_paragraphPaints[1].addUnderlay(
        PaintLayer::dropShadow(0x77000000, {1.5f, 2.0f}, 1.4f));

    m_paragraphPaints[2] = PaintStyle(kBlue);
    m_paragraphPaints[2].addUnderlay(PaintLayer::glow(
        0x8892C7FF, 3.0f, /*spread=*/0.8f, /*intensity=*/1.6f));

    m_paragraphPaints[3] = PaintStyle(SK_ColorWHITE); // water shader per frame
    m_paragraphPaints[3].addUnderlay(
        PaintLayer::outline(0xFF5A1E17, paragraphFontSize * 0.09f));

    m_paragraphPaints[4] = PaintStyle(SK_ColorWHITE); // mesh shader per frame
    m_paragraphPaints[4]
        .addUnderlay(PaintLayer::dropShadow(0x77000000, {2.0f, 2.5f}, 1.8f))
        .addUnderlay(PaintLayer::glow(0x6692C7FF, 3.5f, /*spread=*/0.6f,
                                     /*intensity=*/1.5f))
        .addUnderlay(PaintLayer::outline(kInk, paragraphFontSize * 0.11f));

    SkPaint stars;
    stars.setAntiAlias(true);
    stars.setBlendMode(SkBlendMode::kScreen);
    m_paragraphPaints[4].addOverlay(PaintLayer(std::move(stars)));

    SkPaint sheen;
    sheen.setAntiAlias(true);
    sheen.setBlendMode(SkBlendMode::kScreen);
    m_paragraphPaints[4].addOverlay(PaintLayer(std::move(sheen)));
  }

  std::array<Paragraph, 5> m_paragraphs;
  std::array<ParagraphLayout, 5> m_layouts;
  std::array<PaintStyle, 5> m_paints;
  std::array<uint32_t, 5> m_textLengths{};
  Paragraph m_paragraphBlock;
  ParagraphLayout m_paragraphBlockLayout;
  std::array<PaintStyle, 5> m_paragraphPaints;
  std::array<CharRange, 5> m_paragraphSpans{};
  QString m_text;
  SkTypeface *m_typeface = nullptr;
  sk_sp<SkTypeface> m_serif;
  float m_fontSize = 0;
  float m_paragraphFontSize = 0;
  SkISize m_size = {0, 0};
};

SceneDescriptor makeEffectsDescriptor() {
  SceneDescriptor descriptor;
  descriptor.name = QStringLiteral("Paint layers");
  descriptor.defaultText = effectsDefaultText();
  descriptor.displayOrder = 80;
  descriptor.make = [] { return std::make_unique<EffectsScene>(); };
  return descriptor;
}

} // namespace

REGISTER_GALLERY_SCENE(makeEffectsDescriptor())

} // namespace gallery
