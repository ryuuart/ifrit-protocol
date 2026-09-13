/** @file
 * What both engines share: the CREATE TABLE a Table's columns spell, the
 * identifier quoting, and the ISO instant a text cell answers as.
 */

#include <sigildata/decode/Csv.h>

#include <string>

#include "Engines.h"

namespace sigil::data::detail {

std::string quoteIdentifier(std::string_view name) {
  std::string out = "\"";
  for (const char c : name) {
    if (c == '"') out += '"';
    out += c;
  }
  out += '"';
  return out;
}

std::string createTableSql(std::string_view name, const Table& rows) {
  std::string sql = "CREATE TABLE " + quoteIdentifier(name) + " (";
  bool first = true;
  for (const Column& column : rows.columns()) {
    if (!first) sql += ", ";
    first = false;
    sql += quoteIdentifier(column.name());
    switch (column.type()) {
      case ColumnType::Number:
        sql += " DOUBLE";
        break;
      case ColumnType::Text:
        sql += " TEXT";
        break;
      case ColumnType::Boolean:
        sql += " BOOLEAN";
        break;
      case ColumnType::Time:
        sql += " TIMESTAMP";
        break;
    }
  }
  sql += ")";
  return sql;
}

std::optional<double> isoSeconds(std::string_view text) {
  const std::optional<Instant> instant = decodeInstant(text);
  if (!instant) return std::nullopt;
  return instant->seconds;
}

}  // namespace sigil::data::detail
