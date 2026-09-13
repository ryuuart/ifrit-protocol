#pragma once

/** @file
 * @ingroup query
 *
 * `Database` — a SQL engine a drawing asks its data of, behind one seam:
 * SQLite for a file that will be read for decades, DuckDB for the
 * analytics a plot wants over a large table. A query answers a `Table`,
 * the same value a CSV decodes to, so a sketch that shapes its data in SQL
 * and one that shapes it with `Table::filter` draw from the same thing.
 */

#include <sigildata/table/Table.h>
#include <sigilio/source/Source.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace sigil::data {

/** WHICH ENGINE STANDS BEHIND A DATABASE. Both answer the same calls; the
 *  difference is what each is for. SQLite is the file format that stays
 *  readable, one table at a time, and the engine a small store beside a
 *  sketch is written in. DuckDB is the columnar engine that aggregates a
 *  million rows in the time SQLite scans them, reads a CSV or a Parquet
 *  file straight from a query, and holds its store in memory or in a file
 *  of its own. */
enum class Engine { Sqlite, Duck };

/** AN OPEN DATABASE, IN A FILE OR IN MEMORY.
 *
 *  `query()` answers a `Table`: every column of the result typed by what
 *  its cells hold — a number, a text, a flag, an instant — with a NULL
 *  cell marked missing, so a drawing walks it as it walks a decoded CSV.
 *  `execute()` runs a statement for its effect. `insert()` writes a
 *  `Table` in as a named table, typed from the columns, which is how a
 *  CSV a sketch already decoded becomes something a query can join, and
 *  how a store beside a sketch is built in the first place.
 *
 *  A file is opened by its extension — `.sqlite`, `.sqlite3` and `.db`
 *  are SQLite's, `.duckdb` is DuckDB's — and `memory()` opens an empty
 *  store of either engine that lives as long as the value does. The value
 *  is move-only: one connection, owned. */
class Database {
 public:
  Database(Database&&) noexcept;
  Database& operator=(Database&&) noexcept;
  ~Database();

  /** Opens @p file by its extension. Absent, unreadable or of neither
   *  engine's kind: nullopt, and `why` says which. */
  [[nodiscard]] static std::optional<Database> open(
      const std::filesystem::path& file, std::string* why = nullptr);
  /** Opens the bytes of a SQLite file held in memory — the form a
   *  resource hub hands a decoder — as a read-only store. DuckDB opens
   *  files only, so bytes of a `.duckdb` answer nullopt. */
  [[nodiscard]] static std::optional<Database> fromBytes(
      const io::Bytes& bytes, std::string_view hint,
      std::string* why = nullptr);
  /** An empty store of @p engine, in memory. */
  [[nodiscard]] static std::optional<Database> memory(
      Engine engine, std::string* why = nullptr);

  [[nodiscard]] Engine engine() const;
  /** The file this store was opened from, or empty for a memory store. */
  [[nodiscard]] const std::filesystem::path& file() const;

  /** Runs @p sql and answers its rows as a table; nullopt and `why` on an
   *  error. A column's type is what its cells hold: an integer or a real
   *  is a number, text is text, a boolean is a flag, a date or a
   *  timestamp is an instant — SQLite's declared DATE and DATETIME and
   *  ISO text included — and a NULL is a missing cell. */
  [[nodiscard]] std::optional<Table> query(std::string_view sql,
                                           std::string* why = nullptr) const;
  /** Runs @p sql for its effect. */
  bool execute(std::string_view sql, std::string* why = nullptr);
  /** Creates @p name from @p rows' columns — a number is a DOUBLE, text
   *  is TEXT, a flag is a BOOLEAN, an instant is a TIMESTAMP — and writes
   *  every row, a missing cell as NULL. A table of that name already
   *  there is replaced. */
  bool insert(std::string_view name, const Table& rows,
              std::string* why = nullptr);

  struct Impl;

 private:
  explicit Database(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> m_impl;
};

/** DECODES A DATABASE FILE FOR A RESOURCE HUB: `hub.load<Database>(uri)`.
 *  A resource that is a file on disk is opened in place, by its path, so
 *  both engines answer and the store is read as the engine reads it; a
 *  resource that is bytes alone — a network cache with no file, a byte
 *  source — is a SQLite store deserialised from them, and a `.duckdb`
 *  from bytes alone is refused. */
struct DatabaseDecoder {
  std::optional<Database> decode(const io::Bytes& bytes,
                                 std::string_view hint) const;
};

/** The engine a file's extension names, or nullopt. */
[[nodiscard]] std::optional<Engine> engineOf(const std::filesystem::path& file);

}  // namespace sigil::data
