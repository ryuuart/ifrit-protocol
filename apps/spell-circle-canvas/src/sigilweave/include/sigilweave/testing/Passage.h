#pragma once

/** @file
 * @ingroup weave-testing
 *
 * A passage laid under a stated font context: the paragraph, the options
 * it was set with and the layout, held as one value because a layout
 * borrows the paragraph's glyphs and reads nothing without it.
 */

#include <include/core/SkPoint.h>

#include "sigilweave/layout/Flow.h"
#include "sigilweave/layout/LayoutOptions.h"
#include "sigilweave/layout/ParagraphLayout.h"
#include "sigilweave/paragraph/Paragraph.h"

namespace sigil::weave {
class FontContext;
}

/** THE LIBRARY'S OWN HARNESS: values in, values and images out. A test
 *  lays a passage under the font context it names, reads the setting back
 *  as plain values — lines, runs, glyphs, the fit and the hyphens — and
 *  renders it to a raster plate it can hold against a committed baseline.
 *
 *  Nothing here knows who asks. A host that exposes paragraphs to a
 *  client mirrors these values; the library never learns of that client.
 *  Its target, SigilWeaveTesting, is linked by test binaries and by
 *  nothing that ships.
 *  @trap A test that brings `sigil::weave` in with a using-directive
 *  spells this namespace `weave::testing`: GoogleTest owns `::testing`. */
namespace sigil::weave::testing {

/** ONE PARAGRAPH LAID OUT ONCE, with what it was laid out with. The
 *  layout's runs point into `paragraph`, so the three travel together
 *  and a reading or a render takes the whole value. Moving it keeps the
 *  layout valid, since the glyphs a run points at are shared and not
 *  stored inline; editing `paragraph` does not. */
struct Passage {
  Paragraph paragraph;
  ParagraphLayoutOptions options;
  ParagraphLayout layout;
};

/** Lays @p paragraph into @p geometry under @p fonts with @p options,
 *  keeping all three. The geometry is consumed as layoutParagraph
 *  consumes it and is not kept. */
[[nodiscard]] Passage lay(FontContext& fonts, Paragraph paragraph,
                          FlowGeometry& geometry,
                          ParagraphLayoutOptions options = {});

/** Lays @p paragraph as one unconstrained line whose baseline begins at
 *  @p baselineOrigin, as layoutSingleLine does. */
[[nodiscard]] Passage layLine(FontContext& fonts, Paragraph paragraph,
                              SkPoint baselineOrigin);

}  // namespace sigil::weave::testing
