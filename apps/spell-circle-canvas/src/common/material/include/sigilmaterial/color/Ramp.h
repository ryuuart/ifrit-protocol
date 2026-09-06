#pragma once

/** @file
 * ONE COLOUR RAMP AS A VALUE: the stops, the space they are walked in,
 * the shape of the walk, which way round it runs, and the numbers its
 * ends stand for. It answers a colour for a number, and it is callable,
 * so anything that takes an interpolator over a unit position takes a
 * ramp.
 *
 * `sampleRamp` in `Color.h` is the ladder underneath — stops read in
 * straight sRGB, which is what a renderer's gradient draws. This is that
 * ladder with the four decisions a caller otherwise re-spells at every
 * site: whether the walk is perceptual, whether it is eased, whether it
 * runs the other way, and what range of the caller's own numbers it
 * covers. A look chosen once for a whole sheet is one of these carried
 * as a token rather than a stop list plus four conventions repeated.
 */

#include <sigilmaterial/color/Color.h>

#include <vector>

namespace sigil::material {

/** WHICH SPACE THE COLOURS BETWEEN TWO STOPS ARE WALKED IN.
 *
 *  The four answer different questions and none of them is the default
 *  for every ramp. `Srgb` walks the numbers a file stores and is what a
 *  renderer's gradient does, so it is the space to pick when the ramp
 *  must match one drawn as a gradient. `Linear` walks the light the
 *  numbers stand for, which is the answer when the ramp means a quantity
 *  of light — an exposure, a falloff, an accumulation. `Oklab` walks
 *  what an eye reports: even steps, and no dark band where two
 *  saturated stops cross. `Oklch` walks the same space around the hue
 *  circle instead of across it, which is the difference between a red to
 *  green ramp passing through grey and one passing through orange and
 *  yellow. */
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
 *
 *  `at(v)` maps the caller's own number onto the stops and answers a
 *  colour; `operator()` is the same call, so a ramp IS an interpolator —
 *  a data scale hands its unit position straight to one, and a legend
 *  chip, a scatter's tint and a shader's gradient all read the same
 *  value.
 *
 *  Outside the domain it CLAMPS, exactly as `sampleRamp` clamps outside
 *  the stop list: a ramp carries no answer for what lies beyond its ends,
 *  and a flat band at the end keeps an out-of-range input visible instead
 *  of inventing a colour for it.
 *
 *  Every member is a plain number, a small enumeration or a stop list, so
 *  two ramps compare exactly and a memo keyed on one can be skipped. */
struct Ramp {
  /** The colours and where they sit, in [0, 1], in order. Two stops at
   *  one position are a hard edge — the band boundary a ramp says with no
   *  blend across it. */
  std::vector<RampStop> stops;
  RampSpace space = RampSpace::Oklab;
  HueArc arc = HueArc::Shorter;
  /** The shape of the walk: the position is passed through this before
   *  the stops are read, so a ramp can dwell at one end without moving
   *  its stops. Null is the straight walk.
   *
   *  A PLAIN FUNCTION POINTER, not a callable object: this leaf links
   *  nothing, and a comparable ramp needs a comparable curve. A
   *  captureless lambda converts to one; a curve that carries parameters
   *  belongs to whatever holds the parameters, and is spelled by baking
   *  the shape into the stops instead. */
  float (*easing)(float position) = nullptr;
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
[[nodiscard]] Ramp ramp(const Palette& palette, RampSpace space =
                                                    RampSpace::Oklab);

}  // namespace sigil::material
