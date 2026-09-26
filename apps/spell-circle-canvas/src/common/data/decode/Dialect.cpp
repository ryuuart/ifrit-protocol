/** @file
 * The one pair over every wire: which dialect a value is read and
 * written in is a value, and each dialect's own codec stands behind it.
 */

#include <sigildata/decode/Csv.h>
#include <sigildata/decode/Dialect.h>
#include <sigildata/decode/FlatBuffer.h>

#include <algorithm>
#include <string>
#include <utility>
#include <variant>

#include "Wire.h"

namespace sigil::data {

namespace {

std::span<const std::byte> bytesOf(std::string_view text) {
  return std::as_bytes(std::span(text.data(), text.size()));
}

std::string_view textOf(std::span<const std::byte> bytes) {
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

std::vector<std::byte> ownedBytes(std::string_view text) {
  const std::span<const std::byte> bytes = bytesOf(text);
  return {bytes.begin(), bytes.end()};
}

/** One cell as the value a JSON reader expects of it: a number, a
 *  string, a boolean, an instant's seconds, or null where it is missing. */
Json cellOf(const Column& column, size_t row) {
  if (column.missing(row)) return nullptr;
  const Value cell = column.at(row);
  if (const double* number = std::get_if<double>(&cell)) return *number;
  if (const std::string* text = std::get_if<std::string>(&cell)) return *text;
  if (const Flag* flag = std::get_if<Flag>(&cell)) return (bool)*flag;
  return std::get<Instant>(cell).seconds;
}

/** A table as an array of objects, one per row, keyed by column. */
Json rowsOf(const Table& table) {
  Json::Array rows;
  rows.reserve(table.size());
  for (size_t row = 0; row != table.size(); ++row) {
    Json::Object fields;
    for (const Column& column : table.columns())
      fields.emplace_back(column.name(), cellOf(column, row));
    rows.emplace_back(std::move(fields));
  }
  return rows;
}

/** One field as delimiter-separated text writes it: quoted where it
 *  holds the delimiter, a quote or a line break, a quote doubled. */
void writeField(std::string_view field, std::string& out) {
  if (field.find_first_of(",\"\r\n") == std::string_view::npos) {
    out += field;
    return;
  }
  out += '"';
  for (const char c : field) {
    if (c == '"') out += '"';
    out += c;
  }
  out += '"';
}

/** A cell's text: a string as itself, anything else as its JSON. */
std::string fieldOf(const Json& cell) {
  if (cell.null()) return {};
  if (cell.kind() == Json::Kind::String) return std::string(cell.string());
  return wire::writeJson(cell);
}

/** An array of objects as CSV: the header is every key in the order
 *  the rows first write it. Nothing for any other value. */
std::string writeCsv(const Json& value) {
  if (value.kind() != Json::Kind::Array) return {};
  std::vector<std::string> names;
  for (const Json& row : value.array()) {
    if (row.kind() != Json::Kind::Object) return {};
    for (const auto& [name, cell] : row.object())
      if (std::find(names.begin(), names.end(), name) == names.end())
        names.push_back(name);
  }
  if (names.empty()) return {};
  std::string out;
  for (size_t index = 0; index != names.size(); ++index) {
    if (index) out += ',';
    writeField(names[index], out);
  }
  out += '\n';
  for (const Json& row : value.array()) {
    for (size_t index = 0; index != names.size(); ++index) {
      if (index) out += ',';
      writeField(fieldOf(row[names[index]]), out);
    }
    out += '\n';
  }
  return out;
}

std::optional<Json> throughSchema(std::span<const std::byte> bytes,
                                  const Schema& schema) {
  if (!schema) return std::nullopt;
  std::optional<std::string> form;
  if (flatBufferLooksLikeJson(textOf(bytes), {})) {
    const std::optional<std::vector<std::byte>> buffer =
        schema.binary(textOf(bytes));
    if (!buffer) return std::nullopt;
    form = schema.text(*buffer);
  } else {
    form = schema.text(bytes);
  }
  if (!form) return std::nullopt;
  return wire::readJson(*form);
}

}  // namespace

std::optional<Json> decode(std::span<const std::byte> bytes, Dialect dialect,
                           const Schema& schema) {
  switch (dialect) {
    case Dialect::Json:
      return wire::readJson(textOf(bytes));
    case Dialect::Osc:
      return wire::readOsc(bytes);
    case Dialect::Midi:
      return wire::readMidi(bytes);
    case Dialect::ArtNet:
      return wire::readArtNet(bytes);
    case Dialect::FlatBuffer:
      return throughSchema(bytes, schema);
    case Dialect::Csv: {
      const std::optional<Table> table = decodeCsv(textOf(bytes));
      if (!table) return std::nullopt;
      return rowsOf(*table);
    }
  }
  return std::nullopt;
}

std::optional<Json> decode(std::string_view text, Dialect dialect,
                           const Schema& schema) {
  return decode(bytesOf(text), dialect, schema);
}

std::vector<std::byte> encode(const Json& value, Dialect dialect,
                              const Schema& schema) {
  switch (dialect) {
    case Dialect::Json:
      return ownedBytes(wire::writeJson(value));
    case Dialect::Osc:
      return wire::writeOsc(value);
    case Dialect::Midi:
      return wire::writeMidi(value);
    case Dialect::ArtNet:
      return wire::writeArtNet(value);
    case Dialect::FlatBuffer: {
      if (!schema) return {};
      std::optional<std::vector<std::byte>> buffer =
          schema.binary(wire::writeJson(value));
      return buffer ? std::move(*buffer) : std::vector<std::byte>{};
    }
    case Dialect::Csv:
      return ownedBytes(writeCsv(value));
  }
  return {};
}

Json oscMessage(std::string_view address, Json arguments) {
  if (arguments.kind() != Json::Kind::Array)
    arguments = Json(Json::Array{std::move(arguments)});
  return Json(Json::Object{{"address", Json(std::string(address))},
                           {"arguments", std::move(arguments)}});
}

}  // namespace sigil::data
