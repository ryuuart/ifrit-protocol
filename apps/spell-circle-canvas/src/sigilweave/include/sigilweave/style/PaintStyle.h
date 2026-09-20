#pragma once

/** @file
 * @ingroup weave-shaping
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

/** Draw-time glyph appearance with explicit composition order: underlays
 * in vector order, back to front, then `foreground`, then overlays. The
 * default style owns no vectors and is exactly one glyph draw, and every
 * added layer costs one more draw for its style and font bucket.
 * Updating a paint through `Paragraph::setPaint` is visible to an
 * existing `ParagraphLayout`.
 */
struct PaintStyle {
  SkPaint foreground;  ///< the main glyph pass, drawn between the layer lists
  std::vector<PaintLayer> underlays;  ///< drawn in order beneath `foreground`
  std::vector<PaintLayer> overlays;   ///< drawn in order above `foreground`
  /// Line decorations in vector order — highlights beneath every glyph
  /// pass, the rest above them. See Decoration for band defaults, range
  /// vs per-word spanning, and the straight-horizontal-runs-only scope.
  std::vector<Decoration> decorations;

  /// How far this span's glyphs sit ABOVE their line's baseline, px;
  /// negative sinks them below it. It is placement rather than shaping,
  /// so a shifted span costs no re-shape and shares every cache entry
  /// with an unshifted one.
  /// @silent the run is set down a column, where the same idea is a step
  /// across the axis and is `Decoration::offset`.
  float baselineShift = 0;

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
           overlays == other.overlays && decorations == other.decorations &&
           baselineShift == other.baselineShift;
  }
};

}  // namespace sigil::weave
