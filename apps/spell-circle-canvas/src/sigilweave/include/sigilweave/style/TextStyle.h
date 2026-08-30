#pragma once

/** @file
 * @ingroup shaping
 *
 * TextStyle — the shaping half and the paint half together, with the
 * fluent axis setters over the shaping half. The split is about who owns
 * glyph advances, not about what is visible: a change is paint-side only
 * if it cannot move a glyph.
 */

#include "sigilweave/style/PaintStyle.h"
#include "sigilweave/style/ShapingStyle.h"

namespace sigil::weave {

/** Combines cache-affecting shaping state with draw-time paint state. */
struct TextStyle {
  ShapingStyle shaping;  ///< changes re-shape the covered words
  PaintStyle paint;      ///< changes never re-shape or relayout

  /** Sets or replaces one variable-font axis (fluent sugar over
   *  `shaping.variations`). Replaces in place when the axis is already
   *  present — repeated calls stay order-stable, so styles built by the
   *  same call sequence share one varied-typeface memo entry. */
  TextStyle& variation(const char (&tag)[5], float value) {
    for (FontVariation& v : shaping.variations)
      if (v.tag[0] == tag[0] && v.tag[1] == tag[1] && v.tag[2] == tag[2] &&
          v.tag[3] == tag[3]) {
        v.value = value;
        return *this;
      }
    shaping.variations.emplace_back(tag, value);
    return *this;
  }
  /** The `wght` axis, fluently: `style.weight(650)`. Every axis set here
   *  lands in `shaping.variations` and so participates in shaping identity:
   *  animating one re-shapes the words it covers, which is required for
   *  `wght` because it moves advances on most faces. To animate weight
   *  without re-shaping, use a face with an advance-invariant axis (`GRAD`
   *  on the faces that have it) and drive it at draw time through
   *  ParagraphLayout::LiveVariations instead of setting it here. */
  TextStyle& weight(float wght) { return variation("wght", wght); }
  /** The optical-size axis, fluently: `style.opticalSize(72)`. */
  TextStyle& opticalSize(float opsz) { return variation("opsz", opsz); }
  /** Horizontal condensation, fluently: `style.condense(0.82f)` — see
   *  ShapingStyle::scaleX. */
  TextStyle& condense(float sx) {
    shaping.scaleX = sx;
    return *this;
  }

  /** Compares both shaping and paint configuration. */
  bool operator==(const TextStyle& other) const {
    return shaping == other.shaping && paint == other.paint;
  }
};

}  // namespace sigil::weave
