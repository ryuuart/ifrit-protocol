#pragma once

/** @file
 * AN OSC PACKET AS THE ONE DYNAMIC VALUE, and back out again.
 *
 * Open Sound Control is what the performance tools speak to one
 * another: an address, a run of typed arguments under it, and bundles
 * of those to be acted on at one time. Here a packet is read into a
 * `Json` — the same nested, ordered, comparable value a document is
 * read into — so a sketch listening to a desk and a sketch reading a
 * file work one value rather than two.
 *
 * A MESSAGE is its address and its arguments:
 *
 *     {"address": "/sky/wind", "arguments": [0.5, "gust", true, null]}
 *
 * A BUNDLE is its elements and the time they are for, in seconds since
 * the start of 1900, which is the era the protocol counts from, or null
 * when the bundle says "now". Its elements are messages and bundles,
 * nested the way the packet nests them:
 *
 *     {"bundle": [{"address": "/sky/wind", "arguments": [0.5]}], "at": null}
 *
 * WHAT AN ARGUMENT READS AS. Every number the wire carries — a 32- or
 * 64-bit whole number, a single or double float, a timetag, a
 * character's code — reads as a number, because a number is what a
 * drawing asks it for. A string and a symbol are text. True and false
 * are booleans, and so is an impulse, the argument whose whole content
 * is that it arrived. Nil is null. The three arguments that are bytes
 * with meanings of their own keep them, each a list of byte numbers in
 * the order the wire writes them: a blob is `{"blob": [0, 255]}`, a
 * colour `{"rgba": [red, green, blue, alpha]}` and a MIDI message
 * `{"midi": [port, status, first, second]}`. Arguments the sender
 * bracketed as an array are a list.
 *
 * WHAT A VALUE WRITES AS. A NUMBER IS A 32-BIT FLOAT, always. A number
 * carries no memory of the width it arrived at, and what the
 * performance tools expect under a continuous control is a float, so a
 * number goes out as one rather than as whichever width its value would
 * fit. A value that must go out at another width says so by name:
 * `{"int": 7}` is a 32-bit whole number and `{"double": 0.1}` a 64-bit
 * float. Text is a string, a boolean is true or false, null is nil, a
 * list is an array, and a blob, a colour and a MIDI message are the
 * three records above. A record with none of those five keys has no
 * spelling on the wire and writes as nil, so the arguments keep their
 * count and the positions a reader counts on do not shift.
 *
 * SO AN INTEGER ARGUMENT DOES NOT SURVIVE THE ROUND TRIP, by design: a
 * 32-bit whole number reads as a plain number and writes back out as a
 * float. So does a 64-bit float, a timetag, a character and a 64-bit
 * whole number; a symbol comes back a string and an impulse a true.
 * Reading a packet and writing the reading gives the packet back byte
 * for byte when every argument is a float, a string, a boolean, a nil,
 * a blob, a colour, a MIDI message or an array of those. Going the
 * other way, a number that a 32-bit float does not hold exactly comes
 * back rounded to what one holds, and a number outside what its place
 * on the wire holds at all is written at the nearer end of it, since
 * wrapping it would put a value there that nobody meant.
 *
 * This codec is its own: it reads and writes the wire directly, so
 * nothing here or behind it names a dependency and a consumer compiles
 * against the standard library and this library's own value.
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
 *  inside an array. Nesting is a schedule inside a schedule, which a
 *  few levels say everything a sender means by; past this a packet is
 *  walking its reader down a stack rather than saying anything, and is
 *  read as no packet and written as no bytes. */
inline constexpr int maxOscBundleDepth = 32;

/** @p packet AS A VALUE — a message, or a bundle of them — or nothing
 *  when the bytes are not an OSC packet. Every length on the wire is a
 *  claim the bytes make about themselves — a blob's, a bundle
 *  element's, the null a string ends at — and each is judged against
 *  what actually arrived before a byte of it is read, so a packet that
 *  claims more than it holds answers nothing rather than reading past
 *  its own end. */
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
