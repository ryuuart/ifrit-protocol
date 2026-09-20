#pragma once

/** @file
 * @ingroup material-color
 *
 * ONE COLOUR RAMP AS A VALUE: the stops, the space they are walked in,
 * the shape of the walk, which way round it runs, and the numbers its
 * ends stand for. It answers a colour for a number and is callable, so
 * anything taking an interpolator over a unit position takes a ramp.
 * `sampleRamp` is the ladder underneath; this is that ladder with the
 * four decisions a caller otherwise re-spells at every site.
 */

#include <sigilcore/compute/Curve.h>
#include <sigilmaterial/color/Color.h>

#include <vector>

namespace sigil::material {

/** WHICH SPACE THE COLOURS BETWEEN TWO STOPS ARE WALKED IN. `Srgb`
 *  walks the numbers a file stores, which is what a renderer's gradient
 *  does; `Linear` the light they stand for; `Oklab` what an eye reports;
 *  `Oklch` the same space around the hue circle instead of across it.
 *  @trap None of the four is the default for every ramp — the space is
 *  the question the ramp is answering. */
enum class RampSpace : uint8_t { Srgb, Linear, Oklab, Oklch };

/** WHICH WAY ROUND THE HUE CIRCLE an `Oklch` walk goes.
 *
 *  Two hues are two directions apart, and neither is wrong. `Shorter` is
 *  the arc under half a turn, which is what a two-stop tint wants;
 *  `Longer` is the other one, the way a ramp is made to travel through
 *  the rest of the wheel; `Increasing` and `Decreasing` force the sign,
 *  which is what a sweep meant to keep turning the same way needs when
 *  its stops cross the wrap. Read only when the space is `Oklch`. */
enum class HueArc : uint8_t { Shorter, Longer, Increasing, Decreasing };

/** THE RAMP: stops, and the five decisions about how they are read.
 *  `Ramp::at` maps the caller's own number onto the stops and answers a
 *  colour, and the call operator is the same call, so a ramp IS an
 *  interpolator. Every member is a plain number, a small enumeration or
 *  a stop list, so two ramps compare exactly.
 *  @trap Outside the domain it CLAMPS: a ramp carries no answer for what
 *  lies beyond its ends. */
struct Ramp {
  /** The colours and where they sit, in [0, 1], in order. Two stops at
   *  one position are a hard edge — the band boundary a ramp says with no
   *  blend across it. */
  std::vector<RampStop> stops;
  RampSpace space = RampSpace::Oklab;
  HueArc arc = HueArc::Shorter;
  /** The shape of the walk: the position is passed through this before
   *  the stops are read, so a ramp can dwell at one end without moving
   *  its stops. A default-built curve is the straight walk.
   *
   *  The curve carries its own parameters and compares by them, so a ramp
   *  eased by a named shape — a CSS timing function, a back, a bounce —
   *  is still a value two of which can be proved the same. */
  core::curve::Curve easing{};
  /** The stops read from the far end. It is a prop rather than a second
   *  stop list because a reversed colormap is the same value seen the
   *  other way — the pair a diverging scale needs is one ramp and one
   *  flag, and the two cannot drift apart. */
  bool reverse = false;
  /** WHAT THE CALLER'S NUMBERS MEAN: the value that lands on the first
   *  stop and the value that lands on the last. The ramp reads a
   *  temperature, a depth or a count directly, so the normalisation is
   *  in the value rather than at every call site. A degenerate domain
   *  answers the first stop, which is the only honest reading of a range
   *  with no inside. */
  float domainLow = 0.0f;
  float domainHigh = 1.0f;

  bool operator==(const Ramp&) const = default;

  /** THE COLOUR AT @p value, in the caller's own units. Transparent
   *  black for a ramp with no stops. */
  [[nodiscard]] Color at(float value) const;

  /** The same reading, for anything that hands a unit position to an
   *  interpolator — a data scale's `through()`, a legend, a table. */
  Color operator()(double value) const { return at((float)value); }

  /** Where @p value lands on [0, 1] after the domain, the reversal and
   *  the easing — the position the stops are actually read at. Exposed
   *  because a caller that draws the ramp itself (a shader, a gradient,
   *  a strip of chips) has to walk the same positions this does. */
  [[nodiscard]] float position(float value) const;
};

/** THE RAMP AS A FIXED TABLE: @p entries colours read at the centres of
 *  @p entries equal bands.
 *
 *  Centres rather than ends, because a table of N entries stands for N
 *  bands and not for N points on a line: reading at the ends would make
 *  the first and last entries stand for half a band each. It is the
 *  posterising crossing — a continuous ramp becoming the thing a palette
 *  is, where there is nothing between two entries. */
[[nodiscard]] Palette palette(const Ramp& ramp, int entries);

/** A RAMP FROM A TABLE: one stop per entry, evenly spaced, so a palette
 *  authored as a list of colours can be read continuously.
 *
 *  The way back from `palette()`, and not its inverse: a palette says
 *  there is nothing between its entries and this says what lies between
 *  them, which is a decision the caller is making by asking. */
[[nodiscard]] Ramp ramp(const Palette& palette,
                        RampSpace space = RampSpace::Oklab);

}  // namespace sigil::material
