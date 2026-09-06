#pragma once

/** @file
 * A TABLE OF NAMED, TYPED COLUMNS. Rows of data the way a data file
 * holds them: a number column read as a contiguous span a scale walks
 * straight down, a text column of labels, a boolean column of flags, a
 * time column of instants, and one bit per cell saying whether the
 * source had a value there at all.
 *
 * It is a value: copied, compared and returned. Every reshaping —
 * selecting columns, filtering rows, sorting, grouping — answers a new
 * table and leaves this one alone, and every one of them is the same
 * row-picking underneath, so a filter and a sort compose without either
 * knowing about the other.
 *
 * Standard library only.
 */

#include <compare>
#include <concepts>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace sigil::data {

/** A MOMENT, as seconds since the start of 1970 UTC.
 *
 *  Seconds rather than a clock type because a time column is read as a
 *  span of numbers by a scale and written as a number by an encoder, and
 *  a double holds a second of the current era to well under a
 *  microsecond. What a time PRINTS as belongs to whoever formats it. */
struct Instant {
  double seconds = 0.0;

  auto operator<=>(const Instant&) const = default;
};

/** A BOOLEAN CELL.
 *
 *  Its own type rather than `bool` because a vector of bools is a bit
 *  field that cannot hand out a span, and every column here is read as
 *  one. It converts to `bool`, so a cell reads as a condition. */
struct Flag {
  bool set = false;

  constexpr Flag() = default;
  constexpr Flag(bool value) : set(value) {}
  constexpr operator bool() const { return set; }

  auto operator<=>(const Flag&) const = default;
};

/** What a column holds. */
enum class ColumnType { Number, Text, Boolean, Time };

/** The four things a cell can be. A column holds one of them for every
 *  row; this is one cell read out of it. */
using Value = std::variant<double, std::string, Flag, Instant>;

/** A type a column can be made of. */
template <typename T>
concept Cell = std::same_as<T, double> || std::same_as<T, std::string> ||
               std::same_as<T, Flag> || std::same_as<T, Instant>;

/** Which way an ordering runs. */
enum class Order { Ascending, Descending };

/** ONE COLUMN: a name, the cells, and which of them the source left
 *  empty.
 *
 *  The cells are one contiguous vector of one type — that is the whole
 *  point of a typed column, and it is what lets a scale walk a numeric
 *  column with no copy and no per-cell branch. */
class Column {
 public:
  /** Every storage a column can be. */
  using Cells = std::variant<std::vector<double>, std::vector<std::string>,
                             std::vector<Flag>, std::vector<Instant>>;

  Column() = default;

  template <Cell T>
  Column(std::string name, std::vector<T> cells)
      : m_name(std::move(name)), m_cells(std::move(cells)) {}

  bool operator==(const Column&) const = default;

  const std::string& name() const { return m_name; }
  void rename(std::string name) { m_name = std::move(name); }

  ColumnType type() const { return static_cast<ColumnType>(m_cells.index()); }
  size_t size() const;
  bool empty() const { return size() == 0; }

  /** THE CELLS, as a span of T — empty when this column holds something
   *  other than a T, so a caller that asked for the wrong type reads
   *  nothing rather than reading a reinterpretation. */
  template <Cell T>
  std::span<const T> cells() const {
    const auto* held = std::get_if<std::vector<T>>(&m_cells);
    return held ? std::span<const T>(*held) : std::span<const T>{};
  }

  /** The storage itself, for a caller that dispatches on the type. */
  const Cells& storage() const { return m_cells; }

  /** ONE CELL. A row past the end answers its type's default value, as
   *  a missing cell does. */
  Value at(size_t row) const;

  /** WHETHER THE SOURCE HAD NO VALUE HERE. A row past the end is
   *  missing, and so is a number cell that is not a number, so a gap in
   *  a file and a gap in the arithmetic read the same way. */
  bool missing(size_t row) const;

  /** Records that @p row had no value in the source. The cell keeps
   *  whatever stands in for it. */
  void markMissing(size_t row);

  /** THIS COLUMN'S CELLS AT THOSE ROWS, in that order, keeping the name
   *  and which of them are missing. A row past the end comes through as
   *  a missing cell, so one short column does not shorten a table. */
  [[nodiscard]] Column take(std::span<const size_t> rows) const;

