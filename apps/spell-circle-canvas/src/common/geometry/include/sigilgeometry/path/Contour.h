#pragma once
/** @file
 * @ingroup geometry-path
 *
 * Contours — a path's sub-paths addressed by arc length. Position and
 * tangent at a distance, a segment between two distances, the corners
 * along the way, and the constructions that walk the contour: a parallel
 * curve, a sinusoidal displacement, the windows around corners.
 *
 * Anything that places something ALONG an outline — text on a path, a
 * stroke's ornaments, a marching dash, a pen following a spine — reads
 * the contour through this type, so there is one definition of "distance
 * along" and one of "closed wraps around".
 */
#include <glm/vec2.hpp>

#include "sigilgeometry/path/Outline.h"
#include <memory>
#include <optional>
#include <utility>
#include <vector>

/** THE 2D TIER. Its currency is an `Outline`, and everything here either
 *  measures one, remakes one or bends one: contours addressed by arc
 *  length, polylines and segments an outline is flattened to, the
 *  boolean and distortion operators, the shapers and profiles a stroke
 *  is walked with, node arithmetic that adds and removes nodes, the
 *  interpolation between two compatible outlines, and the scatters,
 *  lattices and traces that put points and lines where a shape says.
 *
 *  Numbers are glm vectors, degrees where a human names
 *  an angle, pixels everywhere else. The 3D tier is
 *  `sigil::geometry::mesh`; its currency is vertices and indices. */
namespace sigil::geometry::path {

/** One sub-path, already measured. Every query is a distance along the
 *  curve rather than a curve parameter, so two constructions that ask for
 *  the same distance land on the same point. The measurement is the
 *  expensive part and is taken once, in `of`; copies share it. A
 *  default-constructed contour measures nothing and answers `valid()`
 *  false. */
class Contour {
 public:
  /** Where the contour is at a distance, and which way it is heading
   *  (unit tangent). */
  struct Sample {
    glm::vec2 position{0, 0};
    glm::vec2 tangent{1, 0};

    /** Value equality: the same place heading the same way. */
    bool operator==(const Sample&) const = default;
  };

  /** A corner sharper than the threshold: the distance it sits at, and
   *  the unit tangents arriving and leaving. */
  struct Corner {
    float distance = 0;
    glm::vec2 in{0, 0};
    glm::vec2 out{0, 0};

    /** Value equality: the same distance and the same two tangents. */
    bool operator==(const Corner&) const = default;
  };

  /** The point on a contour nearest to a query point. */
  using Nearest = path::Nearest;

  /** Every contour of `path`, in path order. Degenerate (zero-length)
   *  contours are skipped. `forceClosed` treats each as closed. */
  static std::vector<Contour> of(const Outline& outline,
                                 bool forceClosed = false);

  /** The total length of every contour of `outline` — the distance a
   *  walk over the whole outline covers, seams not counted. */
  static float lengthOf(const Outline& outline);

  Contour() = default;

  bool valid() const { return m_measure != nullptr; }
  /** Two contours are equal when they are the same measurement — copies
   *  of one `Contour::of` result compare equal, two measurements of the
   *  same path do not. A cache proves reuse by this. */
  bool operator==(const Contour&) const = default;
  float length() const;
  bool closed() const;

  /** The sample at `distance`, clamped to [0, length]. Nullopt only when
   *  the contour cannot be evaluated there. */
  std::optional<Sample> at(float distance) const;

  /** The sample at `distance` wrapped around the contour's length — a
   *  closed contour continues past its seam, an open one clamps. */
  Sample around(float distance) const;

  /** The piece between two distances as its own outline. Distances are
   *  clamped to [0, length]; an empty or inverted window adds nothing. */
  Outline segment(float from, float to) const;

  /** The contour cut in two at `distance` (clamped to [0, length]): the
   *  piece before it and the piece after it, each its own open outline. */
  std::pair<Outline, Outline> split(float distance) const;

  /** The point on this contour nearest to `point`, found by walking the
   *  contour in `step`-length strides and refining around the closest
   *  stride. A contour that bends back within one stride of the answer
   *  is resolved to whichever branch the walk reached first. */
  Nearest nearest(glm::vec2 point, float step = 2.0f) const;

  /** Corners where the tangent turns by more than `angleDeg`, at least
   *  `minSpacing` apart, found by walking the contour in `step`-length
   *  strides and bisecting to the turn. The distance answered is the
   *  last one at which the incoming tangent still holds, which at a real
   *  vertex is the vertex itself when a sample lands on it and a
   *  bisection's last fraction short of it when none does. A closed
   *  contour's seam counts.
   *  `sharpestDeg`, when given, receives the largest turn seen whether or
   *  not it crossed the threshold, so a caller can explain an empty
   *  result. */
  std::vector<Corner> corners(float angleDeg, float minSpacing = 3.0f,
                              float step = 2.0f,
                              float* sharpestDeg = nullptr) const;

 private:
  friend struct ContourAccess;
  explicit Contour(std::shared_ptr<const void> measure);
  /** The measure itself is the renderer's; the header holds it as an
   *  opaque shared pointer so a consumer measuring an outline names
   *  nothing of the renderer's measuring machinery. */
  std::shared_ptr<const void> m_measure;
};

/** The curve a constant distance `across` to the side of every contour,
 *  built by walking in `step`-length strides: outer corners take a
 *  round join, inner corners a miter — cut back no further than the
 *  neighbouring corner, so a turn near a reversal becomes the chord
 *  across it rather than a meeting that never comes — and the samples a
 *  join already answers for are dropped. A join stands for its own
 *  vertex, and a corner the contour turns INTO for everything within
 *  the reach its two offset edges fold across, which below a right
 *  angle is further than the offset itself. Positive `across` is to the
 *  left of the direction of travel in y-down space.
 *
 *  This is the RAIL — one curve, not a region — and it is the walk
 *  `operations::offset` performs at either end of its position dial, where the
 *  offset takes one side only. A caller that wants the band, the grown
 *  silhouette or a join it can name asks the operator; a caller that
 *  wants the curve beside this curve asks here. */
Outline parallel(const Outline& outline, float across, float step = 4.0f);

/** Every contour displaced sideways by a wave: sinusoidal, or a
 *  four-phase zigzag when `zigzag`. The wavelength is rounded so a whole
 *  number of cycles fits each contour and both ends sit on the original
 *  curve. */
Outline displace(const Outline& outline, float amplitude, float wavelength,
                 bool zigzag);

/** The pieces of every contour within `radius` of a corner sharper than
 *  `angleDeg` (`keepNearCorners`), or everything else (not). An open
 *  contour's endpoints count as corners. */
Outline cornerWindows(const Outline& outline, float radius,
                      bool keepNearCorners, float angleDeg);

}  // namespace sigil::geometry::path
