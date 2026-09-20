#pragma once

/** @file
 * @ingroup data-decode
 * A MIDI MESSAGE AS THE ONE DYNAMIC VALUE, and back out again. One
 * message is read into a `Json` carrying its `kind`, the `channel` it
 * was played on where its kind has one, the fields that kind carries —
 * `note` and `velocity`, `controller` and `value`, `bend`, `bytes` —
 * and two every message has: `status`, the byte it opens with, and
 * `data`, the bytes after it. So a reader that knows the wire reads the
 * wire, and a reader that does not reads the names. This codec is its
 * own: it reads and writes the wire directly and names no dependency.
 */

#include <sigildata/decode/Json.h>

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace sigil::data {

/** @p message AS A VALUE, or nothing when the bytes are no MIDI message:
 *  one that opens with a byte the wire cannot open a message with, one
 *  that carries fewer data bytes than its kind takes or more, and one
 *  whose data bytes are not data bytes. A message is read whole or not
 *  at all.
 *  @trap A NOTE ON AT VELOCITY ZERO reads as a `NoteOff`, which is how
 *  a keyboard says a key was released. */
std::optional<Json> decodeMidi(std::span<const std::byte> message);

/** @p message — the kind, channel and fields a message reads as — as
 *  the bytes it goes out on the wire as. Empty for a value this cannot
 *  spell. */
std::vector<std::byte> encodeMidi(const Json& message);

}  // namespace sigil::data
