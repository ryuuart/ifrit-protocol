---
kind: type
library: SigilMeasure
name: Histogram
qualified: sigil::measure::Histogram
group: Statistics
status: stable
---

# Histogram

## Description

EQUAL BINS ACROSS A RANGE, AND WHAT FELL IN EACH.

`sigil::measure::Moments` says how spread a run is in one number; a
histogram says what SHAPE that spread has — one hump or two, a tail on
one side, a wall at a limit. It is also the only summary in this library
that a drawing can be made of directly: the counts are the bar heights
and the edges are where the bars stand.

### What falls outside is counted, not dropped

A value below the low edge or above the high one goes to
`Histogram::below` or `Histogram::above`, so a reader can see that the
range was chosen too narrow. Silently clamping such a value into the end
bin would make a wall that is not in the data, and dropping it would
make a total that does not add up.

`Histogram::over` is the histogram of a run already in hand, over the
range the run itself covers: the natural first look at data whose extent
is not known in advance — and the reason `Histogram::below` and
`Histogram::above` are zero for one of these, since nothing can fall
outside a range taken from the values.

### Weights, not counts

Counts are weights rather than integers so that a run of measurements
with different confidences, or a resampled one, bins without a second
class; an unweighted `Histogram::add` contributes one.

`Histogram::total` is the weight that landed in ANY bin: everything
added but what fell outside. It is what `Histogram::fraction` divides
by. `Histogram::density` is the weight per unit of the value's own axis,
which is what makes two histograms of different bin counts comparable:
the bars sum to one when multiplied by the bin width, whatever the bin
count.

### The bins

`Histogram::binOf` answers which bin a value falls in. The bins are
half-open — a value on a boundary belongs to the bin above it — except
at the top, where the high edge itself belongs to the last bin rather
than to nothing.

`Histogram::edge` is the left edge of a bin, and `edge(bins())` is the
high end, so the edges read as one run of `bins() + 1` values.
`Histogram::centre` is the middle of a bin, where a bar is centred and a
point is plotted. `Histogram::mode` is the heaviest bin, the first of a
tie, and `Histogram::peak` is how heavy it is.

A bin count of zero is one bin. A range that is empty or backwards
collapses to a single point: only the low value itself is inside it, and
it falls in the first bin — never a divide by a width of nothing.
