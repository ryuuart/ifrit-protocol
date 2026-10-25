/** @file
 * The one-line readers: a hub's bytes, read as a document, a table or a
 * store's answer; and the call that puts this library's decoders on a
 * hub.
 */

#include <sigildata/decode/Decoders.h>
#include <sigildata/query/Database.h>
#include <sigildata/read/Read.h>
#include <sigilio/advanced/Decoding.h>
#include <sigilio/advanced/Places.h>
#include <sigilio/hub/Hub.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <utility>

namespace sigil::data {

namespace {

void say(std::string* why, std::string what) {
  if (why) *why = std::move(what);
}

std::shared_ptr<const io::Bytes> bytesAt(io::Hub& hub, std::string_view uri,
                                         std::string* why) {
  std::shared_ptr<const io::Bytes> bytes = hub.read(uri);
  if (!bytes) say(why, "nothing could be read at " + std::string(uri));
  return bytes;
}

/** The store at @p uri: opened in place where the hub resolves it to a
 *  file, and otherwise from the bytes the hub reads, which a SQLite
 *  store opens from and a DuckDB one does not. */
std::optional<Database> storeAt(io::Hub& hub, std::string_view uri,
                                std::string* why) {
  const std::filesystem::path path = io::resolve(hub, uri);
  std::expected<Database, core::Error> store =
      [&]() -> std::expected<Database, core::Error> {
    if (!path.empty() && std::filesystem::exists(path))
      return Database::open(path, Access::ReadOnly);
    const std::shared_ptr<const io::Bytes> bytes = hub.read(uri);
    if (!bytes)
      return std::unexpected(
          core::Error{"nothing could be read at " + std::string(uri)});
    return Database::fromBytes(*bytes, uri);
  }();
  if (!store) {
    say(why, std::move(store.error().message));
    return std::nullopt;
  }
  return std::move(*store);
}

/** The rows @p answer holds, or none with its error said into @p why. */
Table rowsOf(std::expected<Table, core::Error> answer, std::string* why) {
  if (!answer) {
    say(why, std::move(answer.error().message));
    return {};
  }
  return std::move(*answer);
}

}  // namespace

Json json(io::Hub& hub, std::string_view uri, const ReadOptions& options) {
  const std::shared_ptr<const io::Bytes> bytes = bytesAt(hub, uri, options.why);
  if (!bytes) return {};
  std::expected<Json, core::Error> document = JsonDecoder{}.decode(*bytes, uri);
  if (!document) {
    say(options.why, std::string(uri) + " holds no JSON document: " +
                         document.error().message);
    return {};
  }
  return std::move(*document);
}

Table csv(io::Hub& hub, std::string_view uri, const ReadOptions& options) {
  const std::shared_ptr<const io::Bytes> bytes = bytesAt(hub, uri, options.why);
  if (!bytes) return {};
  std::expected<Table, core::Error> sheet = TableDecoder{}.decode(*bytes, uri);
  if (!sheet) {
    say(options.why,
        std::string(uri) + " holds no table: " + sheet.error().message);
    return {};
  }
  return std::move(*sheet);
}

Table table(io::Hub& hub, std::string_view uri, const TableOptions& options) {
  if (engineOf(std::filesystem::path(uri))) {
    if (options.query.empty()) {
      say(options.why, "a store is read through a query");
      return {};
    }
    const std::optional<Database> store = storeAt(hub, uri, options.why);
    if (!store) return {};
    return rowsOf(store->query(options.query), options.why);
  }
  Table rows = csv(hub, uri, {.why = options.why});
  if (options.query.empty() || rows.columns().empty()) return rows;
  // A query over rows already read runs where SQL over a file of rows
  // runs best: a DuckDB store in memory, the rows written in as `source`.
  std::expected<Database, core::Error> scratch = Database::memory(Engine::Duck);
  if (!scratch) {
    say(options.why, std::move(scratch.error().message));
    return {};
  }
  if (auto written = scratch->insert("source", rows); !written) {
    say(options.why, std::move(written.error().message));
    return {};
  }
  return rowsOf(scratch->query(options.query), options.why);
}

void registerDecoders(io::Hub& hub) {
  io::registerDecoder<Table>(hub, TableDecoder{});
  io::registerDecoder<Json>(hub, JsonDecoder{});
  io::registerDecoder<Database>(hub, DatabaseDecoder{});
}

}  // namespace sigil::data
