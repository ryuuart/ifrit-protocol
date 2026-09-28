/** @file
 * A single-span style and a caption drawn in one call, over a paragraph
 * shaped and laid out on the spot.
 */

#include <include/core/SkCanvas.h>
#include <sigilmaterial/skia/Color.h>

#include "sigilweave/advanced/Skia.h"
#include "sigilweave/kit/Labels.h"

#include <sigilweave/layout/Flow.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/paragraph/Paragraph.h>

#include <utility>

namespace sigil::weave::kit {

sigil::weave::TextStyle makeStyle(float fontSize, material::Color color,
                                  const char* language,
                                  Face typeface) {
  sigil::weave::TextStyle style;
  style.shaping.typeface = std::move(typeface);
  style.shaping.fontSize = fontSize;
  style.shaping.languageTag = language;
  style.paint.foreground.setColor4f(material::skia::toSkColor(color));
  return style;
}

namespace {

template <typename TextView>
void drawLabelImpl(SkCanvas* canvas, sigil::weave::FontContext& fontContext,
                   TextView text, glm::vec2 origin, const LabelOptions& options) {
  sigil::weave::Paragraph paragraph;
  paragraph.appendText(text, makeStyle(options.fontSize, options.color,
                                       options.language, options.typeface));
  sigil::weave::BlockFlow flow(
      sigil::geometry::path::Rect::of(origin, {options.width, options.height}));
  layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
}

template <typename TextView>
void drawLabelImpl(SkCanvas* canvas, TextContext& context, TextView text,
                   glm::vec2 origin, const LabelOptions& options) {
  BlockFlow flow(
      sigil::geometry::path::Rect::of(origin, {options.width, options.height}));
  context
      .layout(text,
              makeStyle(options.fontSize, options.color, options.language,
                        options.typeface),
              flow)
      .draw(canvas);
}

}  // namespace

void drawLabel(SkCanvas* canvas, sigil::weave::FontContext& fontContext,
               std::u8string_view text, glm::vec2 origin,
               const LabelOptions& options) {
  drawLabelImpl(canvas, fontContext, text, origin, options);
}

void drawLabel(SkCanvas* canvas, sigil::weave::FontContext& fontContext,
               std::u16string_view text, glm::vec2 origin,
               const LabelOptions& options) {
  drawLabelImpl(canvas, fontContext, text, origin, options);
}

void drawLabel(SkCanvas* canvas, TextContext& context, std::u8string_view text,
               glm::vec2 origin, const LabelOptions& options) {
  drawLabelImpl(canvas, context, text, origin, options);
}

void drawLabel(SkCanvas* canvas, TextContext& context, std::u16string_view text,
               glm::vec2 origin, const LabelOptions& options) {
  drawLabelImpl(canvas, context, text, origin, options);
}

}  // namespace sigil::weave::kit
