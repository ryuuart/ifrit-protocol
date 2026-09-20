#pragma once

/** @file
 * @ingroup data-decode
 * AN ART-NET PACKET AS THE ONE DYNAMIC VALUE, and back out again: a
 * universe of up to 512 dimmers, written into one datagram and sent
 * again many times a second, read into a `Json` carrying its `kind` and
 * that kind's own fields — `universe`, `sequence`, `physical` and
 * `channels`, each dimmer 0 to 255 in the order the desk numbers them,
 * for the `Dmx` form nearly every packet is. A form this has no reading
 * for is carried whole as `bytes` and goes back out as exactly those.
 * This codec is its own: it reads and writes the wire directly and
 * names no dependency.
 */

#include <sigildata/decode/Json.h>

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace sigil::data {

/** @p packet AS A VALUE — a universe of dimmers, a poll, its answer, or
 *  whatever else arrived under a name of its own — or nothing when the
 *  bytes are not an Art-Net packet: bytes that do not open with the
 *  name every one of them opens with, bytes too few to say what they
 *  are, a sender writing an older version of the protocol, and a dimmer
 *  count the packet does not hold.
 *  @trap Every length on the wire is a CLAIM the bytes make about
 *  themselves, judged against what actually arrived before a byte of it
 *  is read. */
std::optional<Json> decodeArtNet(std::span<const std::byte> packet);

/** @p message — the kind and the fields a packet reads as — as the
 *  bytes it goes out on the wire as. A `sequence` or a `physical` left
 *  out is 0, which is what a sender that does not count them writes.
 *  Empty for a value this cannot spell. */
std::vector<std::byte> encodeArtNet(const Json& message);

}  // namespace sigil::data
