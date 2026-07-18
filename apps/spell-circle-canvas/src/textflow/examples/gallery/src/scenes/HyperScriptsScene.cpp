// Scene: deliberately pathological Unicode and complex-script shaping.
// This is a typography stress wall, not a language specimen: it combines
// Arabic joining/calligraphy, Cuneiform, deep combining-mark stacks, Indic
// and Tibetan clusters, rare-script fallback, bidi reordering, emoji ZWJ
// sequences, and supplementary-plane symbols in one cached layout.
#include "SceneRegistry.h"
#include "SceneSupport.h"

#include <textflowqt/TextFlowQt.h>

#include <include/core/SkPaint.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

using namespace textflow;

namespace gallery {

namespace {

struct Coverage {
  int glyphCount = 0;
  int missingGlyphCount = 0;
  std::set<uint32_t> typefaceIds;
};

void includeCoverage(const ParagraphLayout &layout, Coverage &coverage) {
  for (const PositionedRun &run : layout.runs) {
    if (run.shaped->typeface)
      coverage.typefaceIds.insert(run.shaped->typeface->uniqueID());
    for (uint16_t glyph : run.shaped->glyphs) {
      ++coverage.glyphCount;
      coverage.missingGlyphCount += glyph == 0;
    }
  }
}

/// This scene's panel label: the kit caption in the wall's cyan-on-dark.
template <typename TextView>
void drawSceneLabel(SkCanvas *canvas, FontContext &fontContext, TextView text,
                    SkPoint origin, float width, SkColor color = 0xFF86D7FF) {
  kit::drawLabel(canvas, fontContext, text, origin,
                 {.color = color, .width = width, .height = 28,
                  .language = "en"});
}

class HyperScriptsScene final : public Scene {
public:
  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams &params,
                    FontContext &fontContext) override {
    const float baseSize = std::clamp(params.fontSize, 12.0f, 26.0f);
    m_built.ensure({baseSize, params.typeface.get()},
                   [&] { build(baseSize, params.typeface); });

    const float width = static_cast<float>(size.width());
    const float height = static_cast<float>(size.height());
    const float margin = 22.0f;
    const float gap = 11.0f;
    const float headerHeight = 42.0f;
    const float contentHeight = std::max(300.0f, height - headerHeight - 42.0f);
    const float topHeight = contentHeight * 0.22f;
    const float zalgoHeight = contentHeight * 0.24f;
    const float bottomHeight =
        (contentHeight - topHeight - zalgoHeight - gap * 3.0f) * 0.5f;
    const float columnWidth = (width - margin * 2.0f - gap) * 0.5f;
    const float top = headerHeight;
    const float zalgoTop = top + topHeight + gap;
    const float lowerTop = zalgoTop + zalgoHeight + gap;

    const std::array<SkRect, 7> panels = {
        SkRect::MakeXYWH(margin, top, columnWidth, topHeight),
        SkRect::MakeXYWH(margin + columnWidth + gap, top, columnWidth,
                         topHeight),
        SkRect::MakeXYWH(margin, zalgoTop, width - margin * 2.0f, zalgoHeight),
        SkRect::MakeXYWH(margin, lowerTop, columnWidth, bottomHeight),
        SkRect::MakeXYWH(margin + columnWidth + gap, lowerTop, columnWidth,
                         bottomHeight),
        SkRect::MakeXYWH(margin, lowerTop + bottomHeight + gap, columnWidth,
                         bottomHeight),
        SkRect::MakeXYWH(margin + columnWidth + gap,
                         lowerTop + bottomHeight + gap, columnWidth,
                         bottomHeight),
    };

    canvas->clear(0xFF0E1017);
    drawSceneLabel(canvas, fontContext,
                   u8"UNICODE SINGULARITY  ·  fallback + itemization + "
                   "HarfBuzz + UAX#9 in one frame",
                   {margin, 10}, width - margin * 2.0f);

    const float pulse =
        0.5f + 0.5f * static_cast<float>(std::sin(elapsedSeconds * 1.4));
    double layoutMicroseconds = 0;
    int runCount = 0;
    Coverage coverage;
    for (size_t panelIndex = 0; panelIndex < panels.size(); ++panelIndex) {
      const SkRect panel = panels[panelIndex];
      SkPaint fill;
      fill.setAntiAlias(true);
      fill.setColor(panelIndex == 2 ? 0xFF171123 : 0xFF151923);
      canvas->drawRoundRect(panel, 9, 9, fill);

      SkPaint border;
      border.setAntiAlias(true);
      border.setStyle(SkPaint::kStroke_Style);
      border.setStrokeWidth(panelIndex == 2 ? 1.8f : 1.0f);
      border.setColor(panelIndex == 2
                          ? SkColorSetARGB(static_cast<U8CPU>(90 + 90 * pulse),
                                           224, 84, 151)
                          : 0x443D4960);
      canvas->drawRoundRect(panel, 9, 9, border);

      drawSceneLabel(canvas, fontContext, kLabels[panelIndex],
                     {panel.left() + 12, panel.top() + 8}, panel.width() - 24);

      // U+12031's ink is much taller than Noto Sans Cuneiform's ordinary
      // signs and rises above the fallback strut. Keep that overshoot clear of
      // the captions in the two panels that contain it.
      const float fallbackOvershoot =
          panelIndex == 1 || panelIndex == 6 ? baseSize * 1.5f : 0.0f;
      const float textTop = panel.top() + 31 + fallbackOvershoot;
      const SkRect textBox = SkRect::MakeXYWH(
          panel.left() + 12, textTop, panel.width() - 24,
          std::max(1.0f, panel.bottom() - textTop - 7));
      BlockFlow flow(textBox);
      ParagraphLayoutOptions options;
      options.alignment =
          panelIndex <= 2 ? TextAlignment::kCenter : TextAlignment::kStart;
      options.lineMetrics.height = m_lineHeights[panelIndex];
      const auto layoutStart = Clock::now();
      ParagraphLayout layout =
          layoutParagraph(fontContext, m_paragraphs[panelIndex], flow, options);
      layoutMicroseconds += toMicroseconds(Clock::now() - layoutStart);
      runCount += static_cast<int>(layout.runs.size());
      includeCoverage(layout, coverage);
      layout.draw(canvas, m_paragraphs[panelIndex]);
    }

    const QString status =
        QStringLiteral("%1 glyphs · %2 resolved typefaces · %3 missing")
            .arg(coverage.glyphCount)
            .arg(coverage.typefaceIds.size())
            .arg(coverage.missingGlyphCount);
    drawSceneLabel(canvas, fontContext, textflowqt::toU16(status),
                   {margin, height - 25}, width - margin * 2.0f,
                   coverage.missingGlyphCount == 0 ? 0xFF86D7FF : 0xFFFF5B68);

    return {layoutMicroseconds, runCount, coverage.glyphCount};
  }

private:
  TextStyle sampleStyle(float size, SkColor color, const char *language,
                        const sk_sp<SkTypeface> &typeface) const {
    return makeStyle(size, color, language, typeface);
  }

