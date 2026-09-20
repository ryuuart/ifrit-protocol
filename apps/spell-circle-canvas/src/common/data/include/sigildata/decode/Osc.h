#pragma once

/** @file
 * @ingroup data-decode
 * AN OSC PACKET AS THE ONE DYNAMIC VALUE, and back out again: an
 * address, a run of typed arguments under it, and bundles of those to
 * be acted on at one time, all read into a `Json`. A message is
 * `{"address": …, "arguments": […]}`; a bundle is `{"bundle": […],
 * "at": …}`, its time in seconds since the start of 1900 or null for
 * "now". Every number the wire carries reads as a number, and a value
 * writes back out as a 32-BIT FLOAT unless it names its width, so an
 * integer argument does not survive the round trip. This codec is its
 * own: it reads and writes the wire directly and names no dependency.
 */

#include <sigildata/decode/Json.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace sigil::data {

/** THE LARGEST PACKET THIS WRITES. One packet is one datagram, and a
 *  datagram carries what a UDP payload holds. Arguments needing more
 *  than that are not written at all, because half a packet is not a
 *  shorter message. */
inline constexpr size_t maxOscPacketBytes = 65507;

/** HOW DEEP A PACKET MAY NEST — a bundle inside a bundle, an array
 *  inside an array. Past this a packet is read as no packet and
 *  written as no bytes. */
inline constexpr int maxOscBundleDepth = 32;

/** @p packet AS A VALUE — a message, or a bundle of them — or nothing
 *  when the bytes are not an OSC packet.
 *  @trap Every length on the wire is a CLAIM the bytes make about
 *  themselves, judged against what actually arrived before a byte of it
 *  is read, so a packet claiming more than it holds answers nothing
 *  rather than reading past its own end. */
std::optional<Json> decodeOsc(std::span<const std::byte> packet);

/** A MESSAGE TO @p address carrying @p arguments, as the bytes a sender
 *  puts on the wire. @p arguments is the list of them, and a value that
 *  is not a list is the one argument it is. An address that is empty or
 *  does not begin with a slash is no address and answers no bytes, as
 *  do arguments that will not fit one packet. */
std::vector<std::byte> encodeOsc(std::string_view address,
                                 const Json& arguments);

/** @p message — the `address` and `arguments` form a packet reads as —
 *  as the bytes it was read from. A message with no `arguments` member
 *  is a message with no arguments; a value with no `address` is no
 *  message and answers no bytes; a bundle is read but not written. */
std::vector<std::byte> encodeOsc(const Json& message);

}  // namespace sigil::data
