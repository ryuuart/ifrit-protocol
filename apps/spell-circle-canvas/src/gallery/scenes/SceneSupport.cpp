#include "SceneSupport.h"

#include <textflowqt/TextFlowQt.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkPathBuilder.h>

#include <numbers>

using namespace textflow;

namespace gallery {

bool BodyCache::ensure(const SceneParams &params, const QString &fallbackText,
                       const sk_sp<SkTypeface> &fallbackTypeface) {
  const QString &text = params.text.isEmpty() ? fallbackText : params.text;
  const sk_sp<SkTypeface> &typeface =
      params.typeface ? params.typeface : fallbackTypeface;
  return m_guard.ensure({text, typeface.get(), params.fontSize}, [&] {
    paragraph.clear();
    // Zero-copy: QString and Paragraph both store UTF-16.
    textflowqt::appendText(paragraph, text,
                           makeStyle(params.fontSize, kInk, "", typeface));
  });
}

sk_sp<SkTypeface> defaultSerif(FontContext &fontContext) {
  sk_sp<SkTypeface> typeface =
      fontContext.fontManager()->matchFamilyStyle("Noto Serif", SkFontStyle());
  return typeface ? typeface : fontContext.defaultTypeface();
}

void drawCaption(SkCanvas *canvas, FontContext &fontContext,
                 std::u8string_view text, SkPoint baselineOrigin, float width) {
  kit::drawLabel(canvas, fontContext, text, baselineOrigin,
                 {.color = kBlue, .width = width});
}

void drawCaption(SkCanvas *canvas, FontContext &fontContext,
                 std::u16string_view text, SkPoint baselineOrigin,
                 float width) {
  kit::drawLabel(canvas, fontContext, text, baselineOrigin,
                 {.color = kBlue, .width = width});
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
