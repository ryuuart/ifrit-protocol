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

  bool operator==(const Outline& other) const;

 private:
  friend struct OutlineAccess;
  explicit Outline(std::shared_ptr<const detail::OutlineBody> body);
  std::shared_ptr<const detail::OutlineBody> m_body;
};

}  // namespace sigil::geometry::path
