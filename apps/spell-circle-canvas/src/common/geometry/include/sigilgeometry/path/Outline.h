#pragma once
/** @file
 * @ingroup geometry-path
 *
 * THE 2D ANSWER TYPE: an outline as a value. Every shape generator
 * answers one, the outline algebra reads and writes one, and nothing in
 * this header names the renderer that stands behind it — the bridge to
 * and from a renderer's path is `path/Skia.h`.
 */
#include <cstdint>
#include <glm/vec2.hpp>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::geometry::path {

/** An axis-aligned rectangle by its two corners, `min` the top left in
 *  y-down space. A rectangle whose `max` is not past its `min` on both
 *  axes is empty. */
struct Rect {
  glm::vec2 min{0, 0};
  glm::vec2 max{0, 0};

  /** The rectangle at @p origin of @p size. */
  static Rect of(glm::vec2 origin, glm::vec2 size) {
    return {origin, origin + size};
  }
  /** The rectangle of @p size centred on @p centre. */
  static Rect centredOn(glm::vec2 centre, glm::vec2 size) {
    return {centre - size * 0.5f, centre + size * 0.5f};
  }
  float width() const { return max.x - min.x; }
  float height() const { return max.y - min.y; }
  glm::vec2 size() const { return max - min; }
  glm::vec2 centre() const { return (min + max) * 0.5f; }
  bool empty() const { return !(max.x > min.x && max.y > min.y); }
  bool operator==(const Rect&) const = default;
};

/** Which points of a self-overlapping outline are inside it. */
enum class FillRule : uint8_t {
  /** Inside where the contours wind round a point a nonzero number of
   *  times: a hole must run the other way round from its outer. */
  NonZero,
  /** Inside where a ray from the point crosses the outline an odd number
   *  of times: a hole is a hole whichever way it runs. */
  EvenOdd,
};

/** Which way the OUTER rings of an outline are drawn; a hole is always
 *  drawn the other way, which is what makes it a hole under the non-zero
 *  fill rule. Clockwise is as seen on screen, in y-down space. */
enum class Winding : uint8_t {
  /** Outers clockwise, holes counter-clockwise — TrueType's convention
   *  in y-down space, and the direction a circle is drawn by default. */
  OutersClockwise,
  /** Outers counter-clockwise, holes clockwise — PostScript's. */
  OutersCounterClockwise,
};

/** What a distance outside [0, length] means. */
enum class Wrap : uint8_t {
  /** Park at the nearer end. */
  Clamp,
  /** Come round — on CLOSED geometry only; an open curve still parks,
   *  because an open curve has two ends and no seam to come round
   *  through. */
  Around,
};

/** Where a curve is at an arc length, and how it is oriented there.
 *
 *  `normal` is the tangent turned a quarter turn toward +y, which in
 *  y-down space is to the RIGHT of the direction of travel — the
 *  side a positive offset lies on. It carries no information the tangent
 *  does not; it carries the CONVENTION, so a caller offsetting sideways
 *  never picks a sign.
 *
 *  `distance` is where the pose was actually taken, after @ref Wrap
 *  resolved it — so a caller can tell a clamped read from an interior
 *  one without repeating the policy. */
struct Pose {
  glm::vec2 position{0, 0};
  glm::vec2 tangent{1, 0};
  glm::vec2 normal{0, 1};
  float distance = 0;

  /** Value equality: the same place, the same orientation and the same
   *  distance it was taken at. */
  bool operator==(const Pose&) const = default;
};

/** The point on an outline nearest to a query point: the distance along
 *  the outline it sits at, where it is, and how far the query point is
 *  from it. */
struct Nearest {
  float distance = 0;
  glm::vec2 position{0, 0};
  float gap = 0;

  /** Value equality: the same place at the same remove. */
  bool operator==(const Nearest&) const = default;
};

/** How `Outline::resampled()` spaces its points: by a COUNT per contour,
 *  by a SPACING, or as flat as a TOLERANCE allows — the first that is set
 *  wins, in that order. */
struct ResampleOptions {
  /** Exactly this many points per contour, evenly by arc length. */
  int count = 0;
  /** A point at least every this many px, every source vertex kept. */
  float spacing = 0;
  /** Curves flattened until no chord strays further than this, in px. */
  float tolerance = 0.25f;
  bool operator==(const ResampleOptions&) const = default;
};

