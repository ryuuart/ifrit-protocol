/** @file
 * The Runge-Kutta walk, and the three ways a scalar field is read as a
 * direction.
 */
#include "sigilgeometry/path/Trace.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <numbers>

namespace sigil::geometry::path {
namespace {

/** One walk from a seed, in one direction. The seed itself is not
 *  written: the caller joins the two halves and puts it in once. */
void walk(const VectorField& field, glm::vec2 from, float sign,
          const TraceOptions& options, std::vector<glm::vec2>& out) {
  const float step = std::abs(options.step) * sign;
  if (!(std::abs(step) > 0) || !(options.length > 0)) return;
  const auto steps = (size_t)std::max(
      0.0f, std::floor(options.length / std::abs(options.step)));
  const bool bounded = !options.bounds.isEmpty();

  glm::vec2 at = from;
  for (size_t i = 0; i < steps; ++i) {
    // The classical fourth-order Runge-Kutta step, over a normalized
    // field: the walk advances by arc length rather than by whatever
    // magnitude the field happens to carry, which is what lets one set of
    // numbers drive a unit direction field and a velocity buffer alike.
    const auto direction = [&](glm::vec2 p) {
      const glm::vec2 v = field(p);
      const float speed = glm::length(v);
      return speed > options.minSpeed ? v / speed : glm::vec2{0, 0};
    };
    const glm::vec2 k1 = direction(at);
    if (k1 == glm::vec2{0, 0}) return;
    const glm::vec2 k2 = direction(at + k1 * (step * 0.5f));
    const glm::vec2 k3 = direction(at + k2 * (step * 0.5f));
    const glm::vec2 k4 = direction(at + k3 * step);
    const glm::vec2 move = (k1 + 2.0f * k2 + 2.0f * k3 + k4) / 6.0f;
    if (glm::length(move) <= options.minSpeed) return;
    at += move * step;
    if (bounded && !options.bounds.contains(at.x, at.y)) return;
    out.push_back(at);
  }
}

}  // namespace

VectorField flow(const core::noise::Field& field, Flow kind, float turns,
                 float reach) {
  switch (kind) {
    case Flow::Angle:
      return [field, turns](glm::vec2 p) {
        const float angle = field.at(p.x, p.y) * turns *
                            std::numbers::pi_v<float>;
        return glm::vec2{std::cos(angle), std::sin(angle)};
      };
    case Flow::Gradient:
    case Flow::Curl: {
      const float span = reach > 0 ? reach : 1.0f;
      const bool turned = kind == Flow::Curl;
      return [field, span, turned](glm::vec2 p) {
        // A central difference: the two samples straddle the point, so
        // the slope is the one AT it rather than the one just past it.
        const float dx =
            field.at(p.x + span, p.y) - field.at(p.x - span, p.y);
        const float dy =
            field.at(p.x, p.y + span) - field.at(p.x, p.y - span);
        const glm::vec2 gradient{dx, dy};
        const float slope = glm::length(gradient);
        if (!(slope > 0)) return glm::vec2{0, 0};
        const glm::vec2 unit = gradient / slope;
        return turned ? glm::vec2{-unit.y, unit.x} : unit;
      };
    }
  }
  return [](glm::vec2) { return glm::vec2{1, 0}; };
}

Polyline streamline(const VectorField& field, glm::vec2 seed,
                    const TraceOptions& options) {
  Polyline line;
  if (!field) return line;
  if (!options.bounds.isEmpty() && !options.bounds.contains(seed.x, seed.y))
    return line;

  if (options.bothWays) {
    std::vector<glm::vec2> back;
    walk(field, seed, -1.0f, options, back);
    line.points.assign(back.rbegin(), back.rend());
  }
  line.points.push_back(seed);
  walk(field, seed, 1.0f, options, line.points);
  return line;
}

std::vector<Polyline> streamlines(const VectorField& field,
                                  std::span<const glm::vec2> seeds,
                                  const TraceOptions& options) {
  std::vector<Polyline> lines;
  lines.reserve(seeds.size());
  for (const glm::vec2 seed : seeds)
    lines.push_back(streamline(field, seed, options));
  return lines;
}

}  // namespace sigil::geometry::path