  void build(float baseSize, const sk_sp<SkTypeface> &typeface) {
    for (Paragraph &paragraph : m_paragraphs)
      paragraph.clear();

    m_paragraphs[0].appendText(
        u8"﷽  السَّلَامُ عَلَيْكُمْ",
        sampleStyle(baseSize * 2.45f, 0xFFFFD37A, "ar", typeface));
    m_lineHeights[0] = baseSize * 3.25f;

    m_paragraphs[1].appendText(
        u8"𒀱  𒀭𒂗𒆠  𒈙𒁀𒄿𒅗",
        sampleStyle(baseSize * 2.05f, 0xFF86D7FF, "akk", typeface));
    m_lineHeights[1] = baseSize * 2.65f;

    m_paragraphs[2].appendText(
        u8"Z̴̢̨̛̲̦̹̰̓̈́͊͘A̵̛̪̯̜̩͆̈́͝L̷̨̡̲̤̬̝̑̓͑̕G̵̢̺̙͎̺̤̓͛̾Ơ̶̢͙̟̲̦̿̽͋̚  "
        "T̷̨̗̰͉̼̯͛̋E̴̡̨̩̱͕̪͗̎X̷̢̳̮̱̪̿̈́͘T̴̛̬̠̦̞͙̋̄͝",
        sampleStyle(baseSize * 1.75f, 0xFFFF66B3, "und", typeface));
    m_lineHeights[2] = baseSize * 3.2f;

    m_paragraphs[3].appendText(
        u8"ٱلْعَرَبِيَّةُ  ﷽  لَا إِلٰهَ إِلَّا ٱللَّٰهُ",
        sampleStyle(baseSize * 1.35f, 0xFFFFD37A, "ar", typeface));
    m_lineHeights[3] = baseSize * 2.0f;

    m_paragraphs[4].appendText(
        u8"क्ष्ण्य  स्त्रै  བསྒྲུབས་  မြန်မာ  ខ្មែរ  "
        "ශ්‍රී",
        sampleStyle(baseSize * 1.28f, 0xFFA8E6A3, "und", typeface));
    m_lineHeights[4] = baseSize * 2.05f;

    m_paragraphs[5].appendText(
        u8"RTL ← ", sampleStyle(baseSize * 1.12f, 0xFFAAB6CC, "en", typeface));
    m_paragraphs[5].appendText(
        u8"﷽ السَّلَامُ ١٢٣",
        sampleStyle(baseSize * 1.35f, 0xFFFFD37A, "ar", typeface));
    m_paragraphs[5].appendText(
        u8" ⇄ EN 123 ⇄ ",
        sampleStyle(baseSize * 1.12f, 0xFFAAB6CC, "en", typeface));
    m_paragraphs[5].appendText(
        u8"𒈙 𒀭", sampleStyle(baseSize * 1.25f, 0xFF86D7FF, "akk", typeface));
    m_lineHeights[5] = baseSize * 1.95f;

    m_paragraphs[6].appendText(
        u8"𒀱   𰻞   ཧཱུྃ   ꧅   ᎙\n",
        sampleStyle(baseSize * 1.70f, 0xFFFFD37A, "und", typeface));
    m_paragraphs[6].appendText(
        u8"👩🏽‍🚀  🧑🏿‍💻  👨‍👩‍👧‍👦  "
        "🏳️‍🌈 "
        " "
        "𝄞 "
        " "
        "𝕳𝖆𝖗𝖋 "
        " "
        "a̪̺̬̍̎̄͛",
        sampleStyle(baseSize * 1.08f, 0xFFD7B5FF, "und", typeface));
    m_lineHeights[6] = baseSize * 2.05f;
  }

