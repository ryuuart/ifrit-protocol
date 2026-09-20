#pragma once

/** @file
 * @ingroup draw-brush
 *
 * How the device drives a mark: one curve type, read against pressure,
 * speed or tilt, applied to size, opacity and flow.
 */

#include <functional>
#include <optional>

namespace sigil::draw::brush {

struct Dab;

/** What a curve reads. Every drive arrives as a unit value, which is
 *  what lets one curve type serve all three: pressure with the tool's
 *  envelope already applied, velocity against the tool's reference
 *  speed, and tilt from upright at zero to flat at one. */
enum class Drive { Pressure, Velocity, Tilt };

/** A response curve over a unit input: `minimum` at zero, `maximum` at
 *  one, and `bend` shaping the ramp between them. The answer is a
 *  MULTIPLIER on what the tool already says, so a flat curve at one
 *  changes nothing.
 *  @trap A `curve` of the caller's own replaces all three and is not
 *  clamped to the two ends. */
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

/** The responses a tool applies to what each dab deposits, each a
 *  multiplier on the value the tool's own scalar responses have already
 *  produced: size scales the stamp, opacity the tool's load, and flow
 *  the alpha of the one dab. A tool that sets none behaves exactly as a
 *  tool with no dynamics at all. */
struct Dynamics {
  std::optional<Response> size;
  std::optional<Response> opacity;
  std::optional<Response> flow;

  /** Whether any response is set, so a tool with none costs nothing. */
  [[nodiscard]] bool empty() const { return !size && !opacity && !flow; }
};

}  // namespace sigil::draw::brush
