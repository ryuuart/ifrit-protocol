#include <sigildata/decode/Json.h>

#include <simdjson.h>

#include <sigildata/decode/Csv.h>

#include <algorithm>
#include <cmath>

namespace sigil::data {

namespace {

const Json& nothing() {
  static const Json empty;
  return empty;
}

Json converted(simdjson::dom::element element) {
  switch (element.type()) {
    case simdjson::dom::element_type::ARRAY: {
      Json::Array items;
      for (simdjson::dom::element item : simdjson::dom::array(element))
        items.push_back(converted(item));
      return Json(std::move(items));
    }
    case simdjson::dom::element_type::OBJECT: {
      Json::Object fields;
      for (auto [key, value] : simdjson::dom::object(element))
        fields.emplace_back(std::string(key), converted(value));
      return Json(std::move(fields));
    }
    case simdjson::dom::element_type::STRING:
      return Json(std::string(std::string_view(element)));
    case simdjson::dom::element_type::INT64:
      return Json(static_cast<double>(int64_t(element)));
    case simdjson::dom::element_type::UINT64:
      return Json(static_cast<double>(uint64_t(element)));
    case simdjson::dom::element_type::DOUBLE:
      return Json(double(element));
    case simdjson::dom::element_type::BOOL:
      return Json(bool(element));
    case simdjson::dom::element_type::BIGINT:
      // An integer too big for a signed or unsigned 64 is too big for the
      // double a number cell holds. Answering it rounded would be a
      // different number; answering nothing makes it a missing cell.
    case simdjson::dom::element_type::NULL_VALUE:
      break;
  }
  return Json();
}

/** One column from the values a key took down the rows, typed by what
 *  every present value of it turns out to be. A value that is a list or
 *  a record of its own has no place in a rectangle and is missing. */
Column columnOf(std::string name, const std::vector<const Json*>& cells) {
  const size_t count = cells.size();
  bool everyNumber = true, everyFlag = true, everyInstant = true;
  bool anyValue = false;
  std::vector<bool> absent(count, false);

  for (size_t row = 0; row < count; ++row) {
    const Json* cell = cells[row];
    if (!cell || cell->null() || cell->kind() == Json::Kind::List ||
        cell->kind() == Json::Kind::Record) {
      absent[row] = true;
      continue;
    }
    anyValue = true;
    if (cell->kind() != Json::Kind::Number) everyNumber = false;
    if (cell->kind() != Json::Kind::Boolean) everyFlag = false;
    if (cell->kind() != Json::Kind::Text || !decodeInstant(cell->text()))
      everyInstant = false;
  }

  Column column;
  if (anyValue && everyFlag) {
    std::vector<Flag> values(count);
    for (size_t row = 0; row < count; ++row)
      if (!absent[row]) values[row] = Flag(cells[row]->boolean());
    column = Column(std::move(name), std::move(values));
  } else if (anyValue && everyInstant) {
    std::vector<Instant> values(count);
    for (size_t row = 0; row < count; ++row)
      if (!absent[row]) values[row] = *decodeInstant(cells[row]->text());
    column = Column(std::move(name), std::move(values));
  } else if (anyValue && everyNumber) {
    std::vector<double> values(count, std::nan(""));
    for (size_t row = 0; row < count; ++row)
      if (!absent[row]) values[row] = cells[row]->number();
    column = Column(std::move(name), std::move(values));
  } else {
    std::vector<std::string> values(count);
    for (size_t row = 0; row < count; ++row) {
      if (absent[row]) continue;
      const Json& cell = *cells[row];
      if (cell.kind() == Json::Kind::Text) {
        values[row] = std::string(cell.text());
      } else if (cell.kind() == Json::Kind::Boolean) {
        values[row] = cell.boolean() ? "true" : "false";
      } else {
        // A number standing in a text column keeps the digits it was
        // written with as far as a round trip through a double can.
        std::string printed = std::to_string(cell.number());
        while (printed.size() > 1 && printed.back() == '0')
          printed.pop_back();
        if (!printed.empty() && printed.back() == '.') printed.pop_back();
        values[row] = std::move(printed);
      }
    }
    column = Column(std::move(name), std::move(values));
  }
  for (size_t row = 0; row < count; ++row)
    if (absent[row]) column.markMissing(row);
  return column;
}

std::optional<Table> fromRecords(std::span<const Json> rows) {
  std::vector<std::string> names;
  for (const Json& row : rows)
    for (const auto& [key, value] : row.fields())
      if (std::ranges::find(names, key) == names.end()) names.push_back(key);
  if (names.empty()) return std::nullopt;

  Table table;
  for (const std::string& name : names) {
    std::vector<const Json*> cells;
    cells.reserve(rows.size());
    for (const Json& row : rows) {
      const Json& cell = row[name];
      cells.push_back(cell.null() ? nullptr : &cell);
    }
    table.add(columnOf(name, cells));
  }
  return table;
}

std::optional<Table> fromLists(std::span<const Json> rows) {
  size_t widest = 0;
  for (const Json& row : rows) widest = std::max(widest, row.size());
  if (widest == 0) return std::nullopt;

  Table table;
  for (size_t i = 0; i < widest; ++i) {
    std::vector<const Json*> cells;
    cells.reserve(rows.size());
    for (const Json& row : rows) {
      const Json& cell = row[i];
      cells.push_back(cell.null() ? nullptr : &cell);
    }
    table.add(columnOf(std::to_string(i + 1), cells));
  }
  return table;
}

std::optional<Table> fromColumns(
    std::span<const std::pair<std::string, Json>> fields) {
  Table table;
  for (const auto& [name, values] : fields) {
    if (values.kind() != Json::Kind::List) continue;
    std::vector<const Json*> cells;
    cells.reserve(values.size());
    for (const Json& cell : values.items())
      cells.push_back(cell.null() ? nullptr : &cell);
    table.add(columnOf(name, cells));
  }
  if (table.columns().empty()) return std::nullopt;
  return table;
}

}  // namespace

bool Json::boolean(bool fallback) const {
  const bool* held = std::get_if<bool>(&m_held);
  return held ? *held : fallback;
}

double Json::number(double fallback) const {
  const double* held = std::get_if<double>(&m_held);
  return held ? *held : fallback;
}

std::string_view Json::text(std::string_view fallback) const {
  const std::string* held = std::get_if<std::string>(&m_held);
  return held ? std::string_view(*held) : fallback;
}

std::span<const Json> Json::items() const {
  const Array* held = std::get_if<Array>(&m_held);
  return held ? std::span<const Json>(*held) : std::span<const Json>{};
}

std::span<const std::pair<std::string, Json>> Json::fields() const {
  const Object* held = std::get_if<Object>(&m_held);
  return held ? std::span<const std::pair<std::string, Json>>(*held)
              : std::span<const std::pair<std::string, Json>>{};
}

size_t Json::size() const {
  if (const Array* list = std::get_if<Array>(&m_held)) return list->size();
  if (const Object* record = std::get_if<Object>(&m_held))
    return record->size();
  return 0;
}

const Json& Json::operator[](std::string_view key) const {
  for (const auto& [name, value] : fields())
    if (name == key) return value;
  return nothing();
}

const Json& Json::operator[](size_t index) const {
  const std::span<const Json> list = items();
  return index < list.size() ? list[index] : nothing();
}

std::optional<Json> decodeJson(std::string_view text) {
  simdjson::dom::parser parser;
  simdjson::dom::element document;
  if (parser.parse(simdjson::padded_string(text)).get(document))
    return std::nullopt;
  return converted(document);
}

std::optional<Table> tableFromJson(const Json& document) {
  if (document.kind() == Json::Kind::Record)
    return fromColumns(document.fields());
  if (document.kind() != Json::Kind::List) return std::nullopt;
  const std::span<const Json> rows = document.items();
  if (rows.empty()) return std::nullopt;
  if (rows.front().kind() == Json::Kind::Record) return fromRecords(rows);
  if (rows.front().kind() == Json::Kind::List) return fromLists(rows);
  return std::nullopt;
}

}  // namespace sigil::data
