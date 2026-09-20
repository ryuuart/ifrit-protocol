---
kind: type
library: SigilData
name: Table
qualified: sigil::data::Table
group: The table
status: stable
---

# Table

## Description

A TABLE OF NAMED, TYPED COLUMNS. Rows of data the way a data file holds
them: a number column read as a contiguous span a scale walks straight
down, a text column of labels, a boolean column of flags, a time column
of instants, and one bit per cell saying whether the source had a value
there at all.

It is a value: copied, compared and returned. Every reshaping —
selecting columns, filtering rows, sorting, grouping — answers a new
table and leaves this one alone, and every one of them is the same
row-picking underneath, so a filter and a sort compose without either
knowing about the other.

```cpp
Table t;
t.add("month", std::vector<std::string>{"Jan", "Feb"});
t.add("deaths", std::vector<double>{2761, 2120});
t.derive("root", [&](size_t row) {
  return std::sqrt(t.column<double>("deaths")[row]);
});

const Table worst = t.sort("deaths", Order::Descending);
for (double d : worst.column<double>("deaths")) mark(d);
```

### The cell types

`sigil::data::Instant` is a moment, as seconds since the start of 1970
UTC. Seconds rather than a clock type because a time column is read as a
span of numbers by a scale and written as a number by an encoder, and a
double holds a second of the current era to well under a microsecond.
What a time PRINTS as belongs to whoever formats it.

`sigil::data::Flag` is a boolean cell, and its own type rather than
`bool` because a vector of bools is a bit field that cannot hand out a
span, and every column here is read as one. It converts to `bool`, so a
cell reads as a condition.

### One column

`sigil::data::Column` is a name, the cells, and which of them the source
left empty. The cells are one contiguous vector of one type — that is
the whole point of a typed column, and it is what lets a scale walk a
numeric column with no copy and no per-cell branch.

`Column::cells` answers a span of T, empty when the column holds
something other than a T, so a caller that asked for the wrong type
reads nothing rather than reading a reinterpretation. `Column::at`
answers one cell, and a row past the end answers its type's default
value, as a missing cell does. `Column::missing` says whether the source
had no value there: a row past the end is missing, and so is a number
cell that is not a number, so a gap in a file and a gap in the
arithmetic read the same way. `Column::take` answers this column's cells
at those rows, in that order, keeping the name and which of them are
missing; a row past the end comes through as a missing cell, so one
short column does not shorten a table.

### What a table answers

`Table::size` is the length of the longest column. A column shorter than
that reads as missing past its end rather than as a row that is not
there, so a file with a short last line is still a table.

`Table::cell` answers one cell by column name and row. A column that is
not here answers the number 0, as a row past the end of one that is
answers its type's default.

`Table::add` appends a column, replacing any column of the same name in
place so the column order a file arrived in survives a rewrite. From the
cells themselves a vector is MOVED in, and any other input range of
cells — an array, a span, a view that computes them as it is walked — is
walked into one; the cell type is the range's, so nothing at the call
site names it twice.

`Table::derive` computes a column from the rows already here: its
callable is called once per row with the row's index, and its answer's
type decides what the column holds.

`Table::select` answers the named columns only, in the order named; a
name with no column behind it is left out rather than answered as an
empty one. `Table::take` answers the rows at those indices, in that
order — the one reshaping every other is written in terms of — and a row
past the end is left out. `Table::filter` answers the rows its predicate
answers true for, in their present order.

`Table::sort` orders the rows by a column, ties keeping the order they
had. Missing cells go last whichever way the order runs, because a gap
is not the smallest value — it is no value. An unknown column leaves the
order alone.

`Table::group` gathers the rows by the value in a column, groups in the
order their key first appears and rows inside a group in the order they
had. Feed a group's rows to `Table::take` for the sub-table.

## See also

`sigil::data::decodeCsv`, `sigil::data::Json`, `sigil::data::Scale`.
