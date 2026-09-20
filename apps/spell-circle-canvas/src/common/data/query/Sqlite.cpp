/** @file
 * The SQLite engine behind `Database`: a file, a memory store, or the
 * bytes of a file deserialised; a query typed from what its cells hold
 * and the type a column was declared with.
 */

#include <sqlite3.h>

#include <cstring>
#include <string>
#include <vector>

#include "Engines.h"

namespace sigil::data::detail {

namespace {

std::string upper(std::string text) {
  for (char& c : text) c = (char)std::toupper((unsigned char)c);
  return text;
}

/** One result cell as SQLite handed it back, before the column is typed. */
struct Cell {
  enum Kind : uint8_t { Null, Number, Text } kind = Null;
  double number = 0;
  std::string text;
};

struct Held {
  std::string name;
  std::string declared;  ///< the declared type, upper-cased, or empty
  std::vector<Cell> cells;
};

/** THE COLUMN A RUN OF CELLS IS: a declared DATE, TIME or TIMESTAMP whose
 *  cells are ISO text or seconds is a run of instants; a declared BOOL
 *  whose cells are 0 and 1 is a run of flags; every cell a number is a
 *  run of numbers; anything else is text, a number spelled the way SQLite
 *  spells it. A NULL is missing whatever the run is. */
Column typed(Held&& held) {
  const size_t count = held.cells.size();
  bool anyValue = false, everyNumber = true, everyIso = true, everyBit = true;
  for (const Cell& cell : held.cells) {
    if (cell.kind == Cell::Null) continue;
    anyValue = true;
    if (cell.kind != Cell::Number) everyNumber = false;
    if (cell.kind == Cell::Number) {
      if (cell.number != 0 && cell.number != 1) everyBit = false;
    } else {
      everyBit = false;
      if (!isoSeconds(cell.text)) everyIso = false;
    }
  }
  const bool timeDeclared = held.declared.find("DATE") != std::string::npos ||
                            held.declared.find("TIME") != std::string::npos;
  const bool boolDeclared = held.declared.find("BOOL") != std::string::npos;
  Column column;
  if (anyValue && timeDeclared && (everyNumber || everyIso)) {
    std::vector<Instant> cells(count);
    for (size_t row = 0; row < count; ++row) {
      const Cell& cell = held.cells[row];
      if (cell.kind == Cell::Number)
        cells[row] = Instant{cell.number};
      else if (cell.kind == Cell::Text)
        cells[row] = Instant{*isoSeconds(cell.text)};
    }
    column = Column(std::move(held.name), std::move(cells));
  } else if (anyValue && boolDeclared && everyNumber && everyBit) {
    std::vector<Flag> cells(count);
    for (size_t row = 0; row < count; ++row)
      if (held.cells[row].kind == Cell::Number)
        cells[row] = Flag(held.cells[row].number != 0);
    column = Column(std::move(held.name), std::move(cells));
  } else if (anyValue && everyNumber) {
    std::vector<double> cells(count);
    for (size_t row = 0; row < count; ++row)
      if (held.cells[row].kind == Cell::Number)
        cells[row] = held.cells[row].number;
    column = Column(std::move(held.name), std::move(cells));
  } else {
    std::vector<std::string> cells(count);
    for (size_t row = 0; row < count; ++row) {
      const Cell& cell = held.cells[row];
      if (cell.kind == Cell::Text)
        cells[row] = cell.text;
      else if (cell.kind == Cell::Number)
        cells[row] = std::to_string(cell.number);
    }
    column = Column(std::move(held.name), std::move(cells));
  }
  for (size_t row = 0; row < count; ++row)
    if (held.cells[row].kind == Cell::Null) column.markMissing(row);
  return column;
}

class Sqlite final : public Database::Impl {
 public:
  Sqlite(sqlite3* db, std::filesystem::path file, Access access)
      : m_db(db), m_file(std::move(file)), m_access(access) {}
  ~Sqlite() override {
    if (m_db) sqlite3_close(m_db);
  }

