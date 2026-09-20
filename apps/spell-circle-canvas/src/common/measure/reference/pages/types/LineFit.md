---
kind: type
library: SigilMeasure
name: LineFit
qualified: sigil::measure::LineFit
group: Statistics
status: stable
---

# LineFit

## Description

A LINE FITTED BY LEAST SQUARES, and the residuals that say whether the
claim it makes is worth printing. The slope and the intercept are the
answer; `r2`, `maxResidual` and `rmsResidual` are what turns a drawn
line into evidence. A study that quotes a slope without a residual has
stated a preference, not a measurement.

- `stats/Fit.h` — `LineFit`, `slope`, `intercept`, `r2`, `maxResidual`,
  `rmsResidual`, `samples`, `correlation`, `at`, `residual`, `lineFit`

`correlation` is HOW STRONGLY THE TWO RUN TOGETHER, -1 to 1: the square
root of `r2` carrying the slope's sign. It says the same thing `r2`
says about how much was explained, and one thing more that a drawing
usually wants stated — the DIRECTION, so that "they move together" and
"one falls as the other rises" are told apart without reading the slope
in the ordinate's own units.

`r2` is the coefficient of determination, 0 to 1; a run whose abscissae
are all one value has no line to fit and answers 0 with a zero slope.
`maxResidual` is the largest absolute deviation over the points, in the
ordinate's own units, and `rmsResidual` the root mean square of the
same. `at` is the line evaluated at an abscissa, and `residual` how far
a point stands off it, signed.

### The fit

`sigil::measure::lineFit` is ordinary least squares in the ARGUMENT'S
OWN precision: the sums are accumulated in `T`, so a caller that has
always fitted in float gets the float answer it had rather than a double
one rounded back. Fit in double where the answer is the finding and in
float where it feeds a drawing that must not move.

Fewer than two points, or every point at one abscissa, is not a line:
the answer is a zero slope through the mean, with `r2` at 0, which reads
as "nothing was explained" rather than as a divide by zero. The
residuals are still measured, off that flat answer, so a run that has no
slope still reports how far its ordinates stand apart.

The shorter of the two spans is what is read, so a caller with a ragged
pair does not walk off the end of one of them.

### The spread of the abscissae is accumulated, not subtracted

The textbook denominator — n times the sum of the squares less the
square of the sum — is two large numbers differing in their last digits,
and for abscissae that are large and close together (a run of
timestamps, a run of coordinates on a wide sheet) the difference is
nearly all rounding, and in the caller's own float it can vanish or
change sign where there is a real spread to divide by. Each point's
deviation from the mean so far is folded in as it arrives instead, so
the sum is of small numbers and is zero only when the run really is
vertical.

## See also

`sigil::measure::Moments`, which accumulates its spread the same way.
