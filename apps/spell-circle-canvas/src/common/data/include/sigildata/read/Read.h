#pragma once

/** @file
 * @ingroup data-read
 * A FILE, READ WHOLE: the one-line readers over a hub. Each asks the hub
 * for the resource's bytes — a mounted `res://` URI, a network URL, a
 * plain path — and reads them as this library's meaning of them, so a
 * program with a hub and no host reads its data in one line:
 * `data::json(hub, "res://content.json")`. Nothing is cached here beyond
 * the bytes the hub caches; `registerDecoders(hub)` is the cached path,
 * `hub.load<Table>(uri)`, for a host that reloads what changed.
 */

#include <sigildata/decode/Json.h>
#include <sigildata/table/Table.h>

#include <string>
#include <string_view>

namespace sigil::io {
class Hub;
}  // namespace sigil::io

namespace sigil::data {

/** How a file is read: `json(hub, uri, {.why = &why})`. */
struct ReadOptions {
  /** What went wrong, where the answer is null or empty. */
  std::string* why = nullptr;
};

/** How a table is read: `table(hub, uri, {.query = "SELECT …"})`. */
struct TableOptions {
  /** SQL to answer instead of the whole file. Over a store — `.sqlite`,
   *  `.sqlite3`, `.db`, `.duckdb` — it runs in that store; over a CSV or
   *  a JSON rectangle it runs in DuckDB with the file's rows as the
   *  table `source`. Empty reads the file whole, which a store cannot
   *  be. */
  std::string query;
  /** What went wrong, where the answer is empty. */
  std::string* why = nullptr;
};

/** THE JSON DOCUMENT AT @p uri — or, where the name begins `osc://` or
 *  ends `.osc`, the OSC packet it holds — or null where it is missing or
 *  is no document. */
Json json(io::Hub& hub, std::string_view uri, const ReadOptions& options = {});

/** THE TABLE IN THE DELIMITER-SEPARATED FILE AT @p uri, the delimiter and
 *  the header sniffed; a `.json` rectangle reads the same way. Empty
 *  where it is missing or holds no field. */
Table csv(io::Hub& hub, std::string_view uri, const ReadOptions& options = {});

/** THE TABLE AT @p uri, from a store, a CSV or a JSON rectangle, whole
 *  or through @p options' query. Empty where the file is missing, the
 *  query fails, or a store was asked for no query. */
Table table(io::Hub& hub, std::string_view uri,
            const TableOptions& options = {});

/** Puts this library's decoders on @p hub — `Table`, `Json` and
 *  `Database` — so `hub.load<Table>(uri)`, `hub.load<Json>(uri)` and
 *  `hub.load<Database>(uri)` answer, cached and reloaded like anything
 *  else the hub holds. A host calls this once, wherever it builds its
 *  hub.
 *  @trap Registering a type again replaces the decoder later asks run,
 *  so a host wanting a pinned delimiter registers its own
 *  `TableDecoder` AFTERWARDS. */
void registerDecoders(io::Hub& hub);

}  // namespace sigil::data
