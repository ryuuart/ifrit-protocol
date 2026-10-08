#include "sigilweave/layout/GlyphOutline.h"

#include "sigilgeometry/path/Transform.h"
#include "sigilweave/layout/TextContext.h"

namespace sigil::weave {

geometry::path::Outline glyphOutline(FontContext& fonts,
                                     std::u8string_view text,
                                     const GlyphOutlineOptions& options) {
  Face face = options.face;
  if (!face && !options.family.empty())
    face = fonts.familyTypeface(options.family, options.style);
  TextStyle style;
  style.shaping.typeface = face;
  style.shaping.fontSize = options.size;
  style.shaping.letterSpacing = options.track;
  TextContext context(fonts, {.paragraphCacheEntries = 0});
  if (options.anchor == GlyphOutlineAnchor::Baseline)
    return context.singleLine(text, style, options.at).layout().glyphOutline();
  const auto outline =
      context.singleLine(text, style, {0, 0}).layout().glyphOutline();
  if (outline.empty()) return outline;
  return outline.transformed(geometry::path::Transform::translate(
      options.at - outline.bounds().centre()));
}

}  // namespace sigil::weave
