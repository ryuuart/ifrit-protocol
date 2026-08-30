#pragma once

/** @file
 * @ingroup shaping
 *
 * The paint half of a text style: the foreground pass with the ordered
 * underlays and overlays around it and the line decorations. Resolved at
 * draw time only, so recolouring, animating a shader or restyling effects
 * never re-shapes and never relayouts.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPaint.h>

#include <utility>
#include <vector>

#include "sigilweave/style/Decoration.h"
#include "sigilweave/style/PaintLayer.h"

namespace sigil::weave {

/** Draw-time glyph appearance with explicit composition order.
 *
 * Underlays are drawn in vector order (back-to-front), followed by
 * `foreground`, then overlays in vector order. The default style owns no
 * vectors and remains exactly one glyph draw. Every added layer costs one
 * additional draw for its style/font bucket; blur and image filters may add
 * backend-specific work beyond that. Updating any paint or shader through
 * Paragraph::setPaint() is visible to an existing ParagraphLayout.
 */
struct PaintStyle {
  SkPaint foreground;  ///< the main glyph pass, drawn between the layer lists
  std::vector<PaintLayer> underlays;  ///< drawn in order beneath `foreground`
  std::vector<PaintLayer> overlays;   ///< drawn in order above `foreground`
  /// Line decorations in vector order — highlights beneath every glyph
  /// pass, the rest above them. See Decoration for band defaults, range
  /// vs per-word spanning, and the straight-horizontal-runs-only scope.
  std::vector<Decoration> decorations;

  /** Constructs a single anti-aliased black foreground. */
  PaintStyle() { foreground.setAntiAlias(true); }

  /** Preserves the convenient `PaintStyle{SK_ColorRED}` spelling. */
  PaintStyle(SkColor color) : PaintStyle() { foreground.setColor(color); }

  /** Uses a complete caller-configured SkPaint as the foreground. */
  explicit PaintStyle(SkPaint paint) : foreground(std::move(paint)) {}

  /** Appends a pass behind the foreground and returns this style. */
  PaintStyle& addUnderlay(PaintLayer layer) {
    underlays.push_back(std::move(layer));
    return *this;
  }

  /** Appends a pass above the foreground and returns this style. */
  PaintStyle& addOverlay(PaintLayer layer) {
    overlays.push_back(std::move(layer));
    return *this;
  }

  /** Appends a line decoration and returns this style. */
  PaintStyle& addDecoration(Decoration decoration) {
    decorations.push_back(std::move(decoration));
    return *this;
  }

  /** Compares complete paints, layer order, offsets, and decorations. */
  bool operator==(const PaintStyle& other) const {
    return foreground == other.foreground && underlays == other.underlays &&
           overlays == other.overlays && decorations == other.decorations;
  }
};

}  // namespace sigil::weave
