#include "Construction.h"

#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Segments.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <numbers>

namespace path = sigil::geometry::path;

namespace cosmati {
namespace {

constexpr float kTurn = 2.0f * std::numbers::pi_v<float>;

glm::vec2 toward(float radians) {
  return {std::cos(radians), std::sin(radians)};
}

/** A closed piece as straight segments round its corners. */
path::SegmentContour piece(const Points& corners, glm::vec2 at) {
  path::SegmentContour contour{.closed = true};
  for (size_t index = 0; index < corners.size(); ++index) {
    const glm::vec2 from = at + corners[index];
    const glm::vec2 to = at + corners[(index + 1) % corners.size()];
    contour.segments.push_back(
        {.kind = path::SegmentKind::Line, .points = {from, to}});
  }
  return contour;
}

int wrapped(int value, int modulus) {
  const int remainder = value % modulus;
  return remainder < 0 ? remainder + modulus : remainder;
}

}  // namespace

Points upTriangle(float side, float rise) {
  return {{0, rise}, {side * 0.5f, 0}, {side, rise}};
}

Points downTriangle(float side, float rise) {
  return {{side * 0.5f, 0}, {side * 1.5f, 0}, {side, rise}};
}

Points squareOnPoint(float half) {
  return {{0, -half}, {half, 0}, {0, half}, {-half, 0}};
}

path::Outline Course::outline(glm::vec2 size) const {
  // The lattice cells whose pieces can reach the box: the box's corners
  // read in lattice coordinates, widened by a cell each way.
  const float determinant = across.x * down.y - across.y * down.x;
  if (tessera.empty() || std::abs(determinant) < 1e-6f) return {};
  const auto cellOf = [&](glm::vec2 point) {
    const glm::vec2 local = point - origin;
    return glm::vec2{(local.x * down.y - local.y * down.x) / determinant,
                     (across.x * local.y - across.y * local.x) / determinant};
  };
  glm::vec2 lowest{1e9f, 1e9f}, highest{-1e9f, -1e9f};
  for (const glm::vec2 corner :
       {glm::vec2{0, 0}, glm::vec2{size.x, 0}, glm::vec2{0, size.y},
        glm::vec2{size.x, size.y}}) {
    const glm::vec2 cell = cellOf(corner);
    lowest = glm::min(lowest, cell);
    highest = glm::max(highest, cell);
  }
  std::vector<path::SegmentContour> pieces;
  for (int j = (int)std::floor(lowest.y) - 2;
       j <= (int)std::ceil(highest.y) + 2; ++j)
    for (int i = (int)std::floor(lowest.x) - 2;
         i <= (int)std::ceil(highest.x) + 2; ++i) {
      if (wrapped(i - j, every) != offset) continue;
      const glm::vec2 at = origin + (float)i * across + (float)j * down;
      glm::vec2 near{1e9f, 1e9f}, far{-1e9f, -1e9f};
      for (const glm::vec2 corner : tessera) {
        near = glm::min(near, at + corner);
        far = glm::max(far, at + corner);
      }
      if (far.x < 0 || far.y < 0 || near.x > size.x || near.y > size.y)
        continue;
      if (loss > 0) {
        const uint32_t cell = (uint32_t)(i * 7919 + j * 104729);
        if (sigil::core::noise::hash(seed, cell) * 0.5f + 0.5f < loss) continue;
      }
      pieces.push_back(piece(tessera, at));
    }
  return path::toPath(pieces);
}

path::Outline Rosette::outline(glm::vec2 size) const {
  const glm::vec2 centre{size.x * 0.5f, size.y * 0.5f};
  const float half = std::min(size.x, size.y) * 0.5f;
  const float step = kTurn / (float)std::max(count, 1);
  const float waist = step * 0.5f * width;
  const float middle = (inner + outer) * 0.5f * half;
  std::vector<path::SegmentContour> lozenges;
  for (int index = 0; index < count; ++index) {
    const float angle = phase + step * (float)index;
    lozenges.push_back(
        piece({toward(angle) * inner * half, toward(angle - waist) * middle,
               toward(angle) * outer * half, toward(angle + waist) * middle},
              centre));
  }
  return path::toPath(lozenges);
}

Interlace quincunxInterlace(glm::vec2 centre, float centreLoop,
                            float satelliteLoop, float distance,
                            float firstAngle) {
  // Two loops are joined by their INNER tangents, which cross on the line
  // between the centres. A tangent leaves the centre loop at `bend` either
  // side of the satellite's direction and meets the satellite at the same
  // bend either side of the way back.
  const float bend = std::acos((centreLoop + satelliteLoop) / distance);
  constexpr float kStep = 2.0f;  // px between the samples of a loop
  path::Polyline line{.closed = true};
  const auto arc = [&](glm::vec2 about, float radius, float from, float to) {
    const int samples =
        std::max(2, (int)std::ceil(std::abs(to - from) * radius / kStep));
    for (int index = 0; index <= samples; ++index)
      line.points.push_back(
          about +
          toward(from + (to - from) * (float)index / (float)samples) * radius);
  };
  const auto travelled = [&] {
    float length = 0;
    for (size_t index = 1; index < line.points.size(); ++index)
      length += glm::distance(line.points[index - 1], line.points[index]);
    return length;
  };
  const auto stretch = [](glm::vec2 from, glm::vec2 to) {
    return path::toPath(path::Polyline{.points = {from, to}});
  };

  Interlace band;
  float previous = firstAngle - kTurn * 0.25f;
  for (int satellite = 0; satellite < 4; ++satellite) {
    const float direction = firstAngle + kTurn * 0.25f * (float)satellite;
    const glm::vec2 satelliteCentre = centre + toward(direction) * distance;
    // Round the centre loop, from where the last stretch came home to
    // where this one leaves.
    arc(centre, centreLoop, previous + bend, direction - bend);
    Interlace::Knot knot;
    knot.at = centre + toward(direction) * (distance * centreLoop /
                                            (centreLoop + satelliteLoop));
    const glm::vec2 leaves = line.points.back();
    knot.overFrom = travelled();
    // Round the far side of the satellite, the long way.
    const float arrives = direction + kTurn * 0.5f - bend;
    arc(satelliteCentre, satelliteLoop, arrives, arrives - (kTurn - 2 * bend));
    knot.overTo = knot.overFrom +
                  glm::distance(leaves, satelliteCentre +
                                            toward(arrives) * satelliteLoop);
    knot.over = path::toSk(
        stretch(leaves, satelliteCentre + toward(arrives) * satelliteLoop));
    knot.under = path::toSk(stretch(
        line.points.back(), centre + toward(direction + bend) * centreLoop));
    band.knots.push_back(std::move(knot));
    previous = direction;
  }
  band.length =
      travelled() + glm::distance(line.points.back(), line.points.front());
  band.spine = path::toSk(path::toPath(line));
  return band;
}

}  // namespace cosmati
