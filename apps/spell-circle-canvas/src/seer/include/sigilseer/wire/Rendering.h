#pragma once

/** @file
 * A MESSAGE MADE READABLE: the same bytes shown five ways, each of
 * which answers an empty string when the bytes are not that — and the
 * short spelling of an address, for the line a reading stands on.
 *
 * Nothing here decides what a message IS. A reader looking at an unknown
 * wire wants all of them at once — the bytes as they stand, the text if
 * they happen to be text, the document or the packet if they happen to
 * be one — and the emptiness of a reading is the answer that they are
 * not.
 *
 * Four of the five need nothing but the bytes. The fifth needs a schema,
 * because a buffer read in place says nothing about itself: the names
 * of its fields are in the schema and nowhere in the message, so a
 * reader who has not been handed one sees bytes.
 */

#include <sigildata/decode/FlatBuffer.h>
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
 *  a carriage return; empty otherwise. A message that is half text is
 *  not text: a reader who was shown the readable half would take the
 *  whole message for a broken string rather than for bytes. */
std::string printableText(const io::Bytes& bytes);

/** The bytes as an indented JSON document, when they parse as one;
 *  empty otherwise. Members keep the order the document wrote them, and
 *  a member that is a list or a record of its own is indented under its
 *  key. */
std::string indentedJson(const io::Bytes& bytes);

/** The bytes as an OSC packet — an address with its arguments under it,
 *  or a bundle with the packets it carries and the time they are for —
 *  written out indented, the same shape a document is written in;
 *  empty when the bytes are not a packet. Two readings of one message
 *  in one shape is how a reader comparing them sees what differs
 *  rather than how each was printed. */
std::string oscReading(const io::Bytes& bytes);

/** The bytes read through @p schema, written out indented the way the
 *  document and the packet are. A buffer that verifies as the schema's
 *  root is converted to the schema's own form; bytes that are that form
 *  already are converted to a buffer and back, so what a reader is
 *  shown is what a door reading this wire through the same schema would
 *  hold, defaults and all. Empty when the message is neither and when
 *  there is no schema, with @p why carrying the sentence that says
 *  which where it is asked for. */
std::string schemaReading(const io::Bytes& bytes, const data::Schema& schema,
                          std::string* why = nullptr);

/** The end of @p uri, without the scheme in front of it: what follows
 *  the "://", which for an address a transport spells is the host and
 *  the port. A sender is shown beside the wire it arrived on and that
 *  wire's scheme is spelled there already, so repeating it on every
 *  line would crowd out the part that differs. A URI with no scheme in
 *  it is itself. */
std::string hostAndPort(std::string_view uri);

}  // namespace sigil::seer
