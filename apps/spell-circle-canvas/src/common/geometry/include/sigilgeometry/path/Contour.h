#pragma once
/** @file
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
#include <include/core/SkContourMeasure.h>
#include <include/core/SkPath.h>
#include <include/core/SkRefCnt.h>

#include <glm/vec2.hpp>
#include <optional>
#include <vector>

class SkPathBuilder;

namespace sigil::geometry {

class Contour {
 public:
  /** Where the contour is at a distance, and which way it is heading
   *  (unit tangent). */
  struct Sample {
    glm::vec2 position{0, 0};
    glm::vec2 tangent{1, 0};
  };

  /** A corner sharper than the threshold: the distance it sits at, and
   *  the unit tangents arriving and leaving. */
  struct Corner {
    float distance = 0;
    glm::vec2 in{0, 0};
    glm::vec2 out{0, 0};
  };

  /** Every contour of `path`, in path order. Degenerate (zero-length)
   *  contours are skipped. `forceClosed` treats each as closed. */
  static std::vector<Contour> of(const SkPath& path, bool forceClosed = false);

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

  /** The piece between two distances as its own path. */
  SkPath segment(float from, float to) const;
  /** The same piece appended to a builder, starting with a moveTo. */
  void appendSegment(SkPathBuilder& out, float from, float to) const;

  /** Corners where the tangent turns by more than `angleDeg`, at least
   *  `minSpacing` apart, found by walking the contour in `step`-length
   *  strides and bisecting to the turn. A closed contour's seam counts.
   *  `sharpestDeg`, when given, receives the largest turn seen whether or
   *  not it crossed the threshold, so a caller can explain an empty
   *  result. */
  std::vector<Corner> corners(float angleDeg, float minSpacing = 3.0f,
                              float step = 2.0f,
                              float* sharpestDeg = nullptr) const;

 private:
  explicit Contour(sk_sp<SkContourMeasure> measure);
  sk_sp<SkContourMeasure> m_measure;
};

/** The curve a constant distance `across` to the side of every contour,
 *  built by walking in `step`-length strides: outer corners take a round
 *  join, inner corners a miter (or a bevel where a miter would run
 *  away), and samples a miter swallows are dropped. Positive `across`
 *  is to the left of the direction of travel in Skia's y-down space. */
SkPath parallel(const SkPath& path, float across, float step = 4.0f);

/** Every contour displaced sideways by a wave: sinusoidal, or a
 *  four-phase zigzag when `zigzag`. The wavelength is rounded so a whole
 *  number of cycles fits each contour and both ends sit on the original
 *  curve. */
SkPath displace(const SkPath& path, float amplitude, float wavelength,
                bool zigzag);

/** The pieces of every contour within `radius` of a corner sharper than
 *  `angleDeg` (`keepNearCorners`), or everything else (not). An open
 *  contour's endpoints count as corners. */
SkPath cornerWindows(const SkPath& path, float radius, bool keepNearCorners,
                     float angleDeg);

}  // namespace sigil::geometry
