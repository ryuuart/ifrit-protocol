/** @file
 * The seam: opening by extension, from bytes or in memory, and the calls
 * that forward to whichever engine stands behind the value.
 */

#include "sigildata/query/Database.h"

#include <string>
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

namespace {

/** The engine's word, as the error a door answers. */
std::unexpected<core::Error> refusal(std::string why) {
  return std::unexpected(core::Error{std::move(why)});
}

}  // namespace

std::expected<Database, core::Error> Database::open(
    const std::filesystem::path& file) {
  return open(file, Access::ReadWrite);
}

std::expected<Database, core::Error> Database::open(
    const std::filesystem::path& file, Access access) {
  const std::optional<Engine> engine = engineOf(file);
  if (!engine)
    return refusal("not a database file by its extension: " + file.string());
  // DuckDB's file opener also creates stores. Resource opening requires an
  // existing regular file before either engine is asked to interpret it.
  std::error_code error;
  if (!std::filesystem::is_regular_file(file, error))
    return refusal("database file is absent or inaccessible: " +
                   file.string());
  std::string why;
  std::unique_ptr<Impl> impl = *engine == Engine::Sqlite
                                   ? detail::openSqlite(file, access, &why)
                                   : detail::openDuck(file, access, &why);
  if (!impl) return refusal(std::move(why));
  return Database(std::move(impl));
}

std::expected<Database, core::Error> Database::fromBytes(
    const io::Bytes& bytes, std::string_view hint) {
  const std::optional<Engine> engine = engineOf(std::filesystem::path(hint));
  if (engine == Engine::Duck)
    return refusal("a DuckDB store opens from a file, not from bytes");
  std::string why;
  std::unique_ptr<Impl> impl = detail::sqliteFromBytes(bytes, &why);
  if (!impl) return refusal(std::move(why));
  return Database(std::move(impl));
}

std::expected<Database, core::Error> Database::memory(Engine engine) {
  std::string why;
  std::unique_ptr<Impl> impl =
      engine == Engine::Sqlite
          ? detail::openSqlite({}, Access::ReadWrite, &why)
          : detail::openDuck({}, Access::ReadWrite, &why);
  if (!impl) return refusal(std::move(why));
  return Database(std::move(impl));
}

Engine Database::engine() const { return m_impl->engine(); }
const std::filesystem::path& Database::file() const { return m_impl->file(); }
Access Database::access() const { return m_impl->access(); }

std::expected<Table, core::Error> Database::query(std::string_view sql) const {
  std::string why;
  std::optional<Table> table = m_impl->query(sql, &why);
  if (!table) return refusal(std::move(why));
  return std::move(*table);
}

std::expected<bool, core::Error> Database::writes(std::string_view sql) const {
  std::string why;
  const std::optional<bool> writing = m_impl->writes(sql, &why);
  if (!writing) return refusal(std::move(why));
  return *writing;
}

std::expected<void, core::Error> Database::execute(std::string_view sql) {
  std::string why;
  if (!m_impl->execute(sql, &why)) return refusal(std::move(why));
  return {};
}

std::expected<void, core::Error> Database::insert(std::string_view name,
                                                  const Table& rows) {
  std::string why;
  if (!m_impl->insert(name, rows, &why)) return refusal(std::move(why));
  return {};
}

std::expected<Database, core::Error> DatabaseDecoder::decode(
    const io::Bytes& bytes, std::string_view hint) const {
  // A file on disk is opened in place; bytes alone are a SQLite store.
  // Either way for reading only: one cached resource stands behind every
  // holder, and the file behind it belongs to none of them.
  if (!hint.empty() && std::filesystem::exists(std::filesystem::path(hint)))
    return Database::open(std::filesystem::path(hint), Access::ReadOnly);
  return Database::fromBytes(bytes, hint);
}

}  // namespace sigil::data
