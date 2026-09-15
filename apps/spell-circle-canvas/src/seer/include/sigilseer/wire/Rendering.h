#pragma once

/** @file
 * A MESSAGE MADE READABLE: the same bytes shown seven ways, each of
 * which answers an empty string when the bytes are not that — and the
 * two things a URI says about the wire itself, the word its scheme
 * speaks in and the short spelling of an address.
 *
 * Nothing here decides what a message IS. A reader looking at an unknown
 * wire wants all of them at once — the bytes as they stand, the text if
 * they happen to be text, the document, the packet, the message off an
 * instrument or the universe off a lighting desk if they happen to be
 * one — and the emptiness of a reading is the answer that they are not.
 *
 * Six of the seven need nothing but the bytes. The seventh needs a
 * schema, because a buffer read in place says nothing about itself: the
 * names of its fields are in the schema and nowhere in the message, so a
 * reader who has not been handed one sees bytes.
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

/** The bytes as one MIDI message — its kind, the channel it was played
 *  on, the numbers that kind carries and the bytes they went out as —
 *  written out indented the way the document and the packet are; empty
 *  when the bytes are no message the wire can carry.
 *
 *  A message's own bytes are a run along a cable rather than a list of
 *  separate values, so they stand across one line instead of one to a
 *  line: what a reader counts through is the run. */
std::string midiReading(const io::Bytes& bytes);

/** The bytes as one Art-Net packet — a universe of dimmers with the
 *  address it is for, a poll, or whatever else arrived under a name of
 *  its own — written out indented the way the document and the packet
 *  are; empty when the bytes are not a packet.
 *
 *  A universe is up to 512 dimmers, and a column of 512 numbers is a
 *  column nobody reads: a rig that fits one line stands whole beside
 *  its key, and a longer one is rows of sixteen, which is how a desk
 *  counts its own channels across. */
std::string dmxReading(const io::Bytes& bytes);

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

/** THE WORD THE SCHEME OF @p uri SPEAKS IN: "osc" for a wire of
 *  packets, "midi" for what an instrument says, "dmx" for a universe of
 *  dimmers, "lines" for a cable that ends a message at a newline,
 *  "json" for a socket that carries documents, and "bytes" where the
 *  scheme carries whatever a sender puts on it. Empty for a scheme
 *  there is no word for, which is a wire that opens and carries
 *  messages all the same — a word put on a scheme nobody taught this
 *  would be a claim about a wire that does not exist.
 *
 *  It is read off the URI and not off a message, because a reader picks
 *  the wire to watch before anything has arrived on any of them. It is
 *  what the wire SPEAKS and not what one message turned out to be: a
 *  socket named for documents still carries a message that is none, and
 *  the readings are where that shows. */
std::string_view dialect(std::string_view uri);

/** The end of @p uri, without the scheme in front of it: what follows
 *  the "://", which for an address a transport spells is the host and
 *  the port. A sender is shown beside the wire it arrived on and that
 *  wire's scheme is spelled there already, so repeating it on every
 *  line would crowd out the part that differs. A URI with no scheme in
 *  it is itself. */
std::string hostAndPort(std::string_view uri);

}  // namespace sigil::seer
