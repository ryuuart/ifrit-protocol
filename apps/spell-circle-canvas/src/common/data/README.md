# SigilData

Tabular data, and the one value that maps it onto a drawing. A `Table`
holds named, typed columns — numbers, text, flags, instants — as
contiguous spans a drawing walks straight down, and reshapes by
selecting, filtering, sorting and grouping into new tables. A `Scale`
carries a domain, a range and the transform between them and answers
where a value lands, which value a position came from, and which values
deserve a label. Everything that would otherwise be a family of
functions — remapping, normalising, constraining, wrapping, log and
square-root axes, banded categories, quantised bands — is a **prop** on
that one value, so a mark, its axis and a cursor readout go through the
same arithmetic instead of three spellings of it that can drift apart.

Namespace `sigil::data`. One feature library per directory, linked by
what a consumer uses; every public header lives under
`include/sigildata/<feature>/` and is spelled
`<sigildata/<feature>/X.h>`:

| target | headers | holds |
|--------|---------|-------|
| `SigilDataScale` | `scale/Scale.h` | `Interval`, `Transform`, `Overflow` and `Scale` — the mapping, its inverse, its tick ladder and `nice()` |
| `SigilDataTable` | `table/Table.h` | `Instant`, `Flag`, `Value`, `ColumnType`, `Order`, `Column` and `Table` — named typed columns, the cells as spans, and the reshapings |

`SigilData` is the umbrella target over them, and `<sigildata/Data.h>`
the umbrella header.

## Using it

```cpp
#include <sigildata/scale/Scale.h>
#include <sigildata/table/Table.h>

using namespace sigil::data;

// A linear axis, and the two questions a drawing asks of it.
const Scale x{.domain = {0, 1000}, .range = {40, 760}};
const float px = (float)x(value);
const double under = x.invert(cursorX);

// The ordinate runs UP the page: a reversed range, not a subtraction at
// every call site.
const Scale y{.domain = {0, 120}, .range = {560, 40}};

// An area read as a radius. Sqrt is Pow at a half, named because that is
// the reason a square-root axis exists.
const Scale radius{
    .domain = {0, 1200}, .range = {0, 90}, .transform = Transform::Sqrt};

// Twelve months around a circle, each with a wedge of its own.
const Scale month{.range = {0, 360},
                  .transform = Transform::Band,
                  .steps = 12,
                  .padding = 0.05};
const double startAngle = month(index);
const double wedge = month.bandwidth();

// A ladder whose ends ARE the axis: nice() first, then ticks().
const Scale axis = Scale{.domain = {2.3, 17.6}}.nice(5);
for (double at : axis.ticks(5)) label(axis(at), at);

// A table, and the two ways its cells are read: as one span for the
// whole column, and as one cell where the type is not known ahead.
Table sheet;
sheet.add("month", std::vector<std::string>{"Jan", "Feb"});
sheet.add("deaths", std::vector<double>{2761, 2120});
sheet.derive("root", [&](size_t row) {
  return std::sqrt(sheet.column<double>("deaths")[row]);
});

for (double d : sheet.column<double>("deaths")) mark(radius(d));
const Table worst = sheet.sort("deaths", Order::Descending);
for (const Table::Group& g : sheet.group("month")) draw(sheet.take(g.rows));

// A colour range without a colour type: the caller's own interpolator,
// read at the scale's unit position.
const Scale heat{.domain = {0, 40}, .overflow = Overflow::Clamp};
const Color c = heat.through(temperature, [&](double t) {
  return sampleRamp(stops, (float)t);
});
```

## Mental model

**One value, props not functions.** A `Scale` is an aggregate: fill in
the props its transform reads and leave the rest at their defaults.
`apply` (spelled `operator()`) maps a domain value to a range position,
`invert` goes back, `position` stops at the unit number in between, and
`through` hands that unit number to an interpolator over a range this
library cannot name. `slot`, `bandwidth`, `stepWidth`, `ticks`,
`tickStep` and `nice` are readings of the same value. Two scales with
the same props are the same value, so a scale can be a field on a
description or bound into a context without defeating a structural
comparison.