struct Polyline;
struct Transform;

namespace detail {
struct OutlineBody;
}

/** AN OUTLINE: any number of contours, each open or closed, made of
 *  lines and curves, with the rule that says which points are inside.
 *
 *  A value: copies share one immutable body, so passing, storing and
 *  comparing outlines is cheap, and nothing done to one outline is ever
 *  seen through another. Two outlines are equal when they are made of
 *  the same verbs through the same points under the same rule — which is
 *  what a consumer caching a drawing proves reuse by.
 *
 *  The default outline is empty: no contour, nothing inside. */
class Outline {
 public:
  Outline();

  /** The outline an SVG path-data string (`d`) describes, in the string's
   *  own coordinates. A string that does not parse is the empty outline. */
  static Outline svg(std::string_view data);
  /** The closed rectangle @p rect, clockwise from its top-left corner. */
  static Outline rectangle(const Rect& rect);

  /** Whether the outline has no contour at all. */
  bool empty() const;
  /** The rule that says which points are inside. */
  FillRule fillRule() const;
  /** The same contours read under @p rule. */
  Outline withFillRule(FillRule rule) const;
  /** The tightest rectangle round every point the outline passes
   *  through, curves included; empty for an empty outline. */
  Rect bounds() const;

  /** @name Measure
   *  The outline read by DISTANCE along it: every contour in order, each
   *  starting where the one before ended, so a run a frame cut into
   *  several pieces still carries one continuous measure. Measured once,
   *  the first time a query asks, and shared by every copy.
   *  @{ */
  /** The length of every contour together, seams not counted. */
  float length() const;
  /** Where the outline is @p distance along it. */
  glm::vec2 pointAt(float distance, Wrap wrap = Wrap::Clamp) const;
  /** The unit direction of travel there. */
  glm::vec2 tangentAt(float distance, Wrap wrap = Wrap::Clamp) const;
  /** The unit normal there — the tangent turned toward +y, to the right
   *  of travel in y-down space. */
  glm::vec2 normalAt(float distance, Wrap wrap = Wrap::Clamp) const;
  /** All three at once, with the distance the pose was taken at. */
  Pose poseAt(float distance, Wrap wrap = Wrap::Clamp) const;
  /** The piece between two distances, as its own outline. */
  Outline segment(float from, float to) const;
  /** The outline cut in two at @p distance: before it and after it. */
  std::pair<Outline, Outline> split(float distance) const;
  /** The point on the outline nearest @p point, and how far along the
   *  outline it is. */
  Nearest nearest(glm::vec2 point) const;
  /** The outline as points: a polyline per contour, spaced as @p options
   *  says. */
  std::vector<Polyline> resampled(ResampleOptions options = {}) const;
  /** @} */

  /** @name Area
   *  @{ */
  /** Whether @p point is inside, under the outline's own fill rule. */
  bool contains(glm::vec2 point) const;
  /** The area inside, under the fill rule. */
  float area() const;
  /** Which way the first closed contour is drawn. */
  Winding winding() const;
  /** @} */

  /** @name Combine
   *  Booleans, as the web and paper.js name them: each answers the
   *  region, simplified, under the non-zero rule.
   *  @{ */
  Outline united(const Outline& other) const;
  Outline subtracted(const Outline& other) const;
  Outline intersected(const Outline& other) const;
  /** Inside exactly one of the two. */
  Outline excluded(const Outline& other) const;
  /** @} */

  /** @name Rewrite
   *  @{ */
  /** The same region with overlaps and self-crossings resolved into
   *  plain contours. */
  Outline simplified() const;
  /** Every contour run the other way. */
  Outline reversed() const;
  /** @p other's contours after these, as one outline. */
  Outline joined(const Outline& other) const;
  /** The outline carried through @p transform. */
  Outline transformed(const Transform& transform) const;
  /** @} */

  bool operator==(const Outline& other) const;

 private:
  friend struct OutlineAccess;
  explicit Outline(std::shared_ptr<const detail::OutlineBody> body);
  std::shared_ptr<const detail::OutlineBody> m_body;
};

}  // namespace sigil::geometry::path
