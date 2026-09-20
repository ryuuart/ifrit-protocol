#pragma once

/** @file
 * A MESSAGE MADE READABLE: the same bytes shown seven ways, each of
 * which answers an empty string when the bytes are not that — and the
 * two things a URI says about the wire itself. Nothing here decides
 * what a message IS: a reader wants every reading at once and the
 * emptiness of one is the answer that the bytes are not that.
 */

#include <sigildata/decode/Schema.h>
#include <sigilio/source/Source.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace sigil::seer {

/** The first @p limit bytes as lower-case hexadecimal pairs separated by
 *  single spaces, with an ellipsis after the last pair when the message
 *  is longer than that. Empty bytes answer an empty string. */
std::string hexadecimal(const io::Bytes& bytes, size_t limit = 256);

/** The bytes as text, when every one of them is part of well-formed
 *  UTF-8 and none is a control character other than a tab, a newline or
 *  a carriage return; empty otherwise, a message that is half text not
 *  being text. */
std::string printableText(const io::Bytes& bytes);

/** The bytes as an indented JSON document, when they parse as one;
 *  empty otherwise. Members keep the order the document wrote them, and
 *  a member that is a list or a record of its own is indented under its
 *  key. */
std::string indentedJson(const io::Bytes& bytes);

/** The bytes as an OSC packet — an address with its arguments under
 *  it, or a bundle with the packets it carries and the time they are
 *  for — written out indented, the same shape a document is written in;
 *  empty when the bytes are not a packet. */
std::string oscReading(const io::Bytes& bytes);

/** The bytes as one MIDI message — its kind, the channel it was played
 *  on, the numbers that kind carries and the bytes they went out as,
 *  those standing across one line because they are a run along a cable
 *  rather than a list; empty when the bytes are no message the wire can
 *  carry. */
std::string midiReading(const io::Bytes& bytes);

/** The bytes as one Art-Net packet — a universe of dimmers with the
 *  address it is for, a poll, or whatever else arrived under a name of
 *  its own; empty when the bytes are not a packet. A rig that fits one
 *  line stands whole beside its key and a longer one is rows of
 *  sixteen, which is how a desk counts its channels across. */
std::string dmxReading(const io::Bytes& bytes);

/** The bytes read through @p schema: a buffer that verifies as the
 *  schema's root is converted to the schema's own form, and bytes that
 *  are that form already are converted to a buffer and back, so what a
 *  reader is shown is what a door would hold, defaults and all.
 *  @silent the message is neither, or there is no schema — @p why then
 *  carries the sentence that says which. */
std::string schemaReading(const io::Bytes& bytes, const data::Schema& schema,
                          std::string* why = nullptr);

/** THE WORD THE SCHEME OF @p uri SPEAKS IN: "osc", "midi", "dmx",
 *  "lines", "json", or "bytes" where the scheme carries whatever a
 *  sender puts on it. It is read off the URI and not off a message,
 *  because a reader picks the wire before anything has arrived.
 *  @trap It is what the wire SPEAKS and not what one message turned out
 *  to be, and it is empty for a scheme there is no word for. */
std::string_view dialect(std::string_view uri);

/** The end of @p uri, without the scheme in front of it: what follows
 *  the "://", which for an address a transport spells is the host and
 *  the port. A URI with no scheme in it is itself. */
std::string hostAndPort(std::string_view uri);

}  // namespace sigil::seer
