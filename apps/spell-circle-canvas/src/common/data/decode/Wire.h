#pragma once

/** @file
 * Each dialect's own reading and writing, behind `decode` and `encode`:
 * one pair per wire, private to the decode feature, so the public face
 * is the one pair over `Dialect` and a wire's rules are stated once,
 * beside its code.
 */

#include <sigildata/decode/Json.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::data::wire {

/** The document in @p text, or nothing when it is not JSON. */
std::optional<Json> readJson(std::string_view text);

/** @p value as compact JSON text, which readJson reads back as the same
 *  value; a number that is not finite is written null. */
std::string writeJson(const Json& value);

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
