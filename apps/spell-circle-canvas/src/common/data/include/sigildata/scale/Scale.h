#pragma once

/** @file
 * ONE MAPPING VALUE. A scale carries a domain, a range and the transform
 * between them, and answers three questions about that one mapping:
 * where a domain value lands (`apply`), which domain value a range
 * position came from (`invert`), and which domain values deserve a
 * label (`ticks`).
 *
 * Everything that would otherwise be a family of functions — remapping,
 * normalising, constraining, wrapping, log and square-root axes, banded
 * categories, quantised bands — is a PROP on this one value, so a
 * drawing, its axis and a cursor readout all go through the same
 * arithmetic instead of three spellings of it that can drift apart.
 *
 * Numbers only. A colour range is the caller's interpolator read at
 * `position()`, so a colour table plugs in without this file knowing
 * what a colour is. Standard library only.
 */

#include <algorithm>
#include <cmath>
#include <concepts>
#include <type_traits>
#include <vector>

namespace sigil::data {

/** A closed span of numbers, low end first. `low > high` is a reversed
 *  span and is meaningful: a range that runs down the screen is how an
 *  ordinate is drawn. */
struct Interval {
  double low = 0.0;
  double high = 1.0;

  bool operator==(const Interval&) const = default;

  /** high - low, signed. */
  [[nodiscard]] double extent() const { return high - low; }
  /** Whether the two ends are the same number, which is the one case
   *  every mapping here has to answer without dividing. */
  [[nodiscard]] bool degenerate() const { return high == low; }
};

/** HOW A DOMAIN VALUE BECOMES A POSITION.
 *
 *  The first six are continuous: the domain is an interval and every
 *  value in it has a position. The last five are discrete: the input is
 *  an index or a band, and `steps` says how many there are. */
enum class Transform {
  /** Position proportional to the value. */
  Linear,
  /** Position proportional to the logarithm of the value in `base`.
   *  A domain that touches or crosses zero has no logarithm and every
   *  answer from such a scale is not a number, as does a base that is
   *  not above zero and away from one. */
  Log,
  /** Position proportional to `sign(v) * |v|^exponent`. An `exponent` of
   *  zero carries every value onto the same point and answers not a
   *  number. */
  Pow,
  /** `Pow` at exponent 0.5, named because an area read as a radius is
   *  the reason a square-root axis exists. */
  Sqrt,
  /** Logarithmic away from zero and linear within `threshold` of it, so
   *  a domain holding zero and both signs still spreads. A `threshold`
   *  that is not above zero leaves no linear stretch and answers not a
   *  number. */
  Symlog,
  /** Linear in seconds. Only `ticks()` differs: they land on second,
   *  minute, hour and day multiples rather than on powers of ten. */
  Time,
  /** The domain interval cut into `steps` equal bands; the answer is the
   *  band's own position, so a continuous input reads as one of `steps`
   *  values. */
  Quantize,
  /** The domain cut at the values in `thresholds`; the answer is the
   *  slot's position. `thresholds.size() + 1` slots. The cuts are read
   *  in the order given and a value's slot is how many leading cuts it
   *  is at or past, so a list that does not ascend cuts the domain into
   *  slots that overlap. */
  Threshold,
  /** An index in [0, steps) spread evenly across the range, first entry
   *  at `range.low` and last at `range.high` — the plain reading of
   *  "the i-th of n". `padding` and `outerPadding` are not read, and a
   *  lone entry stands at `range.low`, because pinning the ends is the
   *  whole of what this transform promises. */
  Ordinal,
  /** An index in [0, steps) given a band of its own; the answer is where
   *  that band STARTS. `padding` is the gap between neighbouring bands
   *  and `outerPadding` the gap before the first and after the last,
   *  both as fractions of one step; `bandwidth()` is how wide a band
   *  came out. */
  Band,
  /** `Band` whose gap consumes the whole step, so each entry is a
   *  position with no width, `outerPadding` still honoured. Nothing is
   *  pinned to the ends of the range, so a lone entry stands in the
   *  middle of it, which is where one mark belongs. */
  Point,
};

/** WHAT HAPPENS OUTSIDE THE DOMAIN. Applied to the unit position, so it
 *  reads the same on every transform and in both directions. */
enum class Overflow {
  /** The mapping continues past both ends. */
  Extend,
  /** Held at the nearer end. */
  Clamp,
  /** Repeated: the position past the high end re-enters at the low one. */
  Wrap,
  /** Reflected: the position past the high end walks back down. */
  PingPong,
};

/** THE MAPPING.
 *
 *  Aggregate-initialised, compared by value and cheap to copy, so a
 *  scale can be a field on a description, bound into a context, or built
 *  fresh per frame.
 *
 *  ```
 *  Scale x{.domain = {0, 1000}, .range = {40, 760}};
 *  Scale r{.domain = {0, 1200}, .range = {0, 90},
 *          .transform = Transform::Sqrt};
 *  Scale month{.range = {0, 360}, .transform = Transform::Band,
 *              .steps = 12, .padding = 0.1};
 *  ```
 *
 *  Only the props its transform reads matter; the rest keep their
 *  defaults and are ignored. */
struct Scale {
  /** The values coming in. For `Ordinal`, `Band` and `Point` the input
   *  is an index and this is not read. */
  Interval domain{0.0, 1.0};
  /** The positions going out. */
  Interval range{0.0, 1.0};
  Transform transform = Transform::Linear;
  Overflow overflow = Overflow::Extend;

