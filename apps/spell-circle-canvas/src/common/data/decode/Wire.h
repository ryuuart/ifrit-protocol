#pragma once

/** @file
 * Each dialect's own reading and writing, behind `decode` and `encode`:
 * one pair per wire, private to the decode feature, so the public face
 * is the one pair over `Dialect` and a wire's rules are stated once,
 * beside its code.
 */

#include <sigilcore/report/Error.h>
#include <sigildata/decode/Json.h>

#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::data::wire {

/** @p value held inside [@p low, @p high]. A number outside what its
 *  place on a wire holds is written at the nearer end, since wrapping it
 *  would send a value nobody meant — a fixture dark that was meant at
 *  full, a note nobody played; a value that is not a number lands at the
 *  low end, having no side to be on. */
inline double clamped(double value, double low, double high) {
  if (!(value > low)) return low;
  return value > high ? high : value;
}

/** The document in @p text, or the parser's word on why it is not JSON. */
std::expected<Json, core::Error> readJson(std::string_view text);

/** @p value as compact JSON text, which readJson reads back as the same
 *  value; a number that is not finite is written null. */
std::string writeJson(const Json& value);

/** @p text as JSON lines: one document to a line, read into an array in
 *  line order, a blank line skipped; the error names the first line that
 *  is not JSON. */
std::expected<Json, core::Error> readJsonLines(std::string_view text);

/** What a reader that answers nothing beyond the value is handed. */
inline std::optional<Json> held(std::expected<Json, core::Error> answer) {
  if (!answer) return std::nullopt;
  return std::move(*answer);
}

/** Each member of the array @p value as compact JSON on a line of its
 *  own; nothing for a value that is not an array. */
std::string writeJsonLines(const Json& value);

/** @p packet as a message or a bundle, or nothing when the bytes are no
 *  OSC packet. Every length on the wire is a claim judged against what
 *  arrived before a byte of it is read. */
std::optional<Json> readOsc(std::span<const std::byte> packet);

/** A message to @p address carrying @p arguments; no bytes for an
 *  address that is empty or does not begin with a slash, and for
 *  arguments that will not fit one packet. */
std::vector<std::byte> writeOsc(std::string_view address,
                                const Json& arguments);

/** @p message, the `address` and `arguments` form, as a packet; no bytes
 *  for a value with no `address`, and for a bundle. */
std::vector<std::byte> writeOsc(const Json& message);

/** @p message as a value, or nothing when the bytes are no MIDI message:
 *  a message is read whole or not at all, and a note on at velocity
 *  zero reads as a `NoteOff`. */
std::optional<Json> readMidi(std::span<const std::byte> message);

/** @p message as the bytes it goes out as; empty for a value this
 *  cannot spell. */
std::vector<std::byte> writeMidi(const Json& message);

/** @p packet as a value, or nothing when the bytes are not an Art-Net
 *  packet: a wrong name, too few bytes, an older protocol version, or a
 *  dimmer count the packet does not hold. */
std::optional<Json> readArtNet(std::span<const std::byte> packet);

/** @p message as the bytes it goes out as; a `sequence` or `physical`
 *  left out is 0. Empty for a value this cannot spell. */
std::vector<std::byte> writeArtNet(const Json& message);

}  // namespace sigil::data::wire