**The transform is a prop.** `Linear`, `Log`, `Pow`, `Sqrt`, `Symlog`
and `Time` are continuous: the domain is an interval and every value in
it has a position. `Quantize`, `Threshold`, `Ordinal`, `Band` and
`Point` are discrete: the answer is one of a countable set, and `steps`
says how many. `Sqrt` is `Pow` at exponent 0.5 and `Time` is linear in
seconds; both are named because what they mean at a call site is not
"an exponent of a half" or "a linear axis", and because `ticks()`
differs for `Time`.

**Overflow is one rule, applied to the unit position.** `Extend`
continues past both ends, `Clamp` holds at the nearer one, `Wrap`
re-enters at the far one, `PingPong` reflects. It is applied to the unit
number, so it reads the same on every transform and in both directions.

**A degenerate axis maps to the middle.** A domain whose ends are the
same number answers the middle of the range rather than infinity, so an
axis built before its data arrived draws through the middle of the box
instead of drawing nothing.

**A tick is a readable number, not a division.** The step of a ladder is
a power of ten multiplied by 1, 2, 5 or 10, and which of those four is
chosen by comparing the ideal spacing `(high - low) / count` against the
GEOMETRIC midpoints between them — √2, √10 and √50 — because the choice
is between ratios rather than differences: 3 is as far above 2 as it is
below 5 only when the distance is measured multiplicatively. The ticks
are then every multiple of that step inside the domain. So `count` is a
request and the answer is commonly one or two either side of it: a
ladder of 0, 20, 40, 60, 80, 100 for a request of four is the right
answer, and 0, 25, 50, 75, 100 is not, because a reader adds twenties in
their head and does not add twenty-fives.

A step below one is carried as the whole number it divides by rather
than as the fraction it is, because a tenth cannot be written exactly in
binary and five additions of it do not land on a half. Dividing the
index instead puts every tick on the value a reader wrote.

`nice()` rounds the domain outward to the ends of that ladder, so the
first and last tick ARE the ends of the axis and no datum falls outside
it. Rounding outward can coarsen the step, which is rounded outward
again, until the step stops changing.

A `Log` ladder is different in kind: inside a few decades every multiple
of a power of the base is worth a mark (1, 2, … 9, 10, 20, …), and
beyond that only the powers themselves fit, at whatever stride keeps
their number near the request. A `Time` ladder lands on the divisions a
clock face is marked in — 1, 2, 5, 15 and 30 seconds, then minutes,
hours and days — chosen at the same geometric midpoints, because 15 s
and 6 h are readable and 10 s and 5.4 h are not. Past a fortnight there
is no next unit and the power-of-ten rule takes over, in days.

**A discrete scale takes an index.** For `Ordinal`, `Band` and `Point`
the input is the entry's index and `domain` is not read; `steps` is how
many entries there are. `Ordinal` pins the first entry to `range.low`
and the last to `range.high` — the plain "i-th of n", and its lone entry
stands at `range.low`. `Band` gives each entry a width, with `padding`
the gap between neighbours and `outerPadding` the gap at the two ends,
both as fractions of one step; `apply` answers where a band starts and
`bandwidth()` how wide it came out. `Point` is `Band` whose gap consumes
the whole step, so its entries have no width and its lone entry stands
in the middle of the range, which is where one mark belongs.

**Quantize and Threshold band a continuous domain.** `Quantize` cuts the
domain into `steps` equal bands; `Threshold` cuts it at the values in
`thresholds`. `slot()` answers which band a value falls in, held inside
the count so a value past either end reads as a flat band at that end
rather than as a plausible band from the other; `apply()` answers that
band's position in the range, and `ticks()` the values where the answer
changes band.

