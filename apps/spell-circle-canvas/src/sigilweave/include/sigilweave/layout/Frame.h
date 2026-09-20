#pragma once

/** @file
 * @ingroup weave-layout
 *
 * HOW A FRAME SEATS WHAT IT HOLDS — where the first baseline sits and
 * what becomes of the room left over — the band reserved beside every
 * line, and the snapping text on a path is drawn with.
 */

#include <cstdint>

namespace sigil::weave {

/** Rendering-only controls that do not affect line breaking. */
struct PathTextOptions {
  /// Animated path tangents snap to this many directions to avoid creating
  /// a fresh glyph-atlas strike for every tiny rotation change. Zero
  /// preserves exact rotations for static artwork.
  int tangentRotationSteps = 512;
  bool operator==(const PathTextOptions&) const = default;
};

/** How a frame seats the lines it holds — the two decisions a frame
 * makes that no line makes for itself: where the first baseline sits,
 * otherwise the first line's own ascent, and what becomes of the room
 * left over, otherwise air under the last line. Both need the frame's
 * depth, which `FrameOptions::extent` states.
 * @silent `extent` is 0, or the flow's intervals ride a contour, a loop
 * having no near edge to measure from.
 */
struct FrameOptions {
  /** What the first baseline's distance from the top of the frame is
   *  measured as. Two frames of different type seated on the same metric
   *  start their text at the same height. */
  enum class FirstBaseline : uint8_t {
    kAscent,     ///< the first line's own ascent
    kCapHeight,  ///< the first line's cap height
    kXHeight,    ///< the first line's x-height
    kLeading,    ///< the first line's whole pitch
    kFixed,      ///< exactly `firstBaselineOffset`
  };
  /** What becomes of the room left over down the frame once every line
   *  it holds is placed. */
  enum class Distribute : uint8_t {
    kStart,    ///< the leftover room stays past the last line
    kCenter,   ///< half before the first line, half past the last
    kEnd,      ///< all of it before the first line
    kJustify,  ///< spread between the lines as extra leading
  };

  FirstBaseline firstBaseline = FirstBaseline::kAscent;
  /// kFixed states the offset outright; every other mode adds this on top
  /// of what it measured.
  float firstBaselineOffset = 0;
  Distribute distribute = Distribute::kStart;
  /// kJustify: the most any one gap may grow, px. 0 lifts the limit.
  float maximumInterlineSpacing = 0;
  /// How deep the frame is along the axis its lines stack on, px. 0 means
  /// the caller did not say, and both decisions above are left alone.
  float extent = 0;

  bool operator==(const FrameOptions&) const = default;
};

/** SPACE RESERVED BESIDE EVERY LINE, over and above the leading — the
 * band a reading, a row of emphasis dots or a gutter note occupies. It is
 * a LAYOUT INPUT, stated from the annotation's own metrics before the
 * text is broken, so nothing chases anything. `before` is above a line
 * and to the RIGHT of a column, `after` below and to the LEFT; both open
 * the pitch, and `before` also moves the baseline down inside the band.
 */
struct ReservedBand {
  float before = 0;
  float after = 0;
  bool operator==(const ReservedBand&) const = default;
};

}  // namespace sigil::weave
