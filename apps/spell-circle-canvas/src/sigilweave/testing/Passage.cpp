#include "sigilweave/testing/Passage.h"

#include <utility>

namespace sigil::weave::testing {

Passage lay(FontContext& fonts, Paragraph paragraph, FlowGeometry& geometry,
            ParagraphLayoutOptions options) {
  Passage passage{std::move(paragraph), std::move(options), {}};
  passage.layout =
      layoutParagraph(fonts, passage.paragraph, geometry, passage.options);
  return passage;
}

Passage layLine(FontContext& fonts, Paragraph paragraph,
                glm::vec2 baselineOrigin) {
  Passage passage{std::move(paragraph), {}, {}};
  passage.layout = layoutSingleLine(fonts, passage.paragraph, baselineOrigin);
  return passage;
}

}  // namespace sigil::weave::testing
