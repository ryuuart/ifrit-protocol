/** @file
 * The radial arrangement: its vertices, and the loops, chords or marks
 * drawn through them, on a Skia path builder.
 */

#include "sigilgeometry/path/Radial.h"

#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "sigilgeometry/path/Arrange.h"
#include "sigilgeometry/advanced/Skia.h"

namespace sigil::geometry::path {

bool RadialOptions::operator==(const RadialOptions& other) const {
  if (each || other.each) return false;
  return radii == other.radii && fromDegrees == other.fromDegrees &&
         sweepDegrees == other.sweepDegrees &&
         stepDegrees == other.stepDegrees && closed == other.closed &&
         skip == other.skip && connect == other.connect &&
         marks == other.marks && growth == other.growth &&
         inset == other.inset && waist == other.waist &&
         uniform == other.uniform && frame == other.frame;
}

namespace {

/** The frame an arrangement is drawn on, with the half-extents its unit
 *  radius reaches along x and y — equal on a circle, the box's own on
 *  an ellipse. */
struct Placement {
  PolarFrame frame;
  glm::vec2 halfExtents{1, 1};

  glm::vec2 at(float degrees, float normalisedRadius) const {
    return arrange::onEllipse(frame.centre, halfExtents * normalisedRadius,
                              frame.screenRadians(degrees));
  }
  SkRect ring(float normalisedRadius) const {
    const glm::vec2 reach = halfExtents * normalisedRadius;
    return SkRect::MakeLTRB(frame.centre.x - reach.x, frame.centre.y - reach.y,
                            frame.centre.x + reach.x, frame.centre.y + reach.y);
  }
};

int dealt(int count, const RadialOptions& options) {
  const int n = std::max(0, count);
  return options.closed ? n + 1 : n;
}


float radiusOf(int index, int total, const RadialOptions& options) {
  float radius = options.radii.empty()
                     ? 1.0f
                     : options.radii[(size_t)index % options.radii.size()];
  if (total > 0) {
    const float share = ((float)index + 0.5f) / (float)total;
    if (options.growth == Growth::Linear) radius *= share;
    if (options.growth == Growth::SquareRoot) radius *= std::sqrt(share);
  }
  return radius;
}

/** Vertex @p index's degrees in the frame. A stated step strides on from
 *  `fromDegrees` without end, which is how a golden-angle spiral is dealt;
 *  otherwise the sweep is divided as a run: a full ring into `count`
 *  steps, a closed ladder's `count + 1` vertices over both its ends. */
float degreesOf(int index, int count, const RadialOptions& options) {
  if (options.stepDegrees != 0)
    return options.fromDegrees + options.stepDegrees * (float)index;
  return arrange::along(
      options.fromDegrees, options.sweepDegrees, (size_t)index,
      (size_t)dealt(count, options),
      options.closed ? arrange::Turn::Open : arrange::Turn::Closed);
}

std::vector<glm::vec2> vertices(int count, const RadialOptions& options,
                                const Placement& placement) {
  const int total = dealt(count, options);
  std::vector<glm::vec2> out;
  out.reserve((size_t)total);
  for (int index = 0; index < total; ++index)
    out.push_back(placement.at(degreesOf(index, count, options),
                               radiusOf(index, total, options)));
  return out;
}

void drawLoops(SkPathBuilder& builder, int count, const RadialOptions& options,
               const Placement& placement) {
  const std::vector<glm::vec2> points = vertices(count, options, placement);
  const int n = (int)points.size();
  if (n < 2) return;
  const int skip = std::max(1, options.skip);
  const glm::vec2 centre = placement.frame.centre;
  // Walk k, k + skip, k + 2·skip … until the walk comes home, once per
  // orbit the skip makes: gcd(n, skip) rings of n / gcd vertices each.
  std::vector<bool> seen((size_t)n, false);
  for (int start = 0; start < n; ++start) {
    if (seen[(size_t)start]) continue;
    int k = start;
    builder.moveTo(toSk(points[(size_t)k]));
    do {
      seen[(size_t)k] = true;
      const int next = (k + skip) % n;
      const glm::vec2 from = points[(size_t)k], to = points[(size_t)next];
      if (options.waist == 0.0f) {
        if (next != start) builder.lineTo(toSk(to));
      } else {
        // Pull the edge's midpoint toward the centre along its own
        // radius, so both edges of an arm pinch symmetrically and the tip
        // stays put.
        const glm::vec2 middle = (from + to) * 0.5f;
        builder.quadTo(toSk(middle - (middle - centre) * options.waist),
                       toSk(to));
      }
      k = next;
    } while (k != start);
    builder.close();
  }
}

void drawChords(SkPathBuilder& builder, int count,
                const RadialOptions& options, const Placement& placement) {
  const std::vector<glm::vec2> points = vertices(count, options, placement);
  const int n = (int)points.size();
  if (n < 2) return;
  const int skip = std::max(1, options.skip);
  for (int k = 0; k < n; ++k) {
    glm::vec2 from = points[(size_t)k];
    glm::vec2 to = points[(size_t)((k + skip) % n)];
    if (options.inset > 0) {
      const glm::vec2 run = to - from;
      const float length = std::hypot(run.x, run.y);
      if (length <= 2 * options.inset) continue;
      const glm::vec2 unit = run / length;
      from += unit * options.inset;
      to -= unit * options.inset;
    }
    builder.moveTo(toSk(from));
    builder.lineTo(toSk(to));
  }
}

void drawMark(SkPathBuilder& builder, const Mark& mark, float degrees,
              const Placement& placement, float growthScale) {
  if (mark.empty()) return;
  const PolarFrame& frame = placement.frame;
  const float inner = mark.inner * growthScale;
  const float outer = mark.outer * growthScale;
  switch (mark.kind) {
    case Mark::Kind::Line:
      builder.moveTo(toSk(placement.at(degrees, inner)));
      builder.lineTo(toSk(placement.at(degrees, outer)));
      return;
    case Mark::Kind::Bar: {
      // The same radial run given a width across it, perpendicular to
      // the frame's own outward direction, so a mark stands square to its
      // radius whatever the frame's conventions are.
      const glm::vec2 out = frame.direction(degrees);
      const glm::vec2 across =
          glm::vec2{-out.y, out.x} * (mark.width * 0.5f);
      const glm::vec2 a = placement.at(degrees, inner);
      const glm::vec2 b = placement.at(degrees, outer);
      builder.moveTo(toSk(a + across));
      builder.lineTo(toSk(b + across));
      builder.lineTo(toSk(b - across));
      builder.lineTo(toSk(a - across));
      builder.close();
      return;
    }
    case Mark::Kind::Segment: {
      if (mark.spanDegrees == 0.0f) return;
      const float start = degrees - mark.spanDegrees * 0.5f;
      const float end = degrees + mark.spanDegrees * 0.5f;
      // Out along the far edge, in across the end, back along the near
      // one: one contour whose two curved sides are the ring's own arcs
      // rather than a polyline that would show its facets under a stroke.
      builder.arcTo(placement.ring(outer), frame.screenDegrees(start),
                    frame.screenSweep(mark.spanDegrees), true);
      builder.lineTo(toSk(placement.at(end, inner)));
      builder.arcTo(placement.ring(inner), frame.screenDegrees(end),
                    frame.screenSweep(-mark.spanDegrees), false);
      builder.close();
      return;
    }
    case Mark::Kind::Figure: {
      const glm::vec2 where = placement.at(degrees, outer);
      const Rect bounds = mark.figure.bounds();
      const glm::vec2 shift = where - bounds.centre();
      builder.addPath(toSk(mark.figure).makeOffset(shift.x, shift.y));
      return;
    }
  }
}

void drawMarks(SkPathBuilder& builder, int count, const RadialOptions& options,
               const Placement& placement) {
  const int total = dealt(count, options);
  for (int index = 0; index < total; ++index) {
    Mark mark = options.marks.empty()
                    ? Mark::line()
                    : options.marks[(size_t)index % options.marks.size()];
    if (options.each) mark = options.each(index, mark);
    drawMark(builder, mark, degreesOf(index, count, options), placement,
             radiusOf(index, total, options));
  }
}

Outline draw(int count, const RadialOptions& options,
             const Placement& placement) {
  SkPathBuilder builder;
  if (count > 0) {
    switch (options.connect) {
      case Connect::Loop:
        drawLoops(builder, count, options, placement);
        break;
      case Connect::Each:
        drawChords(builder, count, options, placement);
        break;
      case Connect::None:
        drawMarks(builder, count, options, placement);
        break;
    }
  }
  return fromSk(builder.detach());
}

Placement placementIn(const RadialOptions& options, glm::vec2 size) {
  Placement placement{options.frame};
  placement.frame.centre = size * 0.5f;
  const glm::vec2 half = size * 0.5f;
  placement.frame.radius = std::min(half.x, half.y);
  placement.halfExtents =
      options.uniform ? glm::vec2(placement.frame.radius) : half;
  return placement;
}

}  // namespace

std::vector<glm::vec2> radialPoints(int count, const RadialOptions& options,
                                    const PolarFrame& frame) {
  return vertices(count, options, {frame, glm::vec2(frame.radius)});
}

std::vector<glm::vec2> radialPoints(int count, const RadialOptions& options,
                                    glm::vec2 size) {
  return vertices(count, options, placementIn(options, size));
}

Outline radialOutline(int count, const RadialOptions& options,
                      const PolarFrame& frame) {
  return draw(count, options, {frame, glm::vec2(frame.radius)});
}

Outline radialOutline(int count, const RadialOptions& options,
                      glm::vec2 size) {
  return draw(count, options, placementIn(options, size));
}

}  // namespace sigil::geometry::path
