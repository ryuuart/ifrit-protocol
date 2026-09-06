#include <sigildata/table/Table.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace sigil::data {

namespace {

/** Whether a cell of type T is one the arithmetic cannot use. Only a
 *  number has such a state of its own; every other cell is missing only
 *  because the source said so. */
template <typename T>
bool unusable(const T& cell) {
  if constexpr (std::same_as<T, double>) return std::isnan(cell);
  else return false;
}

}  // namespace

size_t Column::size() const {
  return std::visit([](const auto& cells) { return cells.size(); }, m_cells);
}

Value Column::at(size_t row) const {
  return std::visit(
      [row](const auto& cells) -> Value {
        using T = typename std::remove_cvref_t<decltype(cells)>::value_type;
        return row < cells.size() ? Value(cells[row]) : Value(T{});
      },
      m_cells);
}

bool Column::missing(size_t row) const {
  if (row >= size()) return true;
  if (row < m_absent.size() && m_absent[row]) return true;
  return std::visit([row](const auto& cells) { return unusable(cells[row]); },
                    m_cells);
}

void Column::markMissing(size_t row) {
  if (row >= size()) return;
  if (m_absent.empty()) m_absent.assign(size(), false);
  m_absent[row] = true;
}

Column Column::take(std::span<const size_t> rows) const {
  Column picked;
  picked.m_name = m_name;
  std::visit(
      [&](const auto& cells) {
        using T = typename std::remove_cvref_t<decltype(cells)>::value_type;
        std::vector<T> kept;
        kept.reserve(rows.size());
        for (size_t row : rows)
          kept.push_back(row < cells.size() ? cells[row] : T{});
        picked.m_cells = std::move(kept);
      },
      m_cells);
  // Only what the SOURCE said was absent is carried over; a number that
  // is not a number already reads as missing wherever it lands, and
  // recording it again would make a column compare unequal to the same
  // column taken whole.
  for (size_t i = 0; i < rows.size(); ++i) {
    const size_t row = rows[i];
    if (row >= size() || (row < m_absent.size() && m_absent[row]))
      picked.markMissing(i);
  }
  return picked;
}

size_t Table::size() const {
  size_t rows = 0;
  for (const Column& column : m_columns) rows = std::max(rows, column.size());
  return rows;
}

const Column* Table::column(std::string_view name) const {
  for (const Column& held : m_columns)
    if (held.name() == name) return &held;
  return nullptr;
}

Value Table::cell(std::string_view name, size_t row) const {
  const Column* found = column(name);
  return found ? found->at(row) : Value(0.0);
}

void Table::add(Column added) {
  for (Column& held : m_columns)
    if (held.name() == added.name()) {
      held = std::move(added);
      return;
    }
  m_columns.push_back(std::move(added));
}

bool Table::remove(std::string_view name) {
  const auto found = std::ranges::find_if(
      m_columns, [name](const Column& held) { return held.name() == name; });
  if (found == m_columns.end()) return false;
  m_columns.erase(found);
  return true;
}

Table Table::select(std::span<const std::string_view> names) const {
  Table chosen;
  for (std::string_view name : names)
    if (const Column* found = column(name)) chosen.add(*found);
  return chosen;
}

Table Table::select(std::initializer_list<std::string_view> names) const {
  return select(std::span<const std::string_view>(names.begin(), names.size()));
}

Table Table::take(std::span<const size_t> rows) const {
  const size_t end = size();
  std::vector<size_t> inside;
  inside.reserve(rows.size());
  for (size_t row : rows)
    if (row < end) inside.push_back(row);

  Table picked;
  picked.m_columns.reserve(m_columns.size());
  for (const Column& held : m_columns) picked.m_columns.push_back(held.take(inside));
  return picked;
}

Table Table::sort(std::string_view name, Order order) const {
  const Column* by = column(name);
  if (!by) return *this;

  std::vector<size_t> rows(size());
  std::iota(rows.begin(), rows.end(), size_t{0});
  const bool descending = order == Order::Descending;
  std::ranges::stable_sort(rows, [&](size_t a, size_t b) {
    const bool absentA = by->missing(a);
    const bool absentB = by->missing(b);
    if (absentA || absentB) return !absentA && absentB;
    return std::visit(
        [&](const auto& cells) {
          return descending ? cells[b] < cells[a] : cells[a] < cells[b];
        },
        by->storage());
  });
  return take(rows);
}

std::vector<Table::Group> Table::group(std::string_view name) const {
  std::vector<Group> groups;
  const Column* by = column(name);
  if (!by) return groups;
  for (size_t row = 0, end = size(); row < end; ++row) {
    Value key = by->at(row);
    const auto found = std::ranges::find_if(
        groups, [&key](const Group& group) { return group.key == key; });
    if (found != groups.end())
      found->rows.push_back(row);
    else
      groups.push_back(Group{std::move(key), {row}});
  }
  return groups;
}

}  // namespace sigil::data
