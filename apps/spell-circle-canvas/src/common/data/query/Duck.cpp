/** @file
 * The DuckDB engine behind `Database`: a file or a memory store, a query
 * typed from the engine's own column types, and a Table written in
 * through the appender.
 */

#include <duckdb.h>

#include <string>
#include <vector>

#include "Engines.h"

namespace sigil::data::detail {

namespace {

constexpr double kMicrosPerSecond = 1e6;
constexpr double kSecondsPerDay = 86400.0;

class Duck final : public Database::Impl {
 public:
  Duck(duckdb_database db, duckdb_connection connection,
       std::filesystem::path file)
      : m_db(db), m_connection(connection), m_file(std::move(file)) {}
  ~Duck() override {
    duckdb_disconnect(&m_connection);
    duckdb_close(&m_db);
  }

  [[nodiscard]] Engine engine() const override { return Engine::Duck; }
  [[nodiscard]] const std::filesystem::path& file() const override {
    return m_file;
  }

  [[nodiscard]] std::optional<Table> query(std::string_view sql,
                                           std::string* why) const override {
    duckdb_result result{};
    const std::string statement(sql);
    if (duckdb_query(m_connection, statement.c_str(), &result) == DuckDBError) {
      if (why) {
        const char* error = duckdb_result_error(&result);
        *why = error ? error : "duckdb error";
      }
      duckdb_destroy_result(&result);
      return std::nullopt;
    }
    const idx_t width = duckdb_column_count(&result);
    const idx_t height = duckdb_row_count(&result);
    Table table;
    for (idx_t c = 0; c < width; ++c) {
      const char* name = duckdb_column_name(&result, c);
      std::string columnName = name ? name : "";
      std::vector<idx_t> nulls;
      Column column;
      switch (duckdb_column_type(&result, c)) {
        case DUCKDB_TYPE_BOOLEAN: {
          std::vector<Flag> cells((size_t)height);
          for (idx_t r = 0; r < height; ++r) {
            if (duckdb_value_is_null(&result, c, r))
              nulls.push_back(r);
            else
              cells[(size_t)r] = Flag(duckdb_value_boolean(&result, c, r));
          }
          column = Column(std::move(columnName), std::move(cells));
          break;
        }
        case DUCKDB_TYPE_TINYINT:
        case DUCKDB_TYPE_SMALLINT:
        case DUCKDB_TYPE_INTEGER:
        case DUCKDB_TYPE_BIGINT:
        case DUCKDB_TYPE_UTINYINT:
        case DUCKDB_TYPE_USMALLINT:
        case DUCKDB_TYPE_UINTEGER:
        case DUCKDB_TYPE_UBIGINT:
        case DUCKDB_TYPE_HUGEINT:
        case DUCKDB_TYPE_UHUGEINT:
        case DUCKDB_TYPE_FLOAT:
        case DUCKDB_TYPE_DOUBLE:
        case DUCKDB_TYPE_DECIMAL: {
          std::vector<double> cells((size_t)height);
          for (idx_t r = 0; r < height; ++r) {
            if (duckdb_value_is_null(&result, c, r))
              nulls.push_back(r);
            else
              cells[(size_t)r] = duckdb_value_double(&result, c, r);
          }
          column = Column(std::move(columnName), std::move(cells));
          break;
        }
        case DUCKDB_TYPE_DATE: {
          std::vector<Instant> cells((size_t)height);
          for (idx_t r = 0; r < height; ++r) {
            if (duckdb_value_is_null(&result, c, r))
              nulls.push_back(r);
            else
              cells[(size_t)r] =
                  Instant{(double)duckdb_value_date(&result, c, r).days *
                          kSecondsPerDay};
          }
          column = Column(std::move(columnName), std::move(cells));
          break;
        }
        case DUCKDB_TYPE_TIMESTAMP:
        case DUCKDB_TYPE_TIMESTAMP_S:
        case DUCKDB_TYPE_TIMESTAMP_MS:
        case DUCKDB_TYPE_TIMESTAMP_NS:
        case DUCKDB_TYPE_TIMESTAMP_TZ: {
          std::vector<Instant> cells((size_t)height);
          for (idx_t r = 0; r < height; ++r) {
            if (duckdb_value_is_null(&result, c, r))
              nulls.push_back(r);
            else
              cells[(size_t)r] =
                  Instant{(double)duckdb_value_timestamp(&result, c, r).micros /
                          kMicrosPerSecond};
          }
          column = Column(std::move(columnName), std::move(cells));
          break;
        }
        default: {
          std::vector<std::string> cells((size_t)height);
          for (idx_t r = 0; r < height; ++r) {
            if (duckdb_value_is_null(&result, c, r)) {
              nulls.push_back(r);
              continue;
            }
            char* text = duckdb_value_varchar(&result, c, r);
            cells[(size_t)r] = text ? text : "";
            duckdb_free(text);
          }
          column = Column(std::move(columnName), std::move(cells));
          break;
        }
      }
      for (const idx_t r : nulls) column.markMissing((size_t)r);
      table.add(std::move(column));
    }
    duckdb_destroy_result(&result);
    return table;
  }

