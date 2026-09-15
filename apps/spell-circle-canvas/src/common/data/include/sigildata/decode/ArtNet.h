#pragma once

/** @file
 * AN ART-NET PACKET AS THE ONE DYNAMIC VALUE, and back out again.
 *
 * Art-Net is what the lighting desks say over the network: a universe
 * of up to 512 dimmers, written into one datagram and sent again many
 * times a second, to whichever fixtures on the wire are listening for
 * it. Here a packet is read into a `Json` — the same nested, ordered,
 * comparable value a document is read into — so a sketch lit by a desk
 * and a sketch reading a file work one value rather than two.
 *
 * A UNIVERSE OF DIMMERS is what nearly every packet carries:
 *
 *     {"kind": "Dmx", "universe": 0, "sequence": 7, "physical": 0,
 *      "channels": [255, 128, 0, 64]}
 *
 * `universe` is the fifteen-bit port address the packet is for, which
 * the wire carries in two halves and which is put back together here,
 * so nothing reading this value has to know that it travelled in
 * halves. `sequence` counts the packets of one universe as they are
 * sent, so a receiver can tell a datagram that arrived late from one
 * that arrived twice, and is 0 from a sender that does not count.
 * `physical` is which input of the desk the levels came off, which is
 * something for a person to read and nothing a fixture acts on.
 * `channels` are the dimmers themselves, each 0 to 255, in the order
 * the desk numbers them — so the channel a desk calls 1 is the first
 * member of that list.
 *
 * THE OTHER THREE FORMS. A poll is a desk asking who is out there, and
 * carries nothing a scene acts on but the asking:
 *
 *     {"kind": "Poll"}
 *
 * Its answer is a node's name, its addresses and what it can do, in a
 * layout this has no reading for, so it is carried whole — and so is
 * every other operation, which says which one it was:
 *
 *     {"kind": "PollReply", "bytes": [...]}
 *     {"kind": "ArtNet", "opcode": 24576, "bytes": [...]}
 *
 * WHAT GOES BACK OUT. `encodeArtNet()` writes the same forms: a `Dmx`
 * record is a universe of dimmers, a `Poll` is the question, and a
 * record carrying `bytes` goes out as exactly those bytes, which is how
 * a packet this has no reading for is answered the way it arrived. A
 * level outside what a dimmer holds is written at the nearer end of it,
 * since wrapping it would put a fixture at nothing that was meant to
 * stand at full. A value with neither a kind this knows nor bytes of
 * its own has no spelling on the wire and writes as nothing at all, an
 * empty datagram being no dimmer at all rather than a shorter message.
 *
 * THE WIRE COUNTS ITS DIMMERS IN PAIRS: a packet carries an even number
 * of them, at least two and at most one universe of 512. So a list
 * written odd is padded with one dimmer at nothing, a list past the end
 * of a universe stops there, and a packet that arrived claiming any
 * other count is no packet this reads.
 *
 * This codec is its own: it reads and writes the wire directly, so
 * nothing here or behind it names a dependency and a consumer compiles
 * against the standard library and this library's own value.
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
 *  count the packet does not hold. Every length on the wire is a claim
 *  the bytes make about themselves and is judged against what actually
 *  arrived before a byte of it is read, so a packet claiming more than
 *  it holds answers nothing rather than reading past its own end. */
std::optional<Json> decodeArtNet(std::span<const std::byte> packet);

/** @p message — the kind and the fields a packet reads as — as the
 *  bytes it goes out on the wire as. A `sequence` or a `physical` left
 *  out is 0, which is what a sender that does not count them writes.
 *  Empty for a value this cannot spell. */
std::vector<std::byte> encodeArtNet(const Json& message);

}  // namespace sigil::data
