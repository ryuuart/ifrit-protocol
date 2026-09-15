#pragma once

/** @file
 * A MESSAGE MADE READABLE: the same bytes shown three ways, each of
 * which answers an empty string when the bytes are not that.
 *
 * Nothing here decides what a message IS. A reader looking at an unknown
 * wire wants all three at once — the bytes as they stand, the text if
 * they happen to be text, the document if they happen to be one — and
 * the emptiness of a reading is the answer that they are not.
 */

#include <sigilio/source/Source.h>

#include <cstddef>
#include <string>

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

}  // namespace sigil::seer
