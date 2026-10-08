#pragma once

/** @file
 * @ingroup weave-layout
 *
 * A string as the contours of its glyphs, set on one line at a size and
 * stood where a caller asks: what a solid is extruded from, a stencil is
 * cut from, or a depth map is pressed with.
 */

#include <cstdint>
#include <glm/vec2.hpp>
#include <string>
#include <string_view>

#include "sigilgeometry/path/Outline.h"
#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/style/Face.h"

namespace sigil::weave {

/** Which point of the set line `GlyphOutlineOptions::at` names. */
enum class GlyphOutlineAnchor : uint8_t {
  /// The centre of the contours' own tight bounds, ink and counters alike.
  Centre,
  /// The start of the baseline, where the pen begins.
  Baseline,
};

/** How `glyphOutline()` sets its text. */
struct GlyphOutlineOptions {
  /** The face the text is set in. Null asks for `family` instead. */
  Face face;
  /** A family the font service resolves at `style` when no face is
   *  stated. A family it does not know, or none, is its default face. */
  std::string family;
  FaceStyle style;
  /** The font size, in the outline's own units. */
  float size = 16;
  /** Space added after every letter, in the outline's own units. */
  float track = 0;
  /** Where the anchor stands. */
  glm::vec2 at{0, 0};
  GlyphOutlineAnchor anchor = GlyphOutlineAnchor::Centre;
};

/** THE CONTOURS OF @p text set on one unconstrained line, counters kept,
 *  stood with its anchor on `options.at`. The font service shapes it, so
 *  a code point the face lacks is drawn from the face fallback finds.
 *  Each call lays the line out afresh and retains nothing. */
[[nodiscard]] geometry::path::Outline glyphOutline(
    FontContext& fonts, std::u8string_view text,
    const GlyphOutlineOptions& options = {});

}  // namespace sigil::weave