  [[nodiscard]] Engine engine() const override { return Engine::Sqlite; }
  [[nodiscard]] const std::filesystem::path& file() const override {
    return m_file;
  }
  [[nodiscard]] Access access() const override { return m_access; }

  [[nodiscard]] std::optional<bool> writes(std::string_view sql,
                                           std::string* why) const override {
    // The text may hold several statements, and the reader is the one that
    // separates them, so each is prepared in turn and the tail carries the
    // rest. Preparing compiles without running.
    const std::string text(sql);
    const char* head = text.c_str();
    const char* end = head + text.size();
    bool any = false;
    while (head < end) {
      sqlite3_stmt* stmt = nullptr;
      const char* tail = nullptr;
      if (sqlite3_prepare_v2(m_db, head, (int)(end - head), &stmt, &tail) !=
          SQLITE_OK) {
        if (why) *why = sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        return std::nullopt;
      }
      if (!stmt) {  // whitespace or a comment after the last statement
        if (!tail || tail <= head) break;
        head = tail;
        continue;
      }
      any = true;
      const bool reads = sqlite3_stmt_readonly(stmt) != 0;
      sqlite3_finalize(stmt);
      if (!reads) return true;
      if (!tail || tail <= head) break;
      head = tail;
    }
    if (!any && why) *why = "no statement to run";
    return any ? std::optional<bool>(false) : std::nullopt;
  }

  [[nodiscard]] std::optional<Table> query(std::string_view sql,
                                           std::string* why) const override {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.data(), (int)sql.size(), &stmt, nullptr) !=
        SQLITE_OK) {
      if (why) *why = sqlite3_errmsg(m_db);
      sqlite3_finalize(stmt);
      return std::nullopt;
    }
    const int width = sqlite3_column_count(stmt);
    std::vector<Held> held((size_t)width);
    for (int c = 0; c < width; ++c) {
      const char* name = sqlite3_column_name(stmt, c);
      held[(size_t)c].name = name ? name : "";
      const char* declared = sqlite3_column_decltype(stmt, c);
      held[(size_t)c].declared = declared ? upper(declared) : "";
    }
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      for (int c = 0; c < width; ++c) {
        Cell cell;
        switch (sqlite3_column_type(stmt, c)) {
          case SQLITE_INTEGER:
          case SQLITE_FLOAT:
            cell.kind = Cell::Number;
            cell.number = sqlite3_column_double(stmt, c);
            break;
          case SQLITE_TEXT: {
            cell.kind = Cell::Text;
            const unsigned char* text = sqlite3_column_text(stmt, c);
            cell.text = text ? (const char*)text : "";
            break;
          }
          default:  // NULL, and a BLOB a drawing has no cell for
            break;
        }
        held[(size_t)c].cells.push_back(std::move(cell));
      }
    }
    if (rc != SQLITE_DONE) {
      if (why) *why = sqlite3_errmsg(m_db);
      sqlite3_finalize(stmt);
      return std::nullopt;
    }
    sqlite3_finalize(stmt);
    Table table;
    for (Held& column : held) table.add(typed(std::move(column)));
    return table;
  }

  bool execute(std::string_view sql, std::string* why) override {
    char* error = nullptr;
    const std::string statement(sql);
    if (sqlite3_exec(m_db, statement.c_str(), nullptr, nullptr, &error) !=
        SQLITE_OK) {
      if (why) *why = error ? error : "sqlite error";
      sqlite3_free(error);
      return false;
    }
    return true;
  }

  bool insert(std::string_view name, const Table& rows,
              std::string* why) override {
    if (!execute("BEGIN", why)) return false;
    const std::string quoted = quoteIdentifier(name);
    if (!execute("DROP TABLE IF EXISTS " + quoted, why) ||
        !execute(createTableSql(name, rows), why)) {
      execute("ROLLBACK", nullptr);
      return false;
    }
    const std::span<const Column> columns = rows.columns();
    std::string sql = "INSERT INTO " + quoted + " VALUES (";
    for (size_t c = 0; c < columns.size(); ++c) sql += c ? ", ?" : "?";
    sql += ")";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
      if (why) *why = sqlite3_errmsg(m_db);
      sqlite3_finalize(stmt);
      execute("ROLLBACK", nullptr);
      return false;
    }
    for (size_t row = 0, end = rows.size(); row < end; ++row) {
      for (size_t c = 0; c < columns.size(); ++c) {
        const Column& column = columns[c];
        const int slot = (int)c + 1;
        if (column.missing(row)) {
          sqlite3_bind_null(stmt, slot);
          continue;
        }
        switch (column.type()) {
          case ColumnType::Number:
            sqlite3_bind_double(stmt, slot, column.cells<double>()[row]);
            break;
          case ColumnType::Text: {
            const std::string& text = column.cells<std::string>()[row];
            sqlite3_bind_text(stmt, slot, text.c_str(), (int)text.size(),
                              SQLITE_TRANSIENT);
            break;
          }
          case ColumnType::Boolean:
            sqlite3_bind_int(stmt, slot, column.cells<Flag>()[row] ? 1 : 0);
            break;
          case ColumnType::Time:
            sqlite3_bind_double(stmt, slot,
                                column.cells<Instant>()[row].seconds);
            break;
        }
      }
      if (sqlite3_step(stmt) != SQLITE_DONE) {
        if (why) *why = sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        execute("ROLLBACK", nullptr);
        return false;
      }
      sqlite3_reset(stmt);
      sqlite3_clear_bindings(stmt);
    }
    sqlite3_finalize(stmt);
    return execute("COMMIT", why);
  }

 private:
  sqlite3* m_db;
  std::filesystem::path m_file;
  Access m_access;
};

}  // namespace

