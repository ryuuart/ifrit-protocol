/** @file
 * The seam: opening by extension, from bytes or in memory, and the calls
 * that forward to whichever engine stands behind the value.
 */

#include "sigildata/query/Database.h"

#include <utility>

#include "Engines.h"

namespace sigil::data {

Database::Database(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
Database::Database(Database&&) noexcept = default;
Database& Database::operator=(Database&&) noexcept = default;
Database::~Database() = default;

std::optional<Engine> engineOf(const std::filesystem::path& file) {
  std::string ext = file.extension().string();
  for (char& c : ext) c = (char)std::tolower((unsigned char)c);
  if (ext == ".sqlite" || ext == ".sqlite3" || ext == ".db")
    return Engine::Sqlite;
  if (ext == ".duckdb") return Engine::Duck;
  return std::nullopt;
}

std::optional<Database> Database::open(const std::filesystem::path& file,
                                       std::string* why) {
  return open(file, Access::ReadWrite, why);
}

std::optional<Database> Database::open(const std::filesystem::path& file,
                                       Access access, std::string* why) {
  const std::optional<Engine> engine = engineOf(file);
  if (!engine) {
    if (why) *why = "not a database file by its extension: " + file.string();
    return std::nullopt;
  }
  // DuckDB's file opener also creates stores. Resource opening requires an
  // existing regular file before either engine is asked to interpret it.
  std::error_code error;
  if (!std::filesystem::is_regular_file(file, error)) {
    if (why) *why = "database file is absent or inaccessible: " + file.string();
    return std::nullopt;
  }
  std::unique_ptr<Impl> impl = *engine == Engine::Sqlite
                                   ? detail::openSqlite(file, access, why)
                                   : detail::openDuck(file, access, why);
  if (!impl) return std::nullopt;
  return Database(std::move(impl));
}

std::optional<Database> Database::fromBytes(const io::Bytes& bytes,
                                            std::string_view hint,
                                            std::string* why) {
  const std::optional<Engine> engine = engineOf(std::filesystem::path(hint));
  if (engine == Engine::Duck) {
    if (why) *why = "a DuckDB store opens from a file, not from bytes";
    return std::nullopt;
  }
  std::unique_ptr<Impl> impl = detail::sqliteFromBytes(bytes, why);
  if (!impl) return std::nullopt;
  return Database(std::move(impl));
}

std::optional<Database> Database::memory(Engine engine, std::string* why) {
  std::unique_ptr<Impl> impl =
      engine == Engine::Sqlite ? detail::openSqlite({}, Access::ReadWrite, why)
                               : detail::openDuck({}, Access::ReadWrite, why);
  if (!impl) return std::nullopt;
  return Database(std::move(impl));
}

Engine Database::engine() const { return m_impl->engine(); }
const std::filesystem::path& Database::file() const { return m_impl->file(); }
Access Database::access() const { return m_impl->access(); }

std::optional<Table> Database::query(std::string_view sql,
                                     std::string* why) const {
  return m_impl->query(sql, why);
}

std::optional<bool> Database::writes(std::string_view sql,
                                     std::string* why) const {
  return m_impl->writes(sql, why);
}

bool Database::execute(std::string_view sql, std::string* why) {
  return m_impl->execute(sql, why);
}

bool Database::insert(std::string_view name, const Table& rows,
                      std::string* why) {
  return m_impl->insert(name, rows, why);
}

std::optional<Database> DatabaseDecoder::decode(const io::Bytes& bytes,
                                                std::string_view hint) const {
  // A file on disk is opened in place; bytes alone are a SQLite store.
  // Either way for reading only: one cached resource stands behind every
  // holder, and the file behind it belongs to none of them.
  if (!hint.empty() && std::filesystem::exists(std::filesystem::path(hint)))
    return Database::open(std::filesystem::path(hint), Access::ReadOnly);
  return Database::fromBytes(bytes, hint);
}

}  // namespace sigil::data
