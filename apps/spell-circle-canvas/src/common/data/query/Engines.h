#pragma once

/** @file
 * The engine seam behind `Database`: what each engine answers, and the
 * two constructors the seam picks between. Private to the feature.
 */

#include <sigildata/query/Database.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace sigil::data {

struct Database::Impl {
  virtual ~Impl() = default;
  [[nodiscard]] virtual Engine engine() const = 0;
  [[nodiscard]] virtual const std::filesystem::path& file() const = 0;
  [[nodiscard]] virtual std::optional<Table> query(std::string_view sql,
                                                   std::string* why) const = 0;
  virtual bool execute(std::string_view sql, std::string* why) = 0;
  virtual bool insert(std::string_view name, const Table& rows,
                      std::string* why) = 0;
};

namespace detail {

/** An empty path opens a memory store. */
std::unique_ptr<Database::Impl> openSqlite(const std::filesystem::path& file,
                                           std::string* why);
std::unique_ptr<Database::Impl> sqliteFromBytes(const io::Bytes& bytes,
                                                std::string* why);
std::unique_ptr<Database::Impl> openDuck(const std::filesystem::path& file,
                                         std::string* why);

/** The SQL a table is created with from @p rows' columns, and the name
 *  quoted for either engine. Shared so both engines type a Table alike: a
 *  flag column is declared BOOLEAN in both, which DuckDB keeps as one and
 *  SQLite keeps as the declaration its reader types the 0s and 1s by. */
std::string createTableSql(std::string_view name, const Table& rows);
std::string quoteIdentifier(std::string_view name);

/** An ISO-8601 date or timestamp as seconds since the epoch, or nullopt:
 *  what a SQLite DATE or DATETIME column's text answers as an Instant. */
std::optional<double> isoSeconds(std::string_view text);

}  // namespace detail

}  // namespace sigil::data