 private:
  std::string m_name;
  Cells m_cells;
  /** Empty while nothing is missing, which is the common case and costs
   *  a column with no gaps nothing. */
  std::vector<bool> m_absent;
};

/** THE TABLE.
 *
 *  ```
 *  Table t;
 *  t.add("month", std::vector<std::string>{"Jan", "Feb"});
 *  t.add("deaths", std::vector<double>{2761, 2120});
 *  t.derive("root", [&](size_t row) {
 *    return std::sqrt(t.column<double>("deaths")[row]);
 *  });
 *
 *  const Table worst = t.sort("deaths", Order::Descending);
 *  for (double d : worst.column<double>("deaths")) mark(d);
 *  ```
 */
class Table {
 public:
  /** One key and the rows that carry it. */
  struct Group {
    Value key;
    std::vector<size_t> rows;
  };

  Table() = default;

  bool operator==(const Table&) const = default;

  /** HOW MANY ROWS — the length of the longest column. A column shorter
   *  than that reads as missing past its end rather than as a row that
   *  is not there, so a file with a short last line is still a table. */
  size_t size() const;
  bool empty() const { return size() == 0; }

  std::span<const Column> columns() const { return m_columns; }

  /** The column called @p name, or null when there is none. */
  const Column* column(std::string_view name) const;
  bool has(std::string_view name) const { return column(name) != nullptr; }

  /** THE CELLS OF @p name AS A SPAN OF T — empty when there is no such
   *  column or it holds something else. */
  template <Cell T>
  std::span<const T> column(std::string_view name) const {
    const Column* found = column(name);
    return found ? found->cells<T>() : std::span<const T>{};
  }

  /** One cell, by column name and row. A column that is not here
   *  answers the number 0, as a row past the end of one that is
   *  answers its type's default. */
  Value cell(std::string_view name, size_t row) const;

  /** Appends @p column, replacing any column of the same name in place
   *  so the column order a file arrived in survives a rewrite. */
  void add(Column column);

  template <Cell T>
  void add(std::string name, std::vector<T> cells) {
    add(Column(std::move(name), std::move(cells)));
  }

  /** Drops the column called @p name; true when there was one. */
  bool remove(std::string_view name);

  /** A COLUMN COMPUTED FROM THE ROWS ALREADY HERE. @p value is called
   *  once per row with the row's index and its answer's type decides
   *  what the column holds. */
  template <typename F>
  void derive(std::string name, F&& value) {
    using T = std::remove_cvref_t<std::invoke_result_t<F&, size_t>>;
    static_assert(Cell<T>,
                  "a derived column holds double, std::string, Flag or "
                  "Instant");
    std::vector<T> cells;
    cells.reserve(size());
    for (size_t row = 0, end = size(); row < end; ++row)
      cells.push_back(value(row));
    add(Column(std::move(name), std::move(cells)));
  }

  /** THE NAMED COLUMNS ONLY, in the order named. A name with no column
   *  behind it is left out rather than answered as an empty one. */
  Table select(std::span<const std::string_view> names) const;
  Table select(std::initializer_list<std::string_view> names) const;

  /** THE ROWS AT THOSE INDICES, in that order — the one reshaping every
   *  other is written in terms of. A row past the end is left out. */
  Table take(std::span<const size_t> rows) const;

  /** The rows @p keep answers true for, in their present order. */
  template <typename F>
  Table filter(F&& keep) const {
    std::vector<size_t> rows;
    for (size_t row = 0, end = size(); row < end; ++row)
      if (keep(row)) rows.push_back(row);
    return take(rows);
  }

  /** THE ROWS ORDERED BY @p name, ties keeping the order they had.
   *  Missing cells go last whichever way the order runs, because a gap
   *  is not the smallest value — it is no value. An unknown column
   *  leaves the order alone. */
  Table sort(std::string_view name, Order order = Order::Ascending) const;

  /** THE ROWS GATHERED BY THE VALUE IN @p name, groups in the order
   *  their key first appears and rows inside a group in the order they
   *  had. Feed a group's rows to `take()` for the sub-table. */
  std::vector<Group> group(std::string_view name) const;

 private:
  std::vector<Column> m_columns;
};

}  // namespace sigil::data