std::unique_ptr<Database::Impl> openSqlite(const std::filesystem::path& file,
                                           Access access, std::string* why) {
  sqlite3* db = nullptr;
  // A memory store has nothing to read until something is written into it,
  // so it opens for writing whatever access the caller named.
  const bool memory = file.empty();
  const Access opened = memory ? Access::ReadWrite : access;
  const std::string name = memory ? ":memory:" : file.string();
  const int flags =
      opened == Access::ReadOnly
          ? SQLITE_OPEN_READONLY
          : SQLITE_OPEN_READWRITE | (memory ? SQLITE_OPEN_CREATE : 0);
  if (sqlite3_open_v2(name.c_str(), &db, flags, nullptr) != SQLITE_OK) {
    if (why) *why = db ? sqlite3_errmsg(db) : "sqlite could not open";
    sqlite3_close(db);
    return nullptr;
  }
  return std::make_unique<Sqlite>(db, file, opened);
}

std::unique_ptr<Database::Impl> sqliteFromBytes(const io::Bytes& bytes,
                                                std::string* why) {
  sqlite3* db = nullptr;
  if (sqlite3_open_v2(":memory:", &db,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                      nullptr) != SQLITE_OK) {
    if (why) *why = db ? sqlite3_errmsg(db) : "sqlite could not open";
    sqlite3_close(db);
    return nullptr;
  }
  const sqlite3_int64 size = (sqlite3_int64)bytes.bytes.size();
  auto* copy = (unsigned char*)sqlite3_malloc64((sqlite3_uint64)size);
  if (!copy) {
    if (why) *why = "sqlite could not hold the bytes";
    sqlite3_close(db);
    return nullptr;
  }
  std::memcpy(copy, bytes.bytes.data(), (size_t)size);
  if (sqlite3_deserialize(db, "main", copy, size, size,
                          SQLITE_DESERIALIZE_FREEONCLOSE |
                              SQLITE_DESERIALIZE_READONLY) != SQLITE_OK) {
    if (why) *why = sqlite3_errmsg(db);
    sqlite3_close(db);
    return nullptr;
  }
  return std::make_unique<Sqlite>(db, std::filesystem::path{},
                                  Access::ReadOnly);
}

}  // namespace sigil::data::detail
