#pragma once

/** @file
 * @ingroup data-decode
 * THE TABLE AND JSON DECODERS a hub reads a resource through. A hub
 * answers a URI with bytes and hands those bytes to whatever decoder is
 * registered for the type asked for; `registerDecoders(hub)` puts these
 * two and the store's on a hub, so a data file, a nested record and an
 * OSC desk are all `load<T>` on that hub, cached and reloaded like
 * anything else it holds.
 */

#include <sigildata/decode/Csv.h>
#include <sigildata/decode/Json.h>
#include <sigildata/table/Table.h>
#include <sigilio/source/Source.h>

#include <expected>
#include <optional>
#include <string_view>

namespace sigil::data {

/** BYTES AS A TABLE, whichever rectangular format they are in. The
 *  format is decided by the resource's name where it has one — `.csv`,
 *  `.tsv`, `.json`, and `.jsonl` or `.ndjson` for JSON lines, a record
 *  to a line — and otherwise by the first character that is
 *  not a space: `[` or `{` is a JSON document and anything else is
 *  delimiter-separated text. The bytes are read as UTF-8. */
struct TableDecoder {
  /** What a delimiter-separated file is read by. Its zeroed properties still
   *  infer per resource, so one decoder serves comma and tab files
   *  alike; setting one pins every resource this decoder reads. */
  CsvOptions csv;

  /** Reads @p bytes as a table, @p hint naming the resource, or says
   *  why they hold none. */
  std::expected<Table, core::Error> decode(const io::Bytes& bytes,
                                           std::string_view hint) const;
};

/** BYTES AS A JSON DOCUMENT — the nested record a rectangle cannot
 *  hold — or as JSON lines or an OSC packet, which read into the same
 *  value.
 *  @trap THE RESOURCE'S NAME decides which, and nothing in the bytes: a
 *  name beginning `osc://` or ending `.osc` is a packet, one ending
 *  `.jsonl` or `.ndjson` is JSON lines, read as the array of its lines'
 *  values, and everything else is JSON text. */
struct JsonDecoder {
  /** Reads @p bytes as a document, @p hint naming the resource, or says
   *  why they are none. */
  std::expected<Json, core::Error> decode(const io::Bytes& bytes,
                                          std::string_view hint) const;
};

}  // namespace sigil::data
