---
kind: type
library: SigilData
name: Scale
qualified: sigil::data::Scale
group: The mapping
status: stable
---

# Scale

## Description

ONE MAPPING VALUE. A scale carries a domain, a range and the transform
between them, and answers three questions about that one mapping: where
a domain value lands (`Scale::apply`), which domain value a range
position came from (`Scale::invert`), and which domain values deserve a
label (`Scale::ticks`).

Everything that would otherwise be a family of functions — remapping,
normalising, constraining, wrapping, log and square-root axes, banded
categories, quantised bands — is a PROP on this one value, so a drawing,
its axis and a cursor readout all go through the same arithmetic instead
of three spellings of it that can drift apart.

Numbers only. A colour range is the caller's interpolator read at
`Scale::position`, so a colour table plugs in without the header knowing
what a colour is. Standard library only.

### Making one

Aggregate-initialised, compared by value and cheap to copy, so a scale
can be a field on a description, bound into a context, or built fresh
per frame.

```cpp
Scale x{.domain = {0, 1000}, .range = {40, 760}};
Scale r{.domain = {0, 1200}, .range = {0, 90},
        .transform = Transform::Sqrt};
Scale month{.range = {0, 360}, .transform = Transform::Band,
            .steps = 12, .padding = 0.1};
```

Only the properties its transform reads matter; the rest keep their
defaults and are ignored.

`sigil::data::Interval` is a closed span of numbers, low end first.
`low > high` is a reversed span and is meaningful: a range that runs
down the screen is how an ordinate is drawn. `Interval::degenerate` is
the one case every mapping here has to answer without dividing.

### The transforms

`sigil::data::Transform` says how a domain value becomes a position. The
first six are continuous: the domain is an interval and every value in
it has a position. The last five are discrete: the input is an index or
a band, and `Scale::steps` says how many there are.

A transform given a parameter it cannot use answers not a number rather
than an arbitrary position — a `Transform::Log` domain that touches or
crosses zero, a base that is not above zero and away from one, a
`Transform::Pow` exponent of zero, a `Transform::Symlog` threshold that
is not above zero.

`Transform::Ordinal` spreads an index in [0, steps) evenly across the
range, first entry at the low end and last at the high one — the plain
reading of "the i-th of n". `Scale::padding` and `Scale::outerPadding`
are not read, and a lone entry stands at the low end, because pinning
the ends is the whole of what that transform promises.
`Transform::Point` is `Transform::Band` whose gap consumes the whole
step, so each entry is a position with no width, the outer padding still
honoured; nothing is pinned to the ends of the range, so a lone entry
stands in the middle of it, which is where one mark belongs.

`Transform::Threshold` cuts the domain at the values in
`Scale::thresholds`, giving `thresholds.size() + 1` slots. The cuts are
read in the order given and a value's slot is how many leading cuts it
is at or past, so a list that does not ascend cuts the domain into slots
that overlap.

`sigil::data::Overflow` says what happens outside the domain. It is
applied to the unit position, so it reads the same on every transform
and in both directions.

### Asking the mapping

`Scale::apply` answers where a value lands, in range units. A degenerate
domain answers the middle of the range rather than infinity, so an axis
built before its data arrived draws a line through the middle instead of
nothing.

`Scale::invert` answers which domain value a position came from — a
cursor readout, a pick, an axis label placed by hand. `Transform::Ordinal`,
`Transform::Band` and `Transform::Point` answer the index of the entry
whose band holds that position, held inside [0, steps).
`Transform::Quantize` and `Transform::Threshold` answer where their slot
begins, and a threshold scale's first slot begins at negative infinity
because nothing bounds it below.

`Scale::position` answers where a value lands as a unit number, before
the range is applied — 0 at the low end of the domain and 1 at the high
end, with the overflow already honoured. This is what an interpolator
over a range the header cannot name is read at: a colour table, a font
axis, a matrix. `Scale::through` carries a value through any callable
answering a value of its own for a unit position — the general form of a
colour scale, and the reason the header needs no colour type.

`Scale::slot` answers which slot a value falls in, for a transform that
has slots. It is held inside the slot count, so a value past either end
reads as a flat band at that end rather than as a plausible slot from
the other. A continuous transform has no slots and answers 0.

`Scale::bandwidth` is how wide one band came out, in range units, for
`Transform::Band`; point and ordinal scales have no width and answer 0,
and so does a continuous transform. `Scale::stepWidth` is the spacing
between neighbouring entries, in range units, for the three discrete
spreads; 0 for a continuous transform.

### Ladders

`Scale::ticks` answers domain values worth a label, ascending, about the
count asked for. About, not exactly: a readable ladder matters more than
a count, so the step is rounded to a readable one first and the ticks
are its multiples inside the domain. The count is therefore a request,
and the answer is commonly one or two either side of it. A quantised or
threshold scale answers the values where its answer changes band, and a
band, point or ordinal scale answers every index; the count is not read
in those four cases.

`Scale::tickStep` is the readable step such a ladder would use, in
domain units — for a caller drawing its own ladder, or asking how coarse
one would be before committing to it.

`Scale::nice` answers this scale with its domain rounded outward to the
ends of a readable ladder, so the first and last tick are the ends of
the axis and no data falls outside it. Rounding outward can coarsen the
step, which is then rounded outward again, until the step stops
changing. A discrete transform has no interval to round and answers a
copy of itself.

## See also

`sigil::data::Table`, whose numeric columns a scale walks.