  /** `Log`: the base whose logarithm the position is proportional to.
   *  Above zero and not one; a ladder needs it above one. */
  double base = 10.0;
  /** `Pow`: the exponent. Not zero, which would carry every value onto
   *  the same point. `Sqrt` ignores it and uses 0.5. */
  double exponent = 1.0;
  /** `Symlog`: the distance from zero within which the mapping is
   *  linear. Above zero. */
  double threshold = 1.0;
  /** `Quantize`: how many bands the domain is cut into. `Ordinal`,
   *  `Band` and `Point`: how many entries there are. */
  int steps = 0;
  /** `Band`: the gap between neighbouring bands, as a fraction of one
   *  step, in [0, 1). */
  double padding = 0.0;
  /** `Band` and `Point`: the gap before the first entry and after the
   *  last, as a fraction of one step. */
  double outerPadding = 0.0;
  /** `Threshold`: the ascending values the domain is cut at. */
  std::vector<double> thresholds;

  bool operator==(const Scale&) const = default;

  /** WHERE @p value LANDS, in range units.
   *
   *  A degenerate domain answers the middle of the range rather than
   *  infinity, so an axis built before its data arrived draws a line
   *  through the middle instead of nothing. */
  [[nodiscard]] double apply(double value) const;
  double operator()(double value) const { return apply(value); }

  /** WHICH DOMAIN VALUE @p position CAME FROM — a cursor readout, a
   *  pick, an axis label placed by hand.
   *
   *  `Ordinal`, `Band` and `Point` answer the index of the entry whose
   *  band holds @p position, held inside [0, steps). `Quantize` and
   *  `Threshold` answer where their slot begins, and a `Threshold`
   *  scale's first slot begins at negative infinity because nothing
   *  bounds it below. */
  [[nodiscard]] double invert(double position) const;

  /** WHERE @p value LANDS AS A UNIT NUMBER, before the range is applied
   *  — 0 at the low end of the domain and 1 at the high end, with
   *  `overflow` already honoured. This is what an interpolator over a
   *  range this file cannot name is read at: a colour table, a font
   *  axis, a matrix. */
  [[nodiscard]] double position(double value) const;

  /** @p value carried through @p interpolate, which is any callable
   *  answering a value of its own for a unit position — the general
   *  form of a colour scale, and the reason this file needs no colour
   *  type. */
  template <std::invocable<double> F>
  [[nodiscard]] std::invoke_result_t<F&, double> through(
      double value, F&& interpolate) const {
    return interpolate(position(value));
  }

  /** WHICH SLOT @p value falls in, for a transform that has slots —
   *  `Quantize`, `Threshold`, `Ordinal`, `Band` and `Point`. Held inside
   *  the slot count, so a value past either end reads as a flat band at
   *  that end rather than as a plausible slot from the other. A
   *  continuous transform has no slots and answers 0. */
  [[nodiscard]] long slot(double value) const;

  /** HOW WIDE ONE BAND CAME OUT, in range units, for `Band`. `Point` and
   *  `Ordinal` have no width and answer 0, and so does a continuous
   *  transform. */
  [[nodiscard]] double bandwidth() const;

  /** THE SPACING BETWEEN NEIGHBOURING ENTRIES, in range units, for
   *  `Band`, `Point` and `Ordinal`; 0 for a continuous transform. */
  [[nodiscard]] double stepWidth() const;

  /** DOMAIN VALUES WORTH A LABEL, ascending, about @p count of them.
   *
   *  About, not exactly: a readable ladder matters more than a count,
   *  so the step is rounded to a readable one first and the ticks are
   *  its multiples inside the domain. The count is therefore a request,
   *  and the answer is commonly one or two either side of it.
   *
   *  A `Quantize` or `Threshold` scale answers the values where its
   *  answer changes band, and a `Band`, `Point` or `Ordinal` scale
   *  answers every index; @p count is not read in those four cases. */
  [[nodiscard]] std::vector<double> ticks(int count = 10) const;

  /** THE READABLE STEP a ladder of about @p count ticks would use, in
   *  domain units — for a caller drawing its own ladder, or asking how
   *  coarse one would be before committing to it. 0 when the transform
   *  has no such step. */
  [[nodiscard]] double tickStep(int count = 10) const;

  /** THIS SCALE WITH ITS DOMAIN ROUNDED OUTWARD to the ends of a
   *  readable ladder of about @p count ticks, so the first and last tick
   *  are the ends of the axis and no data falls outside it. Rounding
   *  outward can coarsen the step, which is then rounded outward again,
   *  until the step stops changing.
   *
   *  A discrete transform has no interval to round and answers a copy of
   *  itself. */
  [[nodiscard]] Scale nice(int count = 10) const;
};

}  // namespace sigil::data
