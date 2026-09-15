#pragma once

/** @file
 * WHAT A SIGNALLING DOOR CARRIES: the three sentences two ends that
 * cannot dial each other say to introduce themselves, and the JSON text
 * each is written as. Private to the transport feature.
 *
 * It is a format and not a parser of JSON: a flat object of strings,
 * read into the fields named below and written back out of them. The
 * text is what a browser writes with one stringify and reads with one
 * parse, which is the whole reason it is JSON at all.
 */

#include <optional>
#include <string>
#include <string_view>

namespace sigil::io::detail {

/** ONE INTRODUCTION. `kind` says which of the three sentences it is —
 *  an offer, an answer, or one candidate address — `room` says which
 *  conversation it belongs to, so one socket carries as many as there
 *  are rooms on it, and the rest is what that kind carries: a session
 *  description, or an address and the part of the session it belongs
 *  to. A field the kind does not carry is empty. */
struct Introduction {
  std::string kind;
  std::string room;
  std::string sdp;
  std::string candidate;
  std::string mid;
};

/** The introduction @p text spells, or nothing where it is not one flat
 *  JSON object. A FIELD NO INTRODUCTION CARRIES IS STEPPED OVER rather
 *  than refused, whatever kind of value it holds: a page saying more
 *  than these three sentences is still saying them. A text that ends
 *  inside a string or a nesting is nothing at all, a description read
 *  out of half a text not being the one its sender wrote. */
std::optional<Introduction> readIntroduction(std::string_view text);

/** @p message as the JSON text both ends read. A field with nothing in
 *  it is a field left out, so an offer carries a description and a
 *  candidate carries an address. */
std::string writeIntroduction(const Introduction& message);

}  // namespace sigil::io::detail
