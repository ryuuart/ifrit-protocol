#pragma once

/** @file
 * @ingroup data-decode
 * THIS LIBRARY'S DECODERS, and the one call that puts them on a hub. A
 * hub answers a URI with bytes and hands those bytes to whatever
 * decoder is registered for the type asked for; these two are the
 * `Table` decoder and the `Json` decoder, so after
 * `registerDecoders(hub)` a data file, a nested record and an OSC desk
 * are all `load<T>` on that hub, cached and reloaded like anything else
 * it holds. `registerDecoders` is a template over the hub, so this
 * library depends on the byte vocabulary alone.
 */

#include <sigildata/decode/Csv.h>
#include <sigildata/decode/Json.h>
#include <sigildata/decode/Osc.h>
#include <sigildata/table/Table.h>
#include <sigilio/source/Source.h>

#include <optional>
#include <string_view>

namespace sigil::data {

/** BYTES AS A TABLE, whichever rectangular format they are in. The
 *  format is decided by the resource's name where it has one — `.csv`,
 *  `.tsv` and `.json` — and otherwise by the first character that is
 *  not a space: `[` or `{` is a JSON document and anything else is
 *  delimiter-separated text. The bytes are read as UTF-8. */
struct TableDecoder {
  /** What a delimiter-separated file is read by. Its zeroed properties still
   *  infer per resource, so one decoder serves comma and tab files
   *  alike; setting one pins every resource this decoder reads. */
  CsvOptions csv;

  /** Reads @p bytes as a table, @p hint naming the resource. */
  std::optional<Table> decode(const io::Bytes& bytes,
                              std::string_view hint) const;
};

/** BYTES AS A JSON DOCUMENT — the nested record a rectangle cannot
 *  hold — or as an OSC packet, which reads into the same value.
 *  @trap THE RESOURCE'S NAME decides which, and nothing in the bytes: a
 *  name beginning `osc://` or ending `.osc` is a packet, and everything
 *  else is JSON text. */
struct JsonDecoder {
  /** Reads @p bytes as a document, @p hint naming the resource. */
  std::optional<Json> decode(const io::Bytes& bytes,
                             std::string_view hint) const;
};

/** Puts both on @p hub, so `load<Table>` and `load<Json>` answer. A
 *  host calls this once, wherever it builds its hub.
 *  @trap Registering a type again replaces the decoder later asks run,
 *  so a host wanting a pinned delimiter registers its own
 *  `TableDecoder` AFTERWARDS. */
template <typename Hub>
void registerDecoders(Hub& hub) {
  hub.template registerDecoder<Table>(TableDecoder{});
  hub.template registerDecoder<Json>(JsonDecoder{});
}

}  // namespace sigil::data
