#pragma once

/** @file
 * The grid: ONE layout value over the LayoutScheme seam that divides a
 * container into sized tracks, names rectangular regions of those tracks,
 * and places a child in a region by name.
 *
 * It is the general form of every arrangement a page is divided into —
 * equal shares, unequal columns sized by what is in them, a fixed rail
 * beside a flexible body, a wrapped run of panels — because a track
 * carries a SIZING FUNCTION rather than a width, and the four functions
 * below cover the lot.
 */

#include <include/core/SkRect.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Layout.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sigil::compose::layouts {

/** HOW ONE TRACK IS SIZED: a floor and a ceiling, each of which is a
 *  length, the content, or a share of what is left over.
 *
 *  - `px(v)` — v, floor and ceiling both. It neither grows nor shrinks.
 *  - `content()` — the track is as wide as the widest thing in it and no
 *    wider. Free space beside it stays free.
 *  - `fr(w)` — the track takes `w` shares of what is left after the fixed
 *    and content tracks have been served, and nothing else: a share has NO
 *    floor of its own, so a row of equal panels is equal however wide the
 *    things in it are. Put a floor under it explicitly when the content
 *    must not be crushed.
 *  - `minmax(low, high)` — the floor of `low` under the ceiling of
 *    `high`. `minmax(px(180), fr())` is a share that never falls under
 *    180; `minmax(content(), fr())` is one that never falls under what it
 *    holds.
 *
 *  A ceiling under the floor is no ceiling: the track resolves at the
 *  floor. */
struct Track {
  enum class Kind : uint8_t { Fixed, Content, Fraction };

  /** The default is `fr(1)`, one equal share — what an unstated column of
   *  a page is. */
  Kind minKind = Kind::Fixed;
  float minValue = 0.0f;
  Kind maxKind = Kind::Fraction;
  float maxValue = 1.0f;

  bool operator==(const Track&) const = default;

  static constexpr Track px(float v) {
    return {Kind::Fixed, v, Kind::Fixed, v};
  }
  static constexpr Track content() {
    return {Kind::Content, 0.0f, Kind::Content, 0.0f};
  }
  static constexpr Track fr(float weight = 1.0f) {
    return {Kind::Fixed, 0.0f, Kind::Fraction, weight};
  }
  /** The floor of @p low under the ceiling of @p high. A `Fraction` floor
   *  is not a floor — nothing can be a share of the leftover before the
   *  leftover is known — and reads as zero. */
  static constexpr Track minmax(Track low, Track high) {
    Track t;
    t.minKind = low.minKind == Kind::Fraction ? Kind::Fixed : low.minKind;
    t.minValue = low.minKind == Kind::Fraction ? 0.0f : low.minValue;
    t.maxKind = high.maxKind;
    t.maxValue = high.maxValue;
    return t;
  }
};

/** The four sizing functions as free names, so a track list reads the way
 *  it is spoken: `{px(596), fr(1), minmax(px(180), fr(1))}`. They are the
 *  members above under a shorter name and nothing else. */
[[nodiscard]] constexpr Track px(float v) { return Track::px(v); }
[[nodiscard]] constexpr Track content() { return Track::content(); }
[[nodiscard]] constexpr Track fr(float weight = 1.0f) {
  return Track::fr(weight);
}
[[nodiscard]] constexpr Track minmax(Track low, Track high) {
  return Track::minmax(low, high);
}

/** @p count copies of @p track — the spelling for "four equal columns".
 *  Named for what it repeats, because a pool of instances is repeated by
 *  `instancing::place::repeat` and the two answer different questions. */
[[nodiscard]] inline std::vector<Track> repeatTrack(int count, Track track) {
  return std::vector<Track>(count > 0 ? (size_t)count : 0u, track);
}

/** Track layout with named areas and child-owned spans.
 *
 *      layout(layouts::Grid{
 *          .columns = {layouts::px(160), layouts::fr()},
 *          .areas = {"nav content"}, .gap = {12, 12}})
 *          .child(sidebar().area("nav"))
 *          .child(scene().area("content"))
 *
 * Tracks resolve their minimums, accommodate content, then divide remaining
 * space among fractional shares. Rows and columns use the same rule.
 * Empty columns divide the width equally; empty rows fit their content.
 * A track beyond an explicit list is content-sized.
 *
 * Explicit child areas and cells reserve space before undeclared children
 * flow into free cells. Sparse flow moves forward; dense flow fills holes.
 * Unknown area names auto-flow. Nonrectangular named areas use their
 * bounding rectangle and emit a diagnostic.
 *
 * Content tracks need children measured at their intrinsic size: use
 * alignItems(Align::Start) on the layout node. The grid applies across,
 * down and each child's cellAlign after measuring.
 */
struct Grid {
  std::vector<Track> columns;
  std::vector<Track> rows;
  /** The picture: one string per row, one token per cell, `.` a cell no
   *  name claims. Empty leaves the grid addressed by number alone. */
  std::vector<std::string> areas;
  /** Between tracks: x across, y down. */
  SkSize gap = {0.0f, 0.0f};
  /** Fill the earliest hole a flowing child fits rather than never
   *  backtracking past the cursor. */
  bool dense = false;
  /** How a child sits in the box its cells make, when the child itself
   *  did not say with `Element::cellAlign`. `Stretch` sizes it to the box. */
  Align across = Align::Stretch;
  Align down = Align::Stretch;

  /** This scheme reads `LayoutInput::childMinSizes`: a `Content` floor is
   *  a child's minimum, not its measured size. */
  static constexpr bool readsChildMinSizes = true;

  bool operator==(const Grid&) const = default;

  /** THE RESOLVED TRACKS: what the sizing rule arrived at, and the origin
   *  of each. Exposed for the same reason the auto table exposes its own —
   *  a study that reproduces a printed page has to be able to print the
   *  grid it resolved and diff it against the original's measurements, and
   *  reading the numbers back off the placed rects cannot do it, because a
   *  track nothing fills leaves no trace at all. */
  struct Resolved {
    std::vector<float> columnWidths, rowHeights;
    std::vector<float> columnX, rowY;
  };

  [[nodiscard]] Resolved solve(const LayoutInput& in) const;
  [[nodiscard]] std::vector<SkRect> place(const LayoutInput& in) const;
};

}  // namespace sigil::compose::layouts
