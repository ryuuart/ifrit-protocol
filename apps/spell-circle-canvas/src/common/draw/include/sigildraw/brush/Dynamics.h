#pragma once

/** @file
 * How the device drives a mark: one curve type, read against pressure,
 * speed or tilt, applied to size, opacity and flow.
 */

#include <algorithm>
#include <functional>
#include <optional>

namespace sigil::draw::brush {

struct Dab;

/** What a curve reads. Every drive arrives as a unit value, which is
 *  what lets one curve type serve all three.
 *
 *  Pressure is the stylus pressure with the tool's envelope along the
 *  stroke already applied. Velocity is the dab's speed against the
 *  tool's reference speed, one at the reference and above. Tilt is zero
 *  for an upright stylus and one for a stylus flat against the
 *  surface. */
enum class Drive { Pressure, Velocity, Tilt };

/** A response curve over a unit input.
 *
 *  `minimum` is the answer at zero and `maximum` the answer at one;
 *  `bend` shapes the ramp between them — one is straight, above one
 *  holds near the minimum until late in the range, below one rises
 *  early. A `curve` of the caller's own replaces all three and is not
 *  clamped to the two ends. The answer is a MULTIPLIER on what the tool
 *  already says, so a flat curve at one changes nothing. */
struct Curve {
  float minimum = 0.0f;
  float maximum = 1.0f;
  float bend = 1.0f;
  std::function<float(float)> curve;

  [[nodiscard]] float at(float input) const;

  /** A curve that answers @p value at every input. */
  [[nodiscard]] static Curve flat(float value) {
    return {.minimum = value, .maximum = value};
  }
};

/** One curve and the device value it reads. */
struct Response {
  Drive drive = Drive::Pressure;
  Curve curve;

  /** The multiplier for @p dab, given the @p pressure the tool's
   *  envelope has already produced and the @p speedReference the tool
   *  measures travel against. */
  [[nodiscard]] float at(const Dab& dab, float pressure,
                         float speedReference) const;
};

/** The responses a tool applies to what each dab deposits.
 *
 *  Each is a multiplier on the value the tool's own scalar responses
 *  have already produced, so a tool that sets none behaves exactly as a
 *  tool with no dynamics at all. Size scales the stamp; opacity scales
 *  the tool's load; flow scales the alpha of the one dab. There is no
 *  buffer between a dab and the canvas here, so the two alphas multiply
 *  into the same place; they are separate because one may follow the
 *  stylus while the other follows the hand. */
struct Dynamics {
  std::optional<Response> size;
  std::optional<Response> opacity;
  std::optional<Response> flow;

  /** Whether any response is set, so a tool with none costs nothing. */
  [[nodiscard]] bool empty() const { return !size && !opacity && !flow; }
};

}  // namespace sigil::draw::brush
