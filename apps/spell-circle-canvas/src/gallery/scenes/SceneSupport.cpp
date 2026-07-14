#include "SceneSupport.h"

#include <textflowqt/TextFlowQt.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>

#include <numbers>
#include <random>
#include <string>

using namespace textflow;

namespace gallery {

double toMicroseconds(Clock::duration duration) {
  return std::chrono::duration<double, std::micro>(duration).count();
}

TextStyle makeStyle(float fontSize, SkColor color, const char *language,
                    sk_sp<SkTypeface> typeface) {
  TextStyle style;
  style.shaping.typeface = std::move(typeface);
  style.shaping.fontSize = fontSize;
  style.shaping.languageTag = language;
  style.paint.color = color;
  return style;
}

bool BodyCache::ensure(const SceneParams &params, const QString &fallbackText,
                       const sk_sp<SkTypeface> &fallbackTypeface) {
  const QString &text = params.text.isEmpty() ? fallbackText : params.text;
  const sk_sp<SkTypeface> &typeface =
      params.typeface ? params.typeface : fallbackTypeface;
  if (text == builtText && typeface.get() == builtTypeface &&
      params.fontSize == builtFontSize)
    return false;
  builtText = text;
  builtTypeface = typeface.get();
  builtFontSize = params.fontSize;
  paragraph.clear();
  // Zero-copy: QString and Paragraph both store UTF-16.
  textflowqt::appendText(paragraph, text,
                         makeStyle(params.fontSize, kInk, "", typeface));
  return true;
}

sk_sp<SkTypeface> defaultSerif(FontContext &fontContext) {
  sk_sp<SkTypeface> typeface =
      fontContext.fontManager()->matchFamilyStyle("Noto Serif", SkFontStyle());
  return typeface ? typeface : fontContext.defaultTypeface();
}

void drawCaption(SkCanvas *canvas, FontContext &fontContext,
                 std::u8string_view text, SkPoint baselineOrigin, float width) {
  Paragraph paragraph;
  paragraph.appendText(text, makeStyle(12.0f, kBlue));
  BlockFlow flow(
      SkRect::MakeXYWH(baselineOrigin.x(), baselineOrigin.y(), width, 32));
  layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
}

Paragraph makeBigParagraph(int wordCount, float fontSize) {
  const char8_t *latin[] = {u8"the",    u8"letters", u8"fall",   u8"away",
                            u8"from",   u8"their",   u8"lines",  u8"and",
                            u8"return", u8"again",   u8"layout", u8"engine",
                            u8"words",  u8"measure", u8"glyph",  u8"cascade",
                            u8"gentle", u8"steady",  u8"rhythm", u8"flowing"};
  const char8_t *cjk[] = {u8"文字", u8"雨",   u8"波紋", u8"字形",
                          u8"빗물", u8"글자", u8"물결", u8"여울",
                          u8"漣漪", u8"文雨", u8"字落", u8"縦横"};
  const TextStyle styles[3] = {makeStyle(fontSize, kInk),
                               makeStyle(fontSize, kBlue),
                               makeStyle(fontSize, kAccent)};
  std::mt19937 randomEngine(23);
  Paragraph paragraph;
  std::u8string chunk;
  int chunkStyle = 0;
  for (int wordIndex = 0; wordIndex < wordCount; ++wordIndex) {
    if (wordIndex % 5 == 4)
      chunk += cjk[randomEngine() % 12];
    else
      chunk += latin[randomEngine() % 20];
    chunk += ' ';
    if (wordIndex % 120 == 119 || wordIndex + 1 == wordCount) {
      paragraph.appendText(chunk, styles[chunkStyle++ % 3]);
      chunk.clear();
    }
  }
  return paragraph;
}

SkPath spikyRingPath(float elapsedSeconds, float radius) {
  SkPathBuilder pathBuilder;
  constexpr int kSpikes = 9;
  constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
  for (int spikeIndex = 0; spikeIndex < kSpikes; ++spikeIndex) {
    const float baseAngle = static_cast<float>(spikeIndex) * kTwoPi / kSpikes;
    const float tipRadius =
        radius *
        (0.76f + 0.24f * std::sin(elapsedSeconds * 1.7f +
                                  static_cast<float>(spikeIndex) * 2.3f));
    const SkPoint tipPoint = {tipRadius * std::cos(baseAngle),
                              tipRadius * std::sin(baseAngle)};
    const float valleyAngle = baseAngle + kTwoPi / kSpikes * 0.5f;
    const SkPoint valleyPoint = {radius * 0.5f * std::cos(valleyAngle),
                                 radius * 0.5f * std::sin(valleyAngle)};
    if (spikeIndex == 0)
      pathBuilder.moveTo(tipPoint);
    else
      pathBuilder.lineTo(tipPoint);
    pathBuilder.lineTo(valleyPoint);
  }
  pathBuilder.close();
  pathBuilder.addCircle(0, 0, radius * 0.32f);
  pathBuilder.setFillType(SkPathFillType::kEvenOdd);
  return pathBuilder.detach();
}

} // namespace gallery