  static constexpr std::array<const char8_t *, 7> kLabels = {
      u8"ARABIC PRESENTATION FORM + JOINING + VOWEL MARKS",
      u8"CUNEIFORM · SUPPLEMENTARY PLANE · AKKADIAN",
      u8"COMBINING-MARK STORM · ONE BASE, MANY ATTACHMENTS",
      u8"FULLY VOCALIZED RTL · LIGATURES · MARK POSITIONING",
      u8"DEEP CLUSTERS · DEVANAGARI · TIBETAN · MYANMAR · KHMER · SINHALA",
      u8"UAX#9 BIDI COLLISION · RTL + LTR + NUMBERS + CUNEIFORM",
      u8"RARE GLYPHS + DEEP CLUSTER · ZWJ + MODIFIERS + SYMBOLS",
  };

  std::array<Paragraph, 7> m_paragraphs;
  std::array<float, 7> m_lineHeights{};
  kit::RebuildGuard<float, const SkTypeface *> m_built;
};

SceneDescriptor makeHyperScriptsDescriptor() {
  SceneDescriptor descriptor;
  descriptor.name = QStringLiteral("Unicode singularity — 𒀱 ﷽ 𒈙 Z̴̓͝");
  descriptor.textEditable = false;
  descriptor.displayOrder = 70;
  descriptor.make = [] { return std::make_unique<HyperScriptsScene>(); };
  return descriptor;
}

} // namespace

REGISTER_GALLERY_SCENE(makeHyperScriptsDescriptor())

} // namespace gallery
