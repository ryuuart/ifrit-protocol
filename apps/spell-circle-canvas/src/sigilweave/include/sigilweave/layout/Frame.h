#pragma once

/** @file
 * @ingroup layout
 *
 * HOW A FRAME SEATS WHAT IT HOLDS — where the first baseline sits and what
 * becomes of the room left over — the band reserved beside every line for
 * something set alongside the type, and the snapping text on a path is
 * drawn with.
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

/**
 * How a frame seats the lines it holds — the two decisions a frame makes
 * that no line makes for itself.
 *
 * WHERE THE FIRST BASELINE SITS is otherwise the first line's own ascent,
 * so two frames of different type start their text at different heights;
 * naming a cap height, an x-height or a fixed offset instead pins the first
 * line to something the page can be ruled against.
 *
 * WHAT BECOMES OF THE ROOM LEFT OVER is otherwise nothing: the lines stack
 * from the top and the remainder is air underneath. Centring or seating the
 * text against the far edge translates the whole block; justifying it
 * spreads the remainder BETWEEN the lines, as extra leading, which is what
 * a column of a magazine does to reach its foot.
 *
 * Both need to know how deep the frame is, which a geometry knows and the
 * layout does not, so `extent` states it: 0 leaves both decisions alone.
 * Neither applies to a flow whose intervals ride a contour — a loop has no
 * near edge to measure from.
 */
struct FrameOptions {
  enum class FirstBaseline : uint8_t {
    kAscent,     ///< the first line's own ascent
    kCapHeight,  ///< the first line's cap height
    kXHeight,    ///< the first line's x-height
    kLeading,    ///< the first line's whole pitch
    kFixed,      ///< exactly `firstBaselineOffset`
  };
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

/**
 * SPACE RESERVED BESIDE EVERY LINE, over and above the leading — the band
 * something set alongside the type occupies: a reading over a base, a row
 * of emphasis dots, a note in the gutter.
 *
 * It is a LAYOUT INPUT and that is the whole point of it. The band is
 * stated before the text is laid out, from the annotation's own metrics,
 * never from where the base's glyphs turned out to land — so the base is
 * broken and placed once, with the room already in its strut, and the
 * annotation is then placed on the result. Nothing chases anything.
 *
 * `before` is above a line and to the RIGHT of a column, `after` below a
 * line and to the LEFT of one: the sides each writing mode reads its
 * furniture on. Both open the pitch; `before` also moves the baseline down
 * inside the band, so the type stays where the reader expects it and the
 * room appears where the reading goes.
 */
struct ReservedBand {
  float before = 0;
  float after = 0;
  bool operator==(const ReservedBand&) const = default;
};

}  // namespace sigil::weave
