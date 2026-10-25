#include <sigildata/decode/Csv.h>
#include <sigildata/decode/Json.h>
#include <simdjson.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <ranges>
#include <system_error>

#include "UniqueName.h"
#include "Wire.h"

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
    // A number written whole keeps its integer reading, so a reader that
    // tells integers from fractions sees the one the document wrote.
    case simdjson::dom::element_type::INT64:
      return Json(int64_t(element));
    case simdjson::dom::element_type::UINT64:
      return Json(uint64_t(element));
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
    if (!cell || cell->null() || cell->kind() == Json::Kind::Array ||
        cell->kind() == Json::Kind::Object) {
      absent[row] = true;
      continue;
    }
    anyValue = true;
    if (cell->kind() != Json::Kind::Number) everyNumber = false;
    if (cell->kind() != Json::Kind::Boolean) everyFlag = false;
    if (cell->kind() != Json::Kind::String || !decodeInstant(cell->string()))
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
      if (!absent[row]) values[row] = *decodeInstant(cells[row]->string());
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
      if (cell.kind() == Json::Kind::String) {
        values[row] = std::string(cell.string());
      } else if (cell.kind() == Json::Kind::Boolean) {
        values[row] = cell.boolean() ? "true" : "false";
      } else {
        // A number standing in a text column keeps the digits it was
        // written with as far as a round trip through a double can.
        std::string printed = std::to_string(cell.number());
        while (printed.size() > 1 && printed.back() == '0') printed.pop_back();
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
    for (const auto& [key, value] : row.object())
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
    if (values.kind() != Json::Kind::Array) continue;
    const auto cells = values.array() |
                       std::views::transform([](const Json& cell) {
                         return cell.null() ? nullptr : &cell;
                       }) |
                       std::ranges::to<std::vector<const Json*>>();
    table.add(columnOf(unusedName(table, name), cells));
  }
  if (table.columns().empty()) return std::nullopt;
  return table;
}

/** @p text as a JSON string, quoted, with an escape wherever the format
 *  has no other spelling. */
void writeText(std::string_view text, std::string& out) {
  static constexpr char hexDigits[] = "0123456789abcdef";
  out.push_back('"');
  for (char character : text) {
    switch (character) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default: {
        const auto byte = (unsigned char)character;
        // A byte below a space has no spelling of its own and goes out
        // as an escape. Everything above one is written as it stands,
        // so a run of UTF-8 leaves as the run of UTF-8 it arrived as.
        if (byte >= 0x20) {
          out.push_back(character);
          break;
        }
        out += "\\u00";
        out.push_back(hexDigits[byte >> 4]);
        out.push_back(hexDigits[byte & 0xF]);
      }
    }
  }
  out.push_back('"');
}

void writeNumber(double value, std::string& out) {
  // The shortest spelling that reads back as this number, which for a
  // whole number is the whole number and carries no fraction after it.
  char digits[32];
  const auto [end, failure] =
      std::to_chars(digits, digits + sizeof digits, value);
  // A number that is not finite is not a JSON number at all: null is
  // what a reader gets back for it, so null is what is written.
  if (failure != std::errc() || !std::isfinite(value)) {
    out += "null";
    return;
  }
  out.append(digits, end);
}

void write(const Json& value, std::string& out) {
  switch (value.kind()) {
    case Json::Kind::Null:
      out += "null";
      break;
    case Json::Kind::Boolean:
      out += value.boolean() ? "true" : "false";
      break;
    case Json::Kind::Number:
      if (const std::optional<int64_t> whole = value.integer())
        out += std::to_string(*whole);
      else
        writeNumber(value.number(), out);
      break;
    case Json::Kind::String:
      writeText(value.string(), out);
      break;
    case Json::Kind::Array: {
      out.push_back('[');
      bool first = true;
      for (const Json& item : value.array()) {
        if (!first) out.push_back(',');
        first = false;
        write(item, out);
      }
      out.push_back(']');
      break;
    }
    case Json::Kind::Object: {
      out.push_back('{');
      bool first = true;
      for (const auto& [name, member] : value.object()) {
        if (!first) out.push_back(',');
        first = false;
        writeText(name, out);
        out.push_back(':');
        write(member, out);
      }
      out.push_back('}');
      break;
    }
  }
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

std::string_view Json::string(std::string_view fallback) const {
  const std::string* held = std::get_if<std::string>(&m_held);
  return held ? std::string_view(*held) : fallback;
}

std::span<const Json> Json::array() const {
  const Array* held = std::get_if<Array>(&m_held);
  return held ? std::span<const Json>(*held) : std::span<const Json>{};
}

std::span<const std::pair<std::string, Json>> Json::object() const {
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
  for (const auto& [name, value] : object())
    if (name == key) return value;
  return nothing();
}

const Json& Json::operator[](size_t index) const {
  const std::span<const Json> list = array();
  return index < list.size() ? list[index] : nothing();
}

std::expected<Json, core::Error> wire::readJson(std::string_view text) {
  simdjson::dom::parser parser;
  simdjson::dom::element document;
  if (const auto refused = parser.parse(simdjson::padded_string(text)).get(document))
    return std::unexpected(core::Error{
        std::string("the text is not JSON: ") + simdjson::error_message(refused)});
  return converted(document);
}

std::string wire::writeJson(const Json& value) {
  std::string text;
  write(value, text);
  return text;
}

std::expected<Json, core::Error> wire::readJsonLines(std::string_view text) {
  // One parser for every line, so its buffers are grown once.
  simdjson::dom::parser parser;
  Json::Array values;
  int lineNumber = 0;
  while (!text.empty()) {
    ++lineNumber;
    const size_t end = text.find('\n');
    std::string_view line = text.substr(0, end);
    text = end == std::string_view::npos ? std::string_view{}
                                         : text.substr(end + 1);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' ||
                             line.back() == '\t'))
      line.remove_suffix(1);
    while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
      line.remove_prefix(1);
    if (line.empty()) continue;
    simdjson::dom::element document;
    if (const auto refused =
            parser.parse(simdjson::padded_string(line)).get(document))
      return std::unexpected(core::Error{
          std::string("the line is not JSON: ") +
              simdjson::error_message(refused),
          lineNumber,
          {}});
    values.push_back(converted(document));
  }
  return Json(std::move(values));
}

std::string wire::writeJsonLines(const Json& value) {
  std::string text;
  if (value.kind() != Json::Kind::Array) return text;
  for (const Json& member : value.array()) {
    write(member, text);
    text.push_back('\n');
  }
  return text;
}

std::expected<Table, core::Error> tableFromJson(const Json& document) {
  // Each shape's reader refuses only a rectangle with no column in it.
  const auto rectangle = [](std::optional<Table> table)
      -> std::expected<Table, core::Error> {
    if (!table)
      return std::unexpected(
          core::Error{"the document's rectangle holds no column"});
    return std::move(*table);
  };
  if (document.kind() == Json::Kind::Object)
    return rectangle(fromColumns(document.object()));
  if (document.kind() != Json::Kind::Array)
    return std::unexpected(core::Error{
        "the document is neither a list nor a record, so it holds no "
        "rectangle"});
  const std::span<const Json> rows = document.array();
  if (rows.empty())
    return std::unexpected(core::Error{"the document is an empty list"});
  if (rows.front().kind() == Json::Kind::Object)
    return rectangle(fromRecords(rows));
  if (rows.front().kind() == Json::Kind::Array)
    return rectangle(fromLists(rows));
  return std::unexpected(core::Error{
      "the document's first row is neither a list nor a record, so it holds "
      "no rectangle"});
}

}  // namespace sigil::data
