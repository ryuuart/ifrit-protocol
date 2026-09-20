---
kind: type
library: SigilMeasure
name: Rescale
qualified: sigil::measure::Rescale
group: Statistics
status: stable
---

# Rescale

## Description

A STRAIGHT-LINE MAP, AND ITS INVERSE. Three numbers rather than a
lambda, so it is a value that can be stored, compared and inverted: a
caption that says "two deviations above the mean" and the mark it labels
go through the same arithmetic, and a reading taken back off a drawing
goes through it the other way.

### It is a statistic, not a drawing's scale

What is here is DERIVED from a run of numbers — where its centre is, how
wide it is — so that runs of different units can be compared. A
drawing's scale is AUTHORED: a domain someone chose, a range in pixels,
a transform, a tick ladder. Deriving one from the data and calling it a
scale is what makes an axis move whenever a new point arrives.

`Rescale::centre` is subtracted from the value first: where the run's
own zero is. `Rescale::scale` is what the centred value is multiplied
by: how the run's own width becomes the answer's. `Rescale::origin` is
added last: where the answer's zero is. `Rescale::invert` answers the
value that maps to a given one, and a map that collapsed the run to a
point cannot be undone and answers the centre.

### The two derivations

`sigil::measure::zScore` is HOW FAR EACH VALUE STANDS FROM THE MEAN, IN
DEVIATIONS — what makes two runs in different units comparable at all: a
frame time and a byte count both become "how unusual is this, for its
own run". A run with no spread has no deviations to count and maps every
value to zero. The deviation is the whole population's, not a sample
estimate's, because the run being standardised is the run in hand.

`sigil::measure::unitRange` is THE RUN SQUEEZED INTO a span, its
smallest value at the low end and its largest at the high one. It is
sensitive to a single outlier by construction — one wild value pushes
everything else into a corner — which is exactly why the ends are
reported and not hidden: read `Moments::min` and `Moments::max` beside
it when the squeeze looks wrong. A run that is all one value has no
width to stretch and maps every value to the low end.

## See also

`sigil::measure::Moments`, which both derivations are read off.