  bool execute(std::string_view sql, std::string* why) override {
    duckdb_result result{};
    const std::string statement(sql);
    const bool ok =
        duckdb_query(m_connection, statement.c_str(), &result) != DuckDBError;
    if (!ok && why) {
      const char* error = duckdb_result_error(&result);
      *why = error ? error : "duckdb error";
    }
    duckdb_destroy_result(&result);
    return ok;
  }

  bool insert(std::string_view name, const Table& rows,
              std::string* why) override {
    const std::string quoted = quoteIdentifier(name);
    if (!execute("DROP TABLE IF EXISTS " + quoted, why) ||
        !execute(createTableSql(name, rows), why))
      return false;
    duckdb_appender appender;
    const std::string table(name);
    if (duckdb_appender_create(m_connection, nullptr, table.c_str(),
                               &appender) == DuckDBError) {
      if (why) *why = "duckdb could not open an appender on " + table;
      return false;
    }
    const std::span<const Column> columns = rows.columns();
    bool ok = true;
    for (size_t row = 0, end = rows.size(); row < end && ok; ++row) {
      for (const Column& column : columns) {
        if (column.missing(row)) {
          duckdb_append_null(appender);
          continue;
        }
        switch (column.type()) {
          case ColumnType::Number:
            duckdb_append_double(appender, column.cells<double>()[row]);
            break;
          case ColumnType::Text:
            duckdb_append_varchar(appender,
                                  column.cells<std::string>()[row].c_str());
            break;
          case ColumnType::Boolean:
            duckdb_append_bool(appender, (bool)column.cells<Flag>()[row]);
            break;
          case ColumnType::Time: {
            duckdb_timestamp stamp;
            stamp.micros = (int64_t)(column.cells<Instant>()[row].seconds *
                                     kMicrosPerSecond);
            duckdb_append_timestamp(appender, stamp);
            break;
          }
        }
      }
      ok = duckdb_appender_end_row(appender) != DuckDBError;
    }
    if (ok) ok = duckdb_appender_flush(appender) != DuckDBError;
    if (!ok && why) {
      const char* error = duckdb_appender_error(appender);
      *why = error ? error : "duckdb could not append";
    }
    duckdb_appender_destroy(&appender);
    return ok;
  }

 private:
  duckdb_database m_db;
  duckdb_connection m_connection;
  std::filesystem::path m_file;
};

}  // namespace

std::unique_ptr<Database::Impl> openDuck(const std::filesystem::path& file,
                                         std::string* why) {
  duckdb_database db = nullptr;
  char* error = nullptr;
  const std::string name = file.string();
  if (duckdb_open_ext(file.empty() ? nullptr : name.c_str(), &db, nullptr,
                      &error) == DuckDBError) {
    if (why) *why = error ? error : "duckdb could not open";
    duckdb_free(error);
    return nullptr;
  }
  duckdb_connection connection = nullptr;
  if (duckdb_connect(db, &connection) == DuckDBError) {
    if (why) *why = "duckdb could not connect";
    duckdb_close(&db);
    return nullptr;
  }
  return std::make_unique<Duck>(db, connection, file);
}

}  // namespace sigil::data::detail
