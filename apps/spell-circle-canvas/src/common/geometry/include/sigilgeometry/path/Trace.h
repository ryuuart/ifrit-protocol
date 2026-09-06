#pragma once
/** @file
 * WALKING A VECTOR FIELD, and what comes back is a polyline.
 *
 * A flow field, a streamline, a magnetic drawing, hair over a surface,
 * the trail of a particle released into a wind — all of them are one
 * construction: start somewhere, read which way the field points there,
 * step that way, repeat. Written by hand it becomes a forty-step Euler
 * loop with its own bounds test; written once it is a field, a seed and
 * a few numbers.
 *
 * The answer is a `Polyline`, which is what makes this worth having in
 * this library rather than at a call site: everything that acts on an
 * outline — `resample`, `subdivide`, `smoothThrough`, `catmullRom`,
 * `toPath`, the lattice, the band — acts on a streamline unchanged.
 *
 * THE FIELD IS ANY CALLABLE. A noise field carried as a value, a
 * closed-form swirl, a table read with interpolation, a simulation's
 * velocity buffer: they are all `glm::vec2(glm::vec2)`. `flow` below is
 * the one adapter this library ships, because a scalar noise field is
 * what a caller most often has and turning one into a direction is a
 * choice worth spelling once.
 */
#include <include/core/SkRect.h>
#include <sigilcore/compute/Field.h>

#include <cstdint>
#include <functional>
#include <glm/vec2.hpp>
#include <span>
#include <vector>

#include "sigilgeometry/path/Polyline.h"

namespace sigil::geometry::path {

/** WHICH WAY, AND HOW FAST, at a point. A zero vector has no direction
 *  and stops a walk. */
using VectorField = std::function<glm::vec2(glm::vec2)>;

/** HOW A SCALAR FIELD IS READ AS A DIRECTION.
 *
 *  The three are genuinely different pictures of the same noise: an
 *  angle field swirls and folds back on itself, a gradient field runs
 *  straight uphill and never circulates, and a curl field circulates and
 *  never converges — which is what makes it read as a fluid rather than
 *  as a drain. */
enum class Flow : uint8_t {
  /** The value, in [-1, 1], read as an ANGLE through `turns` whole
   *  turns. The cheapest and the one a flow field usually means. */
  Angle,
  /** The direction the field climbs fastest, by a central difference. */
  Gradient,
  /** The gradient turned a quarter turn: the field's curl, which
   *  circulates around its peaks instead of climbing them. */
  Curl,
};

/** THE VECTOR FIELD A SCALAR NOISE FIELD MAKES, as a unit direction.
 *
 *  `turns` is how many whole turns the field's range spans under
 *  `Flow::Angle`. `reach` is how far apart the two samples of a
 *  difference are taken, in the caller's own units, for the two kinds
 *  that need one — too small and the difference is float noise, too
 *  large and the field is blurred. */
[[nodiscard]] VectorField flow(const core::noise::Field& field,
                               Flow kind = Flow::Angle, float turns = 1.0f,
                               float reach = 1.0f);

/** HOW FAR A WALK GOES, AND WHAT STOPS IT. */
struct TraceOptions {
  /** How far one step moves. Smaller follows a turning field more
   *  closely and costs more steps for the same length. */
  float step = 2.0f;
  /** The total arc length walked, which with `step` is what bounds the
   *  number of steps. */
  float length = 200.0f;
  /** The walk stops on leaving this rect. An empty rect does not bound
   *  it, and then only the length and the field itself stop it. */
  SkRect bounds = SkRect::MakeEmpty();
  /** Walk backwards from the seed as well, and join the two halves into
   *  one line through it. What makes a seed the MIDDLE of a streamline
   *  rather than its start. */
  bool bothWays = false;
  /** A field slower than this has no direction to follow and ends the
   *  walk, which is what stops a line spiralling on the spot at a
   *  stagnation point. */
  float minSpeed = 1e-4f;
  friend bool operator==(const TraceOptions&, const TraceOptions&) = default;
};

/** THE LINE THE FIELD CARRIES `seed` ALONG, integrated by the classical
 *  fourth-order Runge-Kutta step — four reads of the field per step,
 *  which is what keeps a curving line on its true path instead of
 *  drifting off it the way a single-read step does.
 *
 *  The seed is always the first point (or, walking both ways, a point in
 *  the middle). A seed the field is already still at answers a polyline
 *  of that one point. */
[[nodiscard]] Polyline streamline(const VectorField& field, glm::vec2 seed,
                                  const TraceOptions& options = {});

/** One line per seed, in seed order. A seed whose line is a single point
 *  still gets its entry, so the answer stays parallel to the seeds. */
[[nodiscard]] std::vector<Polyline> streamlines(
    const VectorField& field, std::span<const glm::vec2> seeds,
    const TraceOptions& options = {});

}  // namespace sigil::geometry::path
