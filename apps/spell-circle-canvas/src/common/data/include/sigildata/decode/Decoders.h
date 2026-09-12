#pragma once

/** @file
 * THIS LIBRARY'S DECODERS, and the one call that puts them on a hub.
 *
 * A hub answers a URI with bytes and hands those bytes to whatever
 * decoder is registered for the type asked for. These two are the
 * `Table` decoder and the `Json` decoder; after `registerDecoders(hub)`
 * a data file is `hub.load<Table>("res://data/deaths.csv")` and a nested
 * record is `hub.load<Json>("res://data/tree.json")`, cached and
 * reloaded like anything else the hub holds.
 *
 * `registerDecoders` is a template over the hub so this library depends
 * on the byte vocabulary alone and never on the hub, its cache or its
 * codecs.
 */

#include <sigildata/decode/Csv.h>
#include <sigildata/decode/Json.h>
#include <sigildata/table/Table.h>
#include <sigilio/source/Source.h>

#include <optional>
#include <string_view>

namespace sigil::data {

/** BYTES AS A TABLE, whichever rectangular format they are in.
 *
 *  The format is decided by the resource's name where it has one —
 *  `.csv`, `.tsv` and `.json` — and otherwise by the first character
 *  that is not a space: `[` or `{` is a JSON document and anything else
 *  is delimiter-separated text. The bytes are read as UTF-8, and a byte
 *  order mark at the front is not a field. */
struct TableDecoder {
  /** What a delimiter-separated file is read by. Its zeroed properties still
   *  infer per resource, so one decoder serves comma and tab files
   *  alike; setting one pins every resource this decoder reads. */
  CsvOptions csv;

  std::optional<Table> decode(const io::Bytes& bytes,
                              std::string_view hint) const;
};

/** BYTES AS A JSON DOCUMENT — the nested record a rectangle cannot
 *  hold. */
struct JsonDecoder {
  std::optional<Json> decode(const io::Bytes& bytes,
                             std::string_view hint) const;
};

/** Puts both on @p hub, so `load<Table>` and `load<Json>` answer.
 *
 *  A host calls this once, wherever it builds its hub. Registering a
 *  type again replaces the decoder later asks run, so a host that wants
 *  a pinned delimiter registers its own `TableDecoder` afterwards. */
template <typename Hub>
void registerDecoders(Hub& hub) {
  hub.template registerDecoder<Table>(TableDecoder{});
  hub.template registerDecoder<Json>(JsonDecoder{});
}

}  // namespace sigil::data
