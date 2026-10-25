#pragma once

/** @file
 * @ingroup core-report
 *
 * WHY A DOOR REFUSED: the one error every library's refusing door
 * answers, so a caller reads one shape whichever library said no.
 */

#include <expected>
#include <optional>
#include <string>

namespace sigil::core {

/** WHY SOMETHING COULD NOT BE DONE — what a door that can refuse answers
 *  beside its value, as `std::expected<T, core::Error>`: a decoder that
 *  could not read its bytes, a database that would not open, a device
 *  that could not be made. The message is written for a person and names
 *  what was refused and why; the line says where in a text resource the
 *  refusal is about, and the code carries a number the refusing library
 *  or the protocol a refusal travels over gives its kinds of refusal.
 *  A library whose own error carries more converts to this one, never
 *  the reverse. */
struct Error {
  /** What was refused and why, in words that read on their own. */
  std::string message;
  /** The line of the text the message is about, counted from one;
   *  unset when it is about the whole of what was refused. */
  std::optional<int> line;
  /** The refusing library's own number for this kind of refusal; unset
   *  when it names none. */
  std::optional<int> code;

  bool operator==(const Error&) const = default;
};

}  // namespace sigil::core
