#pragma once

/** @file
 * @ingroup data-decode
 * THE WIRES, AS ONE VALUE: every way this library reads bytes into a
 * `Json` and writes one back out is a `Dialect`, and `decode` and
 * `encode` are the one pair over all of them. A dialect is a value
 * rather than a function per format because a door already carries one —
 * read off its URI's scheme, or named in its options — and a caller
 * holding bytes of its own names the same word.
 *
 * Each wire's codec is this library's own: the bytes are read and
 * written directly, every length on a wire is measured against what
 * arrived before a byte of it is read, and a packet that makes no sense
 * is no packet rather than a diagnostic.
 */

#include <sigildata/decode/Json.h>
#include <sigildata/decode/Schema.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace sigil::data {

/** HOW BYTES READ AS A VALUE. */
enum class Dialect {
  /** JSON text, UTF-8: the value as written. */
  Json,
  /** One Open Sound Control packet: a message reads as
   *  `{"address": "/sky/wind", "arguments": [0.5]}`, a bundle as
   *  `{"bundle": […], "at": …}`, its time in seconds since the start of
   *  1900 or null for "now". A number writes out as a 32-BIT FLOAT
   *  unless it names its width, `{"int": 7}` or `{"double": 0.1}`, so an
   *  integer argument does not survive the round trip. */
  Osc,
  /** One MIDI message: its `kind`, the `channel` it was played on, the
   *  fields that kind carries — `note` and `velocity`, `controller` and
   *  `value`, `bend`, `bytes` — and the `status` and `data` bytes every
   *  message has. A note on at velocity zero reads as a `NoteOff`. */
  Midi,
  /** One Art-Net packet: nearly always a universe of dimmers,
   *  `{"kind": "Dmx", "universe": 0, "sequence": 7, "physical": 0,
   *  "channels": [255, 128, 0, 64]}`; a form with no reading here is
   *  carried whole as `bytes`. */
  ArtNet,
  /** A FlatBuffer, or its schema's own JSON form, read through the
   *  schema handed beside it and rendered as that JSON form. Nothing
   *  reads without a schema. */
  FlatBuffer,
  /** Delimiter-separated text: an array of objects, one per row, keyed
   *  by the header, each cell the number, string or boolean its column
   *  holds; a missing cell is null. */
  Csv,
};

/** THE LARGEST OSC PACKET `encode` WRITES. One packet is one datagram,
 *  and a datagram carries what a UDP payload holds; arguments needing
 *  more are not written at all, half a packet being no shorter message. */
inline constexpr size_t maxOscPacketBytes = 65507;

/** HOW DEEP AN OSC PACKET MAY NEST — a bundle inside a bundle, an array
 *  inside an array. Past this a packet reads as no packet and writes as
 *  no bytes. */
inline constexpr int maxOscBundleDepth = 32;

/** @p bytes READ IN @p dialect, or nothing when they are no message in
 *  it. @p schema is what a `FlatBuffer` reads through; the other
 *  dialects do not read it.
 *  @trap A lone number, string, boolean or null IS a JSON document;
 *  text that is not valid UTF-8, or nests deeper than the parser reads,
 *  is no document at all. */
std::optional<Json> decode(std::span<const std::byte> bytes, Dialect dialect,
                           const Schema& schema = {});

/** The same, over text already in hand. */
std::optional<Json> decode(std::string_view text, Dialect dialect,
                           const Schema& schema = {});

/** @p value WRITTEN IN @p dialect: JSON compact, with the fewest digits
 *  that read back as each number; an OSC message from its `address` and
 *  `arguments`; the MIDI message or Art-Net packet its `kind` names, a
 *  record carrying `bytes` going out as exactly those; the buffer
 *  @p schema makes of it; a CSV of an array of objects. EMPTY where the
 *  value has no spelling in that dialect.
 *  @trap A number that is not finite has no JSON spelling and is
 *  written null; an OSC bundle is read but not written. */
std::vector<std::byte> encode(const Json& value, Dialect dialect,
                              const Schema& schema = {});

/** ONE OSC MESSAGE AS A VALUE: @p arguments under @p address, the form
 *  an OSC packet reads as. A value that is not an array is the one
 *  argument it is. */
Json oscMessage(std::string_view address, Json arguments = Json::Array{});

}  // namespace sigil::data
