#include "sigilweavekit/Labels.h"

#include <sigilweave/Flow.h>
#include <sigilweave/Paragraph.h>
#include <sigilweave/ParagraphLayout.h>

#include <utility>

namespace sigil::weave::kit {

sigil::weave::TextStyle makeStyle(float fontSize, SkColor color,
                              const char *language,
                              sk_sp<SkTypeface> typeface) {
  sigil::weave::TextStyle style;
  style.shaping.typeface = std::move(typeface);
  style.shaping.fontSize = fontSize;
  style.shaping.languageTag = language;
  style.paint.foreground.setColor(color);
  return style;
}

namespace {

template <typename TextView>
void drawLabelImpl(SkCanvas *canvas, sigil::weave::FontContext &fontContext,
                   TextView text, SkPoint origin, const LabelOptions &options) {
  sigil::weave::Paragraph paragraph;
  paragraph.appendText(text, makeStyle(options.fontSize, options.color,
                                       options.language, options.typeface));
  sigil::weave::BlockFlow flow(SkRect::MakeXYWH(origin.x(), origin.y(),
                                            options.width, options.height));
  layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
}

} // namespace

void drawLabel(SkCanvas *canvas, sigil::weave::FontContext &fontContext,
               std::u8string_view text, SkPoint origin,
               const LabelOptions &options) {
  drawLabelImpl(canvas, fontContext, text, origin, options);
}

void drawLabel(SkCanvas *canvas, sigil::weave::FontContext &fontContext,
               std::u16string_view text, SkPoint origin,
               const LabelOptions &options) {
  drawLabelImpl(canvas, fontContext, text, origin, options);
}

} // namespace sigil::weave::kit