**A column is one contiguous vector of one type.** That is the whole
point of a typed column: a scale walks a numeric column with no copy and
no per-cell branch. `column<double>("x")` answers that span, and answers
an EMPTY span when the column holds something else or is not there, so a
caller that asked for the wrong type reads nothing rather than reading a
reinterpretation. `cell(name, row)` is the other reading, for code that
does not know the type ahead: one `Value`, which is the four things a
cell can be.

A boolean cell is a `Flag` rather than a `bool` because a vector of
bools is a bit field and cannot hand out a span; it converts to `bool`,
so a cell still reads as a condition. A time cell is an `Instant` —
seconds since the start of 1970 UTC — because a time column is read as
numbers by a scale and written as a number by an encoder, and what a
time PRINTS as belongs to whoever formats it.

**Missing is a bit, not a magic number.** A cell the source left empty
is marked, and the marks cost a column with no gaps nothing. A number
that is not a number reads as missing too, so a gap in a file and a gap
in the arithmetic answer the same way.

**Every reshaping is one row-picking.** `take(rows)` answers the rows at
those indices, in that order, and `filter`, `sort` and `group` all
answer through it. So they compose without knowing about each other, the
table they came from is untouched, and a group is a list of row indices
the caller feeds back to `take()` rather than a second kind of table.

**The table is as long as its longest column.** A column shorter than
that reads as missing past its end rather than as a row that is not
there, so a file with a short last line is still a table.

## Gotchas

`Log` has no answer for a domain that touches or crosses zero: every
mapping from such a scale is not a number, and `ticks()` and `nice()`
answer nothing and leave the domain alone. `Symlog` is what a domain
holding zero and both signs wants instead.

`ticks(count)` is a request, never a promise of `count` values. Code
that needs exactly n divisions wants `Band` or `Quantize` with
`steps = n`, not a tick ladder.

`sort()` puts missing cells last whichever way the order runs, because a
gap is not the smallest value — it is no value. Sorting or taking by a
column that is not there is not an error and answers the table
unchanged, since a name that does not name anything cannot be an order.

`invert()` on a discrete scale answers an INDEX, not a domain value,
because a discrete scale has no domain to answer with. On `Quantize` and
`Threshold` it answers where the slot begins, and a `Threshold` scale's
first slot begins at negative infinity because nothing bounds it below.

`overflow` is applied in both directions, so a `Clamp` scale's
`invert()` cannot answer a value outside the domain. That is what a
cursor readout wants; a caller who needs the extrapolation back keeps a
second scale at `Extend`.

## Boundary

SigilData owns **tabular data and the mappings read off it**: what a row
is, what a column holds, where a value lands. It does not own where the
bytes came from — that is SigilIO, which resolves a URI, caches it and
reloads it — and it does not own what a set of numbers says about a
population, which is SigilMeasure's quantiles, line fits and counters.

Nothing here draws and nothing here knows what a colour is. A colour
scale is this library's `position()` handed to somebody else's
interpolator, which is why `through()` is a template over a callable:
a colour table plugs in from the library that owns colour, and no
dependency runs the other way. A two-dimensional mapping — a map
projection — answers in points and belongs beside the path vocabulary in
SigilGeometry, not here.

`SigilDataScale` and `SigilDataTable` depend on the standard library
alone.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Release
cmake --build build --config Release --target data_test
ctest --test-dir build -C Release -R '^Data' --output-on-failure
```

Targets: `SigilDataTable` (`table/`) with `table/test/`, which pins what
each reshaping answers and what it leaves alone — a filtered table's
source unchanged, a tie keeping its order, a missing cell last both ways
— and `SigilDataScale` (`scale/`) with `scale/test/`, whose every
case asserts one thing the header promises against a closed form worked
out by hand — a half of an area is a half of a radius squared, a
ladder's ends are the ends of a niced domain, a lone entry stands where
its transform says it stands — rather than against whatever the code
happens to answer; and `data_bench`, which times one mapping per call on
each transform a per-mark loop runs through and the tick ladder a redraw
rebuilds along with the reshapings a redraw runs.
