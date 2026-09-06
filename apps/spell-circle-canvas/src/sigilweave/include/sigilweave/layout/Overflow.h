#pragma once

/** @file
 * @ingroup layout
 *
 * WHAT BECOMES OF TEXT THAT DOES NOT FIT: the marker it ends with and the
 * number of lines it is held to.
 */

#include <cstdint>
#include <string>

namespace sigil::weave {

/** Controls how overflowing text is represented. */
struct OverflowOptions {
  /// Empty disables the marker. Straight flows render it — at a line's end
  /// or at a column's foot, set the way the text it cut was set — while a
  /// contour flow reports its overflow without one, having no end to put a
  /// marker at.
  std::u16string ellipsis;
  /// > 0: the layout uses at most this many of the geometry's lines
  /// (CSS line-clamp — COLUMNS in a vertical flow); remaining text reports
  /// as overflow and `ellipsis` (when set) lands on the clamped line.
  /// Works with every breaker and geometry — the limit wraps the
  /// FlowGeometry, so exclusion flows and Knuth-Plass need no special
  /// handling.
  int maxLines = 0;
  bool operator==(const OverflowOptions&) const = default;
};

}  // namespace sigil::weave
